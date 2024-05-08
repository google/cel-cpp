// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_OPTIONAL_OR_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_OPTIONAL_OR_STEP_H_

#include <cstdint>
#include <memory>

#include "absl/status/statusor.h"
#include "eval/eval/jump_step.h"

namespace google::api::expr::runtime {

// Factory method for OptionalHasValueJump step, used to implement
// short-circuiting optional.or and optional.orValue.
//
// Requires that the top of the stack is an optional. If `optional.hasValue` is
// true, performs a jump. If `or_value` is true and we are jumping,
// `optional.value` is called and the result replaces the optional at the top of
// the stack.
absl::StatusOr<std::unique_ptr<JumpStepBase>> CreateOptionalHasValueJumpStep(
    bool or_value, int64_t expr_id);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_OPTIONAL_OR_STEP_H_
