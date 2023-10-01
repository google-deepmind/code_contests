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

#ifndef THIRD_PARTY_DEEPMIND_CODE_CONTESTS_EXECUTION_TEMP_PATH_H_H_
#define THIRD_PARTY_DEEPMIND_CODE_CONTESTS_EXECUTION_TEMP_PATH_H_H_

#include <sys/types.h>
#include <unistd.h>

#include <cstdlib>
#include <filesystem>
#include <string>

#include "absl/random/random.h"
#include "absl/strings/str_format.h"
#include "absl/time/clock.h"
#include "absl/time/time.h"

namespace deepmind::code_contests {

class TempPath {
 public:
  TempPath() {
    const absl::Time now = absl::Now();
    const pid_t pid = getpid();
    absl::BitGen bitgen;
    const int rand = absl::Uniform(bitgen, 0, 1000000);
    path_ =
        absl::StrFormat("/tmp/%d-%d-%d", absl::ToUnixMillis(now), pid, rand);
    if (!std::filesystem::create_directory(path_)) {
      std::abort();
    }
  }

  ~TempPath() {}

  TempPath(const TempPath&) = delete;
  TempPath& operator=(const TempPath&) = delete;

  std::string path() const { return path_; }

 private:
  std::string path_;
};

}  // namespace deepmind::code_contests

#endif  // THIRD_PARTY_DEEPMIND_CODE_CONTESTS_EXECUTION_TEMP_PATH_H_H_
