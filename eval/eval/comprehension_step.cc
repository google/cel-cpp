#include "eval/eval/comprehension_step.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "common/casting.h"
#include "common/kind.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/comprehension_slots.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/internal/errors.h"
#include "eval/public/cel_attribute.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {
namespace {

using ::cel::AttributeQualifier;
using ::cel::BoolValue;
using ::cel::Cast;
using ::cel::InstanceOf;
using ::cel::IntValue;
using ::cel::ListValue;
using ::cel::MapValue;
using ::cel::UnknownValue;
using ::cel::Value;
using ::cel::ValueKind;
using ::cel::runtime_internal::CreateNoMatchingOverloadError;

AttributeQualifier AttributeQualifierFromValue(const Value& v) {
  switch (v->kind()) {
    case ValueKind::kString:
      return AttributeQualifier::OfString(v.GetString().ToString());
    case ValueKind::kInt64:
      return AttributeQualifier::OfInt(v.GetInt().NativeValue());
    case ValueKind::kUint64:
      return AttributeQualifier::OfUint(v.GetUint().NativeValue());
    case ValueKind::kBool:
      return AttributeQualifier::OfBool(v.GetBool().NativeValue());
    default:
      // Non-matching qualifier.
      return AttributeQualifier();
  }
}

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
  frame->value_stack().Push(std::move(result));
  frame->comprehension_slots().ClearSlot(accu_slot_);
  return absl::OkStatus();
}

class ComprehensionFinish2 final : public ExpressionStepBase {
 public:
  ComprehensionFinish2(size_t accu_slot, int64_t expr_id)
      : ExpressionStepBase(expr_id), accu_slot_(accu_slot) {}

  // Stack changes of ComprehensionFinish.
  //
  // Stack size before: 4.
  // Stack size after: 1.
  absl::Status Evaluate(ExecutionFrame* frame) const override {
    if (!frame->value_stack().HasEnough(4)) {
      return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
    }
    Value result = frame->value_stack().Peek();
    frame->value_stack().Pop(4);
    frame->value_stack().Push(std::move(result));
    frame->comprehension_slots().ClearSlot(accu_slot_);
    return absl::OkStatus();
  }

 private:
  size_t accu_slot_;
};

class ComprehensionInitStep : public ExpressionStepBase {
 public:
  explicit ComprehensionInitStep(int64_t expr_id)
      : ExpressionStepBase(expr_id, false) {}
  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  absl::Status ProjectKeys(ExecutionFrame* frame) const;
};

absl::StatusOr<Value> ProjectKeysImpl(ExecutionFrameBase& frame,
                                      const MapValue& range,
                                      const AttributeTrail& trail) {
  // Top of stack is map, but could be partially unknown. To tolerate cases when
  // keys are not set for declared unknown values, convert to an unknown set.
  if (frame.unknown_processing_enabled()) {
    if (frame.attribute_utility().CheckForUnknownPartial(trail)) {
      return frame.attribute_utility().CreateUnknownSet(trail.attribute());
    }
  }

  return range.ListKeys(frame.descriptor_pool(), frame.message_factory(),
                        frame.arena());
}

absl::Status ComprehensionInitStep::ProjectKeys(ExecutionFrame* frame) const {
  const auto& map_value = Cast<MapValue>(frame->value_stack().Peek());
  CEL_ASSIGN_OR_RETURN(
      Value keys,
      ProjectKeysImpl(*frame, map_value, frame->value_stack().PeekAttribute()));

  frame->value_stack().PopAndPush(std::move(keys));
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
    frame->value_stack().PopAndPush(
        cel::ErrorValue(CreateNoMatchingOverloadError("<iter_range>")));
  }

  // Initialize current index.
  // Error handling for wrong range type is deferred until the 'Next' step
  // to simplify the number of jumps.
  frame->value_stack().Push(cel::IntValue(-1));
  return absl::OkStatus();
}

class ComprehensionInitStep2 final : public ExpressionStepBase {
 public:
  explicit ComprehensionInitStep2(int64_t expr_id)
      : ExpressionStepBase(expr_id, false) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    if (!frame->value_stack().HasEnough(1)) {
      return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
    }

    const auto& range = frame->value_stack().Peek();
    switch (range.kind()) {
      case ValueKind::kMap: {
        CEL_ASSIGN_OR_RETURN(
            Value keys, ProjectKeysImpl(*frame, range.GetMap(),
                                        frame->value_stack().PeekAttribute()));
        frame->value_stack().Push(std::move(keys));
      } break;
      case ValueKind::kList:
        ABSL_FALLTHROUGH_INTENDED;
      case ValueKind::kError:
        ABSL_FALLTHROUGH_INTENDED;
      case ValueKind::kUnknown:
        frame->value_stack().Push(range);
        break;
      default:
        frame->value_stack().PopAndPush(
            cel::ErrorValue(CreateNoMatchingOverloadError("<iter_range>")));
        break;
    }

    // Initialize current index.
    // Error handling for wrong range type is deferred until the 'Next' step
    // to simplify the number of jumps.
    frame->value_stack().Push(cel::IntValue(-1));
    return absl::OkStatus();
  }
};

class ComprehensionDirectStep : public DirectExpressionStep {
 public:
  explicit ComprehensionDirectStep(
      size_t iter_slot, size_t iter2_slot, size_t accu_slot,
      std::unique_ptr<DirectExpressionStep> range,
      std::unique_ptr<DirectExpressionStep> accu_init,
      std::unique_ptr<DirectExpressionStep> loop_step,
      std::unique_ptr<DirectExpressionStep> condition_step,
      std::unique_ptr<DirectExpressionStep> result_step, bool shortcircuiting,
      int64_t expr_id)
      : DirectExpressionStep(expr_id),
        iter_slot_(iter_slot),
        iter2_slot_(iter2_slot),
        accu_slot_(accu_slot),
        range_(std::move(range)),
        accu_init_(std::move(accu_init)),
        loop_step_(std::move(loop_step)),
        condition_(std::move(condition_step)),
        result_step_(std::move(result_step)),
        shortcircuiting_(shortcircuiting) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, Value& result,
                        AttributeTrail& trail) const final {
    return iter_slot_ == iter2_slot_ ? Evaluate1(frame, result, trail)
                                     : Evaluate2(frame, result, trail);
  }

 private:
  absl::Status Evaluate1(ExecutionFrameBase& frame, Value& result,
                         AttributeTrail& trail) const;

  absl::Status Evaluate2(ExecutionFrameBase& frame, Value& result,
                         AttributeTrail& trail) const;

  size_t iter_slot_;
  size_t iter2_slot_;
  size_t accu_slot_;
  std::unique_ptr<DirectExpressionStep> range_;
  std::unique_ptr<DirectExpressionStep> accu_init_;
  std::unique_ptr<DirectExpressionStep> loop_step_;
  std::unique_ptr<DirectExpressionStep> condition_;
  std::unique_ptr<DirectExpressionStep> result_step_;

  bool shortcircuiting_;
};

absl::Status ComprehensionDirectStep::Evaluate1(ExecutionFrameBase& frame,
                                                Value& result,
                                                AttributeTrail& trail) const {
  cel::Value range;
  AttributeTrail range_attr;
  CEL_RETURN_IF_ERROR(range_->Evaluate(frame, range, range_attr));

  if (InstanceOf<MapValue>(range)) {
    const auto& map_value = Cast<MapValue>(range);
    CEL_ASSIGN_OR_RETURN(range, ProjectKeysImpl(frame, map_value, range_attr));
  }

  switch (range.kind()) {
    case cel::ValueKind::kError:
    case cel::ValueKind::kUnknown:
      result = range;
      return absl::OkStatus();
      break;
    default:
      if (!InstanceOf<ListValue>(range)) {
        result = cel::ErrorValue(CreateNoMatchingOverloadError("<iter_range>"));
        return absl::OkStatus();
      }
  }

  const auto& range_list = Cast<ListValue>(range);

  Value accu_init;
  AttributeTrail accu_init_attr;
  CEL_RETURN_IF_ERROR(accu_init_->Evaluate(frame, accu_init, accu_init_attr));

  frame.comprehension_slots().Set(accu_slot_, std::move(accu_init),
                                  accu_init_attr);
  ComprehensionSlots::Slot* accu_slot =
      frame.comprehension_slots().Get(accu_slot_);
  ABSL_DCHECK(accu_slot != nullptr);

  ComprehensionSlots::Slot* iter_slot =
      frame.comprehension_slots().Set(iter_slot_);
  ABSL_DCHECK(iter_slot != nullptr);

  Value condition;
  AttributeTrail condition_attr;
  bool should_skip_result = false;
  CEL_RETURN_IF_ERROR(range_list.ForEach(
      [&](size_t index, const Value& v) -> absl::StatusOr<bool> {
        CEL_RETURN_IF_ERROR(frame.IncrementIterations());

        // Set the iterator variable(s) first, the loop condition has access to
        // them.
        iter_slot->value = v;
        if (frame.unknown_processing_enabled()) {
          iter_slot->attribute =
              range_attr.Step(CelAttributeQualifier::OfInt(index));
          if (frame.attribute_utility().CheckForUnknownExact(
                  iter_slot->attribute)) {
            iter_slot->value = frame.attribute_utility().CreateUnknownSet(
                iter_slot->attribute.attribute());
          }
        }

        // Evaluate the loop condition.
        CEL_RETURN_IF_ERROR(
            condition_->Evaluate(frame, condition, condition_attr));

        if (condition.kind() == cel::ValueKind::kError ||
            condition.kind() == cel::ValueKind::kUnknown) {
          result = std::move(condition);
          should_skip_result = true;
          return false;
        }
        if (condition.kind() != cel::ValueKind::kBool) {
          result = cel::ErrorValue(
              CreateNoMatchingOverloadError("<loop_condition>"));
          should_skip_result = true;
          return false;
        }
        if (shortcircuiting_ && !Cast<BoolValue>(condition).NativeValue()) {
          return false;
        }

        // Evaluate the loop step.
        CEL_RETURN_IF_ERROR(loop_step_->Evaluate(frame, accu_slot->value,
                                                 accu_slot->attribute));

        return true;
      },
      frame.descriptor_pool(), frame.message_factory(), frame.arena()));

  frame.comprehension_slots().ClearSlot(iter_slot_);
  // Error state is already set to the return value, just clean up.
  if (should_skip_result) {
    frame.comprehension_slots().ClearSlot(accu_slot_);
    return absl::OkStatus();
  }

  CEL_RETURN_IF_ERROR(result_step_->Evaluate(frame, result, trail));
  frame.comprehension_slots().ClearSlot(accu_slot_);
  return absl::OkStatus();
}

absl::Status ComprehensionDirectStep::Evaluate2(ExecutionFrameBase& frame,
                                                Value& result,
                                                AttributeTrail& trail) const {
  cel::Value iter2_range;
  AttributeTrail range_attr;
  CEL_RETURN_IF_ERROR(range_->Evaluate(frame, iter2_range, range_attr));

  absl::optional<MapValue> iter2_range_map;
  cel::Value iter_range;
  if (iter2_range.IsMap()) {
    iter2_range_map = iter2_range.GetMap();
    CEL_ASSIGN_OR_RETURN(iter_range,
                         ProjectKeysImpl(frame, *iter2_range_map, range_attr));
  } else {
    iter_range = iter2_range;
  }

  switch (iter_range.kind()) {
    case cel::ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case cel::ValueKind::kUnknown:
      result = iter_range;
      return absl::OkStatus();
    case cel::ValueKind::kList:
      break;
    default:
      result = cel::ErrorValue(CreateNoMatchingOverloadError("<iter_range>"));
      return absl::OkStatus();
  }

  const auto& iter_range_list = iter_range.GetList();

  Value accu_init;
  AttributeTrail accu_init_attr;
  CEL_RETURN_IF_ERROR(accu_init_->Evaluate(frame, accu_init, accu_init_attr));

  frame.comprehension_slots().Set(accu_slot_, std::move(accu_init),
                                  accu_init_attr);
  ComprehensionSlots::Slot* accu_slot =
      frame.comprehension_slots().Get(accu_slot_);
  ABSL_DCHECK(accu_slot != nullptr);

  ComprehensionSlots::Slot* iter_slot =
      frame.comprehension_slots().Set(iter_slot_);
  ABSL_DCHECK(iter_slot != nullptr);

  ComprehensionSlots::Slot* iter2_slot =
      frame.comprehension_slots().Set(iter2_slot_);
  ABSL_DCHECK(iter2_slot != nullptr);

  Value condition;
  AttributeTrail condition_attr;
  bool should_skip_result = false;
  if (iter2_range_map) {
    CEL_RETURN_IF_ERROR(iter2_range_map->ForEach(
        [&](const Value& k, const Value& v) -> absl::StatusOr<bool> {
          CEL_RETURN_IF_ERROR(frame.IncrementIterations());

          // Set the iterator variable(s) first, the loop condition has access
          // to them.
          iter_slot->value = k;
          if (frame.unknown_processing_enabled()) {
            iter_slot->attribute =
                range_attr.Step(AttributeQualifierFromValue(k));
            if (frame.attribute_utility().CheckForUnknownExact(
                    iter_slot->attribute)) {
              iter_slot->value = frame.attribute_utility().CreateUnknownSet(
                  iter_slot->attribute.attribute());
            }
          }

          iter2_slot->value = v;
          if (frame.unknown_processing_enabled()) {
            iter2_slot->attribute =
                range_attr.Step(AttributeQualifierFromValue(v));
            if (frame.attribute_utility().CheckForUnknownExact(
                    iter2_slot->attribute)) {
              iter2_slot->value = frame.attribute_utility().CreateUnknownSet(
                  iter2_slot->attribute.attribute());
            }
          }

          // Evaluate the loop condition.
          CEL_RETURN_IF_ERROR(
              condition_->Evaluate(frame, condition, condition_attr));

          if (condition.kind() == cel::ValueKind::kError ||
              condition.kind() == cel::ValueKind::kUnknown) {
            result = std::move(condition);
            should_skip_result = true;
            return false;
          }
          if (condition.kind() != cel::ValueKind::kBool) {
            result = cel::ErrorValue(
                CreateNoMatchingOverloadError("<loop_condition>"));
            should_skip_result = true;
            return false;
          }
          if (shortcircuiting_ && !Cast<BoolValue>(condition).NativeValue()) {
            return false;
          }

          // Evaluate the loop step.
          CEL_RETURN_IF_ERROR(loop_step_->Evaluate(frame, accu_slot->value,
                                                   accu_slot->attribute));

          return true;
        },
        frame.descriptor_pool(), frame.message_factory(), frame.arena()));
  } else {
    CEL_RETURN_IF_ERROR(iter_range_list.ForEach(
        [&](size_t index, const Value& v) -> absl::StatusOr<bool> {
          CEL_RETURN_IF_ERROR(frame.IncrementIterations());

          // Set the iterator variable(s) first, the loop condition has access
          // to them.
          iter_slot->value = IntValue(index);
          if (frame.unknown_processing_enabled()) {
            iter_slot->attribute =
                range_attr.Step(CelAttributeQualifier::OfInt(index));
            if (frame.attribute_utility().CheckForUnknownExact(
                    iter_slot->attribute)) {
              iter_slot->value = frame.attribute_utility().CreateUnknownSet(
                  iter_slot->attribute.attribute());
            }
          }
          iter2_slot->value = v;
          if (frame.unknown_processing_enabled()) {
            iter2_slot->attribute =
                range_attr.Step(AttributeQualifierFromValue(v));
            if (frame.attribute_utility().CheckForUnknownExact(
                    iter2_slot->attribute)) {
              iter2_slot->value = frame.attribute_utility().CreateUnknownSet(
                  iter2_slot->attribute.attribute());
            }
          }

          // Evaluate the loop condition.
          CEL_RETURN_IF_ERROR(
              condition_->Evaluate(frame, condition, condition_attr));

          if (condition.kind() == cel::ValueKind::kError ||
              condition.kind() == cel::ValueKind::kUnknown) {
            result = std::move(condition);
            should_skip_result = true;
            return false;
          }
          if (condition.kind() != cel::ValueKind::kBool) {
            result = cel::ErrorValue(
                CreateNoMatchingOverloadError("<loop_condition>"));
            should_skip_result = true;
            return false;
          }
          if (shortcircuiting_ && !Cast<BoolValue>(condition).NativeValue()) {
            return false;
          }

          // Evaluate the loop step.
          CEL_RETURN_IF_ERROR(loop_step_->Evaluate(frame, accu_slot->value,
                                                   accu_slot->attribute));

          return true;
        },
        frame.descriptor_pool(), frame.message_factory(), frame.arena()));
  }

  frame.comprehension_slots().ClearSlot(iter_slot_);
  frame.comprehension_slots().ClearSlot(iter2_slot_);
  // Error state is already set to the return value, just clean up.
  if (should_skip_result) {
    frame.comprehension_slots().ClearSlot(accu_slot_);
    return absl::OkStatus();
  }

  CEL_RETURN_IF_ERROR(result_step_->Evaluate(frame, result, trail));
  frame.comprehension_slots().ClearSlot(accu_slot_);
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

ComprehensionNextStep::ComprehensionNextStep(size_t iter_slot,
                                             size_t iter2_slot,
                                             size_t accu_slot, int64_t expr_id)
    : ExpressionStepBase(expr_id, false),
      iter_slot_(iter_slot),
      iter2_slot_(iter2_slot),
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
absl::Status ComprehensionNextStep::Evaluate1(ExecutionFrame* frame) const {
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
          kStackSize,
          cel::ErrorValue(CreateNoMatchingOverloadError("<iter_range>")));
    }
    return frame->JumpTo(error_jump_offset_);
  }
  const ListValue& iter_range_list = Cast<ListValue>(iter_range);

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

  CEL_ASSIGN_OR_RETURN(auto iter_range_list_size, iter_range_list.Size());

  if (next_index >= static_cast<int64_t>(iter_range_list_size)) {
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
    CEL_RETURN_IF_ERROR(iter_range_list.Get(
        static_cast<size_t>(next_index), frame->descriptor_pool(),
        frame->message_factory(), frame->arena(), &current_value));
  }

  // pop loop step
  // pop old current_index
  // push new current_index
  frame->value_stack().PopAndPush(2, cel::IntValue(next_index));
  frame->comprehension_slots().Set(iter_slot_, std::move(current_value),
                                   std::move(iter_trail));
  return absl::OkStatus();
}

absl::Status ComprehensionNextStep::Evaluate2(ExecutionFrame* frame) const {
  enum {
    POS_ITER2_RANGE,  // Map or same as POS_ITER_RANGE.
    POS_ITER_RANGE,
    POS_CURRENT_INDEX,
    POS_LOOP_STEP_ACCU,
  };
  constexpr int kStackSize = 4;
  if (!frame->value_stack().HasEnough(kStackSize)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }
  absl::Span<const Value> state = frame->value_stack().GetSpan(kStackSize);

  const cel::Value& iter2_range = state[POS_ITER2_RANGE];
  absl::optional<MapValue> iter2_range_map;
  switch (iter2_range.kind()) {
    case ValueKind::kMap:
      iter2_range_map = iter2_range.GetMap();
      break;
    case ValueKind::kList:
      break;
    case ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUnknown:
      // Leave it on the stack.
      frame->value_stack().PopAndPush(kStackSize, std::move(iter2_range));
      return frame->JumpTo(error_jump_offset_);
    default:
      frame->value_stack().PopAndPush(
          kStackSize,
          cel::ErrorValue(CreateNoMatchingOverloadError("<iter_range>")));
      return frame->JumpTo(error_jump_offset_);
  }

  // Get range from the stack.
  const cel::Value& iter_range = state[POS_ITER_RANGE];
  switch (iter_range.kind()) {
    case ValueKind::kList:
      break;
    case ValueKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUnknown:
      frame->value_stack().PopAndPush(kStackSize, std::move(iter_range));
      return frame->JumpTo(error_jump_offset_);
    default:
      frame->value_stack().PopAndPush(
          kStackSize,
          cel::ErrorValue(CreateNoMatchingOverloadError("<iter_range>")));
      return frame->JumpTo(error_jump_offset_);
  }
  ListValue iter_range_list = iter_range.GetList();

  // Get the current index off the stack.
  const cel::Value& current_index_value = state[POS_CURRENT_INDEX];
  if (!current_index_value.IsInt()) {
    return absl::InternalError(absl::StrCat(
        "ComprehensionNextStep: want int, got ",
        cel::KindToString(ValueKindToKind(current_index_value.kind()))));
  }
  CEL_RETURN_IF_ERROR(frame->IncrementIterations());

  int64_t next_index = current_index_value.GetInt().NativeValue() + 1;

  frame->comprehension_slots().Set(accu_slot_, state[POS_LOOP_STEP_ACCU]);

  CEL_ASSIGN_OR_RETURN(auto iter_range_list_size, iter_range_list.Size());

  if (next_index >= static_cast<int64_t>(iter_range_list_size)) {
    // Make sure the iter var is out of scope.
    frame->comprehension_slots().ClearSlot(iter_slot_);
    frame->comprehension_slots().ClearSlot(iter2_slot_);
    // pop loop step
    frame->value_stack().Pop(1);
    // jump to result production step
    return frame->JumpTo(jump_offset_);
  }

  AttributeTrail iter_range_trail;
  if (frame->enable_unknowns()) {
    iter_range_trail =
        frame->value_stack().GetAttributeSpan(kStackSize)[POS_ITER_RANGE].Step(
            cel::AttributeQualifier::OfInt(next_index));
  }

  Value current_iter_var;
  if (frame->enable_unknowns() &&
      frame->attribute_utility().CheckForUnknown(iter_range_trail,
                                                 /*use_partial=*/false)) {
    current_iter_var = frame->attribute_utility().CreateUnknownSet(
        iter_range_trail.attribute());
  } else {
    CEL_RETURN_IF_ERROR(iter_range_list.Get(
        static_cast<size_t>(next_index), frame->descriptor_pool(),
        frame->message_factory(), frame->arena(), &current_iter_var));
  }

  AttributeTrail iter2_range_trail;
  Value current_iter_var2;
  if (iter2_range_map) {
    AttributeTrail iter2_range_trail;
    if (frame->enable_unknowns()) {
      iter2_range_trail =
          frame->value_stack()
              .GetAttributeSpan(kStackSize)[POS_ITER2_RANGE]
              .Step(AttributeQualifierFromValue(current_iter_var));
    }
    if (frame->enable_unknowns() &&
        frame->attribute_utility().CheckForUnknown(iter2_range_trail,
                                                   /*use_partial=*/false)) {
      current_iter_var2 = frame->attribute_utility().CreateUnknownSet(
          iter2_range_trail.attribute());
    } else {
      CEL_RETURN_IF_ERROR(iter2_range_map->Get(
          current_iter_var, frame->descriptor_pool(), frame->message_factory(),
          frame->arena(), &current_iter_var2));
    }
  } else {
    iter2_range_trail = iter_range_trail;
    current_iter_var2 = current_iter_var;
    current_iter_var = IntValue(next_index);
  }

  // pop loop step
  // pop old current_index
  // push new current_index
  frame->value_stack().PopAndPush(2, cel::IntValue(next_index));
  frame->comprehension_slots().Set(iter_slot_, std::move(current_iter_var),
                                   std::move(iter_range_trail));
  frame->comprehension_slots().Set(iter2_slot_, std::move(current_iter_var2),
                                   std::move(iter2_range_trail));
  return absl::OkStatus();
}

ComprehensionCondStep::ComprehensionCondStep(size_t iter_slot,
                                             size_t iter2_slot,
                                             size_t accu_slot,
                                             bool shortcircuiting,
                                             int64_t expr_id)
    : ExpressionStepBase(expr_id, false),
      iter_slot_(iter_slot),
      iter2_slot_(iter2_slot),
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
absl::Status ComprehensionCondStep::Evaluate1(ExecutionFrame* frame) const {
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
          3,
          cel::ErrorValue(CreateNoMatchingOverloadError("<loop_condition>")));
    }
    // The error jump skips the ComprehensionFinish clean-up step, so we
    // need to update the iteration variable stack here.
    frame->comprehension_slots().ClearSlot(iter_slot_);
    frame->comprehension_slots().ClearSlot(accu_slot_);
    return frame->JumpTo(error_jump_offset_);
  }
  bool loop_condition = loop_condition_value.GetBool().NativeValue();
  frame->value_stack().Pop(1);  // loop_condition
  if (!loop_condition && shortcircuiting_) {
    return frame->JumpTo(jump_offset_);
  }
  return absl::OkStatus();
}

// Check the break condition for the comprehension.
//
// If the condition is false jump to the `result` subexpression.
// If not a bool, clear stack and jump past the result expression.
// Otherwise, continue to the accumulate step.
// Stack changes by ComprehensionCondStep.
//
// Stack size before: 4.
// Stack size after: 3.
// Stack size on error: 1.
absl::Status ComprehensionCondStep::Evaluate2(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(4)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }
  auto& loop_condition_value = frame->value_stack().Peek();
  if (!loop_condition_value->Is<cel::BoolValue>()) {
    if (loop_condition_value->Is<cel::ErrorValue>() ||
        loop_condition_value->Is<cel::UnknownValue>()) {
      frame->value_stack().PopAndPush(4, std::move(loop_condition_value));
    } else {
      frame->value_stack().PopAndPush(
          4,
          cel::ErrorValue(CreateNoMatchingOverloadError("<loop_condition>")));
    }
    // The error jump skips the ComprehensionFinish clean-up step, so we
    // need to update the iteration variable stack here.
    frame->comprehension_slots().ClearSlot(iter_slot_);
    frame->comprehension_slots().ClearSlot(iter2_slot_);
    frame->comprehension_slots().ClearSlot(accu_slot_);
    return frame->JumpTo(error_jump_offset_);
  }
  bool loop_condition = loop_condition_value.GetBool().NativeValue();
  frame->value_stack().Pop(1);  // loop_condition
  if (!loop_condition && shortcircuiting_) {
    return frame->JumpTo(jump_offset_);
  }
  return absl::OkStatus();
}

std::unique_ptr<DirectExpressionStep> CreateDirectComprehensionStep(
    size_t iter_slot, size_t iter2_slot, size_t accu_slot,
    std::unique_ptr<DirectExpressionStep> range,
    std::unique_ptr<DirectExpressionStep> accu_init,
    std::unique_ptr<DirectExpressionStep> loop_step,
    std::unique_ptr<DirectExpressionStep> condition_step,
    std::unique_ptr<DirectExpressionStep> result_step, bool shortcircuiting,
    int64_t expr_id) {
  return std::make_unique<ComprehensionDirectStep>(
      iter_slot, iter2_slot, accu_slot, std::move(range), std::move(accu_init),
      std::move(loop_step), std::move(condition_step), std::move(result_step),
      shortcircuiting, expr_id);
}

std::unique_ptr<ExpressionStep> CreateComprehensionFinishStep(size_t accu_slot,
                                                              int64_t expr_id) {
  return std::make_unique<ComprehensionFinish>(accu_slot, expr_id);
}

std::unique_ptr<ExpressionStep> CreateComprehensionInitStep(int64_t expr_id) {
  return std::make_unique<ComprehensionInitStep>(expr_id);
}

std::unique_ptr<ExpressionStep> CreateComprehensionFinishStep2(
    size_t accu_slot, int64_t expr_id) {
  return std::make_unique<ComprehensionFinish2>(accu_slot, expr_id);
}

std::unique_ptr<ExpressionStep> CreateComprehensionInitStep2(int64_t expr_id) {
  return std::make_unique<ComprehensionInitStep2>(expr_id);
}

}  // namespace google::api::expr::runtime
