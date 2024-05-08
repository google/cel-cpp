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

#include "eval/eval/optional_or_step.h"

#include <cstdint>
#include <memory>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/value.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/jump_step.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::As;
using ::cel::ErrorValue;
using ::cel::OptionalValue;
using ::cel::UnknownValue;
using ::cel::Value;

// Implements short-circuiting for optional.or.
// Expected layout if short-circuiting enabled:
//
// +--------+-----------------------+-------------------------------+
// |   idx  |         Step          |   Stack After                 |
// +--------+-----------------------+-------------------------------+
// |    1   |<optional target expr> | OptionalValue                 |
// +--------+-----------------------+-------------------------------+
// |    2   | Jump to 5 if present  | OptionalValue                 |
// +--------+-----------------------+-------------------------------+
// |    3   | <alternative expr>    | OptionalValue, OptionalValue  |
// +--------+-----------------------+-------------------------------+
// |    4   | optional.or           | OptionalValue                 |
// +--------+-----------------------+-------------------------------+
// |    5   | <rest>                | ...                           |
// +--------------------------------+-------------------------------+
//
// If implementing the orValue variant, the jump step handles unwrapping (
// getting the result of optional.value())
class OptionalHasValueJumpStep final : public JumpStepBase {
 public:
  OptionalHasValueJumpStep(int64_t expr_id, bool or_value)
      : JumpStepBase({}, expr_id), or_value_(or_value) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    if (!frame->value_stack().HasEnough(1)) {
      return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
    }
    const auto& value = frame->value_stack().Peek();
    auto optional_value = As<OptionalValue>(value);
    // We jump if the receiver is `optional_type` which has a value or the
    // receiver is an error/unknown. Unlike `_||_` we are not commutative. If
    // we run into an error/unknown, we skip the `else` branch.
    const bool should_jump =
        (optional_value.has_value() && optional_value->HasValue()) ||
        (!optional_value.has_value() && (cel::InstanceOf<ErrorValue>(value) ||
                                         cel::InstanceOf<UnknownValue>(value)));
    if (should_jump) {
      if (or_value_ && optional_value.has_value()) {
        frame->value_stack().PopAndPush(optional_value->Value());
      }
      return Jump(frame);
    }
    return absl::OkStatus();
  }

 private:
  const bool or_value_;
};

}  // namespace

absl::StatusOr<std::unique_ptr<JumpStepBase>> CreateOptionalHasValueJumpStep(
    bool or_value, int64_t expr_id) {
  return std::make_unique<OptionalHasValueJumpStep>(expr_id, or_value);
}

}  // namespace google::api::expr::runtime
