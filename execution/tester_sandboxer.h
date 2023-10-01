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

// A base class for implementing a sandbox for testing code.
//
// To implement a sandbox, inherit from TesterSandboxer and implement the three
// methods:
//   CreatePolicy: This should return a sandbox policy that allows read-write
//     access to the temp path.
//   CompileCode: This should compile the code provided, and write any binaries
//     to the temp path.
//   CreateTestSandbox: This should create the sandbox used to test an input.

#ifndef LEARNING_DEEPMIND_RESEARCH_CODEGEN_EXECUTION_TESTER_SANDBOXER_H_
#define LEARNING_DEEPMIND_RESEARCH_CODEGEN_EXECUTION_TESTER_SANDBOXER_H_

#include <stdint.h>
#include <stdio.h>

#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <string>
#include <tuple>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "execution/temp_path.h"
#include "sandboxed_api/sandbox2/policy.h"
#include "sandboxed_api/sandbox2/policybuilder.h"
#include "sandboxed_api/sandbox2/result.h"
#include "sandboxed_api/sandbox2/sandbox2.h"

namespace deepmind::code_contests {

enum class ProgramStatus { kUnknown, kSuccess, kFailed, kTimeout };

// The result of a single test execution.
struct ExecutionResult {
  // The status of the compilation and/or execution.
  ProgramStatus program_status = ProgramStatus::kUnknown;
  // The hash of the compiled program.
  // This hash is the same for all identical programs compiled with the same
  // binary, but the hash is not guaranteed to be the same when compiling with
  // different binaries (i.e. a hash is only stable within a sampling run).
  uint64_t program_hash = 0;
  // The stdout from the execution.
  std::string stdout;
  // The stderr from the compilation and execution.
  std::string stderr;
  // The execution's duration. Note that this does not
  // include the compilation time.
  absl::Duration execution_duration;
  // A string describing the sandbox result.
  std::string sandbox_result;
  // Whether the output passed, if we are checking outputs.
  std::optional<bool> passed;

  // Returns the equivalent of calling .ToStatus() on the sandbox result. Most
  // users will not need this functionality.
  absl::Status SandboxResultStatus() const;
};

// The result of a call to `Test` below. Consists of a single compilation result
// and a number of test results. If compilation fails, `test_results` will be
// empty. Otherwise it will be the same length as the number of test inputs.
struct MultiTestResult {
  ExecutionResult compilation_result;
  std::vector<ExecutionResult> test_results;
};

std::ostream& operator<<(std::ostream& os, const ExecutionResult& result);
std::ostream& operator<<(std::ostream& os, const MultiTestResult& multi_result);

/* Default to limit of 256 MiB */
inline constexpr int64_t kDefaultMemoryLimitBytes = INT64_C(256) << 20;

struct TestOptions {
  absl::Duration max_execution_duration = absl::Seconds(10);
  int num_threads = 1;
  int64_t memory_limit_bytes = kDefaultMemoryLimitBytes;
  bool stop_on_first_failure = false;
};

// A class that holds a sandbox, with (optional) file descriptors for its
// stdout and stderr. The file descriptors are closed when they are read from,
// or when this object is destroyed, and both stdout and stderr are cached on
// reading, so can be read multiple times.
class SandboxWithOutputFds {
 public:
  explicit SandboxWithOutputFds(std::unique_ptr<sandbox2::Sandbox2> sandbox,
                                int stdout_fd = kInvalidFd,
                                int stderr_fd = kInvalidFd);
  virtual ~SandboxWithOutputFds();

  // Disable copying.
  SandboxWithOutputFds(const SandboxWithOutputFds&) = delete;
  SandboxWithOutputFds& operator=(const SandboxWithOutputFds&) = delete;

  // Enable moving.
  SandboxWithOutputFds(SandboxWithOutputFds&& other);
  SandboxWithOutputFds& operator=(SandboxWithOutputFds&& other);

  absl::StatusOr<std::string> Stdout();
  absl::StatusOr<std::string> Stderr();
  sandbox2::Sandbox2& Sandbox() { return *sandbox_; }

  static constexpr int kInvalidFd = -1;

 private:
  std::unique_ptr<sandbox2::Sandbox2> sandbox_;
  int stdout_fd_;
  int stderr_fd_;
  std::optional<absl::StatusOr<std::string>> stdout_cache_;
  std::optional<absl::StatusOr<std::string>> stderr_cache_;
};

// Returns a copy of the environment variables for the current process.
std::vector<std::string> CopyEnviron();

// Checks whether output is the same, up to whitespace and floating point
// errors.
bool OutputsMatch(absl::string_view output, absl::string_view expected);

// The TesterSandboxer class can execute tests with any suitable sandboxees.
//
// The control flow is as follows:
//   1. The code is written to a temporary directory. The sandbox has read/write
//      access to this directory.
//   2. The compilation command (with the code file appended) is executed in the
//      sandbox.
//   3. If the compilation was successful, the execution command (again with the
//      code file appended) is executed in the sandbox.
class TesterSandboxer {
 public:
  TesterSandboxer();
  virtual ~TesterSandboxer() {}

  // Compiles and executes some test code on the provided input. A non-OK status
  // is returned in the event that we were unable to evaluate the code. Note
  // that a result is returned for compilation errors due to invalid code and
  // for code that raises at runtime.
  //
  // If expected_test_outputs are provided, they should be the same length as
  // the test_inputs, and an outputs_match predicate should also be provided.
  // Providing these is required to support stop_on_first_failure.
  absl::StatusOr<MultiTestResult> Test(
      absl::string_view code, const std::vector<absl::string_view>& test_inputs,
      const TestOptions& test_options = TestOptions(),
      const std::vector<absl::string_view>& expected_test_outputs = {},
      std::function<bool(std::string_view a, std::string_view b)>
          compare_outputs = OutputsMatch) const;

 protected:
  absl::StatusOr<SandboxWithOutputFds> CreateSandboxWithFds(
      const std::vector<std::string>& command, absl::string_view stdin_data,
      const std::vector<std::string>& ro_files,
      const std::vector<std::string>& ro_dirs,
      const std::vector<std::string>& rw_dirs, const TestOptions& test_options,
      const std::string& cwd = "",
      const std::vector<std::string>& env = CopyEnviron()) const;
  // Compiles `code`, writing output (such as a binary) to `temp_path`.
  virtual absl::StatusOr<ExecutionResult> CompileCode(
      absl::string_view code, absl::string_view temp_path,
      absl::Duration max_compilation_duration) const = 0;
  // Creates a sandbox for running the previously compiled code on `test_input`.
  // It should not start the sandbox; i.e. should not call RunAsync().
  virtual absl::StatusOr<SandboxWithOutputFds> CreateTestSandbox(
      absl::string_view test_input, const TestOptions& test_options,
      absl::string_view temp_path) const = 0;
  // Returns a policy for sandboxes. These should have permission to read the
  // code and binary, as well as read-write access to `temp_path`.
  virtual absl::StatusOr<std::unique_ptr<sandbox2::Policy>> CreatePolicy(
      absl::string_view binary_path, const std::vector<std::string>& ro_files,
      const std::vector<std::string>& ro_dirs,
      const std::vector<std::string>& rw_dirs) const = 0;

 private:
  // Runs the previously compiled code on `test_input`.
  absl::StatusOr<ExecutionResult> RunCodeOnInput(
      absl::string_view test_input, const TestOptions& test_options,
      absl::string_view temp_path) const;
};

namespace internal {

struct Mappings {
  std::vector<std::string> ro_files;
  std::vector<std::string> rw_files;
  std::vector<std::string> ro_dirs;
  std::vector<std::string> rw_dirs;
};

// Creates a sandbox2 policy for executing generated programs in a subprocess.
sandbox2::PolicyBuilder CreateBasePolicy(absl::string_view binary,
                                         const Mappings& mappings);

// These are useful helper functions for filling in test results.

// Converts a sandbox result from a compilation into a ExecutionResult struct.
ExecutionResult ExecutionResultFromCompilationSandboxResult(
    const sandbox2::Result& sandbox_result);

// Converts a sandbox result from running a test into a ExecutionResult struct.
ExecutionResult ExecutionResultFromTestSandboxResult(
    const sandbox2::Result& sandbox_result);

inline bool GetCurrentWorkingDirectory(std::string* s) {
  constexpr size_t len = 1ul << 16;
  auto buffer = std::make_unique<char[]>(len);
  char* p = getcwd(buffer.get(), len);
  if (p == nullptr) {
    return false;
  }
  *s = p;
  return true;
}

}  // namespace internal

}  // namespace deepmind::code_contests

#endif  // LEARNING_DEEPMIND_RESEARCH_CODEGEN_EXECUTION_TESTER_SANDBOXER_H_
