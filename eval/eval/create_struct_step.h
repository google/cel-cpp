#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_CREATE_STRUCT_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_CREATE_STRUCT_STEP_H_

#include <cstdint>
#include <memory>

#include "google/protobuf/descriptor.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "eval/eval/evaluator_core.h"

namespace google::api::expr::runtime {

// Factory method for CreateStruct - based Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateCreateStructStep(
    const google::api::expr::v1alpha1::Expr::CreateStruct* create_struct_expr,
    const google::protobuf::Descriptor* message_desc, int64_t expr_id);

inline absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateCreateStructStep(
    const google::api::expr::v1alpha1::Expr::CreateStruct* create_struct_expr,
    int64_t expr_id) {
  return CreateCreateStructStep(create_struct_expr, /*message_desc=*/nullptr,
                                expr_id);
}

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_CREATE_STRUCT_STEP_H_
