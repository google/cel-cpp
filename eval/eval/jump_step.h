#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_JUMP_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_JUMP_STEP_H_

#include <cstdint>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"

namespace google::api::expr::runtime {

class JumpStepBase : public ExpressionStepBase {
 public:
  JumpStepBase(absl::optional<int> jump_offset, int64_t expr_id)
      : ExpressionStepBase(expr_id, false), jump_offset_(jump_offset) {}

  void set_jump_offset(int offset) { jump_offset_ = offset; }

  absl::Status Jump(ExecutionFrame* frame) const {
    if (!jump_offset_.has_value()) {
      return absl::Status(absl::StatusCode::kInternal, "Jump offset not set");
    }
    return frame->JumpTo(jump_offset_.value());
  }

 private:
  absl::optional<int> jump_offset_;
};

// Factory method for Jump step.
absl::StatusOr<std::unique_ptr<JumpStepBase>> CreateJumpStep(
    absl::optional<int> jump_offset, int64_t expr_id);

// Factory method for Conditional Jump step.
// Conditional Jump requires a boolean value to sit on the stack.
// It is compared to jump_condition, and if matched, jump is performed.
// leave on stack indicates whether value should be kept on top of the stack or
// removed.
absl::StatusOr<std::unique_ptr<JumpStepBase>> CreateCondJumpStep(
    bool jump_condition, bool leave_on_stack, absl::optional<int> jump_offset,
    int64_t expr_id);

// Factory method for ErrorJump step.
// This step performs a Jump when an Error is on the top of the stack.
// Value is left on stack if it is a bool or an error.
absl::StatusOr<std::unique_ptr<JumpStepBase>> CreateBoolCheckJumpStep(
    absl::optional<int> jump_offset, int64_t expr_id);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_JUMP_STEP_H_
