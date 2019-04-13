#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_FUNCTION_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_FUNCTION_STEP_H_

#include "eval/eval/evaluator_core.h"
#include "eval/public/activation.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Factory method for Call - based Execution step
// Looks up function registry using data provided through Call parameter.
util::StatusOr<std::unique_ptr<ExpressionStep>> CreateFunctionStep(
    const google::api::expr::v1alpha1::Expr::Call* call,
    const google::api::expr::v1alpha1::Expr* expr,
    const CelFunctionRegistry& function_registry);

// Factory method for Call - based Execution step
// Creates execution step that wraps around the subset of overloads.
util::StatusOr<std::unique_ptr<ExpressionStep>> CreateFunctionStep(
    const google::api::expr::v1alpha1::Expr* expr,
    const std::vector<const CelFunction*> overloads);

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_FUNCTION_STEP_H_
