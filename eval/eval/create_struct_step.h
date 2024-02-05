#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_CREATE_STRUCT_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_CREATE_STRUCT_STEP_H_

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/ast_internal/expr.h"
#include "common/type.h"
#include "eval/eval/evaluator_core.h"

namespace google::api::expr::runtime {

// Creates an `ExpressionStep` which performs `CreateStruct` for a
// message/struct.
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateCreateStructStepForStruct(
    const cel::ast_internal::CreateStruct& create_struct_expr, std::string name,
    int64_t expr_id, cel::TypeManager& type_manager);

// Creates an `ExpressionStep` which performs `CreateStruct` for a map.
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateCreateStructStepForMap(
    const cel::ast_internal::CreateStruct& create_struct_expr, int64_t expr_id);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_CREATE_STRUCT_STEP_H_
