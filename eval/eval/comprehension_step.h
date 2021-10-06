#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_COMPREHENSION_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_COMPREHENSION_STEP_H_

#include <cstdint>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_value.h"

namespace google::api::expr::runtime {

class ComprehensionNextStep : public ExpressionStepBase {
 public:
  ComprehensionNextStep(const std::string& accu_var,
                        const std::string& iter_var, int64_t expr_id);

  void set_jump_offset(int offset);
  void set_error_jump_offset(int offset);

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  std::string accu_var_;
  std::string iter_var_;
  int jump_offset_;
  int error_jump_offset_;
};

class ComprehensionCondStep : public ExpressionStepBase {
 public:
  ComprehensionCondStep(const std::string& accu_var,
                        const std::string& iter_var, bool shortcircuiting,
                        int64_t expr_id);

  void set_jump_offset(int offset);
  void set_error_jump_offset(int offset);

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  std::string iter_var_;
  int jump_offset_;
  int error_jump_offset_;
  bool shortcircuiting_;
};

class ComprehensionFinish : public ExpressionStepBase {
 public:
  ComprehensionFinish(const std::string& accu_var, const std::string& iter_var,
                      int64_t expr_id);

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  std::string accu_var_;
};

// Creates a step that lists the map keys if the top of the stack is a map,
// otherwise it's a no-op.
std::unique_ptr<ExpressionStep> CreateListKeysStep(int64_t expr_id);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_COMPREHENSION_STEP_H_
