// Copyright 2022 DeepMind Technologies Limited
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "execution/tester_sandboxer.h"

#include <errno.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/resource.h>
#include <unistd.h>

#include <algorithm>
#include <complex>
#include <csignal>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/algorithm/container.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "execution/status_macros.h"
#include "execution/temp_path.h"
#include "sandboxed_api/sandbox2/buffer.h"
#include "sandboxed_api/sandbox2/executor.h"
#include "sandboxed_api/sandbox2/policy.h"
#include "sandboxed_api/sandbox2/policybuilder.h"
#include "sandboxed_api/sandbox2/result.h"
#include "sandboxed_api/sandbox2/sandbox2.h"
#include "sandboxed_api/sandbox2/util/bpf_helper.h"
#include "execution/simple_threadpool.h"

// Defined as extern per https://man7.org/linux/man-pages/man7/environ.7.html.
extern char** environ;

namespace deepmind::code_contests {

namespace {

// The max compilation time is not currently configurable. Hopefully 60 seconds
// is more than enough time for our programs.
constexpr absl::Duration kMaxCompilationDuration = absl::Seconds(60);

// Number of retries in the case of failures during testing. This is empirically
// enough to deflake in testing.
constexpr int kMaxTestAttempts = 3;

absl::StatusOr<ExecutionResult> RetryIfFail(
    std::function<absl::StatusOr<ExecutionResult>()> fn) {
  absl::StatusOr<ExecutionResult> result =
      absl::InternalError("No attempts made.");
  for (int retry = 0; retry < kMaxTestAttempts; ++retry) {
    result = fn();
    // We don't retry cancellations to allow stopping on first failure.
    if (result.ok() || result.status().code() == absl::StatusCode::kCancelled) {
      break;
    }
    // Otherwise, it's possible that this is an ephemeral issue such as not
    // having available file descriptors. We retry after a brief pause.
    absl::SleepFor(absl::Milliseconds(2));
  }
  return result;
}

ssize_t BlockingReadIgnoringInterruptions(int fd, char* buf, size_t count) {
  ssize_t bytes_read;
  do {
    bytes_read = ::read(fd, buf, count);
  } while (bytes_read == -1 && (errno == EINTR || errno == EAGAIN));
  return bytes_read;
}

absl::StatusOr<std::string> ReadFd(int fd) {
  std::string contents;
  constexpr int64_t buffer_size = 4096;
  const auto buffer = std::make_unique<char[]>(buffer_size);
  for (;;) {
    const ssize_t n =
        BlockingReadIgnoringInterruptions(fd, buffer.get(), buffer_size);
    if (n < 0) {
      return absl::UnknownError(
          absl::Substitute("Reading FD $0 failed with errno $1", fd, errno));
    }
    if (n == 0) {
      return contents;
    }
    contents.append(buffer.get(), n);
  }
}

absl::StatusOr<std::string> UseCacheOrReadAndClose(
    int& fd, std::optional<absl::StatusOr<std::string>>& cache) {
  if (cache.has_value()) {
    return *cache;
  }
  if (fd == SandboxWithOutputFds::kInvalidFd) {
    return absl::FailedPreconditionError("File descriptor not set.");
  }
  cache = ReadFd(fd);
  close(fd);
  fd = SandboxWithOutputFds::kInvalidFd;
  return *cache;
}

std::vector<std::string> SplitAndLowercase(absl::string_view s) {
  std::vector<std::string> parts =
      absl::StrSplit(s, absl::ByAnyChar(" \n\t\r\v"), absl::SkipEmpty());
  std::vector<std::string> lower;
  std::transform(parts.begin(), parts.end(), std::back_inserter(lower),
                 [](const std::string& s) -> std::string {
                   return absl::AsciiStrToLower(s);
                 });
  return lower;
}

bool ValuesMatch(absl::string_view a, absl::string_view b) {
  constexpr double kDoublePrecision = 1e-5;
  int ai, bi;
  double ad, bd;
  const bool a_is_int = absl::SimpleAtoi(a, &ai);
  const bool b_is_int = absl::SimpleAtoi(b, &bi);
  const bool a_is_double = absl::SimpleAtod(a, &ad);
  const bool b_is_double = absl::SimpleAtod(b, &bd);
  const bool a_is_string = (!a_is_int) && (!a_is_double);
  const bool b_is_string = (!b_is_int) && (!b_is_double);
  if (a_is_string || b_is_string) {
    return a == b;
  }
  // Both are numeric values: tolerate up to a precision for floats.
  if (a_is_double || b_is_double) {
    return std::abs(ad - bd) < kDoublePrecision;
  }
  // Both integers - require equality.
  return ai == bi;
}

}  // namespace

absl::Status ExecutionResult::SandboxResultStatus() const {
  switch (program_status) {
    case ProgramStatus::kUnknown:
      return absl::FailedPreconditionError("program_status is unknown.");
    case ProgramStatus::kSuccess:
      return absl::OkStatus();
    case ProgramStatus::kTimeout:
      return absl::DeadlineExceededError(sandbox_result);
    case ProgramStatus::kFailed:
      return absl::InternalError(sandbox_result);
  }
}

std::ostream& operator<<(std::ostream& os, const ExecutionResult& result) {
  os << "Execution Result: \n"
     << "  status: " << static_cast<int>(result.program_status) << "\n"
     << "  program hash: " << result.program_hash << "\n"
     << "  stdout: \"" << result.stdout << "\"\n"
     << "  stderr: \"" << result.stderr << "\"\n"
     << "  duration: " << result.execution_duration << "\n"
     << "  sandbox result: \"" << result.sandbox_result << "\"\n"
     << "  passed: " << (result.passed ? "true" : "false") << "\n";
  return os;
}

std::ostream& operator<<(std::ostream& os,
                         const MultiTestResult& multi_result) {
  os << "MultiTestResult:\n"
     << "  Compilation result:\n"
     << multi_result.compilation_result << "\n\n";
  int index = 0;
  for (const ExecutionResult& result : multi_result.test_results) {
    os << "  Test Result " << index++ << ":\n";
    os << result << "\n\n";
  }
  return os;
}

SandboxWithOutputFds::SandboxWithOutputFds(
    std::unique_ptr<sandbox2::Sandbox2> sandbox, int stdout_fd, int stderr_fd)
    : sandbox_(std::move(sandbox)),
      stdout_fd_(stdout_fd),
      stderr_fd_(stderr_fd) {}

SandboxWithOutputFds::~SandboxWithOutputFds() {
  if (stdout_fd_ != kInvalidFd) {
    close(stdout_fd_);
  }
  if (stderr_fd_ != kInvalidFd) {
    close(stderr_fd_);
  }
}

SandboxWithOutputFds::SandboxWithOutputFds(SandboxWithOutputFds&& other)
    : sandbox_(std::move(other.sandbox_)),
      stdout_fd_(other.stdout_fd_),
      stderr_fd_(other.stderr_fd_),
      stdout_cache_(std::move(other.stdout_cache_)),
      stderr_cache_(std::move(other.stderr_cache_)) {
  other.stdout_fd_ = kInvalidFd;
  other.stderr_fd_ = kInvalidFd;
}

SandboxWithOutputFds& SandboxWithOutputFds::operator=(
    SandboxWithOutputFds&& other) {
  sandbox_ = std::move(other.sandbox_);
  stdout_fd_ = other.stdout_fd_;
  stderr_fd_ = other.stderr_fd_;
  stdout_cache_ = std::move(other.stdout_cache_);
  stderr_cache_ = std::move(other.stderr_cache_);
  other.stdout_fd_ = kInvalidFd;
  other.stderr_fd_ = kInvalidFd;
  return *this;
}

absl::StatusOr<std::string> SandboxWithOutputFds::Stdout() {
  return UseCacheOrReadAndClose(stdout_fd_, stdout_cache_);
}

absl::StatusOr<std::string> SandboxWithOutputFds::Stderr() {
  return UseCacheOrReadAndClose(stderr_fd_, stderr_cache_);
}

std::vector<std::string> CopyEnviron() {
  return sandbox2::util::CharPtrArray(environ).ToStringVector();
}

bool OutputsMatch(absl::string_view output, absl::string_view expected) {
  std::vector<std::string> output_parts = SplitAndLowercase(output);
  std::vector<std::string> expected_parts = SplitAndLowercase(expected);
  if (output_parts == expected_parts) return true;
  if (output_parts.size() != expected_parts.size()) return false;
  return std::transform_reduce(output_parts.begin(), output_parts.end(),
                               expected_parts.begin(), true,
                               std::logical_and<>(), ValuesMatch);
}

TesterSandboxer::TesterSandboxer() {
  // It's important that we ignore SIGPIPE, which can be caused when the sandbox
  // is terminated (e.g. due to a violation).
  signal(SIGPIPE, SIG_IGN);
  // Disable reporting as much as possible. We generate a lot of bad programs.
}

absl::StatusOr<SandboxWithOutputFds> TesterSandboxer::CreateSandboxWithFds(
    const std::vector<std::string>& command, absl::string_view stdin_data,
    const std::vector<std::string>& ro_files,
    const std::vector<std::string>& ro_dirs,
    const std::vector<std::string>& rw_dirs, const TestOptions& test_options,
    const std::string& cwd, const std::vector<std::string>& env) const {
  if (command.empty()) {
    return absl::InvalidArgumentError("Empty command provided");
  }
  auto executor =
      std::make_unique<sandbox2::Executor>(command[0], command, env);
  if (!cwd.empty()) {
    executor->set_cwd(cwd);
  }

  executor->set_enable_sandbox_before_exec(true)
      .limits()
      // Restrictions on the size of address-space of sandboxed processes, to
      // limit memory usage. Limit is set to the limit of the test + 32 MB for
      // the interpreter / binary itself.
      ->set_rlimit_as(sapi::sanitizers::IsAny()
                          ? RLIM64_INFINITY
                          : test_options.memory_limit_bytes + (32ULL << 20))
      // Don't create core files.
      .set_rlimit_core(0)
      // Kill sandboxed processes with a signal (SIGXFSZ) if it writes more than
      // these many bytes to the file-system
      .set_rlimit_fsize(64ULL << 20)  // 64 MiB
      .set_rlimit_cpu(std::max<int64_t>(
          1, absl::ToInt64Seconds(test_options.max_execution_duration)));

  if (!stdin_data.empty()) {
    // We must only create the buffer if there is data, as buffers of size zero
    // are not supported.
    ASSIGN_OR_RETURN(std::unique_ptr<sandbox2::Buffer> input_buffer,
                     sandbox2::Buffer::CreateWithSize(stdin_data.size()));
    absl::c_copy(stdin_data, input_buffer->data());
    // The dup here is very important. MapFd takes ownership of the FD in its
    // first argument. But buffer expects to retain ownership. We get around
    // this by duplicating the file descriptor.
    executor->ipc()->MapFd(dup(input_buffer->fd()), STDIN_FILENO);
  } else {
    // Otherwise, we explicitly close the stdin pipe.
    const int stdout_fd = executor->ipc()->ReceiveFd(STDIN_FILENO);
    close(stdout_fd);
  }

  const int stdout_fd = executor->ipc()->ReceiveFd(STDOUT_FILENO);
  const int stderr_fd = executor->ipc()->ReceiveFd(STDERR_FILENO);

  ASSIGN_OR_RETURN(std::unique_ptr<sandbox2::Policy> policy,
                   CreatePolicy(command[0], ro_files, ro_dirs, rw_dirs));
  return SandboxWithOutputFds(std::make_unique<sandbox2::Sandbox2>(
                                  std::move(executor), std::move(policy)),
                              stdout_fd, stderr_fd);
}

// Test makes multiple attempts to test the code. This is because our sandbox
// uses a large number of file descriptors (especially when multiple threads are
// running a sandbox). Because these are a global resource that are often in
// contention (especially on testing machines), this causes occasional flakes.
// Retrying seems to be enough to prevent these.
absl::StatusOr<MultiTestResult> TesterSandboxer::Test(
    absl::string_view code, const std::vector<absl::string_view>& test_inputs,
    const TestOptions& test_options,
    const std::vector<absl::string_view>& expected_test_outputs,
    std::function<bool(std::string_view a, std::string_view b)> compare_outputs)
    const {
  const bool checking_outputs = !expected_test_outputs.empty();
  if (checking_outputs) {
    if (test_inputs.size() != expected_test_outputs.size()) {
      return absl::InvalidArgumentError(
          absl::Substitute("Inputs and expected outputs must have the same "
                           "length. Actual lengths: $0 v $1.",
                           test_inputs.size(), expected_test_outputs.size()));
    }
  }
  if (test_options.stop_on_first_failure && !checking_outputs) {
    return absl::InvalidArgumentError(
        "stop_on_first_failure does not work if expected outputs are not "
        "provided.");
  }
  MultiTestResult multi_test_result;
  std::unique_ptr<TempPath> temp_path = std::make_unique<TempPath>();
  if (!temp_path) {
    return absl::UnknownError("Unable to create temporary directory for code.");
  }
  // Compile and return if unsuccessful.
  ASSIGN_OR_RETURN(multi_test_result.compilation_result, RetryIfFail([&] {
                     return CompileCode(code, temp_path->path(),
                                        kMaxCompilationDuration);
                   }));
  if (multi_test_result.compilation_result.program_status !=
      ProgramStatus::kSuccess) {
    return multi_test_result;
  }

  multi_test_result.test_results.resize(test_inputs.size());
  absl::Status overall_status;
  absl::Mutex output_mutex;
  // If we should stop on first failure, we set this on failures. We always set
  // it on failures to execute.
  bool should_stop = false;

  {
    ThreadPool pool(test_options.num_threads);
    pool.StartWorkers();
    for (int i = 0; i < test_inputs.size(); ++i) {
      pool.Schedule([&, i] {
        absl::StatusOr<ExecutionResult> test_result =
            RetryIfFail([&]() -> absl::StatusOr<ExecutionResult> {
              {
                absl::ReaderMutexLock l(&output_mutex);
                if (should_stop) {
                  return absl::CancelledError("should_stop");
                }
              }
              return RunCodeOnInput(test_inputs[i], test_options,
                                    temp_path->path());
            });
        if (test_result.status().code() == absl::StatusCode::kCancelled) {
          return;
        }
        absl::MutexLock l(&output_mutex);
        overall_status.Update(test_result.status());
        if (!test_result.ok()) {
          // If we see a not-OK status, we are not going to return any results,
          // so should stop immediately.
          should_stop = true;
        } else if (checking_outputs) {
          const bool matches =
              compare_outputs(test_result->stdout, expected_test_outputs[i]);
          if (test_options.stop_on_first_failure && !matches) {
            should_stop = true;
          }
          test_result->passed = matches;
        }
        if (test_result.ok()) {
          multi_test_result.test_results[i] = *std::move(test_result);
        }
      });
    }
  }

  RETURN_IF_ERROR(overall_status);

  return multi_test_result;
}

absl::StatusOr<ExecutionResult> TesterSandboxer::RunCodeOnInput(
    absl::string_view test_input, const TestOptions& test_options,
    absl::string_view temp_path) const {
  ASSIGN_OR_RETURN(SandboxWithOutputFds sandbox_with_fds,
                   CreateTestSandbox(test_input, test_options, temp_path));
  const absl::Time start_time = absl::Now();
  if (!sandbox_with_fds.Sandbox().RunAsync()) {
    return absl::UnknownError("Failed to run sandbox on execution.");
  }
  // Set a wall time limit to guard against code that sleeps forever.
  sandbox_with_fds.Sandbox().set_walltime_limit(
      test_options.max_execution_duration * 30);
  absl::StatusOr<std::string> stdout_contents;
  absl::StatusOr<std::string> stderr_contents;
  {
    ThreadPool pool(2);
    pool.StartWorkers();
    pool.Schedule([&] { stdout_contents = sandbox_with_fds.Stdout(); });
    pool.Schedule([&] { stderr_contents = sandbox_with_fds.Stderr(); });
  }
  RETURN_IF_ERROR(stdout_contents.status());
  RETURN_IF_ERROR(stderr_contents.status());
  sandbox2::Result result = sandbox_with_fds.Sandbox().AwaitResult();
  const absl::Time end_time = absl::Now();
  ExecutionResult execution_result =
      internal::ExecutionResultFromTestSandboxResult(result);
  execution_result.stdout = *std::move(stdout_contents);
  execution_result.stderr = *std::move(stderr_contents);
  execution_result.execution_duration = end_time - start_time;
  return execution_result;
}

namespace internal {

// MADV_FREE is not defined in all versions
#ifndef MADV_FREE
#define MADV_FREE 0x8
#endif

sandbox2::PolicyBuilder CreateBasePolicy(absl::string_view binary,
                                         const Mappings& mappings) {
  sandbox2::PolicyBuilder builder;

  // Must be before AllowStaticStartup.
#ifdef __NR_readlink
  builder.AllowSyscall(__NR_readlink);
#endif
  builder.AllowSyscall(__NR_readlinkat);

  // Allow general, safe syscalls.
  builder.AllowExit();           // process/thread: exit, exit_group
  builder.AllowGetIDs();         // process/thread: getuid, getgid, etc.
  builder.AllowGetRandom();      // getrandom
  builder.AllowWipeOnFork();     // madvise(_, _, -1 | MADV_WIPEONFORK)
  builder.AllowHandleSignals();  // rt_sigaction, rt_sigreturn, etc.
  builder.AllowOpen();           // files: open, openat
  builder.AllowRead();           // files: read, readv, preadv, pread64
  builder.AllowReaddir();        // files: getdents, getdents64
  builder.AllowStat();           // files: stat, fstat, lstat, etc.
  builder.AllowStaticStartup();  // allows starting statically linked binary
  builder.AllowTcMalloc();       // tcmalloc malloc, free
  builder.AllowTime();           // time, gettimeofday, clock_gettime
  builder.AllowWrite();          // files: write, writev, pwritev, pwrite64
  builder.AllowSafeFcntl();

  // Allow the process to get its own thread and process IDs.
  builder.AllowSyscalls({__NR_gettid, __NR_getpid});

  // Allow the process to yield the CPU.
  builder.AllowSyscall(__NR_sched_yield);

  // Allow querying for CPU affinities. This is used by libuv to determine
  // number of runnable CPUs.
  builder.AllowSyscall(__NR_sched_getaffinity);

  // Miscellaneous filesystem syscalls not covered above.
  builder.AllowSyscalls({
#ifdef __NR_access
      __NR_access,
#endif
      __NR_faccessat,
      __NR_close,
      __NR_lseek,
      __NR_getcwd,
      __NR_pipe2,
      __NR_ioctl,
#ifdef __NR_unlink
      __NR_unlink,
#endif
      __NR_unlinkat,
      __NR_dup,
#ifdef __NR_poll
      __NR_poll,
#endif
      __NR_ppoll,
      __NR_statx,
  });

  // Event syscalls are needed for Node's async functionality.
  builder.AllowSyscalls({
      __NR_eventfd2,
      __NR_epoll_create1,
      __NR_epoll_ctl,
      __NR_epoll_pwait,
#ifdef __NR_epoll_wait
      __NR_epoll_wait,
#endif
  });

  // syscall needed for Node's available memory check
  builder.AllowSyscalls({__NR_sysinfo});

  // mmap and mprotect are needed to load system libraries and for V8.
  builder.AllowSyscall(__NR_mmap);
  builder.AddPolicyOnSyscall(__NR_mprotect,
                             {
                                 // mprotect shouldn't be used to make memory
                                 // simultaneously writable and executable.
                                 ARG_32(2),
                                 JEQ32(PROT_READ | PROT_EXEC, ALLOW),
                                 JEQ32(PROT_READ | PROT_WRITE, ALLOW),
                                 JEQ32(PROT_READ, ALLOW),
                                 JEQ32(PROT_NONE, ALLOW),
                             });

  // These socket syscalls depend on socket() succeeding, which we've restricted
  // above, so we do not need to restrict these individual syscalls.
  builder.AllowSyscalls({
      __NR_setsockopt,
      __NR_bind,
      __NR_listen,
      __NR_accept4,
      __NR_connect,
      __NR_shutdown,
      __NR_getsockopt,
      __NR_getsockname,
  });

  // Allow use of threads through a restricted clone() syscall.
  builder.AddPolicyOnSyscall(
      __NR_clone, {
                      ARG_32(0),
                      // Allowed argument values based on output from strace.
                      JEQ32((CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND |
                             CLONE_THREAD | CLONE_SYSVSEM | CLONE_SETTLS |
                             CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID),
                            ALLOW),
                  });

  builder.AllowSyscalls({
      __NR_futex,    // "fast userspace mutex"
      __NR_munlock,  // Allows memory to be paged to the swap area.
  });

  builder.AddPolicyOnSyscall(__NR_prctl, {
                                             ARG_32(0),
                                             JEQ32(PR_SET_NAME, ALLOW),
                                         });

  // Miscellaneous time/clock syscalls not covered above.
  builder.AllowSyscalls({__NR_nanosleep, __NR_clock_getres});

  // Only allow reading resource limits for this PID.
  builder.AddPolicyOnSyscall(
      __NR_prlimit64, [](auto& labels) -> std::vector<sock_filter> {
        return {
            ARG(0),
            JNE(0,
                JUMP(&labels, pid_not_zero)),  // pid != 0 (not this process).
            ARG(2),
            JEQ(0, ALLOW),  // new_limit == nullptr
            LABEL(&labels, pid_not_zero),
        };
      });

  // Allow processes to mark a page free but not immediately deallocate it.
  builder.AddPolicyOnSyscall(__NR_madvise, {
                                               ARG_32(2),
                                               JEQ32(MADV_FREE, ALLOW),
                                           });

  // Files used to gather information about the running process.
  builder.AddFile("/proc/version");
  builder.AddFile("/proc/cpuinfo");
  builder.AddFile("/proc/stat");

  std::filesystem::path cwd_path;
  {
    std::string cwd;
    CHECK(internal::GetCurrentWorkingDirectory(&cwd));
    cwd_path = cwd;
  }

  for (const auto& f : mappings.ro_files) {
    builder.AddFile((cwd_path / f).string(), /*is_ro=*/true);
  }
  for (const auto& f : mappings.rw_files) {
    builder.AddFile((cwd_path / f).string(), /*is_ro=*/false);
  }
  for (const auto& d : mappings.ro_dirs) {
    builder.AddDirectory((cwd_path / d).string(), /*is_ro=*/true);
  }
  for (const auto& d : mappings.rw_dirs) {
    builder.AddDirectory((cwd_path / d).string(),
                         /*is_ro=*/false);
  }

  builder.AddLibrariesForBinary(binary);

  builder.AllowLlvmSanitizers();

  // Disables stack traces on violations, crashes, timeouts and signals.
  // Done to reduce the amount of unnecessary clutter in the output logs.
  builder.CollectStacktracesOnViolation(false);
  builder.CollectStacktracesOnSignal(false);
  builder.CollectStacktracesOnTimeout(false);
  builder.CollectStacktracesOnKill(false);

  return builder;
}

ExecutionResult ExecutionResultFromCompilationSandboxResult(
    const sandbox2::Result& sandbox_result) {
  ExecutionResult execution_result;
  execution_result.program_status =
      (sandbox_result.final_status() != sandbox2::Result::OK ||
       sandbox_result.reason_code() != 0)
          ? ProgramStatus::kFailed
          : ProgramStatus::kSuccess;
  execution_result.sandbox_result = sandbox_result.ToString();
  return execution_result;
}

ExecutionResult ExecutionResultFromTestSandboxResult(
    const sandbox2::Result& sandbox_result) {
  ExecutionResult execution_result;
  if (sandbox_result.final_status() == sandbox2::Result::TIMEOUT) {
    execution_result.program_status = ProgramStatus::kTimeout;
  } else if (sandbox_result.final_status() == sandbox2::Result::OK &&
             sandbox_result.reason_code() == 0) {
    execution_result.program_status = ProgramStatus::kSuccess;
  } else if (sandbox_result.final_status() == sandbox2::Result::SIGNALED) {
    // Sandboxee was killed because it violated one of the rlimits, usually this
    // is the cpu time limit.
    execution_result.program_status = ProgramStatus::kTimeout;
  } else {
    execution_result.program_status = ProgramStatus::kFailed;
  }
  execution_result.sandbox_result = sandbox_result.ToString();
  return execution_result;
}

}  // namespace internal

}  // namespace deepmind::code_contests
