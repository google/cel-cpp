#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_FUNCTION_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_FUNCTION_STEP_H_

#include <cstdint>
#include <memory>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/statusor.h"
#include "eval/eval/evaluator_core.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_function_provider.h"

namespace google::api::expr::runtime {

// Factory method for Call-based execution step where the function will be
// resolved at runtime (lazily) from an input Activation.
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateFunctionStep(
    const cel::ast::internal::Call& call, int64_t expr_id,
    std::vector<const CelFunctionProvider*>& lazy_overloads);

// Factory method for Call-based execution step where the function has been
// statically resolved from a set of eagerly functions configured in the
// CelFunctionRegistry.
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateFunctionStep(
    const cel::ast::internal::Call& call, int64_t expr_id,
    std::vector<const CelFunction*>& overloads);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_FUNCTION_STEP_H_
