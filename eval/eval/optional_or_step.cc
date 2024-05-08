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
#include <utility>

#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "common/casting.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/eval/jump_step.h"
#include "runtime/internal/errors.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::As;
using ::cel::ErrorValue;
using ::cel::InstanceOf;
using ::cel::OptionalValue;
using ::cel::UnknownValue;
using ::cel::Value;
using ::cel::runtime_internal::CreateNoMatchingOverloadError;

enum class OptionalOrKind { kOrOptional, kOrValue };

ErrorValue MakeNoOverloadError(OptionalOrKind kind) {
  switch (kind) {
    case OptionalOrKind::kOrOptional:
      return ErrorValue(CreateNoMatchingOverloadError("or"));
    case OptionalOrKind::kOrValue:
      return ErrorValue(CreateNoMatchingOverloadError("orValue"));
  }

  ABSL_UNREACHABLE();
}

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
  OptionalHasValueJumpStep(int64_t expr_id, OptionalOrKind kind)
      : JumpStepBase({}, expr_id), kind_(kind) {}

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
      if (kind_ == OptionalOrKind::kOrValue && optional_value.has_value()) {
        frame->value_stack().PopAndPush(optional_value->Value());
      }
      return Jump(frame);
    }
    return absl::OkStatus();
  }

 private:
  const OptionalOrKind kind_;
};

class OptionalOrStep : public ExpressionStepBase {
 public:
  explicit OptionalOrStep(int64_t expr_id, OptionalOrKind kind)
      : ExpressionStepBase(expr_id), kind_(kind) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  const OptionalOrKind kind_;
};

absl::Status OptionalOrStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(2)) {
    return absl::InternalError("Value stack underflow");
  }

  absl::Span<const Value> args = frame->value_stack().GetSpan(2);
  absl::Span<const AttributeTrail> args_attr =
      frame->value_stack().GetAttributeSpan(2);

  const Value& lhs = args[0];
  const Value& rhs = args[1];

  if (InstanceOf<ErrorValue>(lhs) || InstanceOf<UnknownValue>(lhs)) {
    // Mimic ternary behavior -- just forward lhs instead of attempting to
    // merge since not commutative.
    frame->value_stack().Pop(1);
    return absl::OkStatus();
  }

  auto lhs_optional_value = As<OptionalValue>(lhs);
  if (!lhs_optional_value.has_value()) {
    frame->value_stack().PopAndPush(2, MakeNoOverloadError(kind_));
    return absl::OkStatus();
  }

  if (lhs_optional_value->HasValue()) {
    if (kind_ == OptionalOrKind::kOrValue) {
      auto value = lhs_optional_value->Value();
      frame->value_stack().PopAndPush(2, std::move(value), args_attr[0]);
    } else {
      frame->value_stack().Pop(1);
    }
    return absl::OkStatus();
  }

  if (kind_ == OptionalOrKind::kOrOptional && !InstanceOf<ErrorValue>(rhs) &&
      !InstanceOf<UnknownValue>(rhs) && !InstanceOf<OptionalValue>(rhs)) {
    frame->value_stack().PopAndPush(2, MakeNoOverloadError(kind_));
    return absl::OkStatus();
  }

  frame->value_stack().PopAndPush(2, rhs, args_attr[1]);
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<JumpStepBase>> CreateOptionalHasValueJumpStep(
    bool or_value, int64_t expr_id) {
  return std::make_unique<OptionalHasValueJumpStep>(
      expr_id,
      or_value ? OptionalOrKind::kOrValue : OptionalOrKind::kOrOptional);
}

std::unique_ptr<ExpressionStep> CreateOptionalOrStep(bool is_or_value,
                                                     int64_t expr_id) {
  return std::make_unique<OptionalOrStep>(
      expr_id,
      is_or_value ? OptionalOrKind::kOrValue : OptionalOrKind::kOrOptional);
}

}  // namespace google::api::expr::runtime
