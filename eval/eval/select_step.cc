#include "eval/eval/select_step.h"

#include <cstdint>

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_step_base.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/field_access.h"
#include "eval/public/containers/field_backed_list_impl.h"
#include "eval/public/containers/field_backed_map_impl.h"

namespace google::api::expr::runtime {

namespace {

using ::google::protobuf::Descriptor;
using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::Reflection;

// SelectStep performs message field access specified by Expr::Select
// message.
class SelectStep : public ExpressionStepBase {
 public:
  SelectStep(absl::string_view field, bool test_field_presence, int64_t expr_id,
             absl::string_view select_path)
      : ExpressionStepBase(expr_id),
        field_(field),
        test_field_presence_(test_field_presence),
        select_path_(select_path) {}

  absl::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  absl::Status CreateValueFromField(const google::protobuf::Message* msg,
                                    google::protobuf::Arena* arena,
                                    CelValue* result) const;

  std::string field_;
  bool test_field_presence_;
  std::string select_path_;
};

absl::Status SelectStep::CreateValueFromField(const google::protobuf::Message* msg,
                                              google::protobuf::Arena* arena,
                                              CelValue* result) const {
  const Reflection* reflection = msg->GetReflection();
  const Descriptor* desc = msg->GetDescriptor();
  const FieldDescriptor* field_desc = desc->FindFieldByName(field_);

  if (field_desc == nullptr) {
    *result = CreateNoSuchFieldError(arena, field_);
    return absl::OkStatus();
  }

  if (field_desc->is_map()) {
    // When the map field appears in a has(msg.map_field) expression, the map
    // is considered 'present' when it is non-empty. Since maps are repeated
    // fields they don't participate with standard proto presence testing since
    // the repeated field is always at least empty.
    if (test_field_presence_) {
      *result =
          CelValue::CreateBool(reflection->FieldSize(*msg, field_desc) != 0);
      return absl::OkStatus();
    }
    CelMap* map = google::protobuf::Arena::Create<FieldBackedMapImpl>(arena, msg,
                                                            field_desc, arena);
    *result = CelValue::CreateMap(map);
    return absl::OkStatus();
  }
  if (field_desc->is_repeated()) {
    // When the list field appears in a has(msg.list_field) expression, the list
    // is considered 'present' when it is non-empty.
    if (test_field_presence_) {
      *result =
          CelValue::CreateBool(reflection->FieldSize(*msg, field_desc) != 0);
      return absl::OkStatus();
    }
    CelList* list = google::protobuf::Arena::Create<FieldBackedListImpl>(
        arena, msg, field_desc, arena);
    *result = CelValue::CreateList(list);
    return absl::OkStatus();
  }

  if (test_field_presence_) {
    // Standard proto presence test for non-repeated fields.
    *result = CelValue::CreateBool(reflection->HasField(*msg, field_desc));
    return absl::OkStatus();
  }
  return CreateValueFromSingleField(msg, field_desc, arena, result);
}

absl::Status SelectStep::Evaluate(ExecutionFrame* frame) const {
  if (!frame->value_stack().HasEnough(1)) {
    return absl::Status(absl::StatusCode::kInternal,
                        "No arguments supplied for Select-type expression");
  }

  const CelValue& arg = frame->value_stack().Peek();
  const AttributeTrail& trail = frame->value_stack().PeekAttribute();

  CelValue result;
  AttributeTrail result_trail;

  // Non-empty select path - check if value mapped to unknown or error.
  bool unknown_value = false;
  // TODO(issues/41) deprecate this path after proper support of unknown is
  // implemented
  if (!select_path_.empty()) {
    unknown_value = frame->activation().IsPathUnknown(select_path_);
  }

  // Select steps can be applied to either maps or messages
  switch (arg.type()) {
    case CelValue::Type::kMessage: {
      const google::protobuf::Message* msg = arg.MessageOrDie();

      if (frame->enable_unknowns() ||
          frame->enable_missing_attribute_errors()) {
        result_trail = trail.Step(&field_, frame->arena());
      }

      if (frame->enable_missing_attribute_errors() &&
          frame->attribute_utility().CheckForMissingAttribute(result_trail)) {
        CelValue error_value =
            CreateMissingAttributeError(frame->arena(), select_path_);
        frame->value_stack().PopAndPush(error_value, result_trail);
        return absl::OkStatus();
      }

      if (frame->enable_unknowns() &&
          frame->attribute_utility().CheckForUnknown(result_trail,
                                                     /*use_partial=*/false)) {
        auto unknown_set = google::protobuf::Arena::Create<UnknownSet>(
            frame->arena(), UnknownAttributeSet({result_trail.attribute()}));
        result = CelValue::CreateUnknownSet(unknown_set);
        frame->value_stack().PopAndPush(result, result_trail);
        return absl::OkStatus();
      }

      if (msg == nullptr) {
        CelValue error_value =
            CreateErrorValue(frame->arena(), "Message is NULL");
        frame->value_stack().PopAndPush(error_value, result_trail);
        return absl::OkStatus();
      }

      if (unknown_value) {
        CelValue error_value =
            CreateUnknownValueError(frame->arena(), select_path_);
        frame->value_stack().PopAndPush(error_value, result_trail);
        return absl::OkStatus();
      }

      absl::Status status = CreateValueFromField(msg, frame->arena(), &result);
      if (status.ok()) {
        frame->value_stack().PopAndPush(result, result_trail);
      }

      return status;
    }
    case CelValue::Type::kMap: {
      const CelMap* cel_map = arg.MapOrDie();

      if (cel_map == nullptr) {
        CelValue error_value = CreateErrorValue(frame->arena(), "Map is NULL");
        frame->value_stack().PopAndPush(error_value);
        return absl::OkStatus();
      }

      if (unknown_value) {
        CelValue error_value = CreateErrorValue(
            frame->arena(), absl::StrCat("Unknown value ", select_path_));
        frame->value_stack().PopAndPush(error_value);
        return absl::OkStatus();
      }

      CelValue field_name = CelValue::CreateString(&field_);
      if (test_field_presence_) {
        // Field presence only supports string keys containing valid identifier
        // characters.
        auto presence = cel_map->Has(field_name);
        if (!presence.ok()) {
          CelValue error_value =
              CreateErrorValue(frame->arena(), presence.status());
          frame->value_stack().PopAndPush(error_value);
          return absl::OkStatus();
        }
        result = CelValue::CreateBool(*presence);
        frame->value_stack().PopAndPush(result);
        return absl::OkStatus();
      }

      auto lookup_result = (*cel_map)[field_name];
      if (frame->enable_unknowns()) {
        result_trail = trail.Step(&field_, frame->arena());
        if (frame->attribute_utility().CheckForUnknown(result_trail, false)) {
          auto unknown_set = google::protobuf::Arena::Create<UnknownSet>(
              frame->arena(), UnknownAttributeSet({result_trail.attribute()}));
          result = CelValue::CreateUnknownSet(unknown_set);
          frame->value_stack().PopAndPush(result, result_trail);
          return absl::OkStatus();
        }
      }

      // If object is not found, we return Error, per CEL specification.
      if (lookup_result) {
        result = lookup_result.value();
      } else {
        result = CreateNoSuchKeyError(frame->arena(), field_);
      }
      frame->value_stack().PopAndPush(result, result_trail);
      return absl::OkStatus();
    }
    case CelValue::Type::kUnknownSet: {
      // Parent is unknown already, bubble it up.
      return absl::OkStatus();
    }
    case CelValue::Type::kError: {
      // If argument is CelError, we propagate it forward.
      // It is already on the top of the stack.
      return absl::OkStatus();
    }
    default:
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Applying SELECT to non-message type");
  }
}

}  // namespace

// Factory method for Select - based Execution step
absl::StatusOr<std::unique_ptr<ExpressionStep>> CreateSelectStep(
    const google::api::expr::v1alpha1::Expr::Select* select_expr, int64_t expr_id,
    absl::string_view select_path) {
  return absl::make_unique<SelectStep>(
      select_expr->field(), select_expr->test_only(), expr_id, select_path);
}

}  // namespace google::api::expr::runtime
