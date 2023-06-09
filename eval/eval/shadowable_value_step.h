#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_SHADOWABLE_VALUE_STEP_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_SHADOWABLE_VALUE_STEP_H_

#include <cstdint>
#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "base/handle.h"
#include "base/value.h"
#include "eval/eval/evaluator_core.h"

namespace google::api::expr::runtime {

// Create an identifier resolution step with a default value that may be
// shadowed by an identifier of the same name within the runtime-provided
// Activation.
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateShadowableValueStep(
    std::string identifier, cel::Handle<cel::Value> value, int64_t expr_id);

}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_SHADOWABLE_VALUE_STEP_H_
