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

#include "execution/py_locations.h"

#include <string>
#include <vector>

#include "absl/flags/flag.h"

ABSL_FLAG(std::string, python3_path, "/usr/bin/python3.9",
          "The path to python3.");
ABSL_FLAG(std::vector<std::string>, python3_library_paths,
          {"/usr/lib/python3.9"}, "The paths to python3 libraries.");
ABSL_FLAG(std::string, python2_path, "/usr/bin/python2.7",
          "The path to python2.");
ABSL_FLAG(std::vector<std::string>, python2_library_paths,
          {"/usr/lib/python2.7"}, "The paths to python2 libraries.");

namespace deepmind::code_contests {

std::string Py3InterpreterPath() { return absl::GetFlag(FLAGS_python3_path); }
std::vector<std::string> Py3LibraryPaths() {
  return absl::GetFlag(FLAGS_python3_library_paths);
}
std::string Py2InterpreterPath() { return absl::GetFlag(FLAGS_python2_path); }
std::vector<std::string> Py2LibraryPaths() {
  return absl::GetFlag(FLAGS_python2_library_paths);
}

}  // namespace deepmind::code_contests
