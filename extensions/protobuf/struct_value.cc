// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "extensions/protobuf/struct_value.h"

#include <limits>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/descriptor.pb.h"
#include "absl/base/optimization.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "base/value_factory.h"
#include "base/values/bool_value.h"
#include "base/values/bytes_value.h"
#include "base/values/double_value.h"
#include "base/values/int_value.h"
#include "base/values/list_value.h"
#include "base/values/map_value.h"
#include "base/values/string_value.h"
#include "base/values/uint_value.h"
#include "eval/internal/errors.h"
#include "eval/internal/interop.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/repeated_ptr_field.h"

namespace cel::extensions {

namespace proto_internal {

namespace {

class HeapDynamicParsedProtoStructValue final
    : public DynamicParsedProtoStructValue {
 public:
  HeapDynamicParsedProtoStructValue(Handle<StructType> type,
                                    const google::protobuf::Message* value)
      : DynamicParsedProtoStructValue(std::move(type), value) {
    ABSL_ASSERT(value->GetArena() == nullptr);
  }

  ~HeapDynamicParsedProtoStructValue() override { delete value_ptr(); }
};

class DynamicMemberParsedProtoStructValue : public ParsedProtoStructValue {
 public:
  static absl::StatusOr<Handle<ProtoStructValue>> Create(
      ValueFactory& value_factory, Handle<StructType> type, const Value* parent,
      const google::protobuf::Message* value);

  const google::protobuf::Message& value() const final { return *value_; }

 protected:
  DynamicMemberParsedProtoStructValue(Handle<StructType> type,
                                      const google::protobuf::Message* value)
      : ParsedProtoStructValue(std::move(type)),
        value_(ABSL_DIE_IF_NULL(value)) {}  // Crash OK

  absl::optional<const google::protobuf::Message*> ValueReference(
      google::protobuf::Message& scratch, const google::protobuf::Descriptor& desc,
      internal::TypeInfo type) const final {
    if (ABSL_PREDICT_FALSE(&desc != scratch.GetDescriptor())) {
      return absl::nullopt;
    }
    return &value();
  }

 private:
  const google::protobuf::Message* const value_;
};

class ArenaDynamicMemberParsedProtoStructValue final
    : public DynamicMemberParsedProtoStructValue {
 public:
  ArenaDynamicMemberParsedProtoStructValue(Handle<StructType> type,
                                           const google::protobuf::Message* value)
      : DynamicMemberParsedProtoStructValue(std::move(type), value) {}
};

class ReffedDynamicMemberParsedProtoStructValue final
    : public DynamicMemberParsedProtoStructValue {
 public:
  ReffedDynamicMemberParsedProtoStructValue(Handle<StructType> type,
                                            const Value* parent,
                                            const google::protobuf::Message* value)
      : DynamicMemberParsedProtoStructValue(std::move(type), value),
        parent_(parent) {
    base_internal::Metadata::Ref(*parent_);
  }

  ~ReffedDynamicMemberParsedProtoStructValue() override {
    base_internal::ValueMetadata::Unref(*parent_);
  }

 private:
  const Value* const parent_;
};

absl::StatusOr<Handle<ProtoStructValue>>
DynamicMemberParsedProtoStructValue::Create(ValueFactory& value_factory,
                                            Handle<StructType> type,
                                            const Value* parent,
                                            const google::protobuf::Message* value) {
  if (parent != nullptr &&
      base_internal::Metadata::IsReferenceCounted(*parent)) {
    return value_factory
        .CreateStructValue<ReffedDynamicMemberParsedProtoStructValue>(
            std::move(type), parent, value);
  }
  return value_factory
      .CreateStructValue<ArenaDynamicMemberParsedProtoStructValue>(
          std::move(type), value);
}

}  // namespace

}  // namespace proto_internal

std::unique_ptr<google::protobuf::Message> ProtoStructValue::value(
    google::protobuf::MessageFactory& message_factory) const {
  return absl::WrapUnique(ValuePointer(message_factory, nullptr));
}

std::unique_ptr<google::protobuf::Message> ProtoStructValue::value() const {
  return absl::WrapUnique(
      ValuePointer(*ABSL_DIE_IF_NULL(  // Crash OK
                       google::protobuf::MessageFactory::generated_factory()),
                   nullptr));
}

google::protobuf::Message* ProtoStructValue::value(
    google::protobuf::Arena& arena, google::protobuf::MessageFactory& message_factory) const {
  return ValuePointer(message_factory, &arena);
}

google::protobuf::Message* ProtoStructValue::value(google::protobuf::Arena& arena) const {
  return ValuePointer(*ABSL_DIE_IF_NULL(  // Crash OK
                          google::protobuf::MessageFactory::generated_factory()),
                      &arena);
}

namespace {

void ProtoDebugStringSingular(std::string& out, const google::protobuf::Message& message,
                              const google::protobuf::Reflection* reflect,
                              const google::protobuf::FieldDescriptor* field_desc) {
  switch (field_desc->type()) {
    case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
      out.append(
          DoubleValue::DebugString(reflect->GetDouble(message, field_desc)));
      break;
    case google::protobuf::FieldDescriptor::TYPE_FLOAT:
      out.append(
          DoubleValue::DebugString(reflect->GetFloat(message, field_desc)));
      break;
    case google::protobuf::FieldDescriptor::TYPE_INT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT64:
      out.append(IntValue::DebugString(reflect->GetInt64(message, field_desc)));
      break;
    case google::protobuf::FieldDescriptor::TYPE_INT32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT32:
      out.append(IntValue::DebugString(reflect->GetInt32(message, field_desc)));
      break;
    case google::protobuf::FieldDescriptor::TYPE_UINT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_FIXED64:
      out.append(
          UintValue::DebugString(reflect->GetUInt64(message, field_desc)));
      break;
    case google::protobuf::FieldDescriptor::TYPE_FIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_UINT32:
      out.append(
          UintValue::DebugString(reflect->GetUInt32(message, field_desc)));
      break;
    case google::protobuf::FieldDescriptor::TYPE_BOOL:
      out.append(BoolValue::DebugString(reflect->GetBool(message, field_desc)));
      break;
    case google::protobuf::FieldDescriptor::TYPE_STRING: {
      std::string scratch;
      out.append(StringValue::DebugString(
          reflect->GetStringReference(message, field_desc, &scratch)));
    } break;
    case google::protobuf::FieldDescriptor::TYPE_GROUP:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
      out.append(ProtoStructValue::DebugString(
          reflect->GetMessage(message, field_desc)));
      break;
    case google::protobuf::FieldDescriptor::TYPE_BYTES: {
      std::string scratch;
      out.append(BytesValue::DebugString(
          reflect->GetStringReference(message, field_desc, &scratch)));
    } break;
    case google::protobuf::FieldDescriptor::TYPE_ENUM: {
      const auto* desc = reflect->GetEnum(message, field_desc);
      out.append(field_desc->enum_type()->full_name());
      if (ABSL_PREDICT_TRUE(desc != nullptr)) {
        out.push_back('.');
        out.append(desc->name());
      } else {
        // Fallback when the number doesn't have a corresponding symbol,
        // potentially due to having a different version of the descriptor.
        out.push_back('(');
        out.append(
            IntValue::DebugString(reflect->GetEnumValue(message, field_desc)));
        out.push_back(')');
      }
    } break;
  }
}

void ProtoDebugStringMap(std::string& out, const google::protobuf::Message& message,
                         const google::protobuf::Reflection* reflect,
                         const google::protobuf::FieldDescriptor* field_desc) {
  out.append("**unimplemented**");
  static_cast<void>(message);
  static_cast<void>(reflect);
  static_cast<void>(field_desc);
}

void ProtoDebugStringRepeated(std::string& out, const google::protobuf::Message& message,
                              const google::protobuf::Reflection* reflect,
                              const google::protobuf::FieldDescriptor* field_desc) {
  out.append("**unimplemented**");
  static_cast<void>(message);
  static_cast<void>(reflect);
  static_cast<void>(field_desc);
}

void ProtoDebugString(std::string& out, const google::protobuf::Message& message,
                      const google::protobuf::Reflection* reflect,
                      const google::protobuf::FieldDescriptor* field_desc) {
  if (field_desc->is_map()) {
    ProtoDebugStringMap(out, message, reflect, field_desc);
    return;
  }
  if (field_desc->is_repeated()) {
    ProtoDebugStringRepeated(out, message, reflect, field_desc);
    return;
  }
  ProtoDebugStringSingular(out, message, reflect, field_desc);
}

}  // namespace

absl::StatusOr<Handle<ProtoStructValue>> ProtoStructValue::Create(
    ValueFactory& value_factory, const google::protobuf::Message& message) {
  const auto* descriptor = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(descriptor == nullptr)) {
    return absl::InvalidArgumentError("message missing descriptor");
  }
  CEL_ASSIGN_OR_RETURN(
      auto type,
      ProtoStructType::Resolve(value_factory.type_manager(), *descriptor));
  bool same_descriptors = &type->descriptor() == descriptor;
  if (ProtoMemoryManager::Is(value_factory.memory_manager())) {
    auto* arena =
        ProtoMemoryManager::CastToProtoArena(value_factory.memory_manager());
    if (ABSL_PREDICT_TRUE(arena != nullptr)) {
      google::protobuf::Message* value;
      if (ABSL_PREDICT_TRUE(same_descriptors)) {
        value = message.New(arena);
        value->CopyFrom(message);
      } else {
        const auto* prototype =
            type->factory_->GetPrototype(&type->descriptor());
        if (ABSL_PREDICT_FALSE(prototype == nullptr)) {
          return absl::InternalError(absl::StrCat(
              "cel: unable to get prototype for protocol buffer message \"",
              type->name(), "\""));
        }
        value = prototype->New(arena);
        std::string serialized;
        if (ABSL_PREDICT_FALSE(
                !message.SerializePartialToString(&serialized))) {
          return absl::InternalError(
              "cel: failed to serialize protocol buffer message");
        }
        if (ABSL_PREDICT_FALSE(!value->ParsePartialFromString(serialized))) {
          return absl::InternalError(
              "cel: failed to deserialize protocol buffer message");
        }
      }
      return value_factory.CreateStructValue<
          proto_internal::ArenaDynamicParsedProtoStructValue>(type, value);
    }
  }
  std::unique_ptr<google::protobuf::Message> value;
  if (ABSL_PREDICT_TRUE(same_descriptors)) {
    value = absl::WrapUnique(message.New());
    value->CopyFrom(message);
  } else {
    const auto* prototype = type->factory_->GetPrototype(&type->descriptor());
    if (ABSL_PREDICT_FALSE(prototype == nullptr)) {
      return absl::InternalError(absl::StrCat(
          "cel: unable to get prototype for protocol buffer message \"",
          type->name(), "\""));
    }
    value = absl::WrapUnique(prototype->New());
    std::string serialized;
    if (ABSL_PREDICT_FALSE(!message.SerializePartialToString(&serialized))) {
      return absl::InternalError(
          "cel: failed to serialize protocol buffer message");
    }
    if (ABSL_PREDICT_FALSE(!value->ParsePartialFromString(serialized))) {
      return absl::InternalError(
          "cel: failed to deserialize protocol buffer message");
    }
  }
  auto status_or_message =
      value_factory
          .CreateStructValue<proto_internal::HeapDynamicParsedProtoStructValue>(
              type, value.get());
  if (ABSL_PREDICT_FALSE(!status_or_message.ok())) {
    return status_or_message.status();
  }
  value.release();
  return std::move(status_or_message).value();
}

absl::StatusOr<Handle<ProtoStructValue>> ProtoStructValue::Create(
    ValueFactory& value_factory, google::protobuf::Message&& message) {
  const auto* descriptor = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(descriptor == nullptr)) {
    return absl::InvalidArgumentError("message missing descriptor");
  }
  CEL_ASSIGN_OR_RETURN(
      auto type,
      ProtoStructType::Resolve(value_factory.type_manager(), *descriptor));
  bool same_descriptors = &type->descriptor() == descriptor;
  if (ProtoMemoryManager::Is(value_factory.memory_manager())) {
    auto* arena =
        ProtoMemoryManager::CastToProtoArena(value_factory.memory_manager());
    if (ABSL_PREDICT_TRUE(arena != nullptr)) {
      google::protobuf::Message* value;
      if (ABSL_PREDICT_TRUE(same_descriptors)) {
        value = message.New(arena);
        const auto* reflect = message.GetReflection();
        if (ABSL_PREDICT_TRUE(reflect != nullptr)) {
          reflect->Swap(&message, value);
        } else {
          // Fallback to copy.
          value->CopyFrom(message);
        }
      } else {
        const auto* prototype =
            type->factory_->GetPrototype(&type->descriptor());
        if (ABSL_PREDICT_FALSE(prototype == nullptr)) {
          return absl::InternalError(absl::StrCat(
              "cel: unable to get prototype for protocol buffer message \"",
              type->name(), "\""));
        }
        value = prototype->New(arena);
        std::string serialized;
        if (ABSL_PREDICT_FALSE(
                !message.SerializePartialToString(&serialized))) {
          return absl::InternalError(
              "cel: failed to serialize protocol buffer message");
        }
        if (ABSL_PREDICT_FALSE(!value->ParsePartialFromString(serialized))) {
          return absl::InternalError(
              "cel: failed to deserialize protocol buffer message");
        }
      }
      return value_factory.CreateStructValue<
          proto_internal::ArenaDynamicParsedProtoStructValue>(type, value);
    }
  }
  std::unique_ptr<google::protobuf::Message> value;
  if (ABSL_PREDICT_TRUE(same_descriptors)) {
    value = absl::WrapUnique(message.New());
    const auto* reflect = message.GetReflection();
    if (ABSL_PREDICT_TRUE(reflect != nullptr)) {
      reflect->Swap(&message, value.get());
    } else {
      // Fallback to copy.
      value->CopyFrom(message);
    }
  } else {
    const auto* prototype = type->factory_->GetPrototype(&type->descriptor());
    if (ABSL_PREDICT_FALSE(prototype == nullptr)) {
      return absl::InternalError(absl::StrCat(
          "cel: unable to get prototype for protocol buffer message \"",
          type->name(), "\""));
    }
    value = absl::WrapUnique(prototype->New());
    std::string serialized;
    if (ABSL_PREDICT_FALSE(!message.SerializePartialToString(&serialized))) {
      return absl::InternalError(
          "cel: failed to serialize protocol buffer message");
    }
    if (ABSL_PREDICT_FALSE(!value->ParsePartialFromString(serialized))) {
      return absl::InternalError(
          "cel: failed to deserialize protocol buffer message");
    }
  }
  auto status_or_message =
      value_factory
          .CreateStructValue<proto_internal::HeapDynamicParsedProtoStructValue>(
              type, value.get());
  if (ABSL_PREDICT_FALSE(!status_or_message.ok())) {
    return status_or_message.status();
  }
  value.release();
  return std::move(status_or_message).value();
}

std::string ProtoStructValue::DebugString(const google::protobuf::Message& message) {
  std::string out;
  out.append(message.GetTypeName());
  out.push_back('{');
  const auto* reflect = message.GetReflection();
  if (reflect != nullptr) {
    std::vector<const google::protobuf::FieldDescriptor*> field_descs;
    reflect->ListFields(message, &field_descs);
    auto field_desc = field_descs.begin();
    if (field_desc != field_descs.end()) {
      out.append((*field_desc)->name());
      out.append(": ");
      ProtoDebugString(out, message, reflect, *field_desc);
      ++field_desc;
      for (; field_desc != field_descs.end(); ++field_desc) {
        out.append(", ");
        out.append((*field_desc)->name());
        out.append(": ");
        ProtoDebugString(out, message, reflect, *field_desc);
      }
    }
  }
  out.push_back('}');
  return out;
}

namespace proto_internal {

std::string ParsedProtoStructValue::DebugString() const {
  return ProtoStructValue::DebugString(value());
}

google::protobuf::Message* ParsedProtoStructValue::ValuePointer(
    google::protobuf::MessageFactory& message_factory, google::protobuf::Arena* arena) const {
  const auto* desc = value().GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return nullptr;
  }
  const auto* prototype = message_factory.GetPrototype(desc);
  if (ABSL_PREDICT_FALSE(prototype == nullptr)) {
    return nullptr;
  }
  auto* message = prototype->New(arena);
  if (ABSL_PREDICT_FALSE(message == nullptr)) {
    return nullptr;
  }
  message->CopyFrom(value());
  return message;
}

absl::StatusOr<Handle<Value>> ParsedProtoStructValue::GetFieldByName(
    ValueFactory& value_factory, absl::string_view name) const {
  CEL_ASSIGN_OR_RETURN(
      auto field_type,
      type()->FindField(value_factory.type_manager(), FieldId(name)));
  if (ABSL_PREDICT_FALSE(!field_type)) {
    return interop_internal::CreateNoSuchFieldError(name);
  }
  return GetField(value_factory, *field_type);
}

absl::StatusOr<Handle<Value>> ParsedProtoStructValue::GetFieldByNumber(
    ValueFactory& value_factory, int64_t number) const {
  CEL_ASSIGN_OR_RETURN(
      auto field_type,
      type()->FindField(value_factory.type_manager(), FieldId(number)));
  if (ABSL_PREDICT_FALSE(!field_type)) {
    return interop_internal::CreateNoSuchFieldError(absl::StrCat(number));
  }
  return GetField(value_factory, *field_type);
}

absl::StatusOr<Handle<Value>> ParsedProtoStructValue::GetField(
    ValueFactory& value_factory, const StructType::Field& field) const {
  const auto* reflect = value().GetReflection();
  if (ABSL_PREDICT_FALSE(reflect == nullptr)) {
    return absl::InternalError("message missing reflection");
  }
  const auto* field_desc =
      static_cast<const google::protobuf::FieldDescriptor*>(field.hint);
  if (field_desc->is_map()) {
    return GetMapField(value_factory, field, *reflect, *field_desc);
  }
  if (field_desc->is_repeated()) {
    return GetRepeatedField(value_factory, field, *reflect, *field_desc);
  }
  return GetSingularField(value_factory, field, *reflect, *field_desc);
}

absl::StatusOr<Handle<Value>> ParsedProtoStructValue::GetMapField(
    ValueFactory& value_factory, const StructType::Field& field,
    const google::protobuf::Reflection& reflect,
    const google::protobuf::FieldDescriptor& field_desc) const {
  return absl::UnimplementedError(
      "cel: access to protocol buffer message map fields is not yet "
      "implemented");
}

absl::StatusOr<Handle<Value>> ParsedProtoStructValue::GetRepeatedField(
    ValueFactory& value_factory, const StructType::Field& field,
    const google::protobuf::Reflection& reflect,
    const google::protobuf::FieldDescriptor& field_desc) const {
  return absl::UnimplementedError(
      "cel: access to protocol buffer message repeated fields is not yet "
      "implemented");
}

absl::StatusOr<Handle<Value>> ParsedProtoStructValue::GetSingularField(
    ValueFactory& value_factory, const StructType::Field& field,
    const google::protobuf::Reflection& reflect,
    const google::protobuf::FieldDescriptor& field_desc) const {
  switch (field_desc.type()) {
    case google::protobuf::FieldDescriptor::TYPE_DOUBLE:
      return value_factory.CreateDoubleValue(
          reflect.GetDouble(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_FLOAT:
      return value_factory.CreateDoubleValue(
          reflect.GetFloat(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_INT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT64:
      return value_factory.CreateIntValue(
          reflect.GetInt64(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_INT32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SFIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_SINT32:
      return value_factory.CreateIntValue(
          reflect.GetInt32(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_UINT64:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_FIXED64:
      return value_factory.CreateUintValue(
          reflect.GetUInt64(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_FIXED32:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_UINT32:
      return value_factory.CreateUintValue(
          reflect.GetUInt32(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_BOOL:
      return value_factory.CreateBoolValue(
          reflect.GetBool(value(), &field_desc));
    case google::protobuf::FieldDescriptor::TYPE_STRING:
      if (field_desc.options().ctype() == google::protobuf::FieldOptions::CORD &&
          !field_desc.is_extension()) {
        return value_factory.CreateStringValue(
            reflect.GetCord(value(), &field_desc));
      } else if (cel::base_internal::Metadata::IsReferenceCounted(*this)) {
        return base_internal::ValueFactoryAccess::CreateMemberStringValue(
            value_factory, reflect.GetStringView(value(), &field_desc), this);
      } else {
        return cel::interop_internal::CreateStringValueFromView(
            reflect.GetStringView(value(), &field_desc));
      }
    case google::protobuf::FieldDescriptor::TYPE_GROUP:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::FieldDescriptor::TYPE_MESSAGE:
      return DynamicMemberParsedProtoStructValue::Create(
          value_factory, field.type.As<ProtoStructType>(), this,
          &(reflect.GetMessage(value(), &field_desc)));
    case google::protobuf::FieldDescriptor::TYPE_BYTES:
      if (field_desc.options().ctype() == google::protobuf::FieldOptions::CORD &&
          !field_desc.is_extension()) {
        return value_factory.CreateBytesValue(
            reflect.GetCord(value(), &field_desc));
      } else if (cel::base_internal::Metadata::IsReferenceCounted(*this)) {
        return base_internal::ValueFactoryAccess::CreateMemberBytesValue(
            value_factory, reflect.GetStringView(value(), &field_desc), this);
      } else {
        return cel::interop_internal::CreateBytesValueFromView(
            reflect.GetStringView(value(), &field_desc));
      }
    case google::protobuf::FieldDescriptor::TYPE_ENUM:
      return value_factory.CreateEnumValue(
          field.type.As<EnumType>(),
          reflect.GetEnumValue(value(), &field_desc));
  }
}

absl::StatusOr<bool> ParsedProtoStructValue::HasFieldByName(
    TypeManager& type_manager, absl::string_view name) const {
  CEL_ASSIGN_OR_RETURN(auto field,
                       type()->FindField(type_manager, FieldId(name)));
  if (ABSL_PREDICT_FALSE(!field.has_value())) {
    return interop_internal::CreateNoSuchFieldError(name);
  }
  return HasField(type_manager, *field);
}

absl::StatusOr<bool> ParsedProtoStructValue::HasFieldByNumber(
    TypeManager& type_manager, int64_t number) const {
  CEL_ASSIGN_OR_RETURN(auto field,
                       type()->FindField(type_manager, FieldId(number)));
  if (ABSL_PREDICT_FALSE(!field.has_value())) {
    return interop_internal::CreateNoSuchFieldError(absl::StrCat(number));
  }
  return HasField(type_manager, *field);
}

absl::StatusOr<bool> ParsedProtoStructValue::HasField(
    TypeManager& type_manager, const StructType::Field& field) const {
  const auto* field_desc =
      static_cast<const google::protobuf::FieldDescriptor*>(field.hint);
  const auto* reflect = value().GetReflection();
  if (ABSL_PREDICT_FALSE(reflect == nullptr)) {
    return absl::InternalError("message missing reflection");
  }
  if (field_desc->is_repeated()) {
    return reflect->FieldSize(value(), field_desc) != 0;
  }
  return reflect->HasField(value(), field_desc);
}

}  // namespace proto_internal

}  // namespace cel::extensions
