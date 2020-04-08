#include "eval/eval/comprehension_step.h"

#include "absl/strings/str_cat.h"
#include "base/status_macros.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

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
  constexpr int
      // kPreviousLoopStep = 0,
      kIterRange = 1,
      kCurrentIndex = 2,
      // kCurrentValue = 3,
      kLoopSet = 4;
  if (!frame->value_stack().HasEnough(5)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }
  auto state = frame->value_stack().GetSpan(5);

  // Get range from the stack.
  CelValue iter_range = state[kIterRange];
  if (!iter_range.IsList()) {
    frame->value_stack().Pop(5);
    if (iter_range.IsError() || iter_range.IsUnknownSet()) {
      frame->value_stack().Push(iter_range);
      return frame->JumpTo(error_jump_offset_);
    }
    frame->value_stack().Push(CreateNoMatchingOverloadError(frame->arena()));
    return frame->JumpTo(error_jump_offset_);
  }
  const CelList* cel_list = iter_range.ListOrDie();

  // Get the current index off the stack.
  CelValue current_index_value = state[kCurrentIndex];
  if (!current_index_value.IsInt64()) {
    auto message = absl::StrCat(
        "ComprehensionNextStep: want int64_t, got ",
        CelValue::TypeName(current_index_value.type())
    );
    return absl::Status(absl::StatusCode::kInternal, message);
  }
  auto increment_status = frame->IncrementIterations();
  if (!increment_status.ok()) {
    return increment_status;
  }
  int64_t current_index = current_index_value.Int64OrDie();
  if (current_index == -1) {
    RETURN_IF_ERROR(frame->PushIterFrame());
  }

  // Update stack for breaking out of loop or next round.
  CelValue loop_step = state[kLoopSet];
  frame->value_stack().Pop(5);
  frame->value_stack().Push(loop_step);
  RETURN_IF_ERROR(frame->SetIterVar(accu_var_, loop_step));
  if (current_index >= cel_list->size() - 1) {
    RETURN_IF_ERROR(frame->ClearIterVar(iter_var_));
    return frame->JumpTo(jump_offset_);
  }
  frame->value_stack().Push(iter_range);
  current_index += 1;
  CelValue current_value = (*cel_list)[current_index];
  frame->value_stack().Push(CelValue::CreateInt64(current_index));
  frame->value_stack().Push(current_value);
  RETURN_IF_ERROR(frame->SetIterVar(iter_var_, current_value));
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
      frame->value_stack().Push(CreateNoMatchingOverloadError(frame->arena()));
    }
    // The error jump skips the ComprehensionFinish clean-up step, so we
    // need to update the iteration variable stack here.
    RETURN_IF_ERROR(frame->PopIterFrame());
    return frame->JumpTo(error_jump_offset_);
  }

  bool loop_condition = loop_condition_value.BoolOrDie();
  frame->value_stack().Pop(1);  // loop_condition
  if (!loop_condition && shortcircuiting_) {
    frame->value_stack().Pop(3);  // current_value, current_index, iter_range
    RETURN_IF_ERROR(frame->ClearIterVar(iter_var_));
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
  RETURN_IF_ERROR(frame->PopIterFrame());
  return absl::OkStatus();
}

class ListKeysStep : public ExpressionStepBase {
 public:
  ListKeysStep(int64_t expr_id) : ExpressionStepBase(expr_id, false) {}
  absl::Status Evaluate(ExecutionFrame* frame) const override;
};

std::unique_ptr<ExpressionStep> CreateListKeysStep(int64_t expr_id) {
  return absl::make_unique<ListKeysStep>(expr_id);
}

absl::Status ListKeysStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(1)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }
  CelValue map_value = frame->value_stack().Peek();
  if (map_value.IsMap()) {
    const CelMap* cel_map = map_value.MapOrDie();
    frame->value_stack().PopAndPush(CelValue::CreateList(cel_map->ListKeys()));
    return absl::OkStatus();
  }
  return absl::OkStatus();
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
