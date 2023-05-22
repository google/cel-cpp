#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_CONST_VALUE_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_CONST_VALUE_STEP_H_

#include <cstdint>
#include <memory>

#include "absl/status/statusor.h"
#include "base/ast_internal.h"
#include "base/handle.h"
#include "base/value.h"
#include "eval/eval/evaluator_core.h"

namespace google::api::expr::runtime {

// TODO(uncreated-issue/29): move this somewhere else
cel::Handle<cel::Value> ConvertConstant(
    const cel::ast::internal::Constant& const_expr);

// Factory method for Constant Value expression step.
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateConstValueStep(
    cel::Handle<cel::Value> value, int64_t expr_id, bool comes_from_ast = true);

// Factory method for Constant AST node expression step.
// Copies the Constant Expr node to avoid lifecycle dependency on source
// expression.
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateConstValueStep(
    const cel::ast::internal::Constant&, int64_t expr_id,
    bool comes_from_ast = true);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_CONST_VALUE_STEP_H_
