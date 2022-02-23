#include "eval/eval/ternary_step.h"

#include <cstdint>

#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "eval/eval/expression_step_base.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_value.h"
#include "eval/public/unknown_attribute_set.h"

namespace google::api::expr::runtime {

namespace {

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

  CelValue value;

  const CelValue& condition = args.at(0);
  // As opposed to regular functions, ternary treats unknowns or errors on the
  // condition (arg0) as blocking. If we get an error or unknown then we
  // ignore the other arguments and forward the condition as the result.
  if (frame->enable_unknowns()) {
    // Check if unknown?
    if (condition.IsUnknownSet()) {
      frame->value_stack().Pop(2);
      return absl::OkStatus();
    }
  }

  if (condition.IsError()) {
    frame->value_stack().Pop(2);
    return absl::OkStatus();
  }

  CelValue result;
  if (!condition.IsBool()) {
    result = CreateNoMatchingOverloadError(frame->memory_manager(),
                                           builtin::kTernary);
  } else if (condition.BoolOrDie()) {
    result = args.at(1);
  } else {
    result = args.at(2);
  }

  frame->value_stack().Pop(args.size());
  frame->value_stack().Push(result);

  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateTernaryStep(
    int64_t expr_id) {
  return absl::make_unique<TernaryStep>(expr_id);
}

}  // namespace google::api::expr::runtime
