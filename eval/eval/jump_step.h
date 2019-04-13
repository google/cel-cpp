#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_JUMP_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_JUMP_STEP_H_

#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/public/activation.h"
#include "eval/public/cel_value.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

class JumpStepBase : public ExpressionStepBase {
 public:
  JumpStepBase(absl::optional<int> jump_offset,
               const google::api::expr::v1alpha1::Expr* expr)
      : ExpressionStepBase(expr), jump_offset_(jump_offset) {}

  void set_jump_offset(int offset) { jump_offset_ = offset; }

  util::Status Jump(ExecutionFrame* frame) const {
    if (!jump_offset_.has_value()) {
      return util::MakeStatus(google::rpc::Code::INTERNAL, "Jump offset not set");
    }
    return frame->JumpTo(jump_offset_.value());
  }

 private:
  absl::optional<int> jump_offset_;
};

// Factory method for Jump step.
util::StatusOr<std::unique_ptr<JumpStepBase>> CreateJumpStep(
    absl::optional<int> jump_offset, const google::api::expr::v1alpha1::Expr* expr);

// Factory method for Conditional Jump step.
// Conditional Jump requires a boolean value to sit on the stack.
// It is compared to jump_condition, and if matched, jump is performed.
// leave on stack indicates whether value should be kept on top of the stack or
// removed.
util::StatusOr<std::unique_ptr<JumpStepBase>> CreateCondJumpStep(
    bool jump_condition, bool leave_on_stack, absl::optional<int> jump_offset,
    const google::api::expr::v1alpha1::Expr* expr);

// Factory method for ErrorJump step.
// This step performs a Jump when an Error is on the top of the stack.
// Value is left on stack.
util::StatusOr<std::unique_ptr<JumpStepBase>> CreateErrorJumpStep(
    absl::optional<int> jump_offset, const google::api::expr::v1alpha1::Expr* expr);

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_JUMP_STEP_H_
