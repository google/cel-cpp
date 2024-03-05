#include "eval/eval/select_step.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/kind.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/internal/errors.h"
#include "internal/status_macros.h"
#include "runtime/runtime_options.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::BoolValueView;
using ::cel::ErrorValue;
using ::cel::MapValue;
using ::cel::NullValue;
using ::cel::ProtoWrapperTypeOptions;
using ::cel::StringValue;
using ::cel::StructValue;
using ::cel::UnknownValue;
using ::cel::Value;
using ::cel::ValueKind;
using ::cel::ValueView;
using ::cel::runtime_internal::CreateMissingAttributeError;
using ::cel::runtime_internal::CreateNoSuchKeyError;

// Common error for cases where evaluation attempts to perform select operations
// on an unsupported type.
//
// This should not happen under normal usage of the evaluator, but useful for
// troubleshooting broken invariants.
absl::Status InvalidSelectTargetError() {
  return absl::Status(absl::StatusCode::kInvalidArgument,
                      "Applying SELECT to non-message type");
}

absl::optional<Value> CheckForMarkedAttributes(const AttributeTrail& trail,
                                               ExecutionFrame* frame) {
  if (frame->enable_unknowns() &&
      frame->attribute_utility().CheckForUnknown(trail,
                                                 /*use_partial=*/false)) {
    return frame->attribute_utility().CreateUnknownSet(trail.attribute());
  }

  if (frame->enable_missing_attribute_errors() &&
      frame->attribute_utility().CheckForMissingAttribute(trail)) {
    auto attribute_string = trail.attribute().AsString();
    if (attribute_string.ok()) {
      return frame->value_factory().CreateErrorValue(
          CreateMissingAttributeError(*attribute_string));
    }
    // Invariant broken (an invalid CEL Attribute shouldn't match anything).
    // Log and return a CelError.
    ABSL_LOG(ERROR)
        << "Invalid attribute pattern matched select path: "
        << attribute_string.status().ToString();  // NOLINT: OSS compatibility
    return frame->value_factory().CreateErrorValue(
        std::move(attribute_string).status());
  }

  return absl::nullopt;
}

ValueView TestOnlySelect(const StructValue& msg, const std::string& field,
                         cel::ValueManager& value_factory, Value& scratch) {
  absl::StatusOr<bool> result = msg.HasFieldByName(field);

  if (!result.ok()) {
    scratch = value_factory.CreateErrorValue(std::move(result).status());
    return scratch;
  }
  return BoolValueView(*result);
}

ValueView TestOnlySelect(const MapValue& map, const StringValue& field_name,
                         cel::ValueManager& value_factory, Value& scratch) {
  // Field presence only supports string keys containing valid identifier
  // characters.
  auto presence = map.Has(value_factory, field_name, scratch);

  if (!presence.ok()) {
    scratch = value_factory.CreateErrorValue(std::move(presence).status());
    return scratch;
  }

  return *presence;
}

}  // namespace

// SelectStep performs message field access specified by Expr::Select
// message.
class SelectStep : public ExpressionStepBase {
 public:
  SelectStep(StringValue value, bool test_field_presence, int64_t expr_id,
             bool enable_wrapper_type_null_unboxing)
      : ExpressionStepBase(expr_id),
        field_value_(std::move(value)),
        field_(field_value_.ToString()),
        test_field_presence_(test_field_presence),
        unboxing_option_(enable_wrapper_type_null_unboxing
                             ? ProtoWrapperTypeOptions::kUnsetNull
                             : ProtoWrapperTypeOptions::kUnsetProtoDefault) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  cel::StringValue field_value_;
  std::string field_;
  bool test_field_presence_;
  ProtoWrapperTypeOptions unboxing_option_;
};

absl::Status SelectStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(1)) {
    return absl::Status(absl::StatusCode::kInternal,
                        "No arguments supplied for Select-type expression");
  }

  const Value& arg = frame->value_stack().Peek();
  const AttributeTrail& trail = frame->value_stack().PeekAttribute();

  if (arg->Is<UnknownValue>() || arg->Is<ErrorValue>()) {
    // Bubble up unknowns and errors.
    return absl::OkStatus();
  }

  AttributeTrail result_trail;

  // Handle unknown resolution.
  if (frame->enable_unknowns() || frame->enable_missing_attribute_errors()) {
    result_trail = trail.Step(&field_);
  }

  if (arg->Is<NullValue>()) {
    frame->value_stack().PopAndPush(
        frame->value_factory().CreateErrorValue(
            cel::runtime_internal::CreateError("Message is NULL")),
        std::move(result_trail));
    return absl::OkStatus();
  }

  if (!(arg->Is<MapValue>() || arg->Is<StructValue>())) {
    frame->value_stack().PopAndPush(
        frame->value_factory().CreateErrorValue(InvalidSelectTargetError()),
        std::move(result_trail));
    return absl::OkStatus();
  }

  absl::optional<Value> marked_attribute_check =
      CheckForMarkedAttributes(result_trail, frame);
  if (marked_attribute_check.has_value()) {
    frame->value_stack().PopAndPush(std::move(marked_attribute_check).value(),
                                    std::move(result_trail));
    return absl::OkStatus();
  }

  Value result_scratch;

  // Handle test only Select.
  if (test_field_presence_) {
    switch (arg->kind()) {
      case ValueKind::kMap:
        frame->value_stack().PopAndPush(
            Value{TestOnlySelect(arg.As<MapValue>(), field_value_,
                                 frame->value_factory(), result_scratch)});
        return absl::OkStatus();
      case ValueKind::kMessage:
        frame->value_stack().PopAndPush(
            Value{TestOnlySelect(arg.As<StructValue>(), field_,
                                 frame->value_factory(), result_scratch)});
        return absl::OkStatus();
      default:
        // Control flow should have returned earlier.
        return InvalidSelectTargetError();
    }
  }

  // Normal select path.
  // Select steps can be applied to either maps or messages
  switch (arg->kind()) {
    case ValueKind::kStruct: {
      CEL_ASSIGN_OR_RETURN(auto result, arg.As<StructValue>().GetFieldByName(
                                            frame->value_factory(), field_,
                                            result_scratch, unboxing_option_));
      frame->value_stack().PopAndPush(Value{result}, std::move(result_trail));
      return absl::OkStatus();
    }
    case ValueKind::kMap: {
      CEL_ASSIGN_OR_RETURN(
          auto result, arg.As<MapValue>().Get(frame->value_factory(),
                                              field_value_, result_scratch));
      frame->value_stack().PopAndPush(Value{result}, std::move(result_trail));
      return absl::OkStatus();
    }
    default:
      // Control flow should have returned earlier.
      return InvalidSelectTargetError();
  }
}

// Factory method for Select - based Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateSelectStep(
    const cel::ast_internal::Select& select_expr, int64_t expr_id,
    bool enable_wrapper_type_null_unboxing, cel::ValueManager& value_factory) {
  return std::make_unique<SelectStep>(
      value_factory.CreateUncheckedStringValue(select_expr.field()),
      select_expr.test_only(), expr_id, enable_wrapper_type_null_unboxing);
}

}  // namespace google::api::expr::runtime
