#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_CREATE_STRUCT_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_CREATE_STRUCT_STEP_H_

#include <cstdint>
#include <memory>

#include "absl/status/statusor.h"
#include "base/ast_internal/expr.h"
#include "eval/eval/evaluator_core.h"
#include "eval/public/structs/legacy_type_adapter.h"

namespace google::api::expr::runtime {

// Factory method for CreateStruct - based Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateCreateStructStep(
    const cel::ast_internal::CreateStruct& create_struct_expr,
    const LegacyTypeMutationApis* type_adapter, int64_t expr_id);

inline absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateCreateStructStep(
    const cel::ast_internal::CreateStruct& create_struct_expr,
    int64_t expr_id) {
  return CreateCreateStructStep(create_struct_expr,
                                /*type_adapter=*/nullptr, expr_id);
}

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_CREATE_STRUCT_STEP_H_
