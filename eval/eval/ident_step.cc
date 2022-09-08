#include "eval/eval/ident_step.h"

#include <cstdint>
#include <string>
#include <utility>

#include "google/protobuf/arena.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/public/unknown_attribute_set.h"
#include "extensions/protobuf/memory_manager.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::extensions::ProtoMemoryManager;

class IdentStep : public ExpressionStepBase {
 public:
  IdentStep(absl::string_view name, int64_t expr_id)
      : ExpressionStepBase(expr_id), name_(name) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  absl::Status DoEvaluate(ExecutionFrame* frame, CelValue* result,
                          AttributeTrail* trail) const;

  std::string name_;
};

absl::Status IdentStep::DoEvaluate(ExecutionFrame* frame, CelValue* result,
                                   AttributeTrail* trail) const {
  // Special case - iterator looked up in
  if (frame->GetIterVar(name_, result)) {
    const AttributeTrail* iter_trail;
    if (frame->GetIterAttr(name_, &iter_trail)) {
      *trail = *iter_trail;
    }
    return absl::OkStatus();
  }

  // TODO(issues/5): Update ValueProducer to support generic memory manager
  // API.
  google::protobuf::Arena* arena =
      ProtoMemoryManager::CastToProtoArena(frame->memory_manager());

  auto value = frame->activation().FindValue(name_, arena);

  // Populate trails if either MissingAttributeError or UnknownPattern
  // is enabled.
  if (frame->enable_missing_attribute_errors() || frame->enable_unknowns()) {
    *trail = AttributeTrail(name_);
  }

  if (frame->enable_missing_attribute_errors() && !name_.empty() &&
      frame->attribute_utility().CheckForMissingAttribute(*trail)) {
    *result = CreateMissingAttributeError(frame->memory_manager(), name_);
    return absl::OkStatus();
  }

  if (frame->enable_unknowns()) {
    if (frame->attribute_utility().CheckForUnknown(*trail, false)) {
      auto unknown_set =
          frame->attribute_utility().CreateUnknownSet(trail->attribute());
      *result = CelValue::CreateUnknownSet(unknown_set);
      return absl::OkStatus();
    }
  }

  if (value.has_value()) {
    *result = value.value();
  } else {
    *result = CreateErrorValue(
        frame->memory_manager(),
        absl::StrCat("No value with name \"", name_, "\" found in Activation"));
  }

  return absl::OkStatus();
}

absl::Status IdentStep::Evaluate(ExecutionFrame* frame) const {
  CelValue result;
  AttributeTrail trail;

  CEL_RETURN_IF_ERROR(DoEvaluate(frame, &result, &trail));

  frame->value_stack().Push(result, std::move(trail));

  return absl::OkStatus();
}

}  // namespace

absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateIdentStep(
    const cel::ast::internal::Ident& ident_expr, int64_t expr_id) {
  return absl::make_unique<IdentStep>(ident_expr.name(), expr_id);
}

}  // namespace google::api::expr::runtime
