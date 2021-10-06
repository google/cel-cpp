#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_EXPRESSION_STEP_BASE_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_EXPRESSION_STEP_BASE_H_

#include <cstdint>

#include "eval/eval/evaluator_core.h"

namespace google::api::expr::runtime {

class ExpressionStepBase : public ExpressionStep {
 public:
  explicit ExpressionStepBase(int64_t expr_id, bool comes_from_ast = true)
      : id_(expr_id), comes_from_ast_(comes_from_ast) {}

  // Non-copyable
  ExpressionStepBase(const ExpressionStepBase&) = delete;
  ExpressionStepBase& operator=(const ExpressionStepBase&) = delete;

  // Returns corresponding expression object ID.
  int64_t id() const override { return id_; }

  // Returns if the execution step comes from AST.
  bool ComesFromAst() const override { return comes_from_ast_; }

 private:
  int64_t id_;
  bool comes_from_ast_;
};

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_EXPRESSION_STEP_BASE_H_
