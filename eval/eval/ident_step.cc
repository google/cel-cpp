#include "eval/eval/ident_step.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/comprehension_slots.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/internal/errors.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::Handle;
using ::cel::Value;
using ::cel::runtime_internal::CreateError;
using ::cel::runtime_internal::CreateMissingAttributeError;

class IdentStep : public ExpressionStepBase {
 public:
  IdentStep(absl::string_view name, int64_t expr_id)
      : ExpressionStepBase(expr_id), name_(name) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  struct IdentResult {
    Handle<Value> value;
    AttributeTrail trail;
  };

  absl::StatusOr<IdentResult> DoEvaluate(ExecutionFrame* frame) const;

  std::string name_;
};

absl::StatusOr<IdentStep::IdentResult> IdentStep::DoEvaluate(
    ExecutionFrame* frame) const {
  IdentResult result;
  // Populate trails if either MissingAttributeError or UnknownPattern
  // is enabled.
  if (frame->enable_missing_attribute_errors() || frame->enable_unknowns()) {
    result.trail = AttributeTrail(name_);
  }

  if (frame->enable_missing_attribute_errors() && !name_.empty() &&
      frame->attribute_utility().CheckForMissingAttribute(result.trail)) {
    result.value = frame->value_factory().CreateErrorValue(
        CreateMissingAttributeError(name_));
    return result;
  }

  if (frame->enable_unknowns()) {
    if (frame->attribute_utility().CheckForUnknown(result.trail, false)) {
      auto unknown_set =
          frame->attribute_utility().CreateUnknownSet(result.trail.attribute());
      result.value = std::move(unknown_set);
      return result;
    }
  }

  CEL_ASSIGN_OR_RETURN(auto value, frame->modern_activation().FindVariable(
                                       frame->value_factory(), name_));

  if (value.has_value()) {
    result.value = std::move(value).value();
    return result;
  }

  result.value = frame->value_factory().CreateErrorValue(CreateError(
      absl::StrCat("No value with name \"", name_, "\" found in Activation")));

  return result;
}

absl::Status IdentStep::Evaluate(ExecutionFrame* frame) const {
  CEL_ASSIGN_OR_RETURN(IdentResult result, DoEvaluate(frame));

  frame->value_stack().Push(std::move(result.value), std::move(result.trail));

  return absl::OkStatus();
}

class SlotStep : public ExpressionStepBase {
 public:
  SlotStep(absl::string_view name, size_t slot_index, int64_t expr_id)
      : ExpressionStepBase(expr_id), name_(name), slot_index_(slot_index) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  std::string name_;

  size_t slot_index_;
};

absl::Status SlotStep::Evaluate(ExecutionFrame* frame) const {
  const ComprehensionSlots::Slot* slot =
      frame->comprehension_slots().Get(slot_index_);
  if (slot == nullptr) {
    return absl::InternalError(
        absl::StrCat("Comprehension variable accessed out of scope: ", name_));
  }

  const auto& attribute_trail = slot->attribute;

  if (frame->enable_missing_attribute_errors() &&
      frame->attribute_utility().CheckForMissingAttribute(attribute_trail)) {
    CEL_ASSIGN_OR_RETURN(std::string attribute,
                         attribute_trail.attribute().AsString());
    frame->value_stack().Push(frame->value_factory().CreateErrorValue(
        CreateMissingAttributeError(std::move(attribute))));
    return absl::OkStatus();
  }

  if (frame->enable_unknowns()) {
    if (frame->attribute_utility().CheckForUnknown(attribute_trail, false)) {
      auto unknown_set = frame->attribute_utility().CreateUnknownSet(
          attribute_trail.attribute());
      frame->value_stack().Push(std::move(unknown_set));
      return absl::OkStatus();
    }
  }

  frame->value_stack().Push(slot->value, attribute_trail);
  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateIdentStep(
    const cel::ast_internal::Ident& ident_expr, int64_t expr_id) {
  return std::make_unique<IdentStep>(ident_expr.name(), expr_id);
}

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateIdentStepForSlot(
    const cel::ast_internal::Ident& ident_expr, size_t slot_index,
    int64_t expr_id) {
  return std::make_unique<SlotStep>(ident_expr.name(), slot_index, expr_id);
}

}  // namespace google::api::expr::runtime
