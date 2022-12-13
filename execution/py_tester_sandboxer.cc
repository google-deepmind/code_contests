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

#include "execution/py_tester_sandboxer.h"

#include <asm/unistd.h>
#include <stdio.h>
#include <sys/syscall.h>

#include <filesystem>
#include <fstream>
#include <memory>
#include <string>
#include <type_traits>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "execution/status_macros.h"
#include "execution/temp_path.h"
#include "execution/tester_sandboxer.h"
#include "farmhash.h"
#include "sandboxed_api/sandbox2/policy.h"
#include "sandboxed_api/sandbox2/policybuilder.h"
#include "sandboxed_api/sandbox2/result.h"
#include "sandboxed_api/sandbox2/sandbox2.h"

namespace deepmind::code_contests {

namespace {
constexpr absl::string_view kCodeFile = "code.py";
constexpr absl::string_view kBinaryFile = "code.pyc";
}  // namespace

Py3TesterSandboxer::Py3TesterSandboxer(
    const std::string& interpreter_path,
    const std::vector<std::string>& library_paths)
    : PyTesterSandboxer(
          /*compilation_command=*/{interpreter_path, "-m", "py_compile"},
          /*execution_command=*/{interpreter_path},
          /*library_paths=*/library_paths,
          /*code_preamble=*/"") {}

Py2TesterSandboxer::Py2TesterSandboxer(
    const std::string& interpreter_path,
    const std::vector<std::string>& library_paths)
    : PyTesterSandboxer(
          /*compilation_command=*/{interpreter_path, "-u", "-m", "py_compile"},
          /*execution_command=*/{interpreter_path},
          /*library_paths=*/library_paths,
          /*code_preamble=*/"\xef\xbb\xbf") {}

absl::StatusOr<ExecutionResult> PyTesterSandboxer::CompileCode(
    absl::string_view code, absl::string_view temp_path,
    absl::Duration max_compilation_duration) const {
  const std::filesystem::path temp_fs_path(temp_path);
  std::ofstream ofs(temp_fs_path / kCodeFile);
  ofs << absl::StrCat(code_preamble_, code);
  ofs.close();
  std::vector<std::string> compilation_command = compilation_command_;
  compilation_command.push_back((temp_fs_path / kCodeFile).string());

  ASSIGN_OR_RETURN(
      SandboxWithOutputFds sandbox_with_fds,
      CreateSandboxWithFds(
          /*command=*/compilation_command,
          /*stdin_data=*/"",
          /*ro_files=*/{},
          /*ro_dirs=*/{}, /*rw_dirs=*/{std::string(temp_path)},
          TestOptions{.max_execution_duration = max_compilation_duration}));
  if (!sandbox_with_fds.Sandbox().RunAsync()) {
    return absl::UnknownError("Failed to run sandbox on compilation.");
  }
  sandbox2::Result sandbox_result = sandbox_with_fds.Sandbox().AwaitResult();
  ExecutionResult execution_result =
      internal::ExecutionResultFromCompilationSandboxResult(sandbox_result);
  ASSIGN_OR_RETURN(execution_result.stdout, sandbox_with_fds.Stdout());
  ASSIGN_OR_RETURN(execution_result.stderr, sandbox_with_fds.Stderr());

  if (execution_result.program_status == ProgramStatus::kSuccess) {
    const std::string binary_path = (temp_fs_path / kBinaryFile).string();
    if (!std::filesystem::exists(binary_path)) {
      // The file may have been written to the pycache path instead.
      std::vector<std::string> matches;
      for (const std::filesystem::directory_entry& dir_entry :
           std::filesystem::directory_iterator(temp_fs_path / "__pycache__")) {
        matches.push_back(dir_entry.path());
      }
      if (matches.size() != 1) {
        return absl::InternalError(absl::StrCat(
            "Was not able to identify the compiled Python code "
            "file in ",
            (temp_fs_path / "__pycache__").string(), ".\n",
            "stdout from compilation: \"", execution_result.stdout,
            "\"\nstderr from compilation: \"", execution_result.stderr,
            "\"\nfiles found: ", absl::StrJoin(matches, ", ")));
      }
      std::filesystem::copy(matches.front(), binary_path);
    }

    std::string program_data;
    {
      std::ifstream ifs(temp_fs_path / kBinaryFile);
      std::stringstream buffer;
      buffer << ifs.rdbuf();
      program_data = buffer.str();
    }

    // Byte code includes the absolute path of the source file; replace this
    // with relative path to make byte code deterministic. Also strip the byte
    // preceding the path, which seems to change depending on the path
    // (checksum?).
    auto abs_path = (temp_fs_path / kCodeFile).string();
    program_data.replace(program_data.find(abs_path) - 1, abs_path.size() + 1,
                         kCodeFile);

    // Exclude the byte code header, which includes a timestamp.
    execution_result.program_hash =
        farmhash::Fingerprint64(program_data.substr(32));
  }

  return execution_result;
}

absl::StatusOr<SandboxWithOutputFds> PyTesterSandboxer::CreateTestSandbox(
    absl::string_view test_input, const TestOptions& test_options,
    absl::string_view temp_path) const {
  const std::filesystem::path temp_fs_path(temp_path);
  std::vector<std::string> execution_command = execution_command_;
  execution_command.push_back((temp_fs_path / kBinaryFile).string());
  return CreateSandboxWithFds(
      /*command=*/execution_command,
      /*stdin_data=*/test_input,
      /*ro_files=*/
      {(temp_fs_path / kCodeFile).string(),
       (temp_fs_path / kBinaryFile).string()},
      /*ro_dirs=*/{}, /*rw_dirs=*/{std::string(temp_path)}, test_options);
}

absl::StatusOr<std::unique_ptr<sandbox2::Policy>>
PyTesterSandboxer::CreatePolicy(absl::string_view binary_path,
                                const std::vector<std::string>& ro_files,
                                const std::vector<std::string>& ro_dirs,
                                const std::vector<std::string>& rw_dirs) const {
  sandbox2::PolicyBuilder builder = internal::CreateBasePolicy(
      binary_path,
      internal::Mappings{
          .ro_files = ro_files, .ro_dirs = ro_dirs, .rw_dirs = rw_dirs});

  builder.AllowSyscall(__NR_mprotect);
  builder.AllowSyscall(__NR_mremap);
#ifdef __NR_select
  builder.AllowSyscall(__NR_select);
#endif
  builder.AllowSyscall(__NR_pselect6);
#ifdef __NR_mkdir
  builder.AllowSyscall(__NR_mkdir);
#endif
  builder.AllowSyscall(__NR_mkdirat);
#ifdef __NR_rename
  builder.AllowSyscall(__NR_rename);
#endif
  builder.AllowSyscall(__NR_renameat);
  builder.AllowSyscall(__NR_renameat2);

  // Python fails if /dev/urandom is mounted as read-only, despite opening it as
  // O_RDONLY. For now, just mount it as read-write.
  builder.AddFile("/dev/urandom", /*is_ro=*/false);

  for (const std::string& path : library_paths_) {
    builder.AddDirectory(path);
  }

  std::filesystem::path cwd_path;
  {
    std::string cwd;
    CHECK(internal::GetCurrentWorkingDirectory(&cwd));
    cwd_path = cwd;
  }

  // Map the binary so it can be executed with execveat.
  builder.AddFileAt(cwd_path.append(binary_path).string(), "/dev/fd/1022");

  return builder.TryBuild();
}

}  // namespace deepmind::code_contests
