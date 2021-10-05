#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_SHADOWABLE_VALUE_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_SHADOWABLE_VALUE_STEP_H_

#include <cstdint>
#include <memory>

#include "absl/status/statusor.h"
#include "eval/eval/evaluator_core.h"
#include "eval/public/cel_value.h"

namespace google::api::expr::runtime {

// Create an identifier resolution step with a default value that may be
// shadowed by an identifier of the same name within the runtime-provided
// Activation.
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateShadowableValueStep(
    const std::string& identifier, const CelValue& value, int64_t expr_id);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_SHADOWABLE_VALUE_STEP_H_
