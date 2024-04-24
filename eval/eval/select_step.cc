#include "eval/eval/select_step.h"

#include <cstdint>
#include <memory>
#include <string>
#include <tuple>
#include <utility>

#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "base/kind.h"
#include "common/casting.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/internal/errors.h"
#include "internal/casts.h"
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
using ::cel::OptionalValue;
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
             bool enable_wrapper_type_null_unboxing, bool enable_optional_types)
      : ExpressionStepBase(expr_id),
        field_value_(std::move(value)),
        field_(field_value_.ToString()),
        test_field_presence_(test_field_presence),
        unboxing_option_(enable_wrapper_type_null_unboxing
                             ? ProtoWrapperTypeOptions::kUnsetNull
                             : ProtoWrapperTypeOptions::kUnsetProtoDefault),
        enable_optional_types_(enable_optional_types) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  absl::Status PerformTestOnlySelect(ExecutionFrame* frame, const Value& arg,
                                     Value& scratch) const;
  absl::StatusOr<std::pair<ValueView, bool>> PerformSelect(
      ExecutionFrame* frame, const Value& arg, Value& scratch) const;

  cel::StringValue field_value_;
  std::string field_;
  bool test_field_presence_;
  ProtoWrapperTypeOptions unboxing_option_;
  bool enable_optional_types_;
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

  const cel::OptionalValueInterface* optional_arg = nullptr;

  if (enable_optional_types_ &&
      cel::NativeTypeId::Of(arg) ==
          cel::NativeTypeId::For<cel::OptionalValueInterface>()) {
    optional_arg = cel::internal::down_cast<const cel::OptionalValueInterface*>(
        cel::Cast<cel::OpaqueValue>(arg).operator->());
  }

  if (!(optional_arg != nullptr || arg->Is<MapValue>() ||
        arg->Is<StructValue>())) {
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
    if (optional_arg != nullptr) {
      if (!optional_arg->HasValue()) {
        frame->value_stack().PopAndPush(cel::BoolValue{false});
        return absl::OkStatus();
      }
      return PerformTestOnlySelect(frame, optional_arg->Value(),
                                   result_scratch);
    }
    return PerformTestOnlySelect(frame, arg, result_scratch);
  }

  // Normal select path.
  // Select steps can be applied to either maps or messages
  if (optional_arg != nullptr) {
    if (!optional_arg->HasValue()) {
      // Leave optional_arg at the top of the stack. Its empty.
      return absl::OkStatus();
    }
    ValueView result;
    bool ok;
    CEL_ASSIGN_OR_RETURN(
        std::tie(result, ok),
        PerformSelect(frame, optional_arg->Value(), result_scratch));
    if (!ok) {
      frame->value_stack().PopAndPush(cel::OptionalValue::None(),
                                      std::move(result_trail));
      return absl::OkStatus();
    }
    frame->value_stack().PopAndPush(
        cel::OptionalValue::Of(frame->memory_manager(), cel::Value{result}),
        std::move(result_trail));
    return absl::OkStatus();
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

absl::Status SelectStep::PerformTestOnlySelect(ExecutionFrame* frame,
                                               const Value& arg,
                                               Value& scratch) const {
  switch (arg->kind()) {
    case ValueKind::kMap:
      frame->value_stack().PopAndPush(Value{TestOnlySelect(
          arg.As<MapValue>(), field_value_, frame->value_factory(), scratch)});
      return absl::OkStatus();
    case ValueKind::kMessage:
      frame->value_stack().PopAndPush(Value{TestOnlySelect(
          arg.As<StructValue>(), field_, frame->value_factory(), scratch)});
      return absl::OkStatus();
    default:
      // Control flow should have returned earlier.
      return InvalidSelectTargetError();
  }
}

absl::StatusOr<std::pair<ValueView, bool>> SelectStep::PerformSelect(
    ExecutionFrame* frame, const Value& arg, Value& scratch) const {
  switch (arg->kind()) {
    case ValueKind::kStruct: {
      const auto& struct_value = arg.As<StructValue>();
      CEL_ASSIGN_OR_RETURN(auto ok, struct_value.HasFieldByName(field_));
      if (!ok) {
        return std::pair{cel::NullValueView{}, false};
      }
      CEL_ASSIGN_OR_RETURN(auto result, struct_value.GetFieldByName(
                                            frame->value_factory(), field_,
                                            scratch, unboxing_option_));
      return std::pair{result, true};
    }
    case ValueKind::kMap: {
      return arg.As<MapValue>().Find(frame->value_factory(), field_value_,
                                     scratch);
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
                   bool enable_wrapper_type_null_unboxing,
                   bool enable_optional_types)
      : DirectExpressionStep(expr_id),
        operand_(std::move(operand)),
        field_value_(std::move(field)),
        field_(field_value_.ToString()),
        test_only_(test_only),
        unboxing_option_(enable_wrapper_type_null_unboxing
                             ? ProtoWrapperTypeOptions::kUnsetNull
                             : ProtoWrapperTypeOptions::kUnsetProtoDefault),
        enable_optional_types_(enable_optional_types) {}

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

    const cel::OptionalValueInterface* optional_arg = nullptr;

    if (enable_optional_types_ &&
        cel::NativeTypeId::Of(result) ==
            cel::NativeTypeId::For<cel::OptionalValueInterface>()) {
      optional_arg =
          cel::internal::down_cast<const cel::OptionalValueInterface*>(
              cel::Cast<cel::OpaqueValue>(result).operator->());
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
        if (optional_arg != nullptr) {
          break;
        }
        result =
            frame.value_manager().CreateErrorValue(InvalidSelectTargetError());
        return absl::OkStatus();
    }

    Value scratch;
    if (test_only_) {
      if (optional_arg != nullptr) {
        if (!optional_arg->HasValue()) {
          result = cel::BoolValue{false};
          return absl::OkStatus();
        }
        result = PerformTestOnlySelect(frame, optional_arg->Value(), scratch);
        return absl::OkStatus();
      }
      result = PerformTestOnlySelect(frame, result, scratch);
      return absl::OkStatus();
    }

    if (optional_arg != nullptr) {
      if (!optional_arg->HasValue()) {
        // result is still buffer for the container. just return.
        return absl::OkStatus();
      }
      CEL_ASSIGN_OR_RETURN(
          result, PerformOptionalSelect(frame, optional_arg->Value(), scratch));

      return absl::OkStatus();
    }

    CEL_ASSIGN_OR_RETURN(result, PerformSelect(frame, result, scratch));
    return absl::OkStatus();
  }

 private:
  std::unique_ptr<DirectExpressionStep> operand_;

  ValueView PerformTestOnlySelect(ExecutionFrameBase& frame, const Value& value,
                                  Value& scratch) const;
  absl::StatusOr<ValueView> PerformOptionalSelect(ExecutionFrameBase& frame,
                                                  const Value& value,
                                                  Value& scratch) const;
  absl::StatusOr<ValueView> PerformSelect(ExecutionFrameBase& frame,
                                          const Value& value,
                                          Value& scratch) const;

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
  bool enable_optional_types_;
};

ValueView DirectSelectStep::PerformTestOnlySelect(ExecutionFrameBase& frame,
                                                  const cel::Value& value,
                                                  Value& scratch) const {
  switch (value.kind()) {
    case ValueKind::kMap:
      return TestOnlySelect(Cast<MapValue>(value), field_value_,
                            frame.value_manager(), scratch);
    case ValueKind::kMessage:
      return TestOnlySelect(Cast<StructValue>(value), field_,
                            frame.value_manager(), scratch);
    default:
      // Control flow should have returned earlier.
      scratch =
          frame.value_manager().CreateErrorValue(InvalidSelectTargetError());
      return ValueView{scratch};
  }
}

absl::StatusOr<ValueView> DirectSelectStep::PerformOptionalSelect(
    ExecutionFrameBase& frame, const Value& value, Value& scratch) const {
  switch (value.kind()) {
    case ValueKind::kStruct: {
      auto struct_value = Cast<StructValue>(value);
      CEL_ASSIGN_OR_RETURN(auto ok, struct_value.HasFieldByName(field_));
      if (!ok) {
        scratch = OptionalValue::None();
        return ValueView{scratch};
      }
      CEL_ASSIGN_OR_RETURN(auto result, struct_value.GetFieldByName(
                                            frame.value_manager(), field_,
                                            scratch, unboxing_option_));
      scratch = OptionalValue::Of(frame.value_manager().GetMemoryManager(),
                                  Value(result));
      return ValueView{scratch};
    }
    case ValueKind::kMap: {
      CEL_ASSIGN_OR_RETURN(auto lookup,
                           Cast<MapValue>(value).Find(frame.value_manager(),
                                                      field_value_, scratch));
      if (!lookup.second) {
        scratch = OptionalValue::None();
        return ValueView{scratch};
      }
      scratch = OptionalValue::Of(frame.value_manager().GetMemoryManager(),
                                  Value(lookup.first));
      return ValueView{scratch};
    }
    default:
      // Control flow should have returned earlier.
      return InvalidSelectTargetError();
  }
}

absl::StatusOr<ValueView> DirectSelectStep::PerformSelect(
    ExecutionFrameBase& frame, const cel::Value& value, Value& scratch) const {
  switch (value.kind()) {
    case ValueKind::kStruct: {
      return Cast<StructValue>(value).GetFieldByName(
          frame.value_manager(), field_, scratch, unboxing_option_);
    }
    case ValueKind::kMap: {
      return Cast<MapValue>(value).Get(frame.value_manager(), field_value_,
                                       scratch);
    }
    default:
      // Control flow should have returned earlier.
      return InvalidSelectTargetError();
  }
}

}  // namespace

std::unique_ptr<DirectExpressionStep> CreateDirectSelectStep(
    std::unique_ptr<DirectExpressionStep> operand, StringValue field,
    bool test_only, int64_t expr_id, bool enable_wrapper_type_null_unboxing,
    bool enable_optional_types) {
  return std::make_unique<DirectSelectStep>(
      expr_id, std::move(operand), std::move(field), test_only,
      enable_wrapper_type_null_unboxing, enable_optional_types);
}

// Factory method for Select - based Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateSelectStep(
    const cel::ast_internal::Select& select_expr, int64_t expr_id,
    bool enable_wrapper_type_null_unboxing, cel::ValueManager& value_factory,
    bool enable_optional_types) {
  return std::make_unique<SelectStep>(
      value_factory.CreateUncheckedStringValue(select_expr.field()),
      select_expr.test_only(), expr_id, enable_wrapper_type_null_unboxing,
      enable_optional_types);
}

}  // namespace google::api::expr::runtime
