#include "eval/eval/comprehension_step.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "base/kind.h"
#include "common/casting.h"
#include "common/value.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/comprehension_slots.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/internal/errors.h"
#include "internal/status_macros.h"
#include "runtime/internal/mutable_list_impl.h"

namespace google::api::expr::runtime {
namespace {

using ::cel::Cast;
using ::cel::InstanceOf;
using ::cel::IntValue;
using ::cel::ListValue;
using ::cel::UnknownValue;
using ::cel::Value;
using ::cel::runtime_internal::CreateNoMatchingOverloadError;
using ::cel::runtime_internal::MutableListValue;

class ComprehensionFinish : public ExpressionStepBase {
 public:
  ComprehensionFinish(size_t accu_slot, int64_t expr_id);

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  size_t accu_slot_;
};

ComprehensionFinish::ComprehensionFinish(size_t accu_slot, int64_t expr_id)
    : ExpressionStepBase(expr_id), accu_slot_(accu_slot) {}

// Stack changes of ComprehensionFinish.
//
// Stack size before: 3.
// Stack size after: 1.
absl::Status ComprehensionFinish::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(3)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }
  Value result = frame->value_stack().Peek();
  frame->value_stack().Pop(3);
  if (frame->enable_comprehension_list_append() &&
      MutableListValue::Is(result)) {
    // We assume this is 'owned' by the evaluator stack so const cast is safe
    // here.
    // Convert the buildable list to an actual cel::ListValue.
    MutableListValue& list_value = MutableListValue::Cast(result);
    CEL_ASSIGN_OR_RETURN(result, std::move(list_value).Build());
  }
  frame->value_stack().Push(std::move(result));
  frame->comprehension_slots().ClearSlot(accu_slot_);
  return absl::OkStatus();
}

class ComprehensionInitStep : public ExpressionStepBase {
 public:
  explicit ComprehensionInitStep(int64_t expr_id)
      : ExpressionStepBase(expr_id, false) {}
  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  absl::Status ProjectKeys(ExecutionFrame* frame) const;
};

absl::Status ComprehensionInitStep::ProjectKeys(ExecutionFrame* frame) const {
  // Top of stack is map, but could be partially unknown. To tolerate cases when
  // keys are not set for declared unknown values, convert to an unknown set.
  if (frame->enable_unknowns()) {
    absl::optional<UnknownValue> unknown =
        frame->attribute_utility().IdentifyAndMergeUnknowns(
            frame->value_stack().GetSpan(1),
            frame->value_stack().GetAttributeSpan(1),
            /*use_partial=*/true);
    if (unknown.has_value()) {
      frame->value_stack().PopAndPush(*std::move(unknown));
      return absl::OkStatus();
    }
  }

  CEL_ASSIGN_OR_RETURN(auto list_keys,
                       frame->value_stack().Peek().As<cel::MapValue>().ListKeys(
                           frame->value_factory()));
  frame->value_stack().PopAndPush(std::move(list_keys));
  return absl::OkStatus();
}

// Setup the value stack for comprehension.
// Coerce the top of stack into a list and initilialize an index.
// This should happen after evaluating the iter_range part of the comprehension.
absl::Status ComprehensionInitStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(1)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }
  if (frame->value_stack().Peek()->Is<cel::MapValue>()) {
    CEL_RETURN_IF_ERROR(ProjectKeys(frame));
  }

  const auto& range = frame->value_stack().Peek();
  if (!range->Is<cel::ListValue>() && !range->Is<cel::ErrorValue>() &&
      !range->Is<cel::UnknownValue>()) {
    frame->value_stack().PopAndPush(frame->value_factory().CreateErrorValue(
        CreateNoMatchingOverloadError("<iter_range>")));
  }

  // Initialize current index.
  // Error handling for wrong range type is deferred until the 'Next' step
  // to simplify the number of jumps.
  frame->value_stack().Push(frame->value_factory().CreateIntValue(-1));
  return absl::OkStatus();
}

}  // namespace

// Stack variables during comprehension evaluation:
// 0. iter_range (list)
// 1. current index in iter_range (int64_t)
// 2. current accumulator value or break condition

//  instruction                stack size
//  0. iter_range              (dep) 0 -> 1
//  1. ComprehensionInit             1 -> 2
//  2. accu_init               (dep) 2 -> 3
//  3. ComprehensionNextStep         3 -> 2
//  4. loop_condition          (dep) 2 -> 3
//  5. ComprehensionCondStep         3 -> 2
//  6. loop_step               (dep) 2 -> 3
//  7. goto 3.                       3 -> 3
//  8. result                  (dep) 2 -> 3
//  9. ComprehensionFinish           3 -> 1

ComprehensionNextStep::ComprehensionNextStep(size_t iter_slot, size_t accu_slot,
                                             int64_t expr_id)
    : ExpressionStepBase(expr_id, false),
      iter_slot_(iter_slot),
      accu_slot_(accu_slot) {}

void ComprehensionNextStep::set_jump_offset(int offset) {
  jump_offset_ = offset;
}

void ComprehensionNextStep::set_error_jump_offset(int offset) {
  error_jump_offset_ = offset;
}

// Stack changes of ComprehensionNextStep.
//
// Stack before:
// 0. iter_range (list)
// 1. old current_index in iter_range (int64_t)
// 2. loop_step or accu_init (any)
//
// Stack after:
// 0. iter_range (list)
// 1. new current_index in iter_range (int64_t)
//
// When iter_range is not a list, this step jumps to error_jump_offset_ that is
// controlled by set_error_jump_offset. In that case the stack is cleared
// from values related to this comprehension and an error is put on the stack.
//
// Stack on error:
// 0. error
absl::Status ComprehensionNextStep::Evaluate(ExecutionFrame* frame) const {
  enum {
    POS_ITER_RANGE,
    POS_CURRENT_INDEX,
    POS_LOOP_STEP_ACCU,
  };
  constexpr int kStackSize = 3;
  if (!frame->value_stack().HasEnough(kStackSize)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }
  absl::Span<const Value> state = frame->value_stack().GetSpan(kStackSize);

  // Get range from the stack.
  const cel::Value& iter_range = state[POS_ITER_RANGE];
  if (!iter_range->Is<cel::ListValue>()) {
    if (iter_range->Is<cel::ErrorValue>() ||
        iter_range->Is<cel::UnknownValue>()) {
      frame->value_stack().PopAndPush(kStackSize, std::move(iter_range));
    } else {
      frame->value_stack().PopAndPush(
          kStackSize, frame->value_factory().CreateErrorValue(
                          CreateNoMatchingOverloadError("<iter_range>")));
    }
    return frame->JumpTo(error_jump_offset_);
  }
  ListValue iter_range_list = Cast<ListValue>(iter_range);

  // Get the current index off the stack.
  const auto& current_index_value = state[POS_CURRENT_INDEX];
  if (!InstanceOf<IntValue>(current_index_value)) {
    return absl::InternalError(absl::StrCat(
        "ComprehensionNextStep: want int, got ",
        cel::KindToString(ValueKindToKind(current_index_value->kind()))));
  }
  CEL_RETURN_IF_ERROR(frame->IncrementIterations());

  int64_t next_index = Cast<IntValue>(current_index_value).NativeValue() + 1;

  frame->comprehension_slots().Set(accu_slot_, state[POS_LOOP_STEP_ACCU]);

  if (next_index >= static_cast<int64_t>(iter_range_list.Size())) {
    // Make sure the iter var is out of scope.
    frame->comprehension_slots().ClearSlot(iter_slot_);
    // pop loop step
    frame->value_stack().Pop(1);
    // jump to result production step
    return frame->JumpTo(jump_offset_);
  }

  AttributeTrail iter_trail;
  if (frame->enable_unknowns()) {
    iter_trail =
        frame->value_stack().GetAttributeSpan(kStackSize)[POS_ITER_RANGE].Step(
            cel::AttributeQualifier::OfInt(next_index));
  }

  Value current_value;
  if (frame->enable_unknowns() && frame->attribute_utility().CheckForUnknown(
                                      iter_trail, /*use_partial=*/false)) {
    current_value =
        frame->attribute_utility().CreateUnknownSet(iter_trail.attribute());
  } else {
    CEL_ASSIGN_OR_RETURN(current_value,
                         iter_range_list.Get(frame->value_factory(),
                                             static_cast<size_t>(next_index)));
  }

  // pop loop step
  // pop old current_index
  // push new current_index
  frame->value_stack().PopAndPush(
      2, frame->value_factory().CreateIntValue(next_index));
  frame->comprehension_slots().Set(iter_slot_, std::move(current_value),
                                   std::move(iter_trail));
  return absl::OkStatus();
}

ComprehensionCondStep::ComprehensionCondStep(size_t iter_slot, size_t accu_slot,
                                             bool shortcircuiting,
                                             int64_t expr_id)
    : ExpressionStepBase(expr_id, false),
      iter_slot_(iter_slot),
      accu_slot_(accu_slot),
      shortcircuiting_(shortcircuiting) {}

void ComprehensionCondStep::set_jump_offset(int offset) {
  jump_offset_ = offset;
}

void ComprehensionCondStep::set_error_jump_offset(int offset) {
  error_jump_offset_ = offset;
}

// Check the break condition for the comprehension.
//
// If the condition is false jump to the `result` subexpression.
// If not a bool, clear stack and jump past the result expression.
// Otherwise, continue to the accumulate step.
// Stack changes by ComprehensionCondStep.
//
// Stack size before: 3.
// Stack size after: 2.
// Stack size on error: 1.
absl::Status ComprehensionCondStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(3)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }
  auto& loop_condition_value = frame->value_stack().Peek();
  if (!loop_condition_value->Is<cel::BoolValue>()) {
    if (loop_condition_value->Is<cel::ErrorValue>() ||
        loop_condition_value->Is<cel::UnknownValue>()) {
      frame->value_stack().PopAndPush(3, std::move(loop_condition_value));
    } else {
      frame->value_stack().PopAndPush(
          3, frame->value_factory().CreateErrorValue(
                 CreateNoMatchingOverloadError("<loop_condition>")));
    }
    // The error jump skips the ComprehensionFinish clean-up step, so we
    // need to update the iteration variable stack here.
    frame->comprehension_slots().ClearSlot(iter_slot_);
    frame->comprehension_slots().ClearSlot(accu_slot_);
    return frame->JumpTo(error_jump_offset_);
  }
  bool loop_condition = loop_condition_value.As<cel::BoolValue>().NativeValue();
  frame->value_stack().Pop(1);  // loop_condition
  if (!loop_condition && shortcircuiting_) {
    return frame->JumpTo(jump_offset_);
  }
  return absl::OkStatus();
}

std::unique_ptr<ExpressionStep> CreateComprehensionFinishStep(size_t accu_slot,
                                                              int64_t expr_id) {
  return std::make_unique<ComprehensionFinish>(accu_slot, expr_id);
}

std::unique_ptr<ExpressionStep> CreateComprehensionInitStep(int64_t expr_id) {
  return std::make_unique<ComprehensionInitStep>(expr_id);
}

}  // namespace google::api::expr::runtime
