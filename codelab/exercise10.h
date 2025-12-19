// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_CODELAB_EXERCISE10_H_
#define THIRD_PARTY_CEL_CPP_CODELAB_EXERCISE10_H_

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace cel_codelab {

// Exercise10 -- extension types.
//
// This function compiles an expression then evaluates, expecting a bool
// return type.
//
// Example:
//   net.ParseAddressMatcher("8.8.0.0-8.8.255.255")
//     .containsAddress(
//       net.parseAddress(ip)
//     )
//
// Variables:
// ip - string
//
// Functions:
// net.ParseAddress(string) -> net.Address
// net.ParseAddressMatcher(string) -> net.AddressMatcher
// (net.AddressMatcher).
absl::StatusOr<bool> CompileAndEvaluateExercise10(absl::string_view expression,
                                                  absl::string_view ip);

}  // namespace cel_codelab

#endif  // THIRD_PARTY_CEL_CPP_CODELAB_EXERCISE10_H_
