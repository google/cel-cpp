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

#include "extensions/protobuf/internal/json.h"

#include <cstddef>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/empty.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/variant.h"
#include "common/any.h"
#include "common/json.h"
#include "common/value_manager.h"
#include "extensions/protobuf/internal/any.h"
#include "extensions/protobuf/internal/duration.h"
#include "extensions/protobuf/internal/field_mask.h"
#include "extensions/protobuf/internal/map_reflection.h"
#include "extensions/protobuf/internal/struct.h"
#include "extensions/protobuf/internal/timestamp.h"
#include "extensions/protobuf/internal/wrappers.h"
#include "internal/status_macros.h"
#include "internal/time.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/message.h"

namespace cel::extensions::protobuf_internal {

namespace {

absl::StatusOr<Json> ProtoSingularFieldToJson(
    ValueManager& value_manager, const google::protobuf::Message& message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field) {
  switch (field->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      return JsonInt(reflection->GetInt32(message, field));
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      return JsonInt(reflection->GetInt64(message, field));
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      return JsonUint(reflection->GetUInt32(message, field));
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return JsonUint(reflection->GetUInt64(message, field));
    case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
      return reflection->GetDouble(message, field);
    case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
      return static_cast<double>(reflection->GetFloat(message, field));
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return reflection->GetBool(message, field);
    case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
      return ProtoEnumToJson(field->enum_type(),
                             reflection->GetEnumValue(message, field));
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
      if (field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES) {
        std::string scratch;
        const auto& value =
            reflection->GetStringReference(message, field, &scratch);
        return JsonBytes(value);
      }
      return reflection->GetCord(message, field);
    }
    case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
      return ProtoMessageToJson(value_manager,
                                reflection->GetMessage(message, field));
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "unexpected protocol buffer field type: ",
          google::protobuf::FieldDescriptor::CppTypeName(field->cpp_type())));
  }
}

absl::StatusOr<JsonString> ProtoMapKeyToJsonString(const google::protobuf::MapKey& key) {
  switch (key.type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return key.GetBoolValue() ? JsonString("true") : JsonString("false");
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      return JsonString(absl::StrCat(key.GetInt32Value()));
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      return JsonString(absl::StrCat(key.GetInt64Value()));
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      return JsonString(absl::StrCat(key.GetUInt32Value()));
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return JsonString(absl::StrCat(key.GetUInt64Value()));
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
      return JsonString(key.GetStringValue());
    default:
      return absl::InternalError(
          absl::StrCat("unexpected protocol buffer map key type: ",
                       google::protobuf::FieldDescriptor::CppTypeName(key.type())));
  }
}

absl::StatusOr<Json> ProtoMapValueToJson(
    ValueManager& value_manager,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    const google::protobuf::MapValueRef& value) {
  switch (field->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      return JsonInt(value.GetInt32Value());
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      return JsonInt(value.GetInt64Value());
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      return JsonUint(value.GetUInt32Value());
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      return JsonUint(value.GetUInt64Value());
    case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
      return value.GetDoubleValue();
    case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
      return static_cast<double>(value.GetFloatValue());
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      return value.GetBoolValue();
    case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
      return ProtoEnumToJson(field->enum_type(), value.GetEnumValue());
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING: {
      if (field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES) {
        return JsonBytes(value.GetStringValue());
      }
      return JsonString(value.GetStringValue());
    }
    case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
      return ProtoMessageToJson(value_manager, value.GetMessageValue());
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "unexpected protocol buffer field type: ",
          google::protobuf::FieldDescriptor::CppTypeName(field->cpp_type())));
  }
}

absl::StatusOr<Json> ProtoMapFieldToJson(
    ValueManager& value_manager, const google::protobuf::Message& message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field) {
  JsonObjectBuilder builder;
  const auto field_size = reflection->FieldSize(message, field);
  builder.reserve(static_cast<size_t>(field_size));
  auto begin = MapBegin(*reflection, message, *field);
  auto end = MapEnd(*reflection, message, *field);
  while (begin != end) {
    CEL_ASSIGN_OR_RETURN(auto key, ProtoMapKeyToJsonString(begin.GetKey()));
    CEL_ASSIGN_OR_RETURN(
        auto value,
        ProtoMapValueToJson(value_manager, field->message_type()->map_value(),
                            begin.GetValueRef()));
    builder.insert_or_assign(std::move(key), std::move(value));
    ++begin;
  }
  return std::move(builder).Build();
}

absl::StatusOr<Json> ProtoRepeatedFieldToJson(
    ValueManager& value_manager, const google::protobuf::Message& message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field) {
  JsonArrayBuilder builder;
  const auto field_size = reflection->FieldSize(message, field);
  builder.reserve(static_cast<size_t>(field_size));
  switch (field->cpp_type()) {
    case google::protobuf::FieldDescriptor::CPPTYPE_INT32:
      for (int field_index = 0; field_index < field_size; ++field_index) {
        builder.push_back(
            JsonInt(reflection->GetRepeatedInt32(message, field, field_index)));
      }
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_INT64:
      for (int field_index = 0; field_index < field_size; ++field_index) {
        builder.push_back(
            JsonInt(reflection->GetRepeatedInt64(message, field, field_index)));
      }
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT32:
      for (int field_index = 0; field_index < field_size; ++field_index) {
        builder.push_back(JsonUint(
            reflection->GetRepeatedUInt32(message, field, field_index)));
      }
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_UINT64:
      for (int field_index = 0; field_index < field_size; ++field_index) {
        builder.push_back(JsonUint(
            reflection->GetRepeatedUInt64(message, field, field_index)));
      }
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_DOUBLE:
      for (int field_index = 0; field_index < field_size; ++field_index) {
        builder.push_back(
            reflection->GetRepeatedDouble(message, field, field_index));
      }
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_FLOAT:
      for (int field_index = 0; field_index < field_size; ++field_index) {
        builder.push_back(static_cast<double>(
            reflection->GetRepeatedFloat(message, field, field_index)));
      }
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_BOOL:
      for (int field_index = 0; field_index < field_size; ++field_index) {
        builder.push_back(
            reflection->GetRepeatedBool(message, field, field_index));
      }
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_ENUM:
      for (int field_index = 0; field_index < field_size; ++field_index) {
        builder.push_back(ProtoEnumToJson(
            field->enum_type(),
            reflection->GetRepeatedEnumValue(message, field, field_index)));
      }
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_STRING:
      if (field->type() == google::protobuf::FieldDescriptor::TYPE_BYTES) {
        std::string scratch;
        for (int field_index = 0; field_index < field_size; ++field_index) {
          builder.push_back(JsonBytes(reflection->GetRepeatedStringReference(
              message, field, field_index, &scratch)));
        }
      } else {
        std::string scratch;
        for (int field_index = 0; field_index < field_size; ++field_index) {
          const auto& field_value = reflection->GetRepeatedStringReference(
              message, field, field_index, &scratch);
          if (&field_value == &scratch) {
            builder.push_back(JsonString(std::move(scratch)));
          } else {
            builder.push_back(JsonString(field_value));
          }
        }
      }
      break;
    case google::protobuf::FieldDescriptor::CPPTYPE_MESSAGE:
      for (int field_index = 0; field_index < field_size; ++field_index) {
        CEL_ASSIGN_OR_RETURN(
            auto json, ProtoMessageToJson(value_manager,
                                          reflection->GetRepeatedMessage(
                                              message, field, field_index)));
        builder.push_back(std::move(json));
      }
      break;
    default:
      return absl::InvalidArgumentError(absl::StrCat(
          "unexpected protocol buffer field type: ",
          google::protobuf::FieldDescriptor::CppTypeName(field->cpp_type())));
  }
  return std::move(builder).Build();
}

absl::StatusOr<Json> ProtoFieldToJson(
    ValueManager& value_manager, const google::protobuf::Message& message,
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field) {
  if (field->is_map()) {
    return ProtoMapFieldToJson(value_manager, message, reflection, field);
  }
  if (field->is_repeated()) {
    return ProtoRepeatedFieldToJson(value_manager, message, reflection, field);
  }
  return ProtoSingularFieldToJson(value_manager, message, reflection, field);
}

absl::StatusOr<Json> ProtoAnyToJson(ValueManager& value_manager,
                                    const google::protobuf::Message& message) {
  CEL_ASSIGN_OR_RETURN(auto any, UnwrapDynamicAnyProto(message));
  absl::string_view type_name;
  if (!ParseTypeUrl(any.type_url(), &type_name)) {
    return absl::InvalidArgumentError(
        "invalid `google.protobuf.Any` field `type_url`");
  }
  JsonObjectBuilder builder;
  builder.reserve(2);
  builder.insert_or_assign(JsonString("@type"), JsonString(any.type_url()));
  if (type_name == "google.protobuf.BoolValue") {
    google::protobuf::BoolValue value_message;
    if (!value_message.ParseFromCord(any.value())) {
      return absl::InvalidArgumentError(
          absl::StrCat("failed to parse `", value_message.GetTypeName(), "`"));
    }
    builder.insert_or_assign(JsonString("value"), value_message.value());
    return std::move(builder).Build();
  }
  if (type_name == "google.protobuf.Int32Value") {
    google::protobuf::Int32Value value_message;
    if (!value_message.ParseFromCord(any.value())) {
      return absl::UnknownError(
          absl::StrCat("failed to parse `", value_message.GetTypeName(), "`"));
    }
    builder.insert_or_assign(JsonString("value"),
                             JsonInt(value_message.value()));
    return std::move(builder).Build();
  }
  if (type_name == "google.protobuf.Int64Value") {
    google::protobuf::Int64Value value_message;
    if (!value_message.ParseFromCord(any.value())) {
      return absl::UnknownError(
          absl::StrCat("failed to parse `", value_message.GetTypeName(), "`"));
    }
    builder.insert_or_assign(JsonString("value"),
                             JsonInt(value_message.value()));
    return std::move(builder).Build();
  }
  if (type_name == "google.protobuf.UInt32Value") {
    google::protobuf::UInt32Value value_message;
    if (!value_message.ParseFromCord(any.value())) {
      return absl::UnknownError(
          absl::StrCat("failed to parse `", value_message.GetTypeName(), "`"));
    }
    builder.insert_or_assign(JsonString("value"),
                             JsonUint(value_message.value()));
    return std::move(builder).Build();
  }
  if (type_name == "google.protobuf.UInt64Value") {
    google::protobuf::UInt64Value value_message;
    if (!value_message.ParseFromCord(any.value())) {
      return absl::UnknownError(
          absl::StrCat("failed to parse `", value_message.GetTypeName(), "`"));
    }
    builder.insert_or_assign(JsonString("value"),
                             JsonUint(value_message.value()));
    return std::move(builder).Build();
  }
  if (type_name == "google.protobuf.FloatValue") {
    google::protobuf::FloatValue value_message;
    if (!value_message.ParseFromCord(any.value())) {
      return absl::UnknownError(
          absl::StrCat("failed to parse `", value_message.GetTypeName(), "`"));
    }
    builder.insert_or_assign(JsonString("value"),
                             static_cast<double>(value_message.value()));
    return std::move(builder).Build();
  }
  if (type_name == "google.protobuf.DoubleValue") {
    google::protobuf::DoubleValue value_message;
    if (!value_message.ParseFromCord(any.value())) {
      return absl::UnknownError(
          absl::StrCat("failed to parse `", value_message.GetTypeName(), "`"));
    }
    builder.insert_or_assign(JsonString("value"), value_message.value());
    return std::move(builder).Build();
  }
  if (type_name == "google.protobuf.BytesValue") {
    google::protobuf::BytesValue value_message;
    if (!value_message.ParseFromCord(any.value())) {
      return absl::UnknownError(
          absl::StrCat("failed to parse `", value_message.GetTypeName(), "`"));
    }
    builder.insert_or_assign(JsonString("value"),
                             JsonBytes(value_message.value()));
    return std::move(builder).Build();
  }
  if (type_name == "google.protobuf.StringValue") {
    google::protobuf::StringValue value_message;
    if (!value_message.ParseFromCord(any.value())) {
      return absl::UnknownError(
          absl::StrCat("failed to parse `", value_message.GetTypeName(), "`"));
    }
    builder.insert_or_assign(JsonString("value"),
                             JsonString(value_message.value()));
    return std::move(builder).Build();
  }
  if (type_name == "google.protobuf.FieldMask") {
    google::protobuf::FieldMask value_message;
    if (!value_message.ParseFromCord(any.value())) {
      return absl::UnknownError(
          absl::StrCat("failed to parse `", value_message.GetTypeName(), "`"));
    }
    CEL_ASSIGN_OR_RETURN(auto json_value,
                         GeneratedFieldMaskProtoToJsonString(value_message));
    builder.insert_or_assign(JsonString("value"), std::move(json_value));
    return std::move(builder).Build();
  }
  if (type_name == "google.protobuf.ListValue") {
    google::protobuf::ListValue value_message;
    if (!value_message.ParseFromCord(any.value())) {
      return absl::UnknownError(
          absl::StrCat("failed to parse `", value_message.GetTypeName(), "`"));
    }
    CEL_ASSIGN_OR_RETURN(auto json_value,
                         GeneratedListValueProtoToJson(value_message));
    builder.insert_or_assign(JsonString("value"), std::move(json_value));
    return std::move(builder).Build();
  }
  if (type_name == "google.protobuf.Struct") {
    google::protobuf::Struct value_message;
    if (!value_message.ParseFromCord(any.value())) {
      return absl::UnknownError(
          absl::StrCat("failed to parse `", value_message.GetTypeName(), "`"));
    }
    CEL_ASSIGN_OR_RETURN(auto json_value,
                         GeneratedStructProtoToJson(value_message));
    builder.insert_or_assign(JsonString("value"), std::move(json_value));
    return std::move(builder).Build();
  }
  if (type_name == "google.protobuf.Value") {
    google::protobuf::Value value_message;
    if (!value_message.ParseFromCord(any.value())) {
      return absl::UnknownError(
          absl::StrCat("failed to parse `", value_message.GetTypeName(), "`"));
    }
    CEL_ASSIGN_OR_RETURN(auto json_value,
                         GeneratedValueProtoToJson(value_message));
    builder.insert_or_assign(JsonString("value"), std::move(json_value));
    return std::move(builder).Build();
  }
  if (type_name == "google.protobuf.Duration") {
    google::protobuf::Duration value_message;
    if (!value_message.ParseFromCord(any.value())) {
      return absl::UnknownError(
          absl::StrCat("failed to parse `", value_message.GetTypeName(), "`"));
    }
    CEL_ASSIGN_OR_RETURN(auto json_value,
                         internal::EncodeDurationToJson(
                             absl::Seconds(value_message.seconds()) +
                             absl::Nanoseconds(value_message.nanos())));
    builder.insert_or_assign(JsonString("value"), JsonString(json_value));
    return std::move(builder).Build();
  }
  if (type_name == "google.protobuf.Timestamp") {
    google::protobuf::Timestamp value_message;
    if (!value_message.ParseFromCord(any.value())) {
      return absl::UnknownError(
          absl::StrCat("failed to parse `", value_message.GetTypeName(), "`"));
    }
    CEL_ASSIGN_OR_RETURN(
        auto json_value,
        internal::EncodeTimestampToJson(
            absl::UnixEpoch() + absl::Seconds(value_message.seconds()) +
            absl::Nanoseconds(value_message.nanos())));
    builder.insert_or_assign(JsonString("value"), JsonString(json_value));
    return std::move(builder).Build();
  }
  if (type_name == "google.protobuf.Empty") {
    google::protobuf::Empty value_message;
    if (!value_message.ParseFromCord(any.value())) {
      return absl::UnknownError(
          absl::StrCat("failed to parse `", value_message.GetTypeName(), "`"));
    }
    builder.insert_or_assign(JsonString("value"), JsonObject());
    return std::move(builder).Build();
  }
  if (type_name == "google.protobuf.Any") {
    // Any in an any. Get out.
    return absl::InvalidArgumentError(
        "refusing to convert recursive `google.protobuf.Any` to JSON");
  }
  CEL_ASSIGN_OR_RETURN(
      auto value, value_manager.DeserializeValue(any.type_url(), any.value()));
  if (!value.has_value()) {
    return absl::NotFoundError(
        absl::StrCat("deserializer missing for `", any.type_url(), "`"));
  }
  CEL_ASSIGN_OR_RETURN(auto json_value, value->ConvertToJson(value_manager));
  if (!absl::holds_alternative<JsonObject>(json_value)) {
    return absl::InternalError("expected JSON object");
  }
  const auto& json_object = absl::get<JsonObject>(json_value);
  builder.reserve(json_object.size() + 1);
  for (const auto& json_entry : json_object) {
    builder.insert_or_assign(json_entry.first, json_entry.second);
  }
  return std::move(builder).Build();
}

}  // namespace

absl::StatusOr<Json> ProtoMessageToJson(ValueManager& value_manager,
                                        const google::protobuf::Message& message) {
  const auto* descriptor = message.GetDescriptor();
  if (ABSL_PREDICT_FALSE(descriptor == nullptr)) {
    return absl::InvalidArgumentError(
        absl::StrCat("`", message.GetTypeName(), "` is missing descriptor"));
  }
  const auto* reflection = message.GetReflection();
  if (ABSL_PREDICT_FALSE(reflection == nullptr)) {
    return absl::InvalidArgumentError(
        absl::StrCat("`", message.GetTypeName(), "` is missing reflection"));
  }
  switch (descriptor->well_known_type()) {
    case google::protobuf::Descriptor::WELLKNOWNTYPE_DOUBLEVALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicDoubleValueProto(message));
      return value;
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_FLOATVALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicFloatValueProto(message));
      return value;
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_INT64VALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicInt64ValueProto(message));
      return JsonInt(value);
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT64VALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicUInt64ValueProto(message));
      return JsonUint(value);
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_INT32VALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicInt32ValueProto(message));
      return JsonInt(value);
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_UINT32VALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicUInt32ValueProto(message));
      return JsonUint(value);
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_STRINGVALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicStringValueProto(message));
      return value;
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_BYTESVALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicBytesValueProto(message));
      return JsonBytes(value);
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_BOOLVALUE: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicBoolValueProto(message));
      return value;
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_ANY:
      return ProtoAnyToJson(value_manager, message);
    case google::protobuf::Descriptor::WELLKNOWNTYPE_FIELDMASK: {
      CEL_ASSIGN_OR_RETURN(auto value,
                           DynamicFieldMaskProtoToJsonString(message));
      return value;
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_DURATION: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicDurationProto(message));
      CEL_ASSIGN_OR_RETURN(auto json, internal::EncodeDurationToJson(value));
      return JsonString(std::move(json));
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_TIMESTAMP: {
      CEL_ASSIGN_OR_RETURN(auto value, UnwrapDynamicTimestampProto(message));
      CEL_ASSIGN_OR_RETURN(auto json, internal::EncodeTimestampToJson(value));
      return JsonString(std::move(json));
    }
    case google::protobuf::Descriptor::WELLKNOWNTYPE_VALUE:
      return DynamicValueProtoToJson(message);
    case google::protobuf::Descriptor::WELLKNOWNTYPE_LISTVALUE:
      return DynamicListValueProtoToJson(message);
    case google::protobuf::Descriptor::WELLKNOWNTYPE_STRUCT:
      return DynamicStructProtoToJson(message);
    default:
      break;
  }
  if (descriptor->full_name() == "google.protobuf.Empty") {
    return JsonObject();
  }
  JsonObjectBuilder builder;
  std::vector<const google::protobuf::FieldDescriptor*> fields;
  reflection->ListFields(message, &fields);
  builder.reserve(fields.size());
  for (const auto* field : fields) {
    CEL_ASSIGN_OR_RETURN(
        auto field_value,
        ProtoFieldToJson(value_manager, message, reflection, field));
    builder.insert_or_assign(JsonString(field->json_name()),
                             std::move(field_value));
  }
  return std::move(builder).Build();
}

Json ProtoEnumToJson(absl::Nonnull<const google::protobuf::EnumDescriptor*> descriptor,
                     int value) {
  if (descriptor->full_name() == "google.protobuf.NullValue") {
    return kJsonNull;
  }
  if (const auto* value_descriptor = descriptor->FindValueByNumber(value);
      value_descriptor != nullptr) {
    return ProtoEnumToJson(value_descriptor);
  }
  return JsonInt(value);
}

Json ProtoEnumToJson(
    absl::Nonnull<const google::protobuf::EnumValueDescriptor*> descriptor) {
  if (descriptor->type()->full_name() == "google.protobuf.NullValue") {
    return kJsonNull;
  }
  if (!descriptor->name().empty()) {
    return JsonString(descriptor->name());
  }
  return JsonInt(descriptor->number());
}

}  // namespace cel::extensions::protobuf_internal
