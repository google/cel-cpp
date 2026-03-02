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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_RE2_OPTIONS_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_RE2_OPTIONS_H_

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "re2/re2.h"

namespace cel::internal {

inline RE2::Options MakeRE2Options() {
  RE2::Options options;
  options.set_log_errors(false);
  return options;
}

inline absl::Status CheckRE2(const RE2& re, int max_program_size) {
  if (!re.ok()) {
    switch (re.error_code()) {
      case RE2::ErrorInternal:
        return absl::InternalError(
            absl::StrCat("internal RE2 error: ", re.error()));
      case RE2::ErrorPatternTooLarge:
        return absl::InvalidArgumentError(
            absl::StrCat("regular expression too large: ", re.error()));
      default:
        return absl::InvalidArgumentError(
            absl::StrCat("invalid regular expression: ", re.error()));
    }
  }
  int program_size = re.ProgramSize();
  if (max_program_size > 0 && program_size > 0 &&
      program_size > max_program_size) {
    return absl::InvalidArgumentError(
        "regular expressions exceeds max allowed size");
  }
  int reverse_program_size = re.ReverseProgramSize();
  if (max_program_size > 0 && reverse_program_size > 0 &&
      reverse_program_size > max_program_size) {
    return absl::InvalidArgumentError(
        "regular expressions exceeds max allowed size");
  }
  return absl::OkStatus();
}

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_RE2_OPTIONS_H_
