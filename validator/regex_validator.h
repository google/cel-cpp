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

#ifndef THIRD_PARTY_CEL_CPP_VALIDATOR_REGEX_VALIDATOR_H_
#define THIRD_PARTY_CEL_CPP_VALIDATOR_REGEX_VALIDATOR_H_

#include <string>
#include <vector>

#include "absl/strings/string_view.h"
#include "common/standard_definitions.h"
#include "validator/validator.h"

namespace cel {

// Configuration for the regex pattern validator.
struct RegexPatternValidatorConfig {
  // The resolved function name.
  std::string function_name;
  // the index of the pattern argument (counting the receiver as arg 0 if
  // present).
  int pattern_arg_index;
};

// Returns a `Validation` that checks all calls to the given regex functions
// It validates that the specified argument is a valid regular expression if it
// is a literal string.
Validation RegexPatternValidator(
    absl::string_view id, std::vector<RegexPatternValidatorConfig> config);

// Returns a `Validation` that checks all calls to the CEL `matches` function.
// It validates that if the pattern is a literal string, it is a valid regular
// expression.
inline Validation MatchesValidator() {
  return RegexPatternValidator(
      "cel.validator.matches",
      {{std::string(StandardFunctions::kRegexMatch), 1}});
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_VALIDATOR_REGEX_VALIDATOR_H_
