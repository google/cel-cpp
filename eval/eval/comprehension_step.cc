#include "eval/eval/comprehension_step.h"

#include <cstdint>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/evaluator_core.h"
#include "eval/public/cel_attribute.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

// Stack variables during comprehension evaluation:
// 0. accu_init, then loop_step (any), available through accu_var
// 1. iter_range (list)
// 2. current index in iter_range (int64_t)
// 3. current_value from iter_range (any), available through iter_var
// 4. loop_condition (bool) OR loop_step (any)

// What to put on ExecutionPath:     stack size
//  0. (dummy)                       1
//  1. iter_range              (dep) 2
//  2. -1                            3
//  3. (dummy)                       4
//  4. accu_init               (dep) 5
//  5. ComprehensionNextStep         4
//  6. loop_condition          (dep) 5
//  7. ComprehensionCondStep         4
//  8. loop_step               (dep) 5
//  9. goto 5.                       5
// 10. result                  (dep) 2
// 11. ComprehensionFinish           1

ComprehensionNextStep::ComprehensionNextStep(const std::string& accu_var,
                                             const std::string& iter_var,
                                             int64_t expr_id)
    : ExpressionStepBase(expr_id, false),
      accu_var_(accu_var),
      iter_var_(iter_var) {}

void ComprehensionNextStep::set_jump_offset(int offset) {
  jump_offset_ = offset;
}

void ComprehensionNextStep::set_error_jump_offset(int offset) {
  error_jump_offset_ = offset;
}

// Stack changes of ComprehensionNextStep.
//
// Stack before:
// 0. previous accu_init or "" on the first iteration
// 1. iter_range (list)
// 2. old current_index in iter_range (int64_t)
// 3. old current_value or "" on the first iteration
// 4. loop_step or accu_init (any)
//
// Stack after:
// 0. loop_step or accu_init (any)
// 1. iter_range (list)
// 2. new current_index in iter_range (int64_t)
// 3. new current_value
//
// Stack on break:
// 0. loop_step or accu_init (any)
//
// When iter_range is not a list, this step jumps to error_jump_offset_ that is
// controlled by set_error_jump_offset. In that case the stack is cleared
// from values related to this comprehension and an error is put on the stack.
//
// Stack on error:
// 0. error
absl::Status ComprehensionNextStep::Evaluate(ExecutionFrame* frame) const {
  enum {
    POS_PREVIOUS_LOOP_STEP,
    POS_ITER_RANGE,
    POS_CURRENT_INDEX,
    POS_CURRENT_VALUE,
    POS_LOOP_STEP,
  };
  if (!frame->value_stack().HasEnough(5)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }
  auto state = frame->value_stack().GetSpan(5);
  auto attr = frame->value_stack().GetAttributeSpan(5);

  // Get range from the stack.
  CelValue iter_range = state[POS_ITER_RANGE];
  if (!iter_range.IsList()) {
    frame->value_stack().Pop(5);
    if (iter_range.IsError() || iter_range.IsUnknownSet()) {
      frame->value_stack().Push(iter_range);
      return frame->JumpTo(error_jump_offset_);
    }
    frame->value_stack().Push(
        CreateNoMatchingOverloadError(frame->memory_manager(), "<iter_range>"));
    return frame->JumpTo(error_jump_offset_);
  }
  const CelList* cel_list = iter_range.ListOrDie();
  const AttributeTrail iter_range_attr = attr[POS_ITER_RANGE];

  // Get the current index off the stack.
  CelValue current_index_value = state[POS_CURRENT_INDEX];
  if (!current_index_value.IsInt64()) {
    return absl::InternalError(
        absl::StrCat("ComprehensionNextStep: want int64_t, got ",
                     CelValue::TypeName(current_index_value.type())));
  }
  CEL_RETURN_IF_ERROR(frame->IncrementIterations());

  int64_t current_index = current_index_value.Int64OrDie();
  if (current_index == -1) {
    CEL_RETURN_IF_ERROR(frame->PushIterFrame(iter_var_, accu_var_));
  }

  // Update stack for breaking out of loop or next round.
  CelValue loop_step = state[POS_LOOP_STEP];
  frame->value_stack().Pop(5);
  frame->value_stack().Push(loop_step);
  CEL_RETURN_IF_ERROR(frame->SetAccuVar(loop_step));
  if (current_index >= cel_list->size() - 1) {
    CEL_RETURN_IF_ERROR(frame->ClearIterVar());
    return frame->JumpTo(jump_offset_);
  }
  frame->value_stack().Push(iter_range, iter_range_attr);
  current_index += 1;

  CelValue current_value = (*cel_list)[current_index];
  frame->value_stack().Push(CelValue::CreateInt64(current_index));
  AttributeTrail iter_trail = iter_range_attr.Step(
      CelAttributeQualifier::Create(CelValue::CreateInt64(current_index)),
      frame->memory_manager());
  frame->value_stack().Push(current_value, iter_trail);
  CEL_RETURN_IF_ERROR(frame->SetIterVar(current_value, std::move(iter_trail)));
  return absl::OkStatus();
}

ComprehensionCondStep::ComprehensionCondStep(const std::string&,
                                             const std::string& iter_var,
                                             bool shortcircuiting,
                                             int64_t expr_id)
    : ExpressionStepBase(expr_id, false),
      iter_var_(iter_var),
      shortcircuiting_(shortcircuiting) {}

void ComprehensionCondStep::set_jump_offset(int offset) {
  jump_offset_ = offset;
}

void ComprehensionCondStep::set_error_jump_offset(int offset) {
  error_jump_offset_ = offset;
}

// Stack changes by ComprehensionCondStep.
//
// Stack size before: 5.
// Stack size after: 4.
// Stack size on break: 1.
absl::Status ComprehensionCondStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(5)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }
  CelValue loop_condition_value = frame->value_stack().Peek();
  if (!loop_condition_value.IsBool()) {
    frame->value_stack().Pop(5);
    if (loop_condition_value.IsError() || loop_condition_value.IsUnknownSet()) {
      frame->value_stack().Push(loop_condition_value);
    } else {
      frame->value_stack().Push(CreateNoMatchingOverloadError(
          frame->memory_manager(), "<loop_condition>"));
    }
    // The error jump skips the ComprehensionFinish clean-up step, so we
    // need to update the iteration variable stack here.
    CEL_RETURN_IF_ERROR(frame->PopIterFrame());
    return frame->JumpTo(error_jump_offset_);
  }
  bool loop_condition = loop_condition_value.BoolOrDie();
  frame->value_stack().Pop(1);  // loop_condition
  if (!loop_condition && shortcircuiting_) {
    frame->value_stack().Pop(3);  // current_value, current_index, iter_range
    return frame->JumpTo(jump_offset_);
  }
  return absl::OkStatus();
}

ComprehensionFinish::ComprehensionFinish(const std::string& accu_var,
                                         const std::string&, int64_t expr_id)
    : ExpressionStepBase(expr_id), accu_var_(accu_var) {}

// Stack changes of ComprehensionFinish.
//
// Stack size before: 2.
// Stack size after: 1.
absl::Status ComprehensionFinish::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(2)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }
  CelValue result = frame->value_stack().Peek();
  frame->value_stack().Pop(1);  // result
  frame->value_stack().PopAndPush(result);
  CEL_RETURN_IF_ERROR(frame->PopIterFrame());
  return absl::OkStatus();
}

class ListKeysStep : public ExpressionStepBase {
 public:
  explicit ListKeysStep(int64_t expr_id) : ExpressionStepBase(expr_id, false) {}
  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  absl::Status ProjectKeys(ExecutionFrame* frame) const;
};

std::unique_ptr<ExpressionStep> CreateListKeysStep(int64_t expr_id) {
  return absl::make_unique<ListKeysStep>(expr_id);
}

absl::Status ListKeysStep::ProjectKeys(ExecutionFrame* frame) const {
  // Top of stack is map, but could be partially unknown. To tolerate cases when
  // keys are not set for declared unknown values, convert to an unknown set.
  if (frame->enable_unknowns()) {
    const UnknownSet* unknown = frame->attribute_utility().MergeUnknowns(
        frame->value_stack().GetSpan(1),
        frame->value_stack().GetAttributeSpan(1), nullptr,
        /*use_partial=*/true);
    if (unknown) {
      frame->value_stack().PopAndPush(CelValue::CreateUnknownSet(unknown));
      return absl::OkStatus();
    }
  }

  const CelValue& map = frame->value_stack().Peek();
  auto list_keys = map.MapOrDie()->ListKeys();
  if (!list_keys.ok()) {
    return std::move(list_keys).status();
  }
  frame->value_stack().PopAndPush(CelValue::CreateList(*list_keys));
  return absl::OkStatus();
}

absl::Status ListKeysStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(1)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }
  const CelValue& map_value = frame->value_stack().Peek();
  if (map_value.IsMap()) {
    return ProjectKeys(frame);
  }
  return absl::OkStatus();
}

}  // namespace google::api::expr::runtime
