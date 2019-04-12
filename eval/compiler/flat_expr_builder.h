#ifndef THIRD_PARTY_CEL_CPP_EVAL_COMPILER_FLAT_EXPR_BUILDER_H_
#define THIRD_PARTY_CEL_CPP_EVAL_COMPILER_FLAT_EXPR_BUILDER_H_

#include "eval/public/cel_expression.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// CelExpressionBuilder implementation.
// Builds instances of CelExpressionFlatImpl.
class FlatExprBuilder : public CelExpressionBuilder {
 public:
  FlatExprBuilder() : shortcircuiting_(true) {}

  // set_shortcircuiting regulates shortcircuiting of some expressions.
  // Be default shortcircuiting is enabled.
  void set_shortcircuiting(bool enabled) { shortcircuiting_ = enabled; }

  util::StatusOr<std::unique_ptr<CelExpression>> CreateExpression(
      const google::api::expr::v1alpha1::Expr* expr,
      const google::api::expr::v1alpha1::SourceInfo* source_info) const override;

 private:
  bool shortcircuiting_;
};

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_COMPILER_FLAT_EXPR_BUILDER_H_
