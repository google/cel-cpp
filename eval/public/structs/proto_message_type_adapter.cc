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

#include <string>

#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "eval/public/cel_value.h"
#include "eval/public/cel_value_internal.h"
#include "eval/public/containers/internal_field_backed_list_impl.h"
#include "eval/public/containers/internal_field_backed_map_impl.h"
#include "eval/public/structs/cel_proto_wrap_util.h"
#include "eval/public/structs/field_access_impl.h"
#include "eval/public/structs/legacy_type_adapter.h"
#include "eval/public/structs/legacy_type_info_apis.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/casts.h"
#include "internal/no_destructor.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {
namespace {

using ::cel::extensions::ProtoMemoryManager;
using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::Message;
using ::google::protobuf::Reflection;

const std::string& UnsupportedTypeName() {
  static cel::internal::NoDestructor<std::string> kUnsupportedTypeName(
      "<unknown message>");
  return *kUnsupportedTypeName;
}

CelValue MessageCelValueFactory(const google::protobuf::Message* message);

inline absl::StatusOr<const google::protobuf::Message*> UnwrapMessage(
    const CelValue::MessageWrapper& value, absl::string_view op) {
  if (!value.HasFullProto() || value.message_ptr() == nullptr) {
    return absl::InternalError(
        absl::StrCat(op, " called on non-message type."));
  }
  return cel::internal::down_cast<const google::protobuf::Message*>(value.message_ptr());
}

// Shared implementation for HasField.
// Handles list or map specific behavior before calling reflection helpers.
absl::StatusOr<bool> HasFieldImpl(const google::protobuf::Message* message,
                                  const google::protobuf::Descriptor* descriptor,
                                  absl::string_view field_name) {
  ABSL_ASSERT(descriptor == message->GetDescriptor());
  const Reflection* reflection = message->GetReflection();
  const FieldDescriptor* field_desc = descriptor->FindFieldByName(field_name.data());

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

// Shared implementation for GetField.
// Handles list or map specific behavior before calling reflection helpers.
absl::StatusOr<CelValue> GetFieldImpl(const google::protobuf::Message* message,
                                      const google::protobuf::Descriptor* descriptor,
                                      absl::string_view field_name,
                                      ProtoWrapperTypeOptions unboxing_option,
                                      cel::MemoryManager& memory_manager) {
  ABSL_ASSERT(descriptor == message->GetDescriptor());
  const FieldDescriptor* field_desc = descriptor->FindFieldByName(field_name.data());

  if (field_desc == nullptr) {
    return CreateNoSuchFieldError(memory_manager, field_name);
  }

  google::protobuf::Arena* arena = ProtoMemoryManager::CastToProtoArena(memory_manager);

  if (field_desc->is_map()) {
    auto map = memory_manager.New<internal::FieldBackedMapImpl>(
        message, field_desc, &MessageCelValueFactory, arena);

    return CelValue::CreateMap(map.release());
  }
  if (field_desc->is_repeated()) {
    auto list = memory_manager.New<internal::FieldBackedListImpl>(
        message, field_desc, &MessageCelValueFactory, arena);
    return CelValue::CreateList(list.release());
  }

  CEL_ASSIGN_OR_RETURN(
      CelValue result,
      internal::CreateValueFromSingleField(message, field_desc, unboxing_option,
                                           &MessageCelValueFactory, arena));
  return result;
}

class DucktypedMessageAdapter : public LegacyTypeAccessApis,
                                public LegacyTypeInfoApis {
 public:
  // Implement field access APIs.
  absl::StatusOr<bool> HasField(
      absl::string_view field_name,
      const CelValue::MessageWrapper& value) const override {
    CEL_ASSIGN_OR_RETURN(const google::protobuf::Message* message,
                         UnwrapMessage(value, "HasField"));
    return HasFieldImpl(message, message->GetDescriptor(), field_name);
  }

  absl::StatusOr<CelValue> GetField(
      absl::string_view field_name, const CelValue::MessageWrapper& instance,
      ProtoWrapperTypeOptions unboxing_option,
      cel::MemoryManager& memory_manager) const override {
    CEL_ASSIGN_OR_RETURN(const google::protobuf::Message* message,
                         UnwrapMessage(instance, "GetField"));
    return GetFieldImpl(message, message->GetDescriptor(), field_name,
                        unboxing_option, memory_manager);
  }

  // Implement TypeInfo Apis
  const std::string& GetTypename(
      const internal::MessageWrapper& wrapped_message) const override {
    if (!wrapped_message.HasFullProto() ||
        wrapped_message.message_ptr() == nullptr) {
      return UnsupportedTypeName();
    }
    auto* message = cel::internal::down_cast<const google::protobuf::Message*>(
        wrapped_message.message_ptr());
    return message->GetDescriptor()->full_name();
  }

  std::string DebugString(
      const internal::MessageWrapper& wrapped_message) const override {
    if (!wrapped_message.HasFullProto() ||
        wrapped_message.message_ptr() == nullptr) {
      return UnsupportedTypeName();
    }
    auto* message = cel::internal::down_cast<const google::protobuf::Message*>(
        wrapped_message.message_ptr());
    return message->DebugString();
  }

  const LegacyTypeAccessApis* GetAccessApis(
      const internal::MessageWrapper& wrapped_message) const override {
    return this;
  }

  static DucktypedMessageAdapter& GetSingleton() {
    static cel::internal::NoDestructor<DucktypedMessageAdapter> instance;
    return *instance;
  }
};

CelValue MessageCelValueFactory(const google::protobuf::Message* message) {
  return CelValue::CreateMessageWrapper(internal::MessageWrapper(
      message, &DucktypedMessageAdapter::GetSingleton()));
}

}  // namespace

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
  return CelValue::MessageWrapper(msg, &GetGenericProtoTypeInfoInstance());
}

bool ProtoMessageTypeAdapter::DefinesField(absl::string_view field_name) const {
  return descriptor_->FindFieldByName(field_name.data()) != nullptr;
}

absl::StatusOr<bool> ProtoMessageTypeAdapter::HasField(
    absl::string_view field_name, const CelValue::MessageWrapper& value) const {
  CEL_ASSIGN_OR_RETURN(const google::protobuf::Message* message,
                       UnwrapMessage(value, "HasField"));
  return HasFieldImpl(message, descriptor_, field_name);
}

absl::StatusOr<CelValue> ProtoMessageTypeAdapter::GetField(
    absl::string_view field_name, const CelValue::MessageWrapper& instance,
    ProtoWrapperTypeOptions unboxing_option,
    cel::MemoryManager& memory_manager) const {
  CEL_ASSIGN_OR_RETURN(const google::protobuf::Message* message,
                       UnwrapMessage(instance, "GetField"));

  return GetFieldImpl(message, descriptor_, field_name, unboxing_option,
                      memory_manager);
}

absl::Status ProtoMessageTypeAdapter::SetField(
    absl::string_view field_name, const CelValue& value,
    cel::MemoryManager& memory_manager,
    CelValue::MessageWrapper& instance) const {
  // Assume proto arena implementation if this provider is used.
  google::protobuf::Arena* arena =
      cel::extensions::ProtoMemoryManager::CastToProtoArena(memory_manager);

  CEL_ASSIGN_OR_RETURN(const google::protobuf::Message* message,
                       UnwrapMessage(instance, "SetField"));

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
      CEL_RETURN_IF_ERROR(internal::SetValueToSingleField(
          key, key_field_descriptor, entry_msg, arena));
      CEL_RETURN_IF_ERROR(internal::SetValueToSingleField(
          value.value(), value_field_descriptor, entry_msg, arena));
    }

  } else if (field_descriptor->is_repeated()) {
    const CelList* cel_list;
    CEL_RETURN_IF_ERROR(ValidateSetFieldOp(
        value.GetValue<const CelList*>(&cel_list) && cel_list != nullptr,
        field_name, "expected CelList value"));

    for (int i = 0; i < cel_list->size(); i++) {
      CEL_RETURN_IF_ERROR(internal::AddValueToRepeatedField(
          (*cel_list)[i], field_descriptor, mutable_message, arena));
    }
  } else {
    CEL_RETURN_IF_ERROR(internal::SetValueToSingleField(
        value, field_descriptor, mutable_message, arena));
  }
  return absl::OkStatus();
}

absl::StatusOr<CelValue> ProtoMessageTypeAdapter::AdaptFromWellKnownType(
    cel::MemoryManager& memory_manager,
    CelValue::MessageWrapper instance) const {
  // Assume proto arena implementation if this provider is used.
  google::protobuf::Arena* arena =
      cel::extensions::ProtoMemoryManager::CastToProtoArena(memory_manager);
  CEL_ASSIGN_OR_RETURN(const google::protobuf::Message* message,
                       UnwrapMessage(instance, "AdaptFromWellKnownType"));
  return internal::UnwrapMessageToValue(message, &MessageCelValueFactory,
                                        arena);
}

const LegacyTypeInfoApis& GetGenericProtoTypeInfoInstance() {
  return DucktypedMessageAdapter::GetSingleton();
}

}  // namespace google::api::expr::runtime
