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

#ifndef THIRD_PARTY_DEEPMIND_CODE_CONTESTS_EXECUTION_PY_LOCATIONS_H_
#define THIRD_PARTY_DEEPMIND_CODE_CONTESTS_EXECUTION_PY_LOCATIONS_H_

#include <string>
#include <vector>

namespace deepmind::code_contests {

std::string Py3InterpreterPath();
std::vector<std::string> Py3LibraryPaths();
std::string Py2InterpreterPath();
std::vector<std::string> Py2LibraryPaths();

}  // namespace deepmind::code_contests

#endif  // THIRD_PARTY_DEEPMIND_CODE_CONTESTS_EXECUTION_PY_LOCATIONS_H_
