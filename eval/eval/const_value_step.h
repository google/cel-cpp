#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_CONST_VALUE_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_CONST_VALUE_STEP_H_

#include <cstdint>

#include "absl/status/statusor.h"
#include "base/ast.h"
#include "eval/eval/evaluator_core.h"
#include "eval/public/cel_value.h"

namespace google::api::expr::runtime {

absl::optional<CelValue> ConvertConstant(
    const cel::ast::internal::Constant& const_expr);

// Factory method for Constant - based Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateConstValueStep(
    CelValue value, int64_t expr_id, bool comes_from_ast = true);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_CONST_VALUE_STEP_H_
