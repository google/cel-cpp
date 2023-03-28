#include "eval/eval/shadowable_value_step.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "eval/eval/expression_step_base.h"

namespace google::api::expr::runtime {

namespace {

class ShadowableValueStep : public ExpressionStepBase {
 public:
  ShadowableValueStep(std::string identifier, cel::Handle<cel::Value> value,
                      int64_t expr_id)
      : ExpressionStepBase(expr_id),
        identifier_(std::move(identifier)),
        value_(std::move(value)) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  std::string identifier_;
  cel::Handle<cel::Value> value_;
};

absl::Status ShadowableValueStep::Evaluate(ExecutionFrame* frame) const {
  CEL_ASSIGN_OR_RETURN(auto var, frame->modern_activation().FindVariable(
                                     frame->value_factory(), identifier_));
  if (var.has_value()) {
    frame->value_stack().Push(std::move(var).value());
  } else {
    frame->value_stack().Push(value_);
  }
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateShadowableValueStep(
    std::string identifier, cel::Handle<cel::Value> value, int64_t expr_id) {
  return absl::make_unique<ShadowableValueStep>(std::move(identifier),
                                                std::move(value), expr_id);
}

}  // namespace google::api::expr::runtime
