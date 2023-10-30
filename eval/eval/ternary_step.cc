#include "eval/eval/ternary_step.h"

#include <cstdint>
#include <memory>
#include <utility>

#include "absl/status/statusor.h"
#include "base/builtins.h"
#include "base/handle.h"
#include "base/value.h"
#include "base/values/bool_value.h"
#include "base/values/error_value.h"
#include "base/values/unknown_value.h"
#include "eval/eval/expression_step_base.h"
#include "eval/internal/errors.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::builtin::kTernary;
using ::cel::runtime_internal::CreateNoMatchingOverloadError;

inline constexpr size_t kTernaryStepCondition = 0;
inline constexpr size_t kTernaryStepTrue = 1;
inline constexpr size_t kTernaryStepFalse = 2;

class TernaryStep : public ExpressionStepBase {
 public:
  // Constructs FunctionStep that uses overloads specified.
  explicit TernaryStep(int64_t expr_id) : ExpressionStepBase(expr_id) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;
};

absl::Status TernaryStep::Evaluate(ExecutionFrame* frame) const {
  // Must have 3 or more values on the stack.
  if (!frame->value_stack().HasEnough(3)) {
    return absl::Status(absl::StatusCode::kInternal, "Value stack underflow");
  }

  // Create Span object that contains input arguments to the function.
  auto args = frame->value_stack().GetSpan(3);

  const auto& condition = args[kTernaryStepCondition];
  // As opposed to regular functions, ternary treats unknowns or errors on the
  // condition (arg0) as blocking. If we get an error or unknown then we
  // ignore the other arguments and forward the condition as the result.
  if (frame->enable_unknowns()) {
    // Check if unknown?
    if (condition->Is<cel::UnknownValue>()) {
      frame->value_stack().Pop(2);
      return absl::OkStatus();
    }
  }

  if (condition->Is<cel::ErrorValue>()) {
    frame->value_stack().Pop(2);
    return absl::OkStatus();
  }

  cel::Handle<cel::Value> result;
  if (!condition->Is<cel::BoolValue>()) {
    result = frame->value_factory().CreateErrorValue(
        CreateNoMatchingOverloadError(kTernary));
  } else if (condition.As<cel::BoolValue>()->NativeValue()) {
    result = args[kTernaryStepTrue];
  } else {
    result = args[kTernaryStepFalse];
  }

  frame->value_stack().Pop(args.size());
  frame->value_stack().Push(std::move(result));

  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateTernaryStep(
    int64_t expr_id) {
  return std::make_unique<TernaryStep>(expr_id);
}

}  // namespace google::api::expr::runtime
