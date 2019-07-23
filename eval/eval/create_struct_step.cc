#include "eval/eval/create_struct_step.h"

#include "eval/eval/container_backed_map_impl.h"
#include "eval/eval/field_access.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/strings/substitute.h"
#include "base/canonical_errors.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using ::google::protobuf::Message;
using ::google::protobuf::MessageFactory;
using ::google::protobuf::Arena;
using ::google::protobuf::Descriptor;
using ::google::protobuf::DescriptorPool;
using ::google::protobuf::FieldDescriptor;

class CreateStructStepForMessage : public ExpressionStepBase {
 public:
  struct FieldEntry {
    const FieldDescriptor* field;
  };

  CreateStructStepForMessage(int64_t expr_id, const Descriptor* descriptor,
                             std::vector<FieldEntry> entries)
      : ExpressionStepBase(expr_id),
        descriptor_(descriptor),
        entries_(std::move(entries)) {}

  cel_base::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  cel_base::Status DoEvaluate(ExecutionFrame* frame, CelValue* result) const;

  const Descriptor* descriptor_;
  std::vector<FieldEntry> entries_;
};

class CreateStructStepForMap : public ExpressionStepBase {
 public:
  CreateStructStepForMap(int64_t expr_id, int entry_count)
      : ExpressionStepBase(expr_id), entry_count_(entry_count) {}

  cel_base::Status Evaluate(ExecutionFrame* frame) const override;

 private:
  cel_base::Status DoEvaluate(ExecutionFrame* frame, CelValue* result) const;

  int entry_count_;
};

::cel_base::Status CreateStructStepForMessage::DoEvaluate(ExecutionFrame* frame,
                                                      CelValue* result) const {
  int entries_size = entries_.size();

  absl::Span<const CelValue> args = frame->value_stack().GetSpan(entries_size);

  const Message* prototype =
      MessageFactory::generated_factory()->GetPrototype(descriptor_);

  Message* msg =
      (prototype != nullptr) ? prototype->New(frame->arena()) : nullptr;

  if (msg == nullptr) {
    *result = CreateErrorValue(
        frame->arena(),
        absl::Substitute("Failed to create message $0", descriptor_->name()));
    return ::cel_base::OkStatus();
  }

  int index = 0;
  for (const auto& entry : entries_) {
    const CelValue& arg = args[index++];

    ::cel_base::Status status = ::cel_base::OkStatus();

    if (entry.field->is_map()) {
      constexpr int kKeyField = 1;
      constexpr int kValueField = 2;

      const CelMap* cel_map;
      if (!arg.GetValue<const CelMap*>(&cel_map) || cel_map == nullptr) {
        status = cel_base::InvalidArgumentError(absl::Substitute(
            "Failed to create message $0, field $1: value is not CelMap",
            descriptor_->name(), entry.field->name()));
        break;
      }

      auto entry_descriptor = entry.field->message_type();

      if (entry_descriptor == nullptr) {
        status = cel_base::InvalidArgumentError(
            absl::Substitute("Failed to create message $0, field $1: failed to "
                             "find map entry descriptor",
                             descriptor_->name(), entry.field->name()));
        break;
      }

      auto key_field_descriptor =
          entry_descriptor->FindFieldByNumber(kKeyField);
      auto value_field_descriptor =
          entry_descriptor->FindFieldByNumber(kValueField);

      if (key_field_descriptor == nullptr) {
        status = cel_base::InvalidArgumentError(
            absl::Substitute("Failed to create message $0, field $1: failed to "
                             "find key field descriptor",
                             descriptor_->name(), entry.field->name()));
        break;
      }
      if (value_field_descriptor == nullptr) {
        status = cel_base::InvalidArgumentError(
            absl::Substitute("Failed to create message $0, field $1: failed to "
                             "find value field descriptor",
                             descriptor_->name(), entry.field->name()));
        break;
      }

      const CelList* key_list = cel_map->ListKeys();
      for (int i = 0; i < key_list->size(); i++) {
        CelValue key = (*key_list)[i];

        auto value = (*cel_map)[key];
        if (!value.has_value()) {
          status = cel_base::InvalidArgumentError(absl::Substitute(
              "Failed to create message $0, field $1: Error serializing CelMap",
              descriptor_->name(), entry.field->name()));
          break;
        }

        Message* entry_msg = msg->GetReflection()->AddMessage(msg, entry.field);
        status = SetValueToSingleField(key, key_field_descriptor, entry_msg);
        if (!status.ok()) {
          break;
        }
        status = SetValueToSingleField(value.value(), value_field_descriptor,
                                       entry_msg);
        if (!status.ok()) {
          break;
        }
      }

    } else if (entry.field->is_repeated()) {
      const CelList* cel_list;
      if (!arg.GetValue<const CelList*>(&cel_list) || cel_list == nullptr) {
        *result = CreateErrorValue(
            frame->arena(),
            absl::Substitute(
                "Failed to create message $0: value $1 is not CelList",
                descriptor_->name(), entry.field->name()));
        return ::cel_base::OkStatus();
      }

      for (int i = 0; i < cel_list->size(); i++) {
        status = AddValueToRepeatedField((*cel_list)[i], entry.field, msg);
        if (!status.ok()) break;
      }
    } else {
      status = SetValueToSingleField(arg, entry.field, msg);
    }

    if (!status.ok()) {
      *result = CreateErrorValue(
          frame->arena(),
          absl::Substitute("Failed to create message $0: reason $1",
                           descriptor_->name(), status.ToString()));
      return ::cel_base::OkStatus();
    }
  }

  *result = CelValue::CreateMessage(msg, frame->arena());

  return ::cel_base::OkStatus();
}

::cel_base::Status CreateStructStepForMessage::Evaluate(
    ExecutionFrame* frame) const {
  if (frame->value_stack().size() < entries_.size()) {
    return cel_base::Status(cel_base::StatusCode::kInternal,
                        "CreateStructStepForMessage: stack undeflow");
  }

  CelValue result;

  ::cel_base::Status status = DoEvaluate(frame, &result);
  if (!status.ok()) {
    return status;
  }

  frame->value_stack().Pop(entries_.size());
  frame->value_stack().Push(result);

  return cel_base::OkStatus();
}

::cel_base::Status CreateStructStepForMap::DoEvaluate(ExecutionFrame* frame,
                                                  CelValue* result) const {
  absl::Span<const CelValue> args =
      frame->value_stack().GetSpan(2 * entry_count_);

  std::vector<std::pair<CelValue, CelValue>> map_entries;
  map_entries.reserve(entry_count_);
  for (int i = 0; i < entry_count_; i += 1) {
    map_entries.push_back({args[2 * i], args[2 * i + 1]});
  }

  auto cel_map =
      CreateContainerBackedMap(absl::Span<std::pair<CelValue, CelValue>>(
          map_entries.data(), map_entries.size()));

  if (cel_map == nullptr) {
    *result = CreateErrorValue(frame->arena(), "Failed to create map");

    return ::cel_base::OkStatus();
  }

  *result = CelValue::CreateMap(cel_map.get());

  // Pass object ownership to Arena.
  frame->arena()->Own(cel_map.release());

  return ::cel_base::OkStatus();
}

::cel_base::Status CreateStructStepForMap::Evaluate(ExecutionFrame* frame) const {
  if (frame->value_stack().size() < 2 * entry_count_) {
    return cel_base::Status(cel_base::StatusCode::kInternal,
                        "CreateStructStepForMap: stack undeflow");
  }

  CelValue result;

  ::cel_base::Status status = DoEvaluate(frame, &result);
  if (!status.ok()) {
    return status;
  }

  frame->value_stack().Pop(2 * entry_count_);
  frame->value_stack().Push(result);

  return cel_base::OkStatus();
}

}  // namespace

cel_base::StatusOr<std::unique_ptr<ExpressionStep>> CreateCreateStructStep(
    const google::api::expr::v1alpha1::Expr::CreateStruct* create_struct_expr,
    int64_t expr_id) {
  if (!create_struct_expr->message_name().empty()) {
    // Make message-creating step.
    std::vector<CreateStructStepForMessage::FieldEntry> entries;

    const Descriptor* desc =
        DescriptorPool::generated_pool()->FindMessageTypeByName(
            create_struct_expr->message_name());

    if (desc == nullptr) {
      return cel_base::InvalidArgumentError(
          "Error configuring message creation: message descriptor not found");
    }

    for (const auto& entry : create_struct_expr->entries()) {
      if (entry.field_key().empty()) {
        return cel_base::InvalidArgumentError(
            "Error configuring message creation: field name missing");
      }

      const FieldDescriptor* field_desc =
          desc->FindFieldByName(entry.field_key());
      if (field_desc == nullptr) {
        return cel_base::InvalidArgumentError(
            "Error configuring message creation: field name not found");
      }
      entries.push_back({field_desc});
    }

    return absl::WrapUnique<ExpressionStep>(
        new CreateStructStepForMessage(expr_id, desc, std::move(entries)));
  } else {
    // Make map-creating step.
    return absl::WrapUnique<ExpressionStep>(new CreateStructStepForMap(
        expr_id, create_struct_expr->entries_size()));
  }
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
