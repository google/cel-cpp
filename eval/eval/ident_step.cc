#include "eval/eval/ident_step.h"
#include "eval/eval/expression_step_base.h"
#include "absl/strings/substitute.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {
class IdentStep : public ExpressionStepBase {
 public:
  IdentStep(absl::string_view name, const google::api::expr::v1alpha1::Expr* expr)
      : ExpressionStepBase(expr), name_(name) {}

  util::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  std::string name_;
};

util::Status IdentStep::Evaluate(ExecutionFrame* frame) const {
  CelValue result;
  auto it = frame->iter_vars().find(name_);
  if (it != frame->iter_vars().end()) {
    result = it->second;
  } else {
    auto value = frame->activation().FindValue(name_, frame->arena());

    // We handle masked unknown paths for the sake of uniformity, although it is
    // better not to bind unknown values to activation in first place.
    bool unknown_value = frame->activation().IsPathUnknown(name_);

    if (!unknown_value) {
      if (value.has_value()) {
        result = value.value();
      } else {
        result = CreateErrorValue(
            frame->arena(),
            absl::Substitute("No value with name \"$0\" found in Activation",
                             name_),
            CelError::UNKNOWN);
      }
    } else {
      result = CreateErrorValue(
          frame->arena(),
          absl::Substitute("Value with name \"$0\" is unknown", name_),
          CelError::UNKNOWN);
    }
  }

  frame->value_stack().Push(result);

  return util::OkStatus();
}

}  // namespace

util::StatusOr<std::unique_ptr<ExpressionStep>> CreateIdentStep(
    const google::api::expr::v1alpha1::Expr::Ident* ident_expr,
    const google::api::expr::v1alpha1::Expr* expr) {
  std::unique_ptr<ExpressionStep> step =
      absl::make_unique<IdentStep>(ident_expr->name(), expr);
  return std::move(step);
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
