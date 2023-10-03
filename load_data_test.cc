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

#include <string>

#include "google/protobuf/text_format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "contest_problem.pb.h"

constexpr absl::string_view kTextProtoExample = R"pb(
  name: "123_X. Adding integers"
  description: "Add integers together to get a sum.\n\n"
               "Input:\n\nThe input consists of two integers, n and m, "
               "each between 1 and 100000.\n\n"
               "Output:\n\nA single integer, the sum of n and m."
  public_tests: { input: "1 2\n" output: "3" }
  public_tests: { input: "3 4\n" output: "7" }
  private_tests: { input: "5 6\n" output: "11" }
  private_tests: { input: "7 8\n" output: "15" }
  generated_tests: { input: "9 10\n" output: "19" }
  source: CODEFORCES
  solutions: {
    language: PYTHON3
    solution: "print(int(input()) + int(input()))\n"
  }
  cf_contest_id: 123
  cf_index: "X"
  cf_points: 1000.0
  cf_rating: 1100
  cf_tags: "math"
  incorrect_solutions: {
    language: PYTHON3
    solution: "print(int(input()) - int(input()))\n"
  }
  is_description_translated: true
  untranslated_description: "Bonjour"
)pb";

namespace {

using ::deepmind::code_contests::ContestProblem;

TEST(LoadDataTest, CanParseExample) {
  ContestProblem problem;
  EXPECT_TRUE(google::protobuf::TextFormat::ParseFromString(
      std::string(kTextProtoExample), &problem));
}

}  // namespace
