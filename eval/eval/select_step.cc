#include "eval/eval/select_step.h"

#include <cstdint>
#include <string>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/legacy_type_adapter.h"
#include "eval/public/structs/legacy_type_info_apis.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

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
  absl::Status CreateValueFromField(const CelValue::MessageWrapper& msg,
                                    cel::MemoryManager& manager,
                                    CelValue* result) const;

  std::string field_;
  bool test_field_presence_;
  std::string select_path_;
  ProtoWrapperTypeOptions unboxing_option_;
};

absl::Status SelectStep::CreateValueFromField(
    const CelValue::MessageWrapper& msg, cel::MemoryManager& manager,
    CelValue* result) const {
  const LegacyTypeAccessApis* accessor =
      msg.legacy_type_info()->GetAccessApis(msg);
  if (accessor == nullptr) {
    *result = CreateNoSuchFieldError(manager);
    return absl::OkStatus();
  }
  CEL_ASSIGN_OR_RETURN(
      *result, accessor->GetField(field_, msg, unboxing_option_, manager));
  return absl::OkStatus();
}

absl::optional<CelValue> CheckForMarkedAttributes(const AttributeTrail& trail,
                                                  ExecutionFrame* frame) {
  if (frame->enable_unknowns() &&
      frame->attribute_utility().CheckForUnknown(trail,
                                                 /*use_partial=*/false)) {
    auto unknown_set = frame->memory_manager().New<UnknownSet>(
        UnknownAttributeSet({trail.attribute()}));
    return CelValue::CreateUnknownSet(unknown_set.release());
  }

  if (frame->enable_missing_attribute_errors() &&
      frame->attribute_utility().CheckForMissingAttribute(trail)) {
    auto attribute_string = trail.attribute().AsString();
    if (attribute_string.ok()) {
      return CreateMissingAttributeError(frame->memory_manager(),
                                         *attribute_string);
    }
    // Invariant broken (an invalid CEL Attribute shouldn't match anything).
    // Log and return a CelError.
    LOG(ERROR)
        << "Invalid attribute pattern matched select path: "
        << attribute_string.status().ToString();  // NOLINT: OSS compatibility
    return CreateErrorValue(frame->memory_manager(), attribute_string.status());
  }

  return absl::nullopt;
}

CelValue TestOnlySelect(const CelValue::MessageWrapper& msg,
                        const std::string& field, cel::MemoryManager& manager) {
  const LegacyTypeAccessApis* accessor =
      msg.legacy_type_info()->GetAccessApis(msg);
  if (accessor == nullptr) {
    return CreateNoSuchFieldError(manager);
  }
  // Standard proto presence test for non-repeated fields.
  absl::StatusOr<bool> result = accessor->HasField(field, msg);
  if (!result.ok()) {
    return CreateErrorValue(manager, std::move(result).status());
  }
  return CelValue::CreateBool(*result);
}

CelValue TestOnlySelect(const CelMap& map, const std::string& field_name,
                        cel::MemoryManager& manager) {
  // Field presence only supports string keys containing valid identifier
  // characters.
  auto presence = map.Has(CelValue::CreateStringView(field_name));
  if (!presence.ok()) {
    return CreateErrorValue(manager, presence.status());
  }

  return CelValue::CreateBool(*presence);
}

absl::Status SelectStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(1)) {
    return absl::Status(absl::StatusCode::kInternal,
                        "No arguments supplied for Select-type expression");
  }

  const CelValue& arg = frame->value_stack().Peek();
  const AttributeTrail& trail = frame->value_stack().PeekAttribute();

  if (arg.IsUnknownSet() || arg.IsError()) {
    // Bubble up unknowns and errors.
    return absl::OkStatus();
  }

  CelValue result;
  AttributeTrail result_trail;

  // Handle unknown resolution.
  if (frame->enable_unknowns() || frame->enable_missing_attribute_errors()) {
    result_trail = trail.Step(&field_, frame->memory_manager());
  }

  if (arg.IsNull()) {
    CelValue error_value =
        CreateErrorValue(frame->memory_manager(), "Message is NULL");
    frame->value_stack().PopAndPush(error_value, std::move(result_trail));
    return absl::OkStatus();
  }

  if (!(arg.IsMap() || arg.IsMessage())) {
    return InvalidSelectTargetError();
  }

  absl::optional<CelValue> marked_attribute_check =
      CheckForMarkedAttributes(result_trail, frame);
  if (marked_attribute_check.has_value()) {
    frame->value_stack().PopAndPush(marked_attribute_check.value(),
                                    std::move(result_trail));
    return absl::OkStatus();
  }

  // Nullness checks
  switch (arg.type()) {
    case CelValue::Type::kMap: {
      if (arg.MapOrDie() == nullptr) {
        frame->value_stack().PopAndPush(
            CreateErrorValue(frame->memory_manager(), "Map is NULL"),
            std::move(result_trail));
        return absl::OkStatus();
      }
      break;
    }
    case CelValue::Type::kMessage: {
      if (CelValue::MessageWrapper w;
          arg.GetValue(&w) && w.message_ptr() == nullptr) {
        frame->value_stack().PopAndPush(
            CreateErrorValue(frame->memory_manager(), "Message is NULL"),
            std::move(result_trail));
        return absl::OkStatus();
      }
      break;
    }
    default:
      // Should not be reached by construction.
      return InvalidSelectTargetError();
  }

  // Handle test only Select.
  if (test_field_presence_) {
    if (arg.IsMap()) {
      frame->value_stack().PopAndPush(
          TestOnlySelect(*arg.MapOrDie(), field_, frame->memory_manager()));
      return absl::OkStatus();
    } else if (CelValue::MessageWrapper message; arg.GetValue(&message)) {
      frame->value_stack().PopAndPush(
          TestOnlySelect(message, field_, frame->memory_manager()));
      return absl::OkStatus();
    }
  }

  // Normal select path.
  // Select steps can be applied to either maps or messages
  switch (arg.type()) {
    case CelValue::Type::kMessage: {
      CelValue::MessageWrapper wrapper;
      bool success = arg.GetValue(&wrapper);
      ABSL_ASSERT(success);

      CEL_RETURN_IF_ERROR(
          CreateValueFromField(wrapper, frame->memory_manager(), &result));
      frame->value_stack().PopAndPush(result, std::move(result_trail));

      return absl::OkStatus();
    }
    case CelValue::Type::kMap: {
      // not null.
      const CelMap& cel_map = *arg.MapOrDie();

      CelValue field_name = CelValue::CreateString(&field_);
      absl::optional<CelValue> lookup_result = cel_map[field_name];

      // If object is not found, we return Error, per CEL specification.
      if (lookup_result.has_value()) {
        result = *lookup_result;
      } else {
        result = CreateNoSuchKeyError(frame->memory_manager(), field_);
      }
      frame->value_stack().PopAndPush(result, std::move(result_trail));
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
  return absl::make_unique<SelectStep>(
      select_expr.field(), select_expr.test_only(), expr_id, select_path,
      enable_wrapper_type_null_unboxing);
}

}  // namespace google::api::expr::runtime
