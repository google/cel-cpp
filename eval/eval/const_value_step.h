#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_CONST_VALUE_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_CONST_VALUE_STEP_H_

#include "eval/eval/evaluator_core.h"
#include "eval/public/activation.h"
#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Factory method for Constant - based Execution step
util::StatusOr<std::unique_ptr<ExpressionStep>> CreateConstValueStep(
    const google::api::expr::v1alpha1::Constant* const_expr,
    const google::api::expr::v1alpha1::Expr* expr, bool comes_from_ast = true);

// Factory method for Constant(Enum value) - based Execution step
util::StatusOr<std::unique_ptr<ExpressionStep>> CreateConstValueStep(
    const google::protobuf::EnumValueDescriptor* value_descriptor,
    const google::api::expr::v1alpha1::Expr* expr);

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_CONST_VALUE_STEP_H_
