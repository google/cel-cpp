#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_TERNARY_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_TERNARY_STEP_H_

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/statusor.h"
#include "eval/eval/evaluator_core.h"
#include "eval/public/activation.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Factory method for ternary (_?_:_) execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateTernaryStep(
    int64_t expr_id);

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_TERNARY_STEP_H_
