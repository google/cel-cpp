// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_CREATE_MAP_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_CREATE_MAP_STEP_H_

#include <cstdint>
#include <memory>

#include "absl/status/statusor.h"
#include "base/ast_internal/expr.h"
#include "eval/eval/evaluator_core.h"

namespace google::api::expr::runtime {

// Creates an `ExpressionStep` which performs `CreateStruct` for a map.
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateCreateStructStepForMap(
    const cel::ast_internal::CreateStruct& create_struct_expr, int64_t expr_id);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_CREATE_MAP_STEP_H_
