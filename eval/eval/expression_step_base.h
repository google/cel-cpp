#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_EXPRESSION_STEP_BASE_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_EXPRESSION_STEP_BASE_H_

#include "eval/eval/evaluator_core.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

class ExpressionStepBase : public ExpressionStep {
 public:
  explicit ExpressionStepBase(const google::api::expr::v1alpha1::Expr* expr,
                              bool comes_from_ast = true)
      : expr_(expr), comes_from_ast_(comes_from_ast) {}

  // Non-copyable
  ExpressionStepBase(const ExpressionStepBase&) = delete;
  ExpressionStepBase& operator=(const ExpressionStepBase&) = delete;

  // Returns corresponding expression object
  const google::api::expr::v1alpha1::Expr* expr() const override { return expr_; }

  // Returns if the execution step comes from AST.
  bool ComesFromAst() const override { return comes_from_ast_; }

 private:
  const google::api::expr::v1alpha1::Expr* expr_;
  bool comes_from_ast_;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_EXPRESSION_STEP_BASE_H_
