#include "eval/eval/ident_step.h"

#include "google/protobuf/arena.h"
#include "absl/status/statusor.h"
#include "absl/strings/substitute.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/public/unknown_attribute_set.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {
class IdentStep : public ExpressionStepBase {
 public:
  IdentStep(absl::string_view name, int64_t expr_id)
      : ExpressionStepBase(expr_id), name_(name) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  void DoEvaluate(ExecutionFrame* frame, CelValue* result,
                  AttributeTrail* trail) const;

  std::string name_;
};

void IdentStep::DoEvaluate(ExecutionFrame* frame, CelValue* result,
                           AttributeTrail* trail) const {
  // Special case - iterator looked up in
  if (frame->GetIterVar(name_, result)) {
    const AttributeTrail* iter_trail;
    if (frame->GetIterAttr(name_, &iter_trail)) {
      *trail = *iter_trail;
    }
    return;
  }

  auto value = frame->activation().FindValue(name_, frame->arena());

  // Populate trails if either MissingAttributeError or UnknownPattern
  // is enabled.
  if (frame->enable_missing_attribute_errors() || frame->enable_unknowns()) {
    google::api::expr::v1alpha1::Expr expr;
    expr.mutable_ident_expr()->set_name(name_);
    *trail = AttributeTrail(expr, frame->arena());
  }

  if (frame->enable_missing_attribute_errors() && !name_.empty() &&
      frame->attribute_utility().CheckForMissingAttribute(*trail)) {
    *result = CreateMissingAttributeError(frame->arena(), name_);
    return;
  }

  {
    // We handle masked unknown paths for the sake of uniformity, although it is
    // better not to bind unknown values to activation in first place.
    // TODO(issues/41) Deprecate this style of unknowns handling after
    // Unknowns are properly supported.
    bool unknown_value = frame->activation().IsPathUnknown(name_);

    if (unknown_value) {
      *result = CreateUnknownValueError(frame->arena(), name_);
      return;
    }
  }

  if (frame->enable_unknowns()) {
    if (frame->attribute_utility().CheckForUnknown(*trail, false)) {
      auto unknown_set = google::protobuf::Arena::Create<UnknownSet>(
          frame->arena(), UnknownAttributeSet({trail->attribute()}));
      *result = CelValue::CreateUnknownSet(unknown_set);
      return;
    }
  }

  if (value.has_value()) {
    *result = value.value();
  } else {
    *result = CreateErrorValue(
        frame->arena(),
        absl::Substitute("No value with name \"$0\" found in Activation",
                         name_));
  }
}

absl::Status IdentStep::Evaluate(ExecutionFrame* frame) const {
  CelValue result;
  AttributeTrail trail;

  DoEvaluate(frame, &result, &trail);

  frame->value_stack().Push(result, trail);

  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateIdentStep(
    const google::api::expr::v1alpha1::Expr::Ident* ident_expr, int64_t expr_id) {
  std::unique_ptr<ExpressionStep> step =
      absl::make_unique<IdentStep>(ident_expr->name(), expr_id);
  return std::move(step);
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
