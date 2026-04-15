#include "eval/eval/select_step.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/log/absl_check.h"
#include "absl/log/absl_log.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/expr.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/direct_expression_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "internal/status_macros.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::BoolValue;
using ::cel::ErrorValue;
using ::cel::MapValue;
using ::cel::OptionalValue;
using ::cel::ProtoWrapperTypeOptions;
using ::cel::StringValue;
using ::cel::StructValue;
using ::cel::Value;
using ::cel::ValueKind;

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
    return cel::ErrorValue(std::move(result).status());
  }

  return absl::nullopt;
}

absl::Status PerformHas(const Value& target, absl::string_view field,
                        const StringValue& field_value,
                        const google::protobuf::DescriptorPool* descriptor_pool,
                        google::protobuf::MessageFactory* message_factory,
                        google::protobuf::Arena* arena, Value& result) {
  switch (target.kind()) {
    case ValueKind::kMap: {
      CEL_RETURN_IF_ERROR(target.GetMap().Has(field_value, descriptor_pool,
                                              message_factory, arena, &result));
      return absl::OkStatus();
    }
    case ValueKind::kStruct: {
      auto has_field = target.GetStruct().HasFieldByName(field);
      if (!has_field.ok()) {
        result = ErrorValue(std::move(has_field).status());
      } else {
        result = BoolValue{*has_field};
      }
      return absl::OkStatus();
    }
    default:
      return InvalidSelectTargetError();
  }
}

absl::Status PerformGet(const Value& target, absl::string_view field,
                        const StringValue& field_value,
                        ProtoWrapperTypeOptions unboxing_option,
                        const google::protobuf::DescriptorPool* descriptor_pool,
                        google::protobuf::MessageFactory* message_factory,
                        google::protobuf::Arena* arena, Value& result) {
  switch (target.kind()) {
    case ValueKind::kMap: {
      auto status = target.GetMap().Get(field_value, descriptor_pool,
                                        message_factory, arena, &result);
      if (!status.ok()) {
        result = ErrorValue(std::move(status));
      }
      return absl::OkStatus();
    }
    case ValueKind::kStruct: {
      auto status = target.GetStruct().GetFieldByName(
          field, unboxing_option, descriptor_pool, message_factory, arena,
          &result);
      if (!status.ok()) {
        result = ErrorValue(std::move(status));
      }
      return absl::OkStatus();
    }
    default:
      return InvalidSelectTargetError();
  }
}

absl::Status PerformOptionalGet(const Value& target, absl::string_view field,
                                const StringValue& field_value,
                                ProtoWrapperTypeOptions unboxing_option,
                                const google::protobuf::DescriptorPool* descriptor_pool,
                                google::protobuf::MessageFactory* message_factory,
                                google::protobuf::Arena* arena, Value& result) {
  switch (target.kind()) {
    case ValueKind::kMap: {
      CEL_ASSIGN_OR_RETURN(
          bool found, target.GetMap().Find(field_value, descriptor_pool,
                                           message_factory, arena, &result));
      if (!found) {
        result = OptionalValue::None();
        return absl::OkStatus();
      }
      ABSL_DCHECK(!result.IsUnknown());
      result = OptionalValue::Of(std::move(result), arena);
      return absl::OkStatus();
    }
    case ValueKind::kStruct: {
      CEL_ASSIGN_OR_RETURN(bool found,
                           target.GetStruct().HasFieldByName(field));
      if (!found) {
        result = OptionalValue::None();
        return absl::OkStatus();
      }
      CEL_RETURN_IF_ERROR(target.GetStruct().GetFieldByName(
          field, unboxing_option, descriptor_pool, message_factory, arena,
          &result));

      ABSL_DCHECK(!result.IsUnknown());
      result = OptionalValue::Of(std::move(result), arena);
      return absl::OkStatus();
    }
    default:
      return InvalidSelectTargetError();
  }
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

  if (arg.IsUnknown() || arg.IsError()) {
    // Bubble up unknowns and errors.
    return absl::OkStatus();
  }

  AttributeTrail result_trail;

  // Handle unknown resolution.
  if (frame->enable_unknowns() || frame->enable_missing_attribute_errors()) {
    result_trail = trail.Step(&field_);
  }

  absl::optional<OptionalValue> optional_arg;

  if (enable_optional_types_ && arg.IsOptional()) {
    optional_arg = arg.GetOptional();
  }

  if (!(optional_arg || arg->Is<MapValue>() || arg->Is<StructValue>())) {
    frame->value_stack().PopAndPush(cel::ErrorValue(InvalidSelectTargetError()),
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

  Value result;
  if (test_field_presence_) {
    const Value* target = &arg;
    if (optional_arg) {
      if (!optional_arg->HasValue()) {
        frame->value_stack().PopAndPush(cel::BoolValue{false},
                                        std::move(result_trail));
        return absl::OkStatus();
      }
      optional_arg->Value(&result);
      target = &result;
    }
    CEL_RETURN_IF_ERROR(
        PerformHas(*target, field_, field_value_, frame->descriptor_pool(),
                   frame->message_factory(), frame->arena(), result));
    frame->value_stack().PopAndPush(std::move(result), std::move(result_trail));
    return absl::OkStatus();
  }

  if (optional_arg) {
    if (!optional_arg->HasValue()) {
      frame->value_stack().PopAndPush(OptionalValue::None(),
                                      std::move(result_trail));
      return absl::OkStatus();
    }
    Value value;
    optional_arg->Value(&value);
    auto status = PerformOptionalGet(
        value, field_, field_value_, unboxing_option_, frame->descriptor_pool(),
        frame->message_factory(), frame->arena(), result);
    if (!status.ok()) {
      result = ErrorValue(std::move(status));
    }
    frame->value_stack().PopAndPush(std::move(result), std::move(result_trail));
    return absl::OkStatus();
  }

  CEL_RETURN_IF_ERROR(PerformGet(
      arg, field_, field_value_, unboxing_option_, frame->descriptor_pool(),
      frame->message_factory(), frame->arena(), result));
  frame->value_stack().PopAndPush(std::move(result), std::move(result_trail));
  return absl::OkStatus();
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

    if (result.IsError() || result.IsUnknown()) {
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

    absl::optional<OptionalValue> optional_arg;

    if (enable_optional_types_ && result.IsOptional()) {
      optional_arg = result.GetOptional();
    }

    switch (result.kind()) {
      case ValueKind::kStruct:
      case ValueKind::kMap:
        break;
      default:
        if (optional_arg) {
          break;
        }
        result = cel::ErrorValue(InvalidSelectTargetError());
        return absl::OkStatus();
    }

    if (test_only_) {
      if (optional_arg) {
        if (!optional_arg->HasValue()) {
          result = cel::BoolValue{false};
          return absl::OkStatus();
        }
        Value value;
        optional_arg->Value(&value);
        return PerformHas(value, field_, field_value_, frame.descriptor_pool(),
                          frame.message_factory(), frame.arena(), result);
      }
      return PerformHas(result, field_, field_value_, frame.descriptor_pool(),
                        frame.message_factory(), frame.arena(), result);
    }

    if (optional_arg) {
      if (!optional_arg->HasValue()) {
        // result is still buffer for the container. just return.
        return absl::OkStatus();
      }
      Value value;
      optional_arg->Value(&value);
      auto status =
          PerformOptionalGet(value, field_, field_value_, unboxing_option_,
                             frame.descriptor_pool(), frame.message_factory(),
                             frame.arena(), result);
      if (!status.ok()) {
        result = ErrorValue(std::move(status));
      }
      return absl::OkStatus();
    }

    return PerformGet(result, field_, field_value_, unboxing_option_,
                      frame.descriptor_pool(), frame.message_factory(),
                      frame.arena(), result);
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
  bool enable_optional_types_;
};

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
    const cel::SelectExpr& select_expr, int64_t expr_id,
    bool enable_wrapper_type_null_unboxing, bool enable_optional_types) {
  return std::make_unique<SelectStep>(
      cel::StringValue(select_expr.field()), select_expr.test_only(), expr_id,
      enable_wrapper_type_null_unboxing, enable_optional_types);
}

}  // namespace google::api::expr::runtime
