#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_CREATE_STRUCT_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_CREATE_STRUCT_STEP_H_

#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Factory method for CreateList - based Execution step
cel_base::StatusOr<std::unique_ptr<ExpressionStep>> CreateCreateStructStep(
    const google::api::expr::v1alpha1::Expr::CreateStruct* create_struct_expr,
    int64_t expr_id);

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_CREATE_STRUCT_STEP_H_
