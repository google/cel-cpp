#include "eval/eval/select_step.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/handle.h"
#include "base/memory_manager.h"
#include "base/type_manager.h"
#include "base/value_factory.h"
#include "base/values/error_value.h"
#include "base/values/map_value.h"
#include "base/values/null_value.h"
#include "base/values/string_value.h"
#include "base/values/struct_value.h"
#include "base/values/unknown_value.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/internal/errors.h"
#include "eval/internal/interop.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::ErrorValue;
using ::cel::Handle;
using ::cel::Kind;
using ::cel::MapValue;
using ::cel::NullValue;
using ::cel::StructValue;
using ::cel::UnknownValue;
using ::cel::Value;
using ::cel::extensions::ProtoMemoryManager;
using ::cel::interop_internal::CreateBoolValue;
using ::cel::interop_internal::CreateError;
using ::cel::interop_internal::CreateErrorValueFromView;
using ::cel::interop_internal::CreateMissingAttributeError;
using ::cel::interop_internal::CreateNoSuchKeyError;
using ::cel::interop_internal::CreateStringValueFromView;
using ::cel::interop_internal::CreateUnknownValueFromView;
using ::cel::interop_internal::MessageValueGetFieldWithWrapperAsProtoDefault;
using ::google::protobuf::Arena;

// Common error for cases where evaluation attempts to perform select operations
// on an unsupported type.
//
// This should not happen under normal usage of the evaluator, but useful for
// troubleshooting broken invariants.
absl::Status InvalidSelectTargetError() {
  return absl::Status(absl::StatusCode::kInvalidArgument,
                      "Applying SELECT to non-message type");
}

// SelectStep performs message field access specified by Expr::Select
// message.
class SelectStep : public ExpressionStepBase {
 public:
  SelectStep(absl::string_view field, bool test_field_presence, int64_t expr_id,
             absl::string_view select_path,
             bool enable_wrapper_type_null_unboxing)
      : ExpressionStepBase(expr_id),
        field_(field),
        test_field_presence_(test_field_presence),
        select_path_(select_path),
        unboxing_option_(enable_wrapper_type_null_unboxing
                             ? ProtoWrapperTypeOptions::kUnsetNull
                             : ProtoWrapperTypeOptions::kUnsetProtoDefault) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  absl::StatusOr<Handle<Value>> CreateValueFromField(
      const Handle<StructValue>& msg, ExecutionFrame* frame) const;

  std::string field_;
  bool test_field_presence_;
  std::string select_path_;
  ProtoWrapperTypeOptions unboxing_option_;
};

absl::StatusOr<Handle<Value>> SelectStep::CreateValueFromField(
    const Handle<StructValue>& msg, ExecutionFrame* frame) const {
  StructValue::FieldId field_id(field_);
  switch (unboxing_option_) {
    case ProtoWrapperTypeOptions::kUnsetProtoDefault:
      return MessageValueGetFieldWithWrapperAsProtoDefault(
          msg, frame->value_factory(), field_);
    default:
      return msg->GetField(StructValue::GetFieldContext(frame->value_factory()),
                           field_id);
  }
}

absl::optional<Handle<Value>> CheckForMarkedAttributes(
    const AttributeTrail& trail, ExecutionFrame* frame) {
  Arena* arena = ProtoMemoryManager::CastToProtoArena(frame->memory_manager());

  if (frame->enable_unknowns() &&
      frame->attribute_utility().CheckForUnknown(trail,
                                                 /*use_partial=*/false)) {
    auto unknown_set = Arena::Create<UnknownSet>(
        arena, UnknownAttributeSet({trail.attribute()}));
    return CreateUnknownValueFromView(unknown_set);
  }

  if (frame->enable_missing_attribute_errors() &&
      frame->attribute_utility().CheckForMissingAttribute(trail)) {
    auto attribute_string = trail.attribute().AsString();
    if (attribute_string.ok()) {
      return CreateErrorValueFromView(CreateMissingAttributeError(
          frame->memory_manager(), *attribute_string));
    }
    // Invariant broken (an invalid CEL Attribute shouldn't match anything).
    // Log and return a CelError.
    LOG(ERROR)
        << "Invalid attribute pattern matched select path: "
        << attribute_string.status().ToString();  // NOLINT: OSS compatibility
    return CreateErrorValueFromView(Arena::Create<absl::Status>(
        arena, std::move(attribute_string).status()));
  }

  return absl::nullopt;
}

Handle<Value> TestOnlySelect(const Handle<StructValue>& msg,
                             const std::string& field,
                             cel::MemoryManager& memory_manager,
                             cel::TypeManager& type_manager) {
  StructValue::FieldId field_id(field);
  Arena* arena = ProtoMemoryManager::CastToProtoArena(memory_manager);

  absl::StatusOr<bool> result =
      msg->HasField(StructValue::HasFieldContext(type_manager), field_id);

  if (!result.ok()) {
    return CreateErrorValueFromView(
        Arena::Create<absl::Status>(arena, std::move(result).status()));
  }
  return CreateBoolValue(*result);
}

Handle<Value> TestOnlySelect(const Handle<MapValue>& map,
                             const std::string& field_name,
                             cel::MemoryManager& manager) {
  // Field presence only supports string keys containing valid identifier
  // characters.
  auto presence =
      map->Has(MapValue::HasContext(), CreateStringValueFromView(field_name));

  if (!presence.ok()) {
    Arena* arena = ProtoMemoryManager::CastToProtoArena(manager);
    auto* status =
        Arena::Create<absl::Status>(arena, std::move(presence).status());
    return CreateErrorValueFromView(status);
  }

  return CreateBoolValue(*presence);
}

absl::Status SelectStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(1)) {
    return absl::Status(absl::StatusCode::kInternal,
                        "No arguments supplied for Select-type expression");
  }

  const Handle<Value>& arg = frame->value_stack().Peek();
  const AttributeTrail& trail = frame->value_stack().PeekAttribute();

  if (arg->Is<UnknownValue>() || arg->Is<ErrorValue>()) {
    // Bubble up unknowns and errors.
    return absl::OkStatus();
  }

  AttributeTrail result_trail;

  // Handle unknown resolution.
  if (frame->enable_unknowns() || frame->enable_missing_attribute_errors()) {
    result_trail = trail.Step(&field_, frame->memory_manager());
  }

  if (arg->Is<NullValue>()) {
    frame->value_stack().PopAndPush(
        CreateErrorValueFromView(
            CreateError(frame->memory_manager(), "Message is NULL")),
        std::move(result_trail));
    return absl::OkStatus();
  }

  if (!(arg->Is<MapValue>() || arg->Is<StructValue>())) {
    return InvalidSelectTargetError();
  }

  absl::optional<Handle<Value>> marked_attribute_check =
      CheckForMarkedAttributes(result_trail, frame);
  if (marked_attribute_check.has_value()) {
    frame->value_stack().PopAndPush(std::move(marked_attribute_check).value(),
                                    std::move(result_trail));
    return absl::OkStatus();
  }

  // Handle test only Select.
  if (test_field_presence_) {
    switch (arg->kind()) {
      case Kind::kMap:
        frame->value_stack().PopAndPush(TestOnlySelect(
            arg.As<MapValue>(), field_, frame->memory_manager()));
        return absl::OkStatus();
      case Kind::kMessage:
        frame->value_stack().PopAndPush(
            TestOnlySelect(arg.As<StructValue>(), field_,
                           frame->memory_manager(), frame->type_manager()));
        return absl::OkStatus();
      default:
        return InvalidSelectTargetError();
    }
  }

  // Normal select path.
  // Select steps can be applied to either maps or messages
  switch (arg->kind()) {
    case Kind::kStruct: {
      CEL_ASSIGN_OR_RETURN(Handle<Value> result,
                           CreateValueFromField(arg.As<StructValue>(), frame));
      frame->value_stack().PopAndPush(std::move(result),
                                      std::move(result_trail));

      return absl::OkStatus();
    }
    case CelValue::Type::kMap: {
      const auto& cel_map = arg.As<MapValue>();
      auto cel_field = CreateStringValueFromView(field_);
      CEL_ASSIGN_OR_RETURN(
          auto result,
          cel_map->Get(MapValue::GetContext(frame->value_factory()),
                       cel_field));

      // If object is not found, we return Error, per CEL specification.
      if (!result.has_value()) {
        result = CreateErrorValueFromView(
            CreateNoSuchKeyError(frame->memory_manager(), field_));
      }
      frame->value_stack().PopAndPush(std::move(result).value(),
                                      std::move(result_trail));
      return absl::OkStatus();
    }
    default:
      return InvalidSelectTargetError();
  }
}

}  // namespace

// Factory method for Select - based Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateSelectStep(
    const cel::ast::internal::Select& select_expr, int64_t expr_id,
    absl::string_view select_path, bool enable_wrapper_type_null_unboxing) {
  return std::make_unique<SelectStep>(
      select_expr.field(), select_expr.test_only(), expr_id, select_path,
      enable_wrapper_type_null_unboxing);
}

}  // namespace google::api::expr::runtime
