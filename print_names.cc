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
//
// Example usage:
//
//   print_names /path/to/dataset/code_contests_train*

#include <iostream>
#include <tuple>
#include <vector>

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "contest_problem.pb.h"
#include "riegeli/bytes/fd_reader.h"
#include "riegeli/records/record_reader.h"

namespace {

using ::deepmind::code_contests::ContestProblem;

void PrintNames(const absl::Span<const absl::string_view> filenames) {
  for (const absl::string_view filename : filenames) {
    riegeli::RecordReader<riegeli::FdReader<>> reader(
        std::forward_as_tuple(filename));
    ContestProblem problem;
    while (reader.ReadRecord(problem)) {
      std::cout << problem.name() << '\n';
    }
    reader.Close();
  }
}

}  // namespace

int main(int argc, char* argv[]) {
  std::vector<absl::string_view> filenames;
  filenames.reserve(argc - 1);
  for (int i = 1; i < argc; ++i) {
    filenames.push_back(argv[i]);
  }
  PrintNames(filenames);
}
