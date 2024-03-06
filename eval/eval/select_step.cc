#include "eval/eval/select_step.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "base/kind.h"
#include "common/casting.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/internal/errors.h"
#include "eval/public/ast_visitor.h"
#include "internal/status_macros.h"
#include "runtime/runtime_options.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::BoolValueView;
using ::cel::Cast;
using ::cel::ErrorValue;
using ::cel::InstanceOf;
using ::cel::MapValue;
using ::cel::NullValue;
using ::cel::ProtoWrapperTypeOptions;
using ::cel::StringValue;
using ::cel::StructValue;
using ::cel::UnknownValue;
using ::cel::Value;
using ::cel::ValueKind;
using ::cel::ValueView;

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
                                               ExecutionFrameBase& frame) {
  if (frame.unknown_processing_enabled() &&
      frame.attribute_utility().CheckForUnknownExact(trail)) {
    return frame.attribute_utility().CreateUnknownSet(trail.attribute());
  }

  if (frame.missing_attribute_errors_enabled() &&
      frame.attribute_utility().CheckForMissingAttribute(trail)) {
    auto result = frame.attribute_utility().CreateMissingAttributeError(
        trail.attribute());

    if (result.ok()) {
      return std::move(result).value();
    }
    // Invariant broken (an invalid CEL Attribute shouldn't match anything).
    // Log and return a CelError.
    ABSL_LOG(ERROR) << "Invalid attribute pattern matched select path: "
                    << result.status().ToString();  // NOLINT: OSS compatibility
    return frame.value_manager().CreateErrorValue(std::move(result).status());
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
  absl::StatusOr<ValueView> presence =
      map.Has(value_factory, field_name, scratch);

  if (!presence.ok()) {
    scratch = value_factory.CreateErrorValue(std::move(presence).status());
    return scratch;
  }

  return *presence;
}

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

  if (InstanceOf<UnknownValue>(arg) || InstanceOf<ErrorValue>(arg)) {
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
      CheckForMarkedAttributes(result_trail, *frame);
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

class DirectSelectStep : public DirectExpressionStep {
 public:
  DirectSelectStep(int64_t expr_id,
                   std::unique_ptr<DirectExpressionStep> operand,
                   StringValue field, bool test_only,
                   bool enable_wrapper_type_null_unboxing)
      : DirectExpressionStep(expr_id),
        operand_(std::move(operand)),
        field_value_(std::move(field)),
        field_(field_value_.ToString()),
        test_only_(test_only),
        unboxing_option_(enable_wrapper_type_null_unboxing
                             ? ProtoWrapperTypeOptions::kUnsetNull
                             : ProtoWrapperTypeOptions::kUnsetProtoDefault) {}

  absl::Status Evaluate(ExecutionFrameBase& frame, Value& result,
                        AttributeTrail& attribute) const override {
    CEL_RETURN_IF_ERROR(operand_->Evaluate(frame, result, attribute));

    if (InstanceOf<ErrorValue>(result) || InstanceOf<UnknownValue>(result)) {
      // Just forward.
      return absl::OkStatus();
    }

    if (frame.attribute_tracking_enabled()) {
      attribute = attribute.Step(&field_);
      absl::optional<Value> value = CheckForMarkedAttributes(attribute, frame);
      if (value.has_value()) {
        result = std::move(value).value();
        return absl::OkStatus();
      }
    }

    switch (result.kind()) {
      case ValueKind::kStruct:
      case ValueKind::kMap:
        break;
      case ValueKind::kNull:
        result = frame.value_manager().CreateErrorValue(
            cel::runtime_internal::CreateError("Message is NULL"));
        return absl::OkStatus();
      default:
        result =
            frame.value_manager().CreateErrorValue(InvalidSelectTargetError());
        return absl::OkStatus();
    }

    Value scratch;
    if (test_only_) {
      switch (result->kind()) {
        case ValueKind::kMap:
          result = Value{TestOnlySelect(Cast<MapValue>(result), field_value_,
                                        frame.value_manager(), scratch)};
          return absl::OkStatus();
        case ValueKind::kMessage:
          result = Value{TestOnlySelect(Cast<StructValue>(result), field_,
                                        frame.value_manager(), scratch)};
          return absl::OkStatus();
        default:
          // Control flow should have returned earlier.
          return InvalidSelectTargetError();
      }
    }

    // Normal select path.
    // Select steps can be applied to either maps or messages
    switch (result.kind()) {
      case ValueKind::kStruct: {
        CEL_ASSIGN_OR_RETURN(result, Cast<StructValue>(result).GetFieldByName(
                                         frame.value_manager(), field_, scratch,
                                         unboxing_option_));
        return absl::OkStatus();
      }
      case ValueKind::kMap: {
        CEL_ASSIGN_OR_RETURN(result,
                             Cast<MapValue>(result).Get(frame.value_manager(),
                                                        field_value_, scratch));
        return absl::OkStatus();
      }
      default:
        // Control flow should have returned earlier.
        return InvalidSelectTargetError();
    }
  }

 private:
  std::unique_ptr<DirectExpressionStep> operand_;

  // Field name in formats supported by each of the map and struct field access
  // APIs.
  //
  // ToString or ValueManager::CreateString may force a copy so we do this at
  // plan time.
  StringValue field_value_;
  std::string field_;

  // whether this is a has() expression.
  bool test_only_;
  ProtoWrapperTypeOptions unboxing_option_;
};

}  // namespace

std::unique_ptr<DirectExpressionStep> CreateDirectSelectStep(
    std::unique_ptr<DirectExpressionStep> operand, StringValue field,
    bool test_only, int64_t expr_id, bool enable_wrapper_type_null_unboxing) {
  return std::make_unique<DirectSelectStep>(expr_id, std::move(operand),
                                            std::move(field), test_only,
                                            enable_wrapper_type_null_unboxing);
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
