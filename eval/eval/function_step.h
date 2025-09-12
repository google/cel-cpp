#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_FUNCTION_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_FUNCTION_STEP_H_

#include <cstdint>
#include <memory>
#include <vector>

#include "absl/status/statusor.h"
#include "common/expr.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "runtime/function_overload_reference.h"
#include "runtime/function_registry.h"

namespace google::api::expr::runtime {

// Factory method for Call-based execution step where the function has been
// statically resolved from a set of eagerly functions configured in the
// CelFunctionRegistry.
std::unique_ptr<DirectExpressionStep> CreateDirectFunctionStep(
    int64_t expr_id, const cel::CallExpr& call,
    std::vector<std::unique_ptr<DirectExpressionStep>> deps,
    std::vector<cel::FunctionOverloadReference> overloads,
    std::vector<std::string> overload_id = {});

// Factory method for Call-based execution step where the function has been
// statically resolved from a set of lazy functions configured in the
// CelFunctionRegistry.
std::unique_ptr<DirectExpressionStep> CreateDirectLazyFunctionStep(
    int64_t expr_id, const cel::CallExpr& call,
    std::vector<std::unique_ptr<DirectExpressionStep>> deps,
    std::vector<cel::FunctionRegistry::LazyOverload> providers,
    std::vector<std::string> overload_id = {});

// Factory method for Call-based execution step where the function will be
// resolved at runtime (lazily) from an input Activation.
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateFunctionStep(
    const cel::CallExpr& call, int64_t expr_id,
    std::vector<cel::FunctionRegistry::LazyOverload> lazy_overloads,
    std::vector<std::string> overload_id = {});

// Factory method for Call-based execution step where the function has been
// statically resolved from a set of eagerly functions configured in the
// CelFunctionRegistry.
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateFunctionStep(
    const cel::CallExpr& call, int64_t expr_id,
    std::vector<cel::FunctionOverloadReference> overloads,
    std::vector<std::string> overload_id = {});

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_FUNCTION_STEP_H_
