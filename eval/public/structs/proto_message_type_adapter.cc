// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "eval/public/structs/proto_message_type_adapter.h"

#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "eval/public/cel_value.h"
#include "eval/public/cel_value_internal.h"
#include "eval/public/containers/field_access.h"
#include "eval/public/containers/field_backed_list_impl.h"
#include "eval/public/containers/field_backed_map_impl.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/casts.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {
using ::cel::extensions::ProtoMemoryManager;
using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::Message;
using ::google::protobuf::Reflection;

absl::Status ProtoMessageTypeAdapter::ValidateSetFieldOp(
    bool assertion, absl::string_view field, absl::string_view detail) const {
  if (!assertion) {
    return absl::InvalidArgumentError(
        absl::Substitute("SetField failed on message $0, field '$1': $2",
                         descriptor_->full_name(), field, detail));
  }
  return absl::OkStatus();
}

absl::StatusOr<CelValue::MessageWrapper> ProtoMessageTypeAdapter::NewInstance(
    cel::MemoryManager& memory_manager) const {
  // This implementation requires arena-backed memory manager.
  google::protobuf::Arena* arena = ProtoMemoryManager::CastToProtoArena(memory_manager);
  const Message* prototype = message_factory_->GetPrototype(descriptor_);

  Message* msg = (prototype != nullptr) ? prototype->New(arena) : nullptr;

  if (msg == nullptr) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to create message ", descriptor_->name()));
  }
  return CelValue::MessageWrapper(msg);
}

bool ProtoMessageTypeAdapter::DefinesField(absl::string_view field_name) const {
  return descriptor_->FindFieldByName(field_name.data()) != nullptr;
}

absl::StatusOr<bool> ProtoMessageTypeAdapter::HasField(
    absl::string_view field_name, const CelValue::MessageWrapper& value) const {
  if (!value.HasFullProto() || value.message_ptr() == nullptr) {
    return absl::InvalidArgumentError("GetField called on non-message type.");
  }
  const google::protobuf::Message* message =
      cel::internal::down_cast<const google::protobuf::Message*>(value.message_ptr());

  const Reflection* reflection = message->GetReflection();
  ABSL_ASSERT(descriptor_ == message->GetDescriptor());

  const FieldDescriptor* field_desc = descriptor_->FindFieldByName(field_name.data());

  if (field_desc == nullptr) {
    return absl::NotFoundError(absl::StrCat("no_such_field : ", field_name));
  }

  if (field_desc->is_map()) {
    // When the map field appears in a has(msg.map_field) expression, the map
    // is considered 'present' when it is non-empty. Since maps are repeated
    // fields they don't participate with standard proto presence testing since
    // the repeated field is always at least empty.
    return reflection->FieldSize(*message, field_desc) != 0;
  }

  if (field_desc->is_repeated()) {
    // When the list field appears in a has(msg.list_field) expression, the list
    // is considered 'present' when it is non-empty.
    return reflection->FieldSize(*message, field_desc) != 0;
  }

  // Standard proto presence test for non-repeated fields.
  return reflection->HasField(*message, field_desc);
}

absl::StatusOr<CelValue> ProtoMessageTypeAdapter::GetField(
    absl::string_view field_name, const CelValue::MessageWrapper& instance,
    cel::MemoryManager& memory_manager) const {
  if (!instance.HasFullProto() || instance.message_ptr() == nullptr) {
    return absl::InvalidArgumentError("GetField called on non-message type.");
  }
  const google::protobuf::Message* message =
      cel::internal::down_cast<const google::protobuf::Message*>(instance.message_ptr());
  const FieldDescriptor* field_desc = descriptor_->FindFieldByName(field_name.data());

  if (field_desc == nullptr) {
    return CreateNoSuchFieldError(memory_manager, field_name);
  }

  google::protobuf::Arena* arena = ProtoMemoryManager::CastToProtoArena(memory_manager);

  if (field_desc->is_map()) {
    CelMap* map = google::protobuf::Arena::Create<FieldBackedMapImpl>(arena, message,
                                                            field_desc, arena);
    return CelValue::CreateMap(map);
  }
  if (field_desc->is_repeated()) {
    CelList* list = google::protobuf::Arena::Create<FieldBackedListImpl>(
        arena, message, field_desc, arena);
    return CelValue::CreateList(list);
  }

  CelValue result;
  CEL_RETURN_IF_ERROR(CreateValueFromSingleField(
      message, field_desc, unboxing_option_, arena, &result));
  return result;
}

absl::Status ProtoMessageTypeAdapter::SetField(
    absl::string_view field_name, const CelValue& value,
    cel::MemoryManager& memory_manager,
    CelValue::MessageWrapper& instance) const {
  // Assume proto arena implementation if this provider is used.
  google::protobuf::Arena* arena =
      cel::extensions::ProtoMemoryManager::CastToProtoArena(memory_manager);

  if (!instance.HasFullProto() || instance.message_ptr() == nullptr) {
    return absl::InternalError("SetField called on non-message type.");
  }

  const google::protobuf::Message* message =
      cel::internal::down_cast<const google::protobuf::Message*>(instance.message_ptr());

  // Interpreter guarantees this is the top-level instance.
  google::protobuf::Message* mutable_message = const_cast<Message*>(message);

  const google::protobuf::FieldDescriptor* field_descriptor =
      descriptor_->FindFieldByName(field_name.data());
  CEL_RETURN_IF_ERROR(
      ValidateSetFieldOp(field_descriptor != nullptr, field_name, "not found"));

  if (field_descriptor->is_map()) {
    constexpr int kKeyField = 1;
    constexpr int kValueField = 2;

    const CelMap* cel_map;
    CEL_RETURN_IF_ERROR(ValidateSetFieldOp(
        value.GetValue<const CelMap*>(&cel_map) && cel_map != nullptr,
        field_name, "value is not CelMap"));

    auto entry_descriptor = field_descriptor->message_type();

    CEL_RETURN_IF_ERROR(
        ValidateSetFieldOp(entry_descriptor != nullptr, field_name,
                           "failed to find map entry descriptor"));
    auto key_field_descriptor = entry_descriptor->FindFieldByNumber(kKeyField);
    auto value_field_descriptor =
        entry_descriptor->FindFieldByNumber(kValueField);

    CEL_RETURN_IF_ERROR(
        ValidateSetFieldOp(key_field_descriptor != nullptr, field_name,
                           "failed to find key field descriptor"));

    CEL_RETURN_IF_ERROR(
        ValidateSetFieldOp(value_field_descriptor != nullptr, field_name,
                           "failed to find value field descriptor"));

    const CelList* key_list = cel_map->ListKeys();
    for (int i = 0; i < key_list->size(); i++) {
      CelValue key = (*key_list)[i];

      auto value = (*cel_map)[key];
      CEL_RETURN_IF_ERROR(ValidateSetFieldOp(value.has_value(), field_name,
                                             "error serializing CelMap"));
      Message* entry_msg = mutable_message->GetReflection()->AddMessage(
          mutable_message, field_descriptor);
      CEL_RETURN_IF_ERROR(
          SetValueToSingleField(key, key_field_descriptor, entry_msg, arena));
      CEL_RETURN_IF_ERROR(SetValueToSingleField(
          value.value(), value_field_descriptor, entry_msg, arena));
    }

  } else if (field_descriptor->is_repeated()) {
    const CelList* cel_list;
    CEL_RETURN_IF_ERROR(ValidateSetFieldOp(
        value.GetValue<const CelList*>(&cel_list) && cel_list != nullptr,
        field_name, "expected CelList value"));

    for (int i = 0; i < cel_list->size(); i++) {
      CEL_RETURN_IF_ERROR(AddValueToRepeatedField(
          (*cel_list)[i], field_descriptor, mutable_message, arena));
    }
  } else {
    CEL_RETURN_IF_ERROR(
        SetValueToSingleField(value, field_descriptor, mutable_message, arena));
  }
  return absl::OkStatus();
}

absl::StatusOr<CelValue> ProtoMessageTypeAdapter::AdaptFromWellKnownType(
    cel::MemoryManager& memory_manager,
    CelValue::MessageWrapper instance) const {
  // Assume proto arena implementation if this provider is used.
  google::protobuf::Arena* arena =
      cel::extensions::ProtoMemoryManager::CastToProtoArena(memory_manager);
  if (!instance.HasFullProto() || instance.message_ptr() == nullptr) {
    return absl::InternalError(
        "Adapt from well-known type failed: not a message");
  }
  auto* message =
      cel::internal::down_cast<const google::protobuf::Message*>(instance.message_ptr());
  return CelProtoWrapper::CreateMessage(message, arena);
}

}  // namespace google::api::expr::runtime
