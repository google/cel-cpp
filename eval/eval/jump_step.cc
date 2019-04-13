#include "eval/eval/jump_step.h"
#include "eval/eval/expression_step_base.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

class JumpStep : public JumpStepBase {
 public:
  // Constructs FunctionStep that uses overloads specified.
  JumpStep(absl::optional<int> jump_offset, const google::api::expr::v1alpha1::Expr* expr)
      : JumpStepBase(jump_offset, expr) {}

  util::Status Evaluate(ExecutionFrame* frame) const override {
    return Jump(frame);
  }
};

class CondJumpStep : public JumpStepBase {
 public:
  // Constructs FunctionStep that uses overloads specified.
  CondJumpStep(bool jump_condition, bool leave_on_stack,
               absl::optional<int> jump_offset,
               const google::api::expr::v1alpha1::Expr* expr)
      : JumpStepBase(jump_offset, expr),
        jump_condition_(jump_condition),
        leave_on_stack_(leave_on_stack) {}

  util::Status Evaluate(ExecutionFrame* frame) const override {
    // Peek the top value
    if (!frame->value_stack().HasEnough(1)) {
      return util::MakeStatus(google::rpc::Code::INTERNAL, "Value stack underflow");
    }

    CelValue value = frame->value_stack().Peek();

    if (!leave_on_stack_) {
      frame->value_stack().Pop(1);
    }

    if (value.IsBool() && jump_condition_ == value.BoolOrDie()) {
      return Jump(frame);
    }

    return util::OkStatus();
  }

 private:
  const bool jump_condition_;
  const bool leave_on_stack_;
  int jump_offset_;
};

class ErrorJumpStep : public JumpStepBase {
 public:
  // Constructs FunctionStep that uses overloads specified.
  ErrorJumpStep(absl::optional<int> jump_offset,
                const google::api::expr::v1alpha1::Expr* expr)
      : JumpStepBase(jump_offset, expr) {}

  util::Status Evaluate(ExecutionFrame* frame) const override {
    // Peek the top value
    if (!frame->value_stack().HasEnough(1)) {
      return util::MakeStatus(google::rpc::Code::INTERNAL, "Value stack underflow");
    }

    CelValue value = frame->value_stack().Peek();

    if (value.IsError()) {
      return Jump(frame);
    }

    return util::OkStatus();
  }
};

}  // namespace

// Factory method for Conditional Jump step.
// Conditional Jump requires a boolean value to sit on the stack.
// It is compared to jump_condition, and if matched, jump is performed.
util::StatusOr<std::unique_ptr<JumpStepBase>> CreateCondJumpStep(
    bool jump_condition, bool leave_on_stack, absl::optional<int> jump_offset,
    const google::api::expr::v1alpha1::Expr* expr) {
  std::unique_ptr<JumpStepBase> step = absl::make_unique<CondJumpStep>(
      jump_condition, leave_on_stack, jump_offset, expr);

  return std::move(step);
}

// Factory method for Jump step.
util::StatusOr<std::unique_ptr<JumpStepBase>> CreateJumpStep(
    absl::optional<int> jump_offset, const google::api::expr::v1alpha1::Expr* expr) {
  std::unique_ptr<JumpStepBase> step =
      absl::make_unique<JumpStep>(jump_offset, expr);

  return std::move(step);
}

// Factory method for Conditional Jump step.
// Conditional Jump requires a value to sit on the stack.
// If this value is an error, a jump is performed.
util::StatusOr<std::unique_ptr<JumpStepBase>> CreateErrorJumpStep(
    absl::optional<int> jump_offset, const google::api::expr::v1alpha1::Expr* expr) {
  std::unique_ptr<JumpStepBase> step =
      absl::make_unique<ErrorJumpStep>(jump_offset, expr);

  return std::move(step);
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
