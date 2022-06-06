// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_COMPARISON_FUNCTIONS_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_COMPARISON_FUNCTIONS_H_

#include "absl/status/status.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_options.h"

namespace google::api::expr::runtime {

// Implementation for general equality beteween CELValues. Exposed for
// consistent behavior in set membership functions.
//
// Returns nullopt if the comparison is undefined between differently typed
// values.
absl::optional<bool> CelValueEqualImpl(const CelValue& v1, const CelValue& v2);

// Register built in comparison functions (==, !=, <, <=, >, >=).
//
// This is call is included in RegisterBuiltinFunctions -- calling both
// RegisterBuiltinFunctions and RegisterComparisonFunctions directly on the same
// registry will result in an error.
absl::Status RegisterComparisonFunctions(
    CelFunctionRegistry* registry,
    const InterpreterOptions& options = InterpreterOptions());

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_COMPARISON_FUNCTIONS_H_
