#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_CONTAINER_ACCESS_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_CONTAINER_ACCESS_STEP_H_

#include <cstdint>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/statusor.h"
#include "eval/eval/evaluator_core.h"

namespace google::api::expr::runtime {

// Factory method for Select - based Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateContainerAccessStep(
    const cel::ast::internal::Call& call, int64_t expr_id);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_CONTAINER_ACCESS_STEP_H_
