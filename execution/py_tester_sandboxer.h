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

#ifndef LEARNING_DEEPMIND_RESEARCH_CODEGEN_EXECUTION_PY_TESTER_SANDBOXER_H_
#define LEARNING_DEEPMIND_RESEARCH_CODEGEN_EXECUTION_PY_TESTER_SANDBOXER_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/span.h"
#include "execution/temp_path.h"
#include "execution/tester_sandboxer.h"
#include "sandboxed_api/sandbox2/policy.h"

namespace deepmind::code_contests {

class PyTesterSandboxer : public TesterSandboxer {
 public:
  // The commands should not include the code/binary filename, which will be
  // appended automatically.
  PyTesterSandboxer(absl::Span<const std::string> compilation_command,
                    absl::Span<const std::string> execution_command,
                    const std::vector<std::string>& library_paths,
                    std::string code_preamble)
      : compilation_command_(compilation_command.begin(),
                             compilation_command.end()),
        execution_command_(execution_command.begin(), execution_command.end()),
        library_paths_(library_paths.begin(), library_paths.end()),
        code_preamble_(std::move(code_preamble)) {}

 private:
  absl::StatusOr<ExecutionResult> CompileCode(
      absl::string_view code, absl::string_view temp_path,
      absl::Duration max_compilation_duration) const override;
  absl::StatusOr<SandboxWithOutputFds> CreateTestSandbox(
      absl::string_view test_input, const TestOptions& test_options,
      absl::string_view temp_path) const override;
  absl::StatusOr<std::unique_ptr<sandbox2::Policy>> CreatePolicy(
      absl::string_view binary_path, const std::vector<std::string>& ro_files,
      const std::vector<std::string>& ro_dirs,
      const std::vector<std::string>& rw_dirs) const override;

  std::vector<std::string> compilation_command_;
  std::vector<std::string> execution_command_;
  std::vector<std::string> library_paths_;
  std::string code_preamble_;
};

class Py3TesterSandboxer : public PyTesterSandboxer {
 public:
  explicit Py3TesterSandboxer(const std::string& interpreter_path,
                              const std::vector<std::string>& library_paths);
};

class Py2TesterSandboxer : public PyTesterSandboxer {
 public:
  explicit Py2TesterSandboxer(const std::string& interpreter_path,
                              const std::vector<std::string>& library_paths);
};

}  // namespace deepmind::code_contests

#endif  // LEARNING_DEEPMIND_RESEARCH_CODEGEN_EXECUTION_PY_TESTER_SANDBOXER_H_
