#include "eval/eval/shadowable_value_step.h"

#include "absl/status/statusor.h"
#include "eval/eval/expression_step_base.h"
#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

class ShadowableValueStep : public ExpressionStepBase {
 public:
  ShadowableValueStep(const std::string& identifier, const CelValue& value,
                      int64_t expr_id)
      : ExpressionStepBase(expr_id), identifier_(identifier), value_(value) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  std::string identifier_;
  CelValue value_;
};

absl::Status ShadowableValueStep::Evaluate(ExecutionFrame* frame) const {
  auto var = frame->activation().FindValue(identifier_, frame->arena());
  frame->value_stack().Push(var.value_or(value_));
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateShadowableValueStep(
    const std::string& identifier, const CelValue& value, int64_t expr_id) {
  std::unique_ptr<ExpressionStep> step =
      absl::make_unique<ShadowableValueStep>(identifier, value, expr_id);
  return std::move(step);
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
