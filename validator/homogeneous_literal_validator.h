// Copyright 2026 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_VALIDATOR_HOMOGENEOUS_LITERAL_VALIDATOR_H_
#define THIRD_PARTY_CEL_CPP_VALIDATOR_HOMOGENEOUS_LITERAL_VALIDATOR_H_

#include <string>
#include <vector>

#include "validator/validator.h"

namespace cel {

// Returns a `Validation` that checks that all literals in map or list literals
// are the same type. If the list or map is part of an argument to an exempted
// function, it is not checked.
Validation HomogeneousLiteralValidator(
    std::vector<std::string> exempt_functions);

inline Validation HomogeneousLiteralValidator() {
  // Default to exempting the strings extension "format" function.
  return HomogeneousLiteralValidator({"format"});
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_VALIDATOR_HOMOGENEOUS_LITERAL_VALIDATOR_H_
