#include "eval/eval/jump_step.h"

#include <cstdint>

#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "eval/eval/expression_step_base.h"

namespace google::api::expr::runtime {

namespace {

class JumpStep : public JumpStepBase {
 public:
  // Constructs FunctionStep that uses overloads specified.
  JumpStep(absl::optional<int> jump_offset, int64_t expr_id)
      : JumpStepBase(jump_offset, expr_id) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    return Jump(frame);
  }
};

class CondJumpStep : public JumpStepBase {
 public:
  // Constructs FunctionStep that uses overloads specified.
  CondJumpStep(bool jump_condition, bool leave_on_stack,
               absl::optional<int> jump_offset, int64_t expr_id)
      : JumpStepBase(jump_offset, expr_id),
        jump_condition_(jump_condition),
        leave_on_stack_(leave_on_stack) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    // Peek the top value
    if (!frame->value_stack().HasEnough(1)) {
      return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
    }

    CelValue value = frame->value_stack().Peek();

    if (!leave_on_stack_) {
      frame->value_stack().Pop(1);
    }

    if (value.IsBool() && jump_condition_ == value.BoolOrDie()) {
      return Jump(frame);
    }

    return absl::OkStatus();
  }

 private:
  const bool jump_condition_;
  const bool leave_on_stack_;
};

class BoolCheckJumpStep : public JumpStepBase {
 public:
  // Checks if the top value is a boolean:
  // - no-op if it is a boolean
  // - jump to the label if it is an error value
  // - jump to the label if it is unknown value
  // - jump to the label if it is neither an error nor a boolean, pops it and
  // pushes "no matching overload" error
  BoolCheckJumpStep(absl::optional<int> jump_offset, int64_t expr_id)
      : JumpStepBase(jump_offset, expr_id) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override {
    // Peek the top value
    if (!frame->value_stack().HasEnough(1)) {
      return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
    }

    CelValue value = frame->value_stack().Peek();

    if (value.IsError()) {
      return Jump(frame);
    }

    if (value.IsUnknownSet()) {
      return Jump(frame);
    }

    if (!value.IsBool()) {
      CelValue error_value = CreateNoMatchingOverloadError(
          frame->memory_manager(), "<jump_condition>");
      frame->value_stack().PopAndPush(error_value);
      return Jump(frame);
    }

    return absl::OkStatus();
  }
};

}  // namespace

// Factory method for Conditional Jump step.
// Conditional Jump requires a boolean value to sit on the stack.
// It is compared to jump_condition, and if matched, jump is performed.
absl::StatusOr<std::unique_ptr<JumpStepBase>> CreateCondJumpStep(
    bool jump_condition, bool leave_on_stack, absl::optional<int> jump_offset,
    int64_t expr_id) {
  return absl::make_unique<CondJumpStep>(jump_condition, leave_on_stack,
                                         jump_offset, expr_id);
}

// Factory method for Jump step.
absl::StatusOr<std::unique_ptr<JumpStepBase>> CreateJumpStep(
    absl::optional<int> jump_offset, int64_t expr_id) {
  return absl::make_unique<JumpStep>(jump_offset, expr_id);
}

// Factory method for Conditional Jump step.
// Conditional Jump requires a value to sit on the stack.
// If this value is an error or unknown, a jump is performed.
absl::StatusOr<std::unique_ptr<JumpStepBase>> CreateBoolCheckJumpStep(
    absl::optional<int> jump_offset, int64_t expr_id) {
  return absl::make_unique<BoolCheckJumpStep>(jump_offset, expr_id);
}

// TODO(issues/41) Make sure Unknowns are properly supported by ternary
// operation.

}  // namespace google::api::expr::runtime
