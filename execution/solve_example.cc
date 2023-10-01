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

// A simple utility that prints the names of the problems in a dataset. If
// provided multiple filenames as arguments, these are read sequentially.

#include <fcntl.h>

#include <functional>
#include <iostream>
#include <string>
#include <tuple>
#include <vector>

#include "absl/flags/parse.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "contest_problem.pb.h"
#include "execution/py_locations.h"
#include "execution/py_tester_sandboxer.h"
#include "execution/status_macros.h"
#include "execution/tester_sandboxer.h"
#include "riegeli/bytes/fd_reader.h"
#include "riegeli/records/record_reader.h"

ABSL_FLAG(std::string, valid_path, "", "Path to validation dataset.");

namespace deepmind::code_contests {
namespace {

constexpr absl::string_view kGoodSolution = R"py(
t = int(input())
while t:
  n = int(input())
  print(2, n-1)
  t -= 1
)py";

constexpr absl::string_view kBadSolution = R"py(
t = int(input())
while t:
  n = int(input())
  if n > 20:
    print(1, 1)
  else:
    print(2, n-1)
  t -= 1
)py";

constexpr absl::string_view kInvalidSolution = ")";

absl::StatusOr<ContestProblem> FindGregorAndCryptography(
    const absl::string_view filename) {
  riegeli::RecordReader<riegeli::FdReader<>> reader(
      std::forward_as_tuple(filename));
  ContestProblem problem;
  while (reader.ReadRecord(problem)) {
    if (problem.name() == "1549_A. Gregor and Cryptography") return problem;
  }
  return absl::NotFoundError(
      "Gregor and Cryptography problem not found. Did you pass the "
      "validation dataset?");
}

std::vector<absl::string_view> GetInputs(const ContestProblem& problem,
                                         int max_size) {
  std::vector<absl::string_view> inputs;
  for (const auto& test : problem.public_tests()) {
    inputs.push_back(test.input());
  }
  for (const auto& test : problem.private_tests()) {
    inputs.push_back(test.input());
  }
  for (const auto& test : problem.generated_tests()) {
    inputs.push_back(test.input());
  }
  inputs.resize(max_size);
  return inputs;
}

std::vector<absl::string_view> GetOutputs(const ContestProblem& problem,
                                          int max_size) {
  std::vector<absl::string_view> outputs;
  for (const auto& test : problem.public_tests()) {
    outputs.push_back(test.output());
  }
  for (const auto& test : problem.private_tests()) {
    outputs.push_back(test.output());
  }
  for (const auto& test : problem.generated_tests()) {
    outputs.push_back(test.output());
  }
  outputs.resize(max_size);
  return outputs;
}

void ReportResults(const MultiTestResult& multi_result) {
  std::cout << "Compilation "
            << (multi_result.compilation_result.program_status ==
                        ProgramStatus::kSuccess
                    ? "succeeded"
                    : "failed")
            << "\n";
  int i = 0;
  for (const auto& test_result : multi_result.test_results) {
    if (!test_result.passed.has_value()) {
      std::cout << "Test " << i << " did not run.\n";
    } else if (*test_result.passed) {
      std::cout << "Test " << i << " passed.\n";
    } else {
      std::cout << "Test " << i << " failed.\n";
    }
    ++i;
  }
}

absl::Status SolveGregorAndCryptography(
    const absl::string_view valid_filename) {
  ASSIGN_OR_RETURN(ContestProblem gregor_and_cryptography,
                   FindGregorAndCryptography(valid_filename));
  const std::vector<absl::string_view> inputs =
      GetInputs(gregor_and_cryptography,
                /*max_size=*/10);
  const std::vector<absl::string_view> outputs =
      GetOutputs(gregor_and_cryptography,
                 /*max_size=*/10);

  Py3TesterSandboxer tester(Py3InterpreterPath(), Py3LibraryPaths());
  TestOptions options;
  options.num_threads = 4;
  options.stop_on_first_failure = true;

  std::cout << R"(We will try to solve "Gregor and Cryptography":
https://codeforces.com/problemset/problem/1549/A

We will run:
  1. A program that does not compile.
  2. A program that runs successfully, but gives the wrong answer sometimes.
  3. A correct solution.

--------------------------------------------------------------------------------
An invalid program is reported as not compiling:

)";
  ASSIGN_OR_RETURN(MultiTestResult invalid_result,
                   tester.Test(kInvalidSolution, inputs, options, outputs));
  ReportResults(invalid_result);

  std::cout << R"(
--------------------------------------------------------------------------------
The bad solution passes a few tests but then fails.
Because we set stop_on_first_failure to True, we stop once we see a failure.
We are running on 4 threads, so it's possible that more than one failure occurs
before all threads stop.

)";
  ASSIGN_OR_RETURN(MultiTestResult bad_result,
                   tester.Test(kBadSolution, inputs, options, outputs));
  ReportResults(bad_result);

  std::cout << R"(
--------------------------------------------------------------------------------
The good solution passes all tests.

)";

  ASSIGN_OR_RETURN(MultiTestResult good_result,
                   tester.Test(kGoodSolution, inputs, options, outputs));
  ReportResults(good_result);

  return absl::OkStatus();
}

}  // namespace
}  // namespace deepmind::code_contests

int main(int argc, char* argv[]) {
  absl::ParseCommandLine(argc, argv);
  const std::string filename = absl::GetFlag(FLAGS_valid_path);
  if (filename.empty()) {
    std::cerr << "The flag `valid_path` was empty and it should not be, please "
                 "pass `--valid_path=...` "
              << std::endl;
  } else {
    absl::Status status =
        deepmind::code_contests::SolveGregorAndCryptography(filename);
    if (!status.ok()) {
      std::cerr << "Failed: " << status.message() << std::endl;
    }
  }
}
