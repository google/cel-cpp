#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_COMPREHENSION_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_COMPREHENSION_STEP_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>

#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"

namespace google::api::expr::runtime {

class ComprehensionNextStep : public ExpressionStepBase {
 public:
  ComprehensionNextStep(size_t slot_offset, int64_t expr_id);

  void set_jump_offset(int offset);
  void set_error_jump_offset(int offset);

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  size_t iter_slot_;
  size_t accu_slot_;
  int jump_offset_;
  int error_jump_offset_;
};

class ComprehensionCondStep : public ExpressionStepBase {
 public:
  ComprehensionCondStep(size_t slot_offset, bool shortcircuiting,
                        int64_t expr_id);

  void set_jump_offset(int offset);
  void set_error_jump_offset(int offset);

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  size_t iter_slot_;
  size_t accu_slot_;
  int jump_offset_;
  int error_jump_offset_;
  bool shortcircuiting_;
};

class ComprehensionFinish : public ExpressionStepBase {
 public:
  ComprehensionFinish(size_t slot_offset, int64_t expr_id);

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  size_t accu_slot_;
};

// Creates a step that lists the map keys if the top of the stack is a map,
// otherwise it's a no-op.
std::unique_ptr<ExpressionStep> CreateListKeysStep(int64_t expr_id);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_COMPREHENSION_STEP_H_
