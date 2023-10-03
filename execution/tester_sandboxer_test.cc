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

#include <unistd.h>

#include <cstdint>
#include <cstdio>
#include <functional>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/algorithm/container.h"
#include "absl/base/log_severity.h"
#include "absl/flags/flag.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "execution/py_locations.h"
#include "execution/py_tester_sandboxer.h"
#include "execution/status_macros.h"
#include "execution/status_matchers.h"
#include "sandboxed_api/sandbox2/sandbox2.h"
#include "execution/simple_threadpool.h"

ABSL_FLAG(bool, test_py2, false,
          "Whether to test python2. Requires a working python2 binary to be "
          "installed.");

namespace deepmind::code_contests {
namespace {

using ::testing::AllOf;
using ::testing::Each;
using ::testing::ElementsAre;
using ::testing::ExplainMatchResult;
using ::testing::SizeIs;

// Matchers for MultiTestResult
MATCHER_P(CompilationResultMatches, matcher, "") {
  return ExplainMatchResult(matcher, arg.compilation_result, result_listener);
}
MATCHER_P(TestResultsMatches, matcher, "") {
  return ExplainMatchResult(matcher, arg.test_results, result_listener);
}

// Matchers for ExecutionResult
MATCHER_P(HasProgramStatus, value, "") { return arg.program_status == value; }
MATCHER_P(HasStdout, value, "") { return arg.stdout == value; }
MATCHER_P(HasStderr, value, "") { return arg.stderr == value; }
MATCHER_P(HasStderrSubstring, value, "") {
  return absl::StrContains(arg.stderr, value);
}
MATCHER_P2(HasDurationBetween, low, high, "") {
  return low < arg.execution_duration && arg.execution_duration < high;
}

std::string CreateLargeInput() {
  std::string long_string;
  for (int i = 0; i < 1000; ++i) {
    for (int j = 0; j < 10; ++j) {
      absl::StrAppend(&long_string, "this is long. ");
    }
    absl::StrAppend(&long_string, "\n");
  }
  return long_string;
}

// A struct representing inputs for tests that should pass for every language.
// To add a new language, add an additional entry to the test instantiation
// below.
struct LanguageTestParams {
  // Name of the language
  std::string name;
  // A function that returns a TesterSandboxer for the language.
  std::function<std::unique_ptr<TesterSandboxer>()> init;
  // A program that prints "hello\n"
  std::string hello;
  // A program that copies stdin to stdout.
  std::string cat;
  // A program that copies stdin to stderr.
  std::string cat_to_stderr;
  // A program that fails to compile.
  std::string bad_syntax;
  // A substring of the expected stderr when compiling the above program.
  std::string bad_syntax_error;
  // A program that raises an assertion.
  std::string asserts;
  // A program that loops forever.
  std::string loops_forever;
  // A program that sleeps for two seconds.
  std::string sleeps_2_seconds;
  // A program that contains unicode.
  std::string has_unicode;
  // A program that attempts to chdir to /tmp.
  std::string does_chdir;
};

std::ostream& operator<<(std::ostream& os, const LanguageTestParams& params) {
  os << "name:\n"
     << params.name << "\n\n"
     << "hello:\n"
     << params.hello << "\n\n"
     << "cat:\n"
     << params.cat << "\n\n"
     << "cat_to_stderr:\n"
     << params.cat_to_stderr << "\n\n"
     << "bad_syntax:\n"
     << params.bad_syntax << "\n\n"
     << "bad_syntax_error:\n"
     << params.bad_syntax_error << "\n\n"
     << "asserts:\n"
     << params.asserts << "\n\n"
     << "loops_forever:\n"
     << params.loops_forever << "\n\n"
     << "sleeps_2_seconds:\n"
     << params.sleeps_2_seconds << "\n\n"
     << "has_unicode:\n"
     << params.has_unicode << "\n\n"
     << "does_chdir:\n"
     << params.does_chdir << "\n\n";
  return os;
}

class TesterSandboxerLanguageTest
    : public testing::TestWithParam<LanguageTestParams> {};

// Add new languages here.
std::vector<LanguageTestParams> LanguageSweep() {
  LanguageTestParams py3{
      .name = "py3",
      .init =
          []() {
            return std::make_unique<Py3TesterSandboxer>(Py3InterpreterPath(),
                                                        Py3LibraryPaths());
          },
      .hello = "print('hello')",
      .cat = R"py(
import sys
while True:
  s = sys.stdin.read(1024)
  if not s:
    break
  sys.stdout.write(s)
            )py",
      .cat_to_stderr = R"py(
import sys
while True:
  s = sys.stdin.read(1024)
  if not s:
    break
  sys.stderr.write(s)
            )py",
      .bad_syntax = ")",
      .bad_syntax_error = "SyntaxError",
      .asserts = "assert 1 == 2",
      .loops_forever = R"py(
import math
x = 1.
while True:
  x += math.sin(x)
)py",
      .sleeps_2_seconds = "import time; time.sleep(2)",
      .has_unicode = "print('money')  # £££££",
      .does_chdir = "import os; os.chdir('/tmp')",
  };

  // Py2 is the same as Py3 except for print statements.
  LanguageTestParams py2 = py3;
  py2.name = "py2";
  py2.init =
      []() {
        return std::make_unique<Py2TesterSandboxer>(Py2InterpreterPath(),
                                                    Py2LibraryPaths());
      },
  py2.hello = "print 'hello'";
  py2.has_unicode = "print 'money'   # £££££";

  std::vector<LanguageTestParams> params = {py3};
  if (absl::GetFlag(FLAGS_test_py2)) {
    params.push_back(py2);
  }
  return params;
}

INSTANTIATE_TEST_SUITE_P(
    AllLanguages, TesterSandboxerLanguageTest,
    testing::ValuesIn(LanguageSweep()),
    // Use the name of the language as the name of the test.
    [](const testing::TestParamInfo<TesterSandboxerLanguageTest::ParamType>&
           info) { return info.param.name; });

TEST_P(TesterSandboxerLanguageTest, RunsHelloWorldTest) {
  const LanguageTestParams& params = GetParam();
  std::unique_ptr<TesterSandboxer> tester_sandboxer = params.init();
  EXPECT_THAT(
      tester_sandboxer->Test(params.hello, {""}),
      IsOkAndHolds(TestResultsMatches(ElementsAre(AllOf(
          HasProgramStatus(ProgramStatus::kSuccess), HasStdout("hello\n"),
          HasStderr(""),
          HasDurationBetween(absl::ZeroDuration(), absl::Seconds(10)))))));
}

TEST_P(TesterSandboxerLanguageTest, RunsCatTest) {
  const LanguageTestParams& params = GetParam();
  std::unique_ptr<TesterSandboxer> tester_sandboxer = params.init();
  EXPECT_THAT(
      tester_sandboxer->Test(params.cat, {"hello", "42\n", "£ is unicode"}),
      IsOkAndHolds(TestResultsMatches(ElementsAre(
          AllOf(HasProgramStatus(ProgramStatus::kSuccess), HasStdout("hello")),
          AllOf(HasProgramStatus(ProgramStatus::kSuccess), HasStdout("42\n")),
          AllOf(HasProgramStatus(ProgramStatus::kSuccess),
                HasStdout("£ is unicode"))))));
}

TEST_P(TesterSandboxerLanguageTest, HugeInput) {
  const LanguageTestParams& params = GetParam();
  std::unique_ptr<TesterSandboxer> tester_sandboxer = params.init();
  std::string long_string = CreateLargeInput();
  EXPECT_THAT(tester_sandboxer->Test(params.cat, {long_string}),
              IsOkAndHolds(TestResultsMatches(
                  ElementsAre(AllOf(HasProgramStatus(ProgramStatus::kSuccess),
                                    HasStdout(long_string), HasStderr(""))))));
}

TEST_P(TesterSandboxerLanguageTest, LongLineToStdout) {
  const LanguageTestParams& params = GetParam();
  std::unique_ptr<TesterSandboxer> tester_sandboxer = params.init();
  const std::string long_line(500000, 'x');
  EXPECT_THAT(tester_sandboxer->Test(params.cat, {long_line}),
              IsOkAndHolds(TestResultsMatches(
                  ElementsAre(AllOf(HasProgramStatus(ProgramStatus::kSuccess),
                                    HasStdout(long_line), HasStderr(""))))));
}

TEST_P(TesterSandboxerLanguageTest, LongLineToStderr) {
  const LanguageTestParams& params = GetParam();
  std::unique_ptr<TesterSandboxer> tester_sandboxer = params.init();
  const std::string long_line(500000, 'x');
  EXPECT_THAT(tester_sandboxer->Test(params.cat_to_stderr, {long_line}),
              IsOkAndHolds(TestResultsMatches(
                  ElementsAre(AllOf(HasProgramStatus(ProgramStatus::kSuccess),
                                    HasStdout(""), HasStderr(long_line))))));
}

TEST_P(TesterSandboxerLanguageTest, FailsToCompileWithBadInput) {
  const LanguageTestParams& params = GetParam();
  std::unique_ptr<TesterSandboxer> tester_sandboxer = params.init();
  EXPECT_THAT(tester_sandboxer->Test(params.bad_syntax, {""}),
              IsOkAndHolds(CompilationResultMatches(
                  AllOf(HasProgramStatus(ProgramStatus::kFailed),
                        HasStderrSubstring(params.bad_syntax_error)))));
}

TEST_P(TesterSandboxerLanguageTest, FailsIfCodeAsserts) {
  const LanguageTestParams& params = GetParam();
  std::unique_ptr<TesterSandboxer> tester_sandboxer = params.init();
  EXPECT_THAT(tester_sandboxer->Test(params.asserts, {""}),
              IsOkAndHolds(TestResultsMatches(ElementsAre(AllOf(
                  HasProgramStatus(ProgramStatus::kFailed), HasStdout(""))))));
}

TEST_P(TesterSandboxerLanguageTest, CanExecuteInParallel) {
  constexpr int kNumTests = 16;
  std::vector<absl::StatusOr<MultiTestResult>> results;
  absl::Mutex mu;
  {
    ThreadPool pool(kNumTests);
    pool.StartWorkers();
    for (int i = 0; i < kNumTests; ++i) {
      pool.Schedule([&] {
        const LanguageTestParams& params = GetParam();
        const auto result = params.init()->Test(params.hello, {"", ""});
        absl::MutexLock l(&mu);
        results.push_back(std::move(result));
      });
    }
  }
  ASSERT_THAT(results, testing::SizeIs(kNumTests));
  for (const absl::StatusOr<MultiTestResult>& result : results) {
    ASSERT_THAT(result, IsOkAndHolds(TestResultsMatches(ElementsAre(
                            HasProgramStatus(ProgramStatus::kSuccess),
                            HasProgramStatus(ProgramStatus::kSuccess)))));
  }
}

// This test is about parallelising within a single test call.
TEST_P(TesterSandboxerLanguageTest, CanTestInParallel) {
  const LanguageTestParams& params = GetParam();
  absl::string_view null_string = "";
  std::vector<absl::string_view> many_inputs(8, null_string);
  TestOptions options;
  options.num_threads = 4;
  options.max_execution_duration = absl::Seconds(2);
  const auto result =
      params.init()->Test(params.loops_forever, many_inputs, options);
  ASSERT_THAT(
      result,
      IsOkAndHolds(TestResultsMatches(
          AllOf(SizeIs(8), Each(HasProgramStatus(ProgramStatus::kTimeout))))));
}

TEST_P(TesterSandboxerLanguageTest, HandlesTimeout) {
  const LanguageTestParams& params = GetParam();
  std::unique_ptr<TesterSandboxer> tester_sandboxer = params.init();
  TestOptions options;
  options.max_execution_duration = absl::Seconds(1);
  EXPECT_THAT(tester_sandboxer->Test(params.loops_forever, {""}, options),
              IsOkAndHolds(TestResultsMatches(
                  ElementsAre(HasProgramStatus(ProgramStatus::kTimeout)))));
}

TEST_P(TesterSandboxerLanguageTest, DurationSetCorrectly) {
  const LanguageTestParams& params = GetParam();
  std::unique_ptr<TesterSandboxer> tester_sandboxer = params.init();
  EXPECT_THAT(tester_sandboxer->Test(params.sleeps_2_seconds, {""}),
              IsOkAndHolds(TestResultsMatches(ElementsAre(AllOf(
                  HasProgramStatus(ProgramStatus::kSuccess),
                  HasDurationBetween(absl::Seconds(2), absl::Seconds(10)))))));
}

TEST_P(TesterSandboxerLanguageTest, HandlesUnicode) {
  const LanguageTestParams& params = GetParam();
  std::unique_ptr<TesterSandboxer> tester_sandboxer = params.init();
  EXPECT_THAT(tester_sandboxer->Test(params.has_unicode, {""}),
              IsOkAndHolds(TestResultsMatches(
                  ElementsAre(HasProgramStatus(ProgramStatus::kSuccess)))));
}

TEST_P(TesterSandboxerLanguageTest, HandlesViolation) {
  const LanguageTestParams& params = GetParam();
  std::unique_ptr<TesterSandboxer> tester_sandboxer = params.init();
  EXPECT_THAT(tester_sandboxer->Test(params.does_chdir, {""}),
              IsOkAndHolds(TestResultsMatches(
                  ElementsAre(HasProgramStatus(ProgramStatus::kFailed)))));
}

TEST_P(TesterSandboxerLanguageTest, HandlesViolationWithInput) {
  const LanguageTestParams& params = GetParam();
  std::unique_ptr<TesterSandboxer> tester_sandboxer = params.init();
  EXPECT_THAT(tester_sandboxer->Test(params.does_chdir, {CreateLargeInput()}),
              IsOkAndHolds(TestResultsMatches(
                  ElementsAre(HasProgramStatus(ProgramStatus::kFailed)))));
}

TEST_P(TesterSandboxerLanguageTest, StableHash) {
  std::optional<uint64_t> hash;
  const LanguageTestParams& params = GetParam();
  for (int i = 0; i < 5; ++i) {
    std::unique_ptr<TesterSandboxer> tester_sandboxer = params.init();

    ASSERT_OK_AND_ASSIGN(auto result,
                         tester_sandboxer->Test(params.hello, {""}));
    if (hash.has_value()) {
      EXPECT_EQ(result.compilation_result.program_hash, *hash);
    } else {
      hash = result.compilation_result.program_hash;
    }

    // Sleep to ensure the hash does not depend on the current time.
    absl::SleepFor(absl::Seconds(1));
  }
}

TEST_P(TesterSandboxerLanguageTest, ExpectedOutputs) {
  const LanguageTestParams& params = GetParam();
  const std::vector<absl::string_view> inputs(100);
  const std::string hello_output = "hello\n";
  std::vector<std::string_view> expected_outputs(100, hello_output);
  // One failing test early on.
  expected_outputs[3] = "goodbye\n";
  TestOptions opts;
  opts.num_threads = 4;
  std::unique_ptr<TesterSandboxer> tester_sandboxer = params.init();
  ASSERT_OK_AND_ASSIGN(auto result,
                       tester_sandboxer->Test(
                           params.hello, inputs, opts, expected_outputs,
                           [](std::string_view a, std::string_view b) -> bool {
                             return a == b;
                           }));
  // There was exactly one test that should have failed.
  EXPECT_EQ(
      absl::c_count_if(result.test_results,
                       [](const ExecutionResult& execution_result) -> bool {
                         return execution_result.passed.has_value() &&
                                !*execution_result.passed;
                       }),
      1);
  // The others should have succeeded.
  EXPECT_EQ(
      absl::c_count_if(result.test_results,
                       [](const ExecutionResult& execution_result) -> bool {
                         return execution_result.passed.has_value() &&
                                *execution_result.passed;
                       }),
      99);
}

TEST_P(TesterSandboxerLanguageTest, ExpectedOutputsStopOnFirstFailure) {
  const LanguageTestParams& params = GetParam();
  const std::vector<absl::string_view> inputs(100);
  const std::string hello_output = "hello\n";
  std::vector<std::string_view> expected_outputs(100, hello_output);
  // One failing test early on.
  expected_outputs[3] = "goodbye\n";
  TestOptions opts;
  opts.num_threads = 4;
  opts.stop_on_first_failure = true;
  std::unique_ptr<TesterSandboxer> tester_sandboxer = params.init();
  ASSERT_OK_AND_ASSIGN(auto result,
                       tester_sandboxer->Test(
                           params.hello, inputs, opts, expected_outputs,
                           [](std::string_view a, std::string_view b) -> bool {
                             return a == b;
                           }));
  // Seeing as we failed early, we expect at least one of the tests to have
  // stopped instead of running in full.
  EXPECT_GT(
      absl::c_count_if(result.test_results,
                       [](const ExecutionResult& execution_result) -> bool {
                         return !execution_result.passed.has_value();
                       }),
      0);
  // There was exactly one test that should have failed.
  EXPECT_EQ(
      absl::c_count_if(result.test_results,
                       [](const ExecutionResult& execution_result) -> bool {
                         return execution_result.passed.has_value() &&
                                !*execution_result.passed;
                       }),
      1);
}

// Below are all tests that are specific to a language, so cannot be included in
// the parameterized test.

TEST(TesterSandboxerTest, PyDoesNotLeaveStateBehind) {
  std::unique_ptr<TesterSandboxer> tester_sandboxer =
      std::make_unique<Py3TesterSandboxer>(Py3InterpreterPath(),
                                           Py3LibraryPaths());
  std::string code = R"py(
assert 'x' not in globals()
x = 1
)py";

  // Second call verifies that `x` from the first call has not persisted.
  EXPECT_THAT(tester_sandboxer->Test(code, {"", ""}),
              IsOkAndHolds(TestResultsMatches(
                  Each(HasProgramStatus(ProgramStatus::kSuccess)))));

  // Also check there's no persistence between calls to `Test`.
  EXPECT_THAT(tester_sandboxer->Test(code, {"", ""}),
              IsOkAndHolds(TestResultsMatches(
                  Each(HasProgramStatus(ProgramStatus::kSuccess)))));
}

TEST(TesterSandboxerTest, Py3HandlesReadWhenNoInput) {
  std::unique_ptr<TesterSandboxer> tester_sandboxer =
      std::make_unique<Py3TesterSandboxer>(Py3InterpreterPath(),
                                           Py3LibraryPaths());
  std::string program = R"py(import sys
sys.stdout.write(input()))py";
  EXPECT_THAT(tester_sandboxer->Test(program, {""}),
              IsOkAndHolds(TestResultsMatches(
                  ElementsAre(HasProgramStatus(ProgramStatus::kFailed)))));
}

TEST(TesterSandboxerTest, Py3HandlesReadWhenNotEnoughInput) {
  std::unique_ptr<TesterSandboxer> tester_sandboxer =
      std::make_unique<Py3TesterSandboxer>(Py3InterpreterPath(),
                                           Py3LibraryPaths());
  std::string program = R"py(import sys
sys.stdout.write(input())
sys.stdout.write(input())
)py";
  EXPECT_THAT(tester_sandboxer->Test(program, {"just one line\n"}),
              IsOkAndHolds(TestResultsMatches(
                  ElementsAre(HasProgramStatus(ProgramStatus::kFailed)))));
}

TEST(TesterSandboxerTest, PyProgramHash) {
  std::unique_ptr<TesterSandboxer> tester_sandboxer =
      std::make_unique<Py3TesterSandboxer>(Py3InterpreterPath(),
                                           Py3LibraryPaths());

  // Different programs with the same semantics.
  const std::string program_a = R"(
print(5 + 2)
  )";
  const std::string program_b = R"(
print(7)
  )";
  ASSERT_OK_AND_ASSIGN(auto result_a, tester_sandboxer->Test(program_a, {""}));
  ASSERT_OK_AND_ASSIGN(auto result_b, tester_sandboxer->Test(program_b, {""}));

  ASSERT_EQ(result_b.compilation_result.program_status, ProgramStatus::kSuccess)
      << result_b;

  // After compilation, the hash of the optimized binary should be the same.
  EXPECT_NE(result_a.compilation_result.program_hash, 0);
  EXPECT_EQ(result_a.compilation_result.program_hash,
            result_b.compilation_result.program_hash);
}

TEST(TesterSandboxerTest, IncorrectNumberOfExpectedOutputs) {
  std::unique_ptr<TesterSandboxer> tester_sandboxer =
      std::make_unique<Py3TesterSandboxer>(Py3InterpreterPath(),
                                           Py3LibraryPaths());
  std::string program = "print('hello')\n";
  std::vector<std::string_view> inputs = {"", ""};
  std::vector<std::string_view> expected_outputs = {"hello\n"};  // Wrong length
  EXPECT_THAT(
      tester_sandboxer->Test(program, inputs, TestOptions(), expected_outputs,
                             std::equal_to<std::string_view>()),
      StatusIs(absl::StatusCode::kInvalidArgument,
               "Inputs and expected outputs must have the same length. Actual "
               "lengths: 2 v 1."));
}

TEST(TesterSandboxerTest, CannotStopOnFirstFailureIfNoOutputs) {
  std::unique_ptr<TesterSandboxer> tester_sandboxer =
      std::make_unique<Py3TesterSandboxer>(Py3InterpreterPath(),
                                           Py3LibraryPaths());
  std::string program = "print('hello')\n";
  std::vector<std::string_view> inputs = {""};
  std::vector<std::string_view> expected_outputs = {};
  TestOptions opts;
  opts.stop_on_first_failure = true;
  EXPECT_THAT(tester_sandboxer->Test(program, inputs, opts, expected_outputs,
                                     std::equal_to<std::string_view>()),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       "stop_on_first_failure does not work if expected "
                       "outputs are not provided."));
}

TEST(SandboxWithOutputFdsTest, CanReadStdout) {
  int pipe_ends[2];
  ASSERT_EQ(pipe(pipe_ends), 0);
  ASSERT_EQ(write(pipe_ends[1], "hello", 5), 5);
  ASSERT_EQ(close(pipe_ends[1]), 0);
  SandboxWithOutputFds sandbox(nullptr, pipe_ends[0],
                               SandboxWithOutputFds::kInvalidFd);
  EXPECT_THAT(sandbox.Stdout(), IsOkAndHolds("hello"));
  EXPECT_THAT(sandbox.Stdout(), IsOkAndHolds("hello"));
}

TEST(SandboxWithOutputFdsTest, CanReadStderr) {
  int pipe_ends[2];
  ASSERT_EQ(pipe(pipe_ends), 0);
  ASSERT_EQ(write(pipe_ends[1], "hello", 5), 5);
  ASSERT_EQ(close(pipe_ends[1]), 0);
  SandboxWithOutputFds sandbox(nullptr, SandboxWithOutputFds::kInvalidFd,
                               pipe_ends[0]);
  EXPECT_THAT(sandbox.Stderr(), IsOkAndHolds("hello"));
  EXPECT_THAT(sandbox.Stderr(), IsOkAndHolds("hello"));
}

TEST(SandboxWithOutputFdsTest, CanReadStdoutAfterMove) {
  int pipe_ends[2];
  ASSERT_EQ(pipe(pipe_ends), 0);
  ASSERT_EQ(write(pipe_ends[1], "hello", 5), 5);
  ASSERT_EQ(close(pipe_ends[1]), 0);
  SandboxWithOutputFds sandbox(nullptr, pipe_ends[0],
                               SandboxWithOutputFds::kInvalidFd);
  SandboxWithOutputFds sandbox2 = std::move(sandbox);
  EXPECT_THAT(sandbox2.Stdout(), IsOkAndHolds("hello"));
}

TEST(SandboxWithOutputFdsTest, CanReadStderrAfterMove) {
  int pipe_ends[2];
  ASSERT_EQ(pipe(pipe_ends), 0);
  ASSERT_EQ(write(pipe_ends[1], "hello", 5), 5);
  ASSERT_EQ(close(pipe_ends[1]), 0);
  SandboxWithOutputFds sandbox(nullptr, SandboxWithOutputFds::kInvalidFd,
                               pipe_ends[0]);
  SandboxWithOutputFds sandbox2 = std::move(sandbox);
  EXPECT_THAT(sandbox2.Stderr(), IsOkAndHolds("hello"));
}

TEST(OutputsMatchTest, MatchingStrings) {
  EXPECT_TRUE(OutputsMatch("abc def", "abc def"));
}

TEST(OutputsMatchTest, DifferingStrings) {
  EXPECT_FALSE(OutputsMatch("abc deg", "abc def"));
}

TEST(OutputsMatchTest, DifferingStringsPrefix) {
  EXPECT_FALSE(OutputsMatch("abc def", "abc def 123"));
}

TEST(OutputsMatchTest, CaseIgnored) {
  EXPECT_TRUE(OutputsMatch("abc DEF", "abc def"));
}

TEST(OutputsMatchTest, CloseFloatsAccepted) {
  EXPECT_TRUE(OutputsMatch("abc 123", "abc 123.000001"));
}

TEST(OutputsMatchTest, NonCloseFloatsNotAccepted) {
  EXPECT_FALSE(OutputsMatch("abc 123", "abc 123.1"));
}

}  // namespace
}  // namespace deepmind::code_contests
