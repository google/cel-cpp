#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_LOGIC_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_LOGIC_STEP_H_

#include "eval/eval/evaluator_core.h"
#include "eval/public/activation.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_value.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Factory method for "And" Execution step
util::StatusOr<std::unique_ptr<ExpressionStep>> CreateAndStep(
    const google::api::expr::v1alpha1::Expr* expr);

// Factory method for "Or" Execution step
util::StatusOr<std::unique_ptr<ExpressionStep>> CreateOrStep(
    const google::api::expr::v1alpha1::Expr* expr);

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_LOGIC_STEP_H_
