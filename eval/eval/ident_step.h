#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_IDENT_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_IDENT_STEP_H_

#include <cstdint>
#include <memory>

#include "absl/status/statusor.h"
#include "base/ast_internal/expr.h"
#include "eval/eval/evaluator_core.h"

namespace google::api::expr::runtime {

// Factory method for Ident - based Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateIdentStep(
    const cel::ast_internal::Ident& ident, int64_t expr_id);

// Factory method for identifier that has been assigned to a slot.
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateIdentStepForSlot(
    const cel::ast_internal::Ident& ident_expr, size_t slot_index,
    int64_t expr_id);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_IDENT_STEP_H_
