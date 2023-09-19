// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_STANDARD_EQUALITY_FUNCTIONS_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_STANDARD_EQUALITY_FUNCTIONS_H_

#include "absl/status/status.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"

namespace cel {

// Register equality functions
// ==, !=
//
// options.enable_heterogeneous_equality controls which flavor of equality is
// used.
//
// For legacy equality (.enable_heterogeneous_equality = false), equality is
// defined between same-typed values only.
//
// For the CEL Specification's definition of equality
// (.enable_heterogeneous_equality = true), equality is defined between most
// types, with false returned if the two different types are incomparable.
absl::Status RegisterEqualityFunctions(FunctionRegistry& registry,
                                       const RuntimeOptions& options);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_STANDARD_EQUALITY_FUNCTIONS_H_
