// Copyright 2024 Google LLC
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

#include "extensions/protobuf/internal/message.h"

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "base/internal/message_wrapper.h"
#include "common/any.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_factory.h"
#include "common/value_kind.h"
#include "common/value_manager.h"
#include "extensions/protobuf/internal/any.h"
#include "extensions/protobuf/internal/duration.h"
#include "extensions/protobuf/internal/struct.h"
#include "extensions/protobuf/internal/timestamp.h"
#include "extensions/protobuf/internal/wrappers.h"
#include "extensions/protobuf/json.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/message.h"
#include "google/protobuf/util/message_differencer.h"

namespace cel {

namespace extensions::protobuf_internal {

namespace {

absl::StatusOr<absl::Nonnull<ArenaUniquePtr<google::protobuf::Message>>> NewProtoMessage(
    absl::Nonnull<const google::protobuf::DescriptorPool*> pool,
    absl::Nonnull<google::protobuf::MessageFactory*> factory, absl::string_view name,
    google::protobuf::Arena* arena) {
  const auto* desc = pool->FindMessageTypeByName(name);
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::NotFoundError(
        absl::StrCat("descriptor missing: `", name, "`"));
  }
  const auto* proto = factory->GetPrototype(desc);
  if (ABSL_PREDICT_FALSE(proto == nullptr)) {
    return absl::NotFoundError(absl::StrCat("prototype missing: `", name, "`"));
  }
  return ArenaUniquePtr<google::protobuf::Message>(proto->New(arena),
                                         DefaultArenaDeleter{arena});
}

// -----------------------------------------------------------------------------
// cel::Value -> google::protobuf::MapKey

absl::Status ProtoBoolMapKeyFromValueConverter(const Value& value,
                                               google::protobuf::MapKey& key,
                                               std::string&) {
  if (auto bool_value = As<BoolValue>(value); bool_value) {
    key.SetBoolValue(bool_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "bool").NativeValue();
}

absl::Status ProtoInt32MapKeyFromValueConverter(const Value& value,
                                                google::protobuf::MapKey& key,
                                                std::string&) {
  if (auto int_value = As<IntValue>(value); int_value) {
    if (int_value->NativeValue() < std::numeric_limits<int32_t>::min() ||
        int_value->NativeValue() > std::numeric_limits<int32_t>::max()) {
      return absl::OutOfRangeError("int64 to int32_t overflow");
    }
    key.SetInt32Value(static_cast<int32_t>(int_value->NativeValue()));
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "int").NativeValue();
}

absl::Status ProtoInt64MapKeyFromValueConverter(const Value& value,
                                                google::protobuf::MapKey& key,
                                                std::string&) {
  if (auto int_value = As<IntValue>(value); int_value) {
    key.SetInt64Value(int_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "int").NativeValue();
}

absl::Status ProtoUInt32MapKeyFromValueConverter(const Value& value,
                                                 google::protobuf::MapKey& key,
                                                 std::string&) {
  if (auto uint_value = As<UintValue>(value); uint_value) {
    if (uint_value->NativeValue() > std::numeric_limits<uint32_t>::max()) {
      return absl::OutOfRangeError("uint64 to uint32_t overflow");
    }
    key.SetUInt32Value(static_cast<uint32_t>(uint_value->NativeValue()));
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "uint").NativeValue();
}

absl::Status ProtoUInt64MapKeyFromValueConverter(const Value& value,
                                                 google::protobuf::MapKey& key,
                                                 std::string&) {
  if (auto uint_value = As<UintValue>(value); uint_value) {
    key.SetUInt64Value(uint_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "uint").NativeValue();
}

absl::Status ProtoStringMapKeyFromValueConverter(const Value& value,
                                                 google::protobuf::MapKey& key,
                                                 std::string& key_string) {
  if (auto string_value = As<StringValue>(value); string_value) {
    key_string = string_value->NativeString();
    key.SetStringValue(key_string);
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "string").NativeValue();
}

}  // namespace

absl::StatusOr<ProtoMapKeyFromValueConverter> GetProtoMapKeyFromValueConverter(
    google::protobuf::FieldDescriptor::CppType cpp_type) {
  switch (cpp_type) {
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return ProtoBoolMapKeyFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      return ProtoInt32MapKeyFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      return ProtoInt64MapKeyFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      return ProtoUInt32MapKeyFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return ProtoUInt64MapKeyFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
      return ProtoStringMapKeyFromValueConverter;
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("unexpected protocol buffer map key type: ",
                       google::protobuf::FieldDescriptor::CppTypeName(cpp_type)));
  }
}

namespace {

// -----------------------------------------------------------------------------
// cel::Value -> google::protobuf::MapValueRef

absl::Status ProtoBoolMapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto bool_value = As<BoolValue>(value); bool_value) {
    value_ref.SetBoolValue(bool_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "bool").NativeValue();
}

absl::Status ProtoInt32MapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto int_value = As<IntValue>(value); int_value) {
    if (int_value->NativeValue() < std::numeric_limits<int32_t>::min() ||
        int_value->NativeValue() > std::numeric_limits<int32_t>::max()) {
      return absl::OutOfRangeError("int64 to int32_t overflow");
    }
    value_ref.SetInt32Value(static_cast<int32_t>(int_value->NativeValue()));
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "int").NativeValue();
}

absl::Status ProtoInt64MapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto int_value = As<IntValue>(value); int_value) {
    value_ref.SetInt64Value(int_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "int").NativeValue();
}

absl::Status ProtoUInt32MapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto uint_value = As<UintValue>(value); uint_value) {
    if (uint_value->NativeValue() > std::numeric_limits<uint32_t>::max()) {
      return absl::OutOfRangeError("uint64 to uint32_t overflow");
    }
    value_ref.SetUInt32Value(static_cast<uint32_t>(uint_value->NativeValue()));
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "uint").NativeValue();
}

absl::Status ProtoUInt64MapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto uint_value = As<UintValue>(value); uint_value) {
    value_ref.SetUInt64Value(uint_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "uint").NativeValue();
}

absl::Status ProtoFloatMapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto double_value = As<DoubleValue>(value); double_value) {
    value_ref.SetFloatValue(double_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "double").NativeValue();
}

absl::Status ProtoDoubleMapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto double_value = As<DoubleValue>(value); double_value) {
    value_ref.SetDoubleValue(double_value->NativeValue());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "double").NativeValue();
}

absl::Status ProtoBytesMapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto bytes_value = As<BytesValue>(value); bytes_value) {
    value_ref.SetStringValue(bytes_value->NativeString());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "bytes").NativeValue();
}

absl::Status ProtoStringMapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    google::protobuf::MapValueRef& value_ref) {
  if (auto string_value = As<StringValue>(value); string_value) {
    value_ref.SetStringValue(string_value->NativeString());
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "string").NativeValue();
}

absl::Status ProtoNullMapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    google::protobuf::MapValueRef& value_ref) {
  if (InstanceOf<NullValue>(value) || InstanceOf<IntValue>(value)) {
    value_ref.SetEnumValue(0);
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "google.protobuf.NullValue")
      .NativeValue();
}

absl::Status ProtoEnumMapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    google::protobuf::MapValueRef& value_ref) {
  if (auto int_value = As<IntValue>(value); int_value) {
    if (int_value->NativeValue() < std::numeric_limits<int32_t>::min() ||
        int_value->NativeValue() > std::numeric_limits<int32_t>::max()) {
      return absl::OutOfRangeError("int64 to int32_t overflow");
    }
    value_ref.SetEnumValue(static_cast<int32_t>(int_value->NativeValue()));
    return absl::OkStatus();
  }
  return TypeConversionError(value.GetTypeName(), "enum").NativeValue();
}

absl::Status ProtoMessageMapValueFromValueConverter(
    const Value& value, absl::Nonnull<const google::protobuf::FieldDescriptor*>,
    google::protobuf::MapValueRef& value_ref) {
  return ProtoMessageFromValueImpl(value, value_ref.MutableMessageValue());
}

}  // namespace

absl::StatusOr<ProtoMapValueFromValueConverter>
GetProtoMapValueFromValueConverter(
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field) {
  ABSL_DCHECK(field->is_map());
  const auto* value_field = field->message_type()->map_value();
  switch (value_field->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return ProtoBoolMapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      return ProtoInt32MapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      return ProtoInt64MapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      return ProtoUInt32MapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return ProtoUInt64MapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
      return ProtoFloatMapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
      return ProtoDoubleMapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
      if (value_field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES) {
        return ProtoBytesMapValueFromValueConverter;
      }
      return ProtoStringMapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
      if (value_field->enum_type()->full_name() ==
          "google.protobuf.NullValue") {
        return ProtoNullMapValueFromValueConverter;
      }
      return ProtoEnumMapValueFromValueConverter;
    case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
      return ProtoMessageMapValueFromValueConverter;
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "unexpected protocol buffer map value type: ",
          google::protobuf::FieldDescriptor::CppTypeName(value_field->cpp_type())));
  }
}

namespace {

absl::StatusOr<absl::optional<Value>> WellKnownProtoMessageToValue(
    ValueFactory& value_factory, const TypeReflector& type_reflector,
    absl::Nonnull<const google::protobuf::Message*> message) {
  const auto* desc = message->GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::nullopt;
  }
  switch (desc->well_known_type()) {
    case google::protobuf::Descriptor::WELLKNOWNTYPE_FLOATVALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicFloatValueProto(*message));
      return DoubleValue{value};
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_DOUBLEVALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicDoubleValueProto(*message));
      return DoubleValue{value};
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_INT32VALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicInt32ValueProto(*message));
      return IntValue{value};
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_INT64VALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicInt64ValueProto(*message));
      return IntValue{value};
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT32VALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicUInt32ValueProto(*message));
      return UintValue{value};
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT64VALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicUInt64ValueProto(*message));
      return UintValue{value};
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_STRINGVALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicStringValueProto(*message));
      return StringValue{std::move(value)};
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_BYTESVALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicBytesValueProto(*message));
      return BytesValue{std::move(value)};
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_BOOLVALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicBoolValueProto(*message));
      return BoolValue{value};
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_ANY: {
      CEL_ASSIGN_OR_RETURN(auto any, UnwrapDynamicAnyProto(*message));
      CEL_ASSIGN_OR_RETURN(auto value, type_reflector.DeserializeValue(
                                           value_factory, any.type_url(),
                                           GetAnyValueAsCord(any)));
      if (!value) {
        return absl::NotFoundError(
            absl::StrCat("unable to find deserializer for ", any.type_url()));
      }
      return std::move(value).value();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_DURATION: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicDurationProto(*message));
      return DurationValue{value};
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_TIMESTAMP: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicTimestampProto(*message));
      return TimestampValue{value};
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, DynamicValueProtoToJson(*message));
      return value_factory.CreateValueFromJson(std::move(value));
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, DynamicListValueProtoToJson(*message));
      return value_factory.CreateValueFromJson(std::move(value));
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT: {
      CEL_ASSIGN_OR_RETURN(auto value, DynamicStructProtoToJson(*message));
      return value_factory.CreateValueFromJson(std::move(value));
    }
    default:
      return absl::nullopt;
  }
}

absl::StatusOr<absl::optional<Value>> WellKnownProtoMessageToValue(
    ValueManager& value_manager,
    absl::Nonnull<const google::protobuf::Message*> message) {
  return WellKnownProtoMessageToValue(value_manager,
                                      value_manager.type_provider(), message);
}

absl::Status ProtoMessageCopyUsingSerialization(
    google::protobuf::MessageLite* to, const google::protobuf::MessageLite* from) {
  ABSL_DCHECK_EQ(to->GetTypeName(), from->GetTypeName());
  absl::Cord serialized;
  if (!from->SerializePartialToCord(&serialized)) {
    return absl::UnknownError(
        absl::StrCat("failed to serialize `", from->GetTypeName(), "`"));
  }
  if (!to->ParsePartialFromCord(serialized)) {
    return absl::UnknownError(
        absl::StrCat("failed to parse `", to->GetTypeName(), "`"));
  }
  return absl::OkStatus();
}

absl::Status ProtoMessageCopy(
    absl::Nonnull<google::protobuf::Message*> to_message,
    absl::Nonnull<const google::protobuf::Descriptor*> to_descriptor,
    absl::Nonnull<const google::protobuf::Message*> from_message) {
  CEL_ASSIGN_OR_RETURN(const auto* from_descriptor,
                       GetDescriptor(*from_message));
  if (to_descriptor == from_descriptor) {
    // Same.
    to_message->CopyFrom(*from_message);
    return absl::OkStatus();
  }
  if (to_descriptor->full_name() == from_descriptor->full_name()) {
    // Same type, different descriptors.
    return ProtoMessageCopyUsingSerialization(to_message, from_message);
  }
  return TypeConversionError(from_descriptor->full_name(),
                             to_descriptor->full_name())
      .NativeValue();
}

absl::Status ProtoMessageCopy(
    absl::Nonnull<google::protobuf::Message*> to_message,
    absl::Nonnull<const google::protobuf::Descriptor*> to_descriptor,
    absl::Nonnull<const google::protobuf::MessageLite*> from_message) {
  const auto& from_type_name = from_message->GetTypeName();
  if (from_type_name == to_descriptor->full_name()) {
    return ProtoMessageCopyUsingSerialization(to_message, from_message);
  }
  return TypeConversionError(from_type_name, to_descriptor->full_name())
      .NativeValue();
}

}  // namespace

absl::StatusOr<absl::Nonnull<const google::protobuf::Descriptor*>> GetDescriptor(
    const google::protobuf::Message& message) {
  const auto* desc = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(desc == nullptr)) {
    return absl::InvalidArgumentError(
        absl::StrCat(message.GetTypeName(), " is missing descriptor"));
  }
  return desc;
}

absl::StatusOr<absl::Nonnull<const google::protobuf::Reflection*>> GetReflection(
    const google::protobuf::Message& message) {
  const auto* reflect = message.GetReflection();
  if (ABSL_PREDICT_FALSE(reflect == nullptr)) {
    return absl::InvalidArgumentError(
        absl::StrCat(message.GetTypeName(), " is missing reflection"));
  }
  return reflect;
}

absl::Nonnull<const google::protobuf::Reflection*> GetReflectionOrDie(
    const google::protobuf::Message& message) {
  const auto* reflection = message.GetReflection();
  ABSL_CHECK(reflection != nullptr)  // Crash OK
      << message.GetTypeName() << " is missing reflection";
  return reflection;
}

absl::StatusOr<Value> ProtoMessageToValueImpl(
    ValueManager& value_manager, absl::Nonnull<const google::protobuf::Message*> message,
    absl::Nonnull<ProtoMessageCopyConstructor> copy_construct) {
  {
    CEL_ASSIGN_OR_RETURN(auto well_known,
                         WellKnownProtoMessageToValue(value_manager, message));
    if (well_known) {
      return std::move(well_known).value();
    }
  }
  auto memory_manager = value_manager.GetMemoryManager();
  auto* arena = ProtoMemoryManagerArena(memory_manager);
  return ParsedMessageValue(
      WrapShared((*copy_construct)(arena, message), arena));
}

absl::StatusOr<Value> ProtoMessageToValueImpl(
    ValueManager& value_manager, absl::Nonnull<google::protobuf::Message*> message,
    absl::Nonnull<ProtoMessageMoveConstructor> move_construct) {
  {
    CEL_ASSIGN_OR_RETURN(auto well_known,
                         WellKnownProtoMessageToValue(value_manager, message));
    if (well_known) {
      return std::move(well_known).value();
    }
  }
  auto memory_manager = value_manager.GetMemoryManager();
  auto* arena = ProtoMemoryManagerArena(memory_manager);
  return ParsedMessageValue(
      WrapShared((*move_construct)(arena, message), arena));
}

absl::StatusOr<absl::Nonnull<google::protobuf::Message*>> ProtoMessageFromValueImpl(
    const Value& value, absl::Nonnull<const google::protobuf::DescriptorPool*> pool,
    absl::Nonnull<google::protobuf::MessageFactory*> factory, google::protobuf::Arena* arena) {
  switch (value.kind()) {
    case ValueKind::kNull: {
      CEL_ASSIGN_OR_RETURN(
          auto message,
          NewProtoMessage(pool, factory, "google.protobuf.Value", arena));
      CEL_RETURN_IF_ERROR(DynamicValueProtoFromJson(kJsonNull, *message));
      return message.release();
    }
    case ValueKind::kBool: {
      CEL_ASSIGN_OR_RETURN(
          auto message,
          NewProtoMessage(pool, factory, "google.protobuf.BoolValue", arena));
      CEL_RETURN_IF_ERROR(WrapDynamicBoolValueProto(
          Cast<BoolValue>(value).NativeValue(), *message));
      return message.release();
    }
    case ValueKind::kInt: {
      CEL_ASSIGN_OR_RETURN(
          auto message,
          NewProtoMessage(pool, factory, "google.protobuf.Int64Value", arena));
      CEL_RETURN_IF_ERROR(WrapDynamicInt64ValueProto(
          Cast<IntValue>(value).NativeValue(), *message));
      return message.release();
    }
    case ValueKind::kUint: {
      CEL_ASSIGN_OR_RETURN(
          auto message,
          NewProtoMessage(pool, factory, "google.protobuf.UInt64Value", arena));
      CEL_RETURN_IF_ERROR(WrapDynamicUInt64ValueProto(
          Cast<UintValue>(value).NativeValue(), *message));
      return message.release();
    }
    case ValueKind::kDouble: {
      CEL_ASSIGN_OR_RETURN(
          auto message,
          NewProtoMessage(pool, factory, "google.protobuf.DoubleValue", arena));
      CEL_RETURN_IF_ERROR(WrapDynamicDoubleValueProto(
          Cast<DoubleValue>(value).NativeValue(), *message));
      return message.release();
    }
    case ValueKind::kString: {
      CEL_ASSIGN_OR_RETURN(
          auto message,
          NewProtoMessage(pool, factory, "google.protobuf.StringValue", arena));
      CEL_RETURN_IF_ERROR(WrapDynamicStringValueProto(
          Cast<StringValue>(value).NativeCord(), *message));
      return message.release();
    }
    case ValueKind::kBytes: {
      CEL_ASSIGN_OR_RETURN(
          auto message,
          NewProtoMessage(pool, factory, "google.protobuf.BytesValue", arena));
      CEL_RETURN_IF_ERROR(WrapDynamicBytesValueProto(
          Cast<BytesValue>(value).NativeCord(), *message));
      return message.release();
    }
    case ValueKind::kStruct: {
      CEL_ASSIGN_OR_RETURN(
          auto message,
          NewProtoMessage(pool, factory, value.GetTypeName(), arena));
      ProtoAnyToJsonConverter converter(pool, factory);
      absl::Cord serialized;
      CEL_RETURN_IF_ERROR(value.SerializeTo(converter, serialized));
      if (!message->ParsePartialFromCord(serialized)) {
        return absl::UnknownError(
            absl::StrCat("failed to parse `", message->GetTypeName(), "`"));
      }
      return message.release();
    }
    case ValueKind::kDuration: {
      CEL_ASSIGN_OR_RETURN(
          auto message,
          NewProtoMessage(pool, factory, "google.protobuf.Duration", arena));
      CEL_RETURN_IF_ERROR(WrapDynamicDurationProto(
          Cast<DurationValue>(value).NativeValue(), *message));
      return message.release();
    }
    case ValueKind::kTimestamp: {
      CEL_ASSIGN_OR_RETURN(
          auto message,
          NewProtoMessage(pool, factory, "google.protobuf.Timestamp", arena));
      CEL_RETURN_IF_ERROR(WrapDynamicTimestampProto(
          Cast<TimestampValue>(value).NativeValue(), *message));
      return message.release();
    }
    case ValueKind::kList: {
      CEL_ASSIGN_OR_RETURN(
          auto message,
          NewProtoMessage(pool, factory, "google.protobuf.ListValue", arena));
      ProtoAnyToJsonConverter converter(pool, factory);
      CEL_ASSIGN_OR_RETURN(
          auto json, Cast<ListValue>(value).ConvertToJsonArray(converter));
      CEL_RETURN_IF_ERROR(DynamicListValueProtoFromJson(json, *message));
      return message.release();
    }
    case ValueKind::kMap: {
      CEL_ASSIGN_OR_RETURN(
          auto message,
          NewProtoMessage(pool, factory, "google.protobuf.Struct", arena));
      ProtoAnyToJsonConverter converter(pool, factory);
      CEL_ASSIGN_OR_RETURN(
          auto json, Cast<MapValue>(value).ConvertToJsonObject(converter));
      CEL_RETURN_IF_ERROR(DynamicStructProtoFromJson(json, *message));
      return message.release();
    }
    default:
      break;
  }
  return TypeConversionError(value.GetTypeName(), "*message*").NativeValue();
}

absl::Status ProtoMessageFromValueImpl(
    const Value& value, absl::Nonnull<const google::protobuf::DescriptorPool*> pool,
    absl::Nonnull<google::protobuf::MessageFactory*> factory,
    absl::Nonnull<google::protobuf::Message*> message) {
  CEL_ASSIGN_OR_RETURN(const auto* to_desc, GetDescriptor(*message));
  switch (to_desc->well_known_type()) {
    case google::protobuf::Descriptor::WELLKNOWNTYPE_FLOATVALUE: {
      if (auto double_value = As<DoubleValue>(value); double_value) {
        return WrapDynamicFloatValueProto(
            static_cast<float>(double_value->NativeValue()), *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_DOUBLEVALUE: {
      if (auto double_value = As<DoubleValue>(value); double_value) {
        return WrapDynamicDoubleValueProto(
            static_cast<float>(double_value->NativeValue()), *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_INT32VALUE: {
      if (auto int_value = As<IntValue>(value); int_value) {
        if (int_value->NativeValue() < std::numeric_limits<int32_t>::min() ||
            int_value->NativeValue() > std::numeric_limits<int32_t>::max()) {
          return absl::OutOfRangeError("int64 to int32_t overflow");
        }
        return WrapDynamicInt32ValueProto(
            static_cast<int32_t>(int_value->NativeValue()), *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_INT64VALUE: {
      if (auto int_value = As<IntValue>(value); int_value) {
        return WrapDynamicInt64ValueProto(int_value->NativeValue(), *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT32VALUE: {
      if (auto uint_value = As<UintValue>(value); uint_value) {
        if (uint_value->NativeValue() > std::numeric_limits<uint32_t>::max()) {
          return absl::OutOfRangeError("uint64 to uint32_t overflow");
        }
        return WrapDynamicUInt32ValueProto(
            static_cast<uint32_t>(uint_value->NativeValue()), *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT64VALUE: {
      if (auto uint_value = As<UintValue>(value); uint_value) {
        return WrapDynamicUInt64ValueProto(uint_value->NativeValue(), *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_STRINGVALUE: {
      if (auto string_value = As<StringValue>(value); string_value) {
        return WrapDynamicStringValueProto(string_value->NativeCord(),
                                           *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_BYTESVALUE: {
      if (auto bytes_value = As<BytesValue>(value); bytes_value) {
        return WrapDynamicBytesValueProto(bytes_value->NativeCord(), *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_BOOLVALUE: {
      if (auto bool_value = As<BoolValue>(value); bool_value) {
        return WrapDynamicBoolValueProto(bool_value->NativeValue(), *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_ANY: {
      ProtoAnyToJsonConverter converter(pool, factory);
      absl::Cord serialized;
      CEL_RETURN_IF_ERROR(value.SerializeTo(converter, serialized));
      std::string type_url;
      switch (value.kind()) {
        case ValueKind::kNull:
          type_url = MakeTypeUrl("google.protobuf.Value");
          break;
        case ValueKind::kBool:
          type_url = MakeTypeUrl("google.protobuf.BoolValue");
          break;
        case ValueKind::kInt:
          type_url = MakeTypeUrl("google.protobuf.Int64Value");
          break;
        case ValueKind::kUint:
          type_url = MakeTypeUrl("google.protobuf.UInt64Value");
          break;
        case ValueKind::kDouble:
          type_url = MakeTypeUrl("google.protobuf.DoubleValue");
          break;
        case ValueKind::kBytes:
          type_url = MakeTypeUrl("google.protobuf.BytesValue");
          break;
        case ValueKind::kString:
          type_url = MakeTypeUrl("google.protobuf.StringValue");
          break;
        case ValueKind::kList:
          type_url = MakeTypeUrl("google.protobuf.ListValue");
          break;
        case ValueKind::kMap:
          type_url = MakeTypeUrl("google.protobuf.Struct");
          break;
        case ValueKind::kDuration:
          type_url = MakeTypeUrl("google.protobuf.Duration");
          break;
        case ValueKind::kTimestamp:
          type_url = MakeTypeUrl("google.protobuf.Timestamp");
          break;
        default:
          type_url = MakeTypeUrl(value.GetTypeName());
          break;
      }
      return WrapDynamicAnyProto(type_url, serialized, *message);
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_DURATION: {
      if (auto duration_value = As<DurationValue>(value); duration_value) {
        return WrapDynamicDurationProto(duration_value->NativeValue(),
                                        *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_TIMESTAMP: {
      if (auto timestamp_value = As<TimestampValue>(value); timestamp_value) {
        return WrapDynamicTimestampProto(timestamp_value->NativeValue(),
                                         *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE: {
      ProtoAnyToJsonConverter converter(pool, factory);
      CEL_ASSIGN_OR_RETURN(auto json, value.ConvertToJson(converter));
      return DynamicValueProtoFromJson(json, *message);
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE: {
      ProtoAnyToJsonConverter converter(pool, factory);
      CEL_ASSIGN_OR_RETURN(auto json, value.ConvertToJson(converter));
      if (absl::holds_alternative<JsonArray>(json)) {
        return DynamicListValueProtoFromJson(absl::get<JsonArray>(json),
                                             *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT: {
      ProtoAnyToJsonConverter converter(pool, factory);
      CEL_ASSIGN_OR_RETURN(auto json, value.ConvertToJson(converter));
      if (absl::holds_alternative<JsonObject>(json)) {
        return DynamicStructProtoFromJson(absl::get<JsonObject>(json),
                                          *message);
      }
      return TypeConversionError(value.GetTypeName(), to_desc->full_name())
          .NativeValue();
    }
    default:
      break;
  }

  // Not a well known type.

  // Deal with legacy values.
  if (auto legacy_value = common_internal::AsLegacyStructValue(value);
      legacy_value) {
    if ((legacy_value->message_ptr() & base_internal::kMessageWrapperTagMask) ==
        base_internal::kMessageWrapperTagMessageValue) {
      // Full.
      const auto* from_message = reinterpret_cast<const google::protobuf::Message*>(
          legacy_value->message_ptr() & base_internal::kMessageWrapperPtrMask);
      return ProtoMessageCopy(message, to_desc, from_message);
    } else {
      // Lite.
      // Only thing we can do is check type names, which is gross because proto
      // returns `std::string`.
      const auto* from_message = reinterpret_cast<const google::protobuf::MessageLite*>(
          legacy_value->message_ptr() & base_internal::kMessageWrapperPtrMask);
      return ProtoMessageCopy(message, to_desc, from_message);
    }
  }

  // Deal with modern values.
  if (auto parsed_message_value = value.AsParsedMessage();
      parsed_message_value) {
    return ProtoMessageCopy(message, to_desc,
                            cel::to_address(*parsed_message_value));
  }

  return TypeConversionError(value.GetTypeName(), message->GetTypeName())
      .NativeValue();
}

absl::StatusOr<Value> ProtoMessageToValueImpl(
    ValueFactory& value_factory, const TypeReflector& type_reflector,
    absl::Nonnull<const google::protobuf::Message*> prototype,
    const absl::Cord& serialized) {
  auto* arena = ProtoMemoryManagerArena(value_factory.GetMemoryManager());
  auto message = WrapShared(prototype->New(arena), arena);
  if (!message->ParsePartialFromCord(serialized)) {
    return absl::InvalidArgumentError(
        absl::StrCat("failed to parse `", prototype->GetTypeName(), "`"));
  }
  {
    CEL_ASSIGN_OR_RETURN(auto well_known, WellKnownProtoMessageToValue(
                                              value_factory, type_reflector,
                                              cel::to_address(message)));
    if (well_known) {
      return std::move(well_known).value();
    }
  }
  return ParsedMessageValue(std::move(message));
}

StructValue ProtoMessageAsStructValueImpl(
    ValueFactory& value_factory, absl::Nonnull<google::protobuf::Message*> message) {
  return ParsedMessageValue(
      WrapShared(message, value_factory.GetMemoryManager().arena()));
}

}  // namespace extensions::protobuf_internal

}  // namespace cel
