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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_SIGNATURE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_SIGNATURE_H_

#include <string>
#include <string_view>
#include <vector>

#include "absl/status/statusor.h"
#include "common/type.h"

namespace cel::common_internal {

// Generates an signature for a `cel::Type`, which is a string representation of
// the type.
//
// Examples:
//
//  - `int`
//  - `list<int>`
//  - `list<my_type<~A>>`
absl::StatusOr<std::string> MakeTypeSignature(const Type& type);

// Generates an identifier for a function overload based on the function name
// and the types of the arguments.  If `is_member` is true, the first argument
// type is used as the receiver and is prepended to the function name, followed
// by a dollar sign.
//
// Examples:
//
//  - `foo()`
//  - `foo(int)`
//  - `bar.foo(int)`
//  - `foo(int,string)`
//  - `foo(list<int>,list<string>)`
//  - `bar.foo(list<int>,list<my_type<~A>>)`
//
// If the function name contains a period, it is escaped with a backslash, e.g.
// `foo.bar` becomes `foo\.bar`. This allows to disambiguate between a member
// function and qualified target type name.
//
absl::StatusOr<std::string> MakeOverloadSignature(
    std::string_view function_name, const std::vector<Type>& args,
    bool is_member);

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_SIGNATURE_H_
