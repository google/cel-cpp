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

#include "internal/json.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/functional/overload.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/ascii.h"
#include "absl/strings/cord.h"
#include "absl/strings/escaping.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "common/json.h"
#include "extensions/protobuf/internal/map_reflection.h"
#include "internal/status_macros.h"
#include "internal/well_known_types.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/map_field.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"
#include "google/protobuf/util/time_util.h"

namespace cel::internal {

namespace {

using ::cel::well_known_types::AsVariant;
using ::cel::well_known_types::GetRepeatedBytesField;
using ::cel::well_known_types::GetRepeatedStringField;
using ::cel::well_known_types::ListValueReflection;
using ::cel::well_known_types::Reflection;
using ::cel::well_known_types::StructReflection;
using ::cel::well_known_types::ValueReflection;
using ::google::protobuf::Descriptor;
using ::google::protobuf::FieldDescriptor;
using ::google::protobuf::util::TimeUtil;

// Yanked from the implementation `google::protobuf::util::TimeUtil`.
template <typename Chars>
absl::Status SnakeCaseToCamelCaseImpl(Chars input,
                                      absl::Nonnull<std::string*> output) {
  output->clear();
  bool after_underscore = false;
  for (char input_char : input) {
    if (absl::ascii_isupper(input_char)) {
      // The field name must not contain uppercase letters.
      return absl::InvalidArgumentError(
          "field mask path name contains uppercase letters");
    }
    if (after_underscore) {
      if (absl::ascii_islower(input_char)) {
        output->push_back(absl::ascii_toupper(input_char));
        after_underscore = false;
      } else {
        // The character after a "_" must be a lowercase letter.
        return absl::InvalidArgumentError(
            "field mask path contains '_' not followed by a lowercase letter");
      }
    } else if (input_char == '_') {
      after_underscore = true;
    } else {
      output->push_back(input_char);
    }
  }
  if (after_underscore) {
    // Trailing "_".
    return absl::InvalidArgumentError("field mask path contains trailing '_'");
  }
  return absl::OkStatus();
}

absl::Status SnakeCaseToCamelCase(const well_known_types::StringValue& input,
                                  absl::Nonnull<std::string*> output) {
  return absl::visit(absl::Overload(
                         [&](absl::string_view string) -> absl::Status {
                           return SnakeCaseToCamelCaseImpl(string, output);
                         },
                         [&](const absl::Cord& cord) -> absl::Status {
                           return SnakeCaseToCamelCaseImpl(cord.Chars(),
                                                           output);
                         }),
                     AsVariant(input));
}

struct MessageToJsonState;

using MapFieldKeyToString = std::string (*)(const google::protobuf::MapKey&);

std::string BoolMapFieldKeyToString(const google::protobuf::MapKey& key) {
  return key.GetBoolValue() ? "true" : "false";
}

std::string Int32MapFieldKeyToString(const google::protobuf::MapKey& key) {
  return absl::StrCat(key.GetInt32Value());
}

std::string Int64MapFieldKeyToString(const google::protobuf::MapKey& key) {
  return absl::StrCat(key.GetInt64Value());
}

std::string UInt32MapFieldKeyToString(const google::protobuf::MapKey& key) {
  return absl::StrCat(key.GetUInt32Value());
}

std::string UInt64MapFieldKeyToString(const google::protobuf::MapKey& key) {
  return absl::StrCat(key.GetUInt64Value());
}

std::string StringMapFieldKeyToString(const google::protobuf::MapKey& key) {
  return std::string(key.GetStringValue());
}

MapFieldKeyToString GetMapFieldKeyToString(
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field) {
  switch (field->cpp_type()) {
    case FieldDescriptor::CPPTYPE_BOOL:
      return &BoolMapFieldKeyToString;
    case FieldDescriptor::CPPTYPE_INT32:
      return &Int32MapFieldKeyToString;
    case FieldDescriptor::CPPTYPE_INT64:
      return &Int64MapFieldKeyToString;
    case FieldDescriptor::CPPTYPE_UINT32:
      return &UInt32MapFieldKeyToString;
    case FieldDescriptor::CPPTYPE_UINT64:
      return &UInt64MapFieldKeyToString;
    case FieldDescriptor::CPPTYPE_STRING:
      return &StringMapFieldKeyToString;
    default:
      ABSL_UNREACHABLE();
  }
}

using MapFieldValueToValue = absl::Status (MessageToJsonState::*)(
    const google::protobuf::MapValueConstRef& value,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<google::protobuf::MessageLite*> result);

using RepeatedFieldToValue = absl::Status (MessageToJsonState::*)(
    absl::Nonnull<const google::protobuf::Reflection*> reflection,
    const google::protobuf::Message& message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
    absl::Nonnull<google::protobuf::MessageLite*> result);

class MessageToJsonState {
 public:
  MessageToJsonState(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory)
      : descriptor_pool_(descriptor_pool), message_factory_(message_factory) {}

  virtual ~MessageToJsonState() = default;

  absl::Status ToJson(const google::protobuf::Message& message,
                      absl::Nonnull<google::protobuf::MessageLite*> result) {
    const auto* descriptor = message.GetDescriptor();
    switch (descriptor->well_known_type()) {
      case Descriptor::WELLKNOWNTYPE_DOUBLEVALUE: {
        CEL_RETURN_IF_ERROR(reflection_.DoubleValue().Initialize(descriptor));
        SetNumberValue(result, reflection_.DoubleValue().GetValue(message));
      } break;
      case Descriptor::WELLKNOWNTYPE_FLOATVALUE: {
        CEL_RETURN_IF_ERROR(reflection_.FloatValue().Initialize(descriptor));
        SetNumberValue(result, reflection_.FloatValue().GetValue(message));
      } break;
      case Descriptor::WELLKNOWNTYPE_INT64VALUE: {
        CEL_RETURN_IF_ERROR(reflection_.Int64Value().Initialize(descriptor));
        SetNumberValue(result, reflection_.Int64Value().GetValue(message));
      } break;
      case Descriptor::WELLKNOWNTYPE_UINT64VALUE: {
        CEL_RETURN_IF_ERROR(reflection_.UInt64Value().Initialize(descriptor));
        SetNumberValue(result, reflection_.UInt64Value().GetValue(message));
      } break;
      case Descriptor::WELLKNOWNTYPE_INT32VALUE: {
        CEL_RETURN_IF_ERROR(reflection_.Int32Value().Initialize(descriptor));
        SetNumberValue(result, reflection_.Int32Value().GetValue(message));
      } break;
      case Descriptor::WELLKNOWNTYPE_UINT32VALUE: {
        CEL_RETURN_IF_ERROR(reflection_.UInt32Value().Initialize(descriptor));
        SetNumberValue(result, reflection_.UInt32Value().GetValue(message));
      } break;
      case Descriptor::WELLKNOWNTYPE_STRINGVALUE: {
        CEL_RETURN_IF_ERROR(reflection_.StringValue().Initialize(descriptor));
        StringValueToJson(reflection_.StringValue().GetValue(message, scratch_),
                          result);
      } break;
      case Descriptor::WELLKNOWNTYPE_BYTESVALUE: {
        CEL_RETURN_IF_ERROR(reflection_.BytesValue().Initialize(descriptor));
        BytesValueToJson(reflection_.BytesValue().GetValue(message, scratch_),
                         result);
      } break;
      case Descriptor::WELLKNOWNTYPE_BOOLVALUE: {
        CEL_RETURN_IF_ERROR(reflection_.BoolValue().Initialize(descriptor));
        SetBoolValue(result, reflection_.BoolValue().GetValue(message));
      } break;
      case Descriptor::WELLKNOWNTYPE_ANY: {
        CEL_ASSIGN_OR_RETURN(auto unpacked,
                             well_known_types::UnpackAnyFrom(
                                 result->GetArena(), reflection_.Any(), message,
                                 descriptor_pool_, message_factory_));
        auto* struct_result = MutableStructValue(result);
        const auto* unpacked_descriptor = unpacked->GetDescriptor();
        SetStringValue(InsertField(struct_result, "@type"),
                       absl::StrCat("type.googleapis.com/",
                                    unpacked_descriptor->full_name()));
        switch (unpacked_descriptor->well_known_type()) {
          case Descriptor::WELLKNOWNTYPE_DOUBLEVALUE:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_FLOATVALUE:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_INT64VALUE:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_UINT64VALUE:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_INT32VALUE:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_UINT32VALUE:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_STRINGVALUE:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_BYTESVALUE:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_BOOLVALUE:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_FIELDMASK:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_DURATION:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_TIMESTAMP:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_VALUE:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_LISTVALUE:
            ABSL_FALLTHROUGH_INTENDED;
          case Descriptor::WELLKNOWNTYPE_STRUCT:
            return ToJson(*unpacked, InsertField(struct_result, "value"));
          default:
            if (unpacked_descriptor->full_name() == "google.protobuf.Empty") {
              MutableStructValue(InsertField(struct_result, "value"));
              return absl::OkStatus();
            } else {
              return MessageToJson(*unpacked, struct_result);
            }
        }
      }
      case Descriptor::WELLKNOWNTYPE_FIELDMASK: {
        CEL_RETURN_IF_ERROR(reflection_.FieldMask().Initialize(descriptor));
        std::vector<std::string> paths;
        const int paths_size = reflection_.FieldMask().PathsSize(message);
        for (int i = 0; i < paths_size; ++i) {
          CEL_RETURN_IF_ERROR(SnakeCaseToCamelCase(
              reflection_.FieldMask().Paths(message, i, scratch_),
              &paths.emplace_back()));
        }
        SetStringValue(result, absl::StrJoin(paths, ","));
      } break;
      case Descriptor::WELLKNOWNTYPE_DURATION: {
        CEL_RETURN_IF_ERROR(reflection_.Duration().Initialize(descriptor));
        google::protobuf::Duration duration;
        duration.set_seconds(reflection_.Duration().GetSeconds(message));
        duration.set_nanos(reflection_.Duration().GetNanos(message));
        SetStringValue(result, TimeUtil::ToString(duration));
      } break;
      case Descriptor::WELLKNOWNTYPE_TIMESTAMP: {
        CEL_RETURN_IF_ERROR(reflection_.Timestamp().Initialize(descriptor));
        google::protobuf::Timestamp timestamp;
        timestamp.set_seconds(reflection_.Timestamp().GetSeconds(message));
        timestamp.set_nanos(reflection_.Timestamp().GetNanos(message));
        SetStringValue(result, TimeUtil::ToString(timestamp));
      } break;
      case Descriptor::WELLKNOWNTYPE_VALUE: {
        absl::Cord serialized;
        if (!message.SerializePartialToCord(&serialized)) {
          return absl::UnknownError(
              "failed to serialize message google.protobuf.Value");
        }
        if (!result->ParsePartialFromCord(serialized)) {
          return absl::UnknownError(
              "failed to parsed message: google.protobuf.Value");
        }
      } break;
      case Descriptor::WELLKNOWNTYPE_LISTVALUE: {
        absl::Cord serialized;
        if (!message.SerializePartialToCord(&serialized)) {
          return absl::UnknownError(
              "failed to serialize message google.protobuf.ListValue");
        }
        if (!MutableListValue(result)->ParsePartialFromCord(serialized)) {
          return absl::UnknownError(
              "failed to parsed message: google.protobuf.ListValue");
        }
      } break;
      case Descriptor::WELLKNOWNTYPE_STRUCT: {
        absl::Cord serialized;
        if (!message.SerializePartialToCord(&serialized)) {
          return absl::UnknownError(
              "failed to serialize message google.protobuf.Struct");
        }
        if (!MutableStructValue(result)->ParsePartialFromCord(serialized)) {
          return absl::UnknownError(
              "failed to parsed message: google.protobuf.Struct");
        }
      } break;
      default:
        return MessageToJson(message, MutableStructValue(result));
    }
    return absl::OkStatus();
  }

  absl::Status FieldToJson(const google::protobuf::Message& message,
                           absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
                           absl::Nonnull<google::protobuf::MessageLite*> result) {
    return MessageFieldToJson(message, field, result);
  }

  virtual absl::Status Initialize(
      absl::Nonnull<google::protobuf::MessageLite*> message) = 0;

 private:
  absl::StatusOr<MapFieldValueToValue> GetMapFieldValueToValue(
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field) {
    switch (field->type()) {
      case FieldDescriptor::TYPE_DOUBLE:
        return &MessageToJsonState::MapDoubleFieldToValue;
      case FieldDescriptor::TYPE_FLOAT:
        return &MessageToJsonState::MapFloatFieldToValue;
      case FieldDescriptor::TYPE_FIXED64:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_UINT64:
        return &MessageToJsonState::MapUInt64FieldToValue;
      case FieldDescriptor::TYPE_BOOL:
        return &MessageToJsonState::MapBoolFieldToValue;
      case FieldDescriptor::TYPE_STRING:
        return &MessageToJsonState::MapStringFieldToValue;
      case FieldDescriptor::TYPE_GROUP:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_MESSAGE:
        return &MessageToJsonState::MapMessageFieldToValue;
      case FieldDescriptor::TYPE_BYTES:
        return &MessageToJsonState::MapBytesFieldToValue;
      case FieldDescriptor::TYPE_FIXED32:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_UINT32:
        return &MessageToJsonState::MapUInt32FieldToValue;
      case FieldDescriptor::TYPE_ENUM: {
        const auto* enum_descriptor = field->enum_type();
        if (enum_descriptor->full_name() == "google.protobuf.NullValue") {
          return &MessageToJsonState::MapNullFieldToValue;
        } else {
          return &MessageToJsonState::MapEnumFieldToValue;
        }
      }
      case FieldDescriptor::TYPE_SFIXED32:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_SINT32:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_INT32:
        return &MessageToJsonState::MapInt32FieldToValue;
      case FieldDescriptor::TYPE_SFIXED64:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_SINT64:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_INT64:
        return &MessageToJsonState::MapInt64FieldToValue;
      default:
        return absl::InvalidArgumentError(absl::StrCat(
            "unexpected message field type: ", field->type_name()));
    }
  }

  absl::Status MapBoolFieldToValue(
      const google::protobuf::MapValueConstRef& value,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(value.type(), field->cpp_type());
    ABSL_DCHECK(!field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_BOOL);
    SetBoolValue(result, value.GetBoolValue());
    return absl::OkStatus();
  }

  absl::Status MapInt32FieldToValue(
      const google::protobuf::MapValueConstRef& value,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(value.type(), field->cpp_type());
    ABSL_DCHECK(!field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_INT32);
    SetNumberValue(result, value.GetInt32Value());
    return absl::OkStatus();
  }

  absl::Status MapInt64FieldToValue(
      const google::protobuf::MapValueConstRef& value,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(value.type(), field->cpp_type());
    ABSL_DCHECK(!field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_INT64);
    SetNumberValue(result, value.GetInt64Value());
    return absl::OkStatus();
  }

  absl::Status MapUInt32FieldToValue(
      const google::protobuf::MapValueConstRef& value,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(value.type(), field->cpp_type());
    ABSL_DCHECK(!field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_UINT32);
    SetNumberValue(result, value.GetUInt32Value());
    return absl::OkStatus();
  }

  absl::Status MapUInt64FieldToValue(
      const google::protobuf::MapValueConstRef& value,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(value.type(), field->cpp_type());
    ABSL_DCHECK(!field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_UINT64);
    SetNumberValue(result, value.GetUInt64Value());
    return absl::OkStatus();
  }

  absl::Status MapFloatFieldToValue(
      const google::protobuf::MapValueConstRef& value,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(value.type(), field->cpp_type());
    ABSL_DCHECK(!field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_FLOAT);
    SetNumberValue(result, value.GetFloatValue());
    return absl::OkStatus();
  }

  absl::Status MapDoubleFieldToValue(
      const google::protobuf::MapValueConstRef& value,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(value.type(), field->cpp_type());
    ABSL_DCHECK(!field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_DOUBLE);
    SetNumberValue(result, value.GetDoubleValue());
    return absl::OkStatus();
  }

  absl::Status MapBytesFieldToValue(
      const google::protobuf::MapValueConstRef& value,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(value.type(), field->cpp_type());
    ABSL_DCHECK(!field->is_repeated());
    ABSL_DCHECK_EQ(field->type(), FieldDescriptor::TYPE_BYTES);
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_STRING);
    SetStringValueFromBytes(result, value.GetStringValue());
    return absl::OkStatus();
  }

  absl::Status MapStringFieldToValue(
      const google::protobuf::MapValueConstRef& value,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(value.type(), field->cpp_type());
    ABSL_DCHECK(!field->is_repeated());
    ABSL_DCHECK_EQ(field->type(), FieldDescriptor::TYPE_STRING);
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_STRING);
    SetStringValue(result, value.GetStringValue());
    return absl::OkStatus();
  }

  absl::Status MapMessageFieldToValue(
      const google::protobuf::MapValueConstRef& value,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(value.type(), field->cpp_type());
    ABSL_DCHECK(!field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_MESSAGE);
    return ToJson(value.GetMessageValue(), result);
  }

  absl::Status MapEnumFieldToValue(
      const google::protobuf::MapValueConstRef& value,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(value.type(), field->cpp_type());
    ABSL_DCHECK(!field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_ENUM);
    ABSL_DCHECK_NE(field->enum_type()->full_name(),
                   "google.protobuf.NullValue");
    if (const auto* value_descriptor =
            field->enum_type()->FindValueByNumber(value.GetEnumValue());
        value_descriptor != nullptr) {
      SetStringValue(result, value_descriptor->name());
    } else {
      SetNumberValue(result, value.GetEnumValue());
    }
    return absl::OkStatus();
  }

  absl::Status MapNullFieldToValue(
      const google::protobuf::MapValueConstRef& value,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(value.type(), field->cpp_type());
    ABSL_DCHECK(!field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_ENUM);
    ABSL_DCHECK_EQ(field->enum_type()->full_name(),
                   "google.protobuf.NullValue");
    SetNullValue(result);
    return absl::OkStatus();
  }

  absl::StatusOr<RepeatedFieldToValue> GetRepeatedFieldToValue(
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field) {
    switch (field->type()) {
      case FieldDescriptor::TYPE_DOUBLE:
        return &MessageToJsonState::RepeatedDoubleFieldToValue;
      case FieldDescriptor::TYPE_FLOAT:
        return &MessageToJsonState::RepeatedFloatFieldToValue;
      case FieldDescriptor::TYPE_FIXED64:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_UINT64:
        return &MessageToJsonState::RepeatedUInt64FieldToValue;
      case FieldDescriptor::TYPE_BOOL:
        return &MessageToJsonState::RepeatedBoolFieldToValue;
      case FieldDescriptor::TYPE_STRING:
        return &MessageToJsonState::RepeatedStringFieldToValue;
      case FieldDescriptor::TYPE_GROUP:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_MESSAGE:
        return &MessageToJsonState::RepeatedMessageFieldToValue;
      case FieldDescriptor::TYPE_BYTES:
        return &MessageToJsonState::RepeatedBytesFieldToValue;
      case FieldDescriptor::TYPE_FIXED32:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_UINT32:
        return &MessageToJsonState::RepeatedUInt32FieldToValue;
      case FieldDescriptor::TYPE_ENUM: {
        const auto* enum_descriptor = field->enum_type();
        if (enum_descriptor->full_name() == "google.protobuf.NullValue") {
          return &MessageToJsonState::RepeatedNullFieldToValue;
        } else {
          return &MessageToJsonState::RepeatedEnumFieldToValue;
        }
      }
      case FieldDescriptor::TYPE_SFIXED32:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_SINT32:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_INT32:
        return &MessageToJsonState::RepeatedInt32FieldToValue;
      case FieldDescriptor::TYPE_SFIXED64:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_SINT64:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_INT64:
        return &MessageToJsonState::RepeatedInt64FieldToValue;
      default:
        return absl::InvalidArgumentError(absl::StrCat(
            "unexpected message field type: ", field->type_name()));
    }
  }

  absl::Status RepeatedBoolFieldToValue(
      absl::Nonnull<const google::protobuf::Reflection*> reflection,
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(reflection, message.GetReflection());
    ABSL_DCHECK(!field->is_map() && field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_BOOL);
    SetBoolValue(result, reflection->GetRepeatedBool(message, field, index));
    return absl::OkStatus();
  }

  absl::Status RepeatedInt32FieldToValue(
      absl::Nonnull<const google::protobuf::Reflection*> reflection,
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(reflection, message.GetReflection());
    ABSL_DCHECK(!field->is_map() && field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_INT32);
    SetNumberValue(result, reflection->GetRepeatedInt32(message, field, index));
    return absl::OkStatus();
  }

  absl::Status RepeatedInt64FieldToValue(
      absl::Nonnull<const google::protobuf::Reflection*> reflection,
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(reflection, message.GetReflection());
    ABSL_DCHECK(!field->is_map() && field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_INT64);
    SetNumberValue(result, reflection->GetRepeatedInt64(message, field, index));
    return absl::OkStatus();
  }

  absl::Status RepeatedUInt32FieldToValue(
      absl::Nonnull<const google::protobuf::Reflection*> reflection,
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(reflection, message.GetReflection());
    ABSL_DCHECK(!field->is_map() && field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_UINT32);
    SetNumberValue(result,
                   reflection->GetRepeatedUInt32(message, field, index));
    return absl::OkStatus();
  }

  absl::Status RepeatedUInt64FieldToValue(
      absl::Nonnull<const google::protobuf::Reflection*> reflection,
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(reflection, message.GetReflection());
    ABSL_DCHECK(!field->is_map() && field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_UINT64);
    SetNumberValue(result,
                   reflection->GetRepeatedUInt64(message, field, index));
    return absl::OkStatus();
  }

  absl::Status RepeatedFloatFieldToValue(
      absl::Nonnull<const google::protobuf::Reflection*> reflection,
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(reflection, message.GetReflection());
    ABSL_DCHECK(!field->is_map() && field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_FLOAT);
    SetNumberValue(result, reflection->GetRepeatedFloat(message, field, index));
    return absl::OkStatus();
  }

  absl::Status RepeatedDoubleFieldToValue(
      absl::Nonnull<const google::protobuf::Reflection*> reflection,
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(reflection, message.GetReflection());
    ABSL_DCHECK(!field->is_map() && field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_DOUBLE);
    SetNumberValue(result,
                   reflection->GetRepeatedDouble(message, field, index));
    return absl::OkStatus();
  }

  absl::Status RepeatedBytesFieldToValue(
      absl::Nonnull<const google::protobuf::Reflection*> reflection,
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(reflection, message.GetReflection());
    ABSL_DCHECK(!field->is_map() && field->is_repeated());
    ABSL_DCHECK_EQ(field->type(), FieldDescriptor::TYPE_BYTES);
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_STRING);
    absl::visit(absl::Overload(
                    [&](absl::string_view string) -> void {
                      SetStringValueFromBytes(result, string);
                    },
                    [&](absl::Cord&& cord) -> void {
                      SetStringValueFromBytes(result, cord);
                    }),
                AsVariant(GetRepeatedBytesField(reflection, message, field,
                                                index, scratch_)));
    return absl::OkStatus();
  }

  absl::Status RepeatedStringFieldToValue(
      absl::Nonnull<const google::protobuf::Reflection*> reflection,
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(reflection, message.GetReflection());
    ABSL_DCHECK(!field->is_map() && field->is_repeated());
    ABSL_DCHECK_EQ(field->type(), FieldDescriptor::TYPE_STRING);
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_STRING);
    absl::visit(
        absl::Overload(
            [&](absl::string_view string) -> void {
              SetStringValue(result, string);
            },
            [&](absl::Cord&& cord) -> void { SetStringValue(result, cord); }),
        AsVariant(GetRepeatedStringField(reflection, message, field, index,
                                         scratch_)));
    return absl::OkStatus();
  }

  absl::Status RepeatedMessageFieldToValue(
      absl::Nonnull<const google::protobuf::Reflection*> reflection,
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(reflection, message.GetReflection());
    ABSL_DCHECK(!field->is_map() && field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_MESSAGE);
    return ToJson(reflection->GetRepeatedMessage(message, field, index),
                  result);
  }

  absl::Status RepeatedEnumFieldToValue(
      absl::Nonnull<const google::protobuf::Reflection*> reflection,
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(reflection, message.GetReflection());
    ABSL_DCHECK(!field->is_map() && field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_ENUM);
    ABSL_DCHECK_NE(field->enum_type()->full_name(),
                   "google.protobuf.NullValue");
    if (const auto* value = reflection->GetRepeatedEnum(message, field, index);
        value != nullptr) {
      SetStringValue(result, value->name());
    } else {
      SetNumberValue(result,
                     reflection->GetRepeatedEnumValue(message, field, index));
    }
    return absl::OkStatus();
  }

  absl::Status RepeatedNullFieldToValue(
      absl::Nonnull<const google::protobuf::Reflection*> reflection,
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field, int index,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    ABSL_DCHECK_EQ(reflection, message.GetReflection());
    ABSL_DCHECK(!field->is_map() && field->is_repeated());
    ABSL_DCHECK_EQ(field->cpp_type(), FieldDescriptor::CPPTYPE_ENUM);
    ABSL_DCHECK_EQ(field->enum_type()->full_name(),
                   "google.protobuf.NullValue");
    SetNullValue(result);
    return absl::OkStatus();
  }

  absl::Status MessageMapFieldToJson(
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    const auto* reflection = message.GetReflection();
    if (reflection->FieldSize(message, field) == 0) {
      return absl::OkStatus();
    }
    const auto key_to_string =
        GetMapFieldKeyToString(field->message_type()->map_key());
    const auto* value_descriptor = field->message_type()->map_value();
    CEL_ASSIGN_OR_RETURN(const auto value_to_value,
                         GetMapFieldValueToValue(value_descriptor));
    auto begin =
        extensions::protobuf_internal::MapBegin(*reflection, message, *field);
    const auto end =
        extensions::protobuf_internal::MapEnd(*reflection, message, *field);
    for (; begin != end; ++begin) {
      auto key = (*key_to_string)(begin.GetKey());
      CEL_RETURN_IF_ERROR((this->*value_to_value)(
          begin.GetValueRef(), value_descriptor, InsertField(result, key)));
    }
    return absl::OkStatus();
  }

  absl::Status MessageRepeatedFieldToJson(
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    const auto* reflection = message.GetReflection();
    const int size = reflection->FieldSize(message, field);
    if (size == 0) {
      return absl::OkStatus();
    }
    ReserveValues(result, size);
    CEL_ASSIGN_OR_RETURN(const auto to_value, GetRepeatedFieldToValue(field));
    for (int index = 0; index < size; ++index) {
      CEL_RETURN_IF_ERROR((this->*to_value)(reflection, message, field, index,
                                            AddValues(result)));
    }
    return absl::OkStatus();
  }

  absl::Status MessageFieldToJson(
      const google::protobuf::Message& message,
      absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
      absl::Nonnull<google::protobuf::MessageLite*> result) {
    if (field->is_map()) {
      return MessageMapFieldToJson(message, field, MutableStructValue(result));
    }
    if (field->is_repeated()) {
      return MessageRepeatedFieldToJson(message, field,
                                        MutableListValue(result));
    }
    const auto* reflection = message.GetReflection();
    switch (field->type()) {
      case FieldDescriptor::TYPE_DOUBLE:
        SetNumberValue(result, reflection->GetDouble(message, field));
        break;
      case FieldDescriptor::TYPE_FLOAT:
        SetNumberValue(result, reflection->GetFloat(message, field));
        break;
      case FieldDescriptor::TYPE_FIXED64:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_UINT64:
        SetNumberValue(result, reflection->GetUInt64(message, field));
        break;
      case FieldDescriptor::TYPE_BOOL:
        SetBoolValue(result, reflection->GetBool(message, field));
        break;
      case FieldDescriptor::TYPE_STRING:
        StringValueToJson(
            well_known_types::GetStringField(message, field, scratch_), result);
        break;
      case FieldDescriptor::TYPE_GROUP:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_MESSAGE:
        return ToJson(reflection->GetMessage(message, field), result);
      case FieldDescriptor::TYPE_BYTES:
        BytesValueToJson(
            well_known_types::GetBytesField(message, field, scratch_), result);
        break;
      case FieldDescriptor::TYPE_FIXED32:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_UINT32:
        SetNumberValue(result, reflection->GetUInt32(message, field));
        break;
      case FieldDescriptor::TYPE_ENUM: {
        const auto* enum_descriptor = field->enum_type();
        if (enum_descriptor->full_name() == "google.protobuf.NullValue") {
          SetNullValue(result);
        } else {
          const auto* enum_value_descriptor =
              reflection->GetEnum(message, field);
          if (enum_value_descriptor != nullptr) {
            SetStringValue(result, enum_value_descriptor->name());
          } else {
            SetNumberValue(result, reflection->GetEnumValue(message, field));
          }
        }
      } break;
      case FieldDescriptor::TYPE_SFIXED32:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_SINT32:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_INT32:
        SetNumberValue(result, reflection->GetInt32(message, field));
        break;
      case FieldDescriptor::TYPE_SFIXED64:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_SINT64:
        ABSL_FALLTHROUGH_INTENDED;
      case FieldDescriptor::TYPE_INT64:
        SetNumberValue(result, reflection->GetInt64(message, field));
        break;
      default:
        return absl::InvalidArgumentError(absl::StrCat(
            "unexpected message field type: ", field->type_name()));
    }
    return absl::OkStatus();
  }

  absl::Status MessageToJson(const google::protobuf::Message& message,
                             absl::Nonnull<google::protobuf::MessageLite*> result) {
    std::vector<const google::protobuf::FieldDescriptor*> fields;
    const auto* reflection = message.GetReflection();
    reflection->ListFields(message, &fields);
    if (!fields.empty()) {
      for (const auto* field : fields) {
        CEL_RETURN_IF_ERROR(MessageFieldToJson(
            message, field, InsertField(result, field->json_name())));
      }
    }
    return absl::OkStatus();
  }

  void StringValueToJson(const well_known_types::StringValue& value,
                         absl::Nonnull<google::protobuf::MessageLite*> result) const {
    absl::visit(absl::Overload([&](absl::string_view string)
                                   -> void { SetStringValue(result, string); },
                               [&](const absl::Cord& cord) -> void {
                                 SetStringValue(result, cord);
                               }),
                AsVariant(value));
  }

  void BytesValueToJson(const well_known_types::BytesValue& value,
                        absl::Nonnull<google::protobuf::MessageLite*> result) const {
    absl::visit(absl::Overload(
                    [&](absl::string_view string) -> void {
                      SetStringValueFromBytes(result, string);
                    },
                    [&](const absl::Cord& cord) -> void {
                      SetStringValueFromBytes(result, cord);
                    }),
                AsVariant(value));
  }

  virtual void SetNullValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const = 0;

  virtual void SetBoolValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                            bool value) const = 0;

  virtual void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                              double value) const = 0;

  void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      float value) const {
    SetNumberValue(message, static_cast<double>(value));
  }

  virtual void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                              int64_t value) const = 0;

  void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      int32_t value) const {
    SetNumberValue(message, static_cast<double>(value));
  }

  virtual void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                              uint64_t value) const = 0;

  void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      uint32_t value) const {
    SetNumberValue(message, static_cast<double>(value));
  }

  virtual void SetStringValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                              absl::string_view value) const = 0;

  virtual void SetStringValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                              const absl::Cord& value) const = 0;

  void SetStringValueFromBytes(absl::Nonnull<google::protobuf::MessageLite*> message,
                               absl::string_view value) const {
    if (value.empty()) {
      SetStringValue(message, value);
      return;
    }
    SetStringValue(message, absl::Base64Escape(value));
  }

  void SetStringValueFromBytes(absl::Nonnull<google::protobuf::MessageLite*> message,
                               const absl::Cord& value) const {
    if (value.empty()) {
      SetStringValue(message, value);
      return;
    }
    if (auto flat = value.TryFlat(); flat) {
      SetStringValue(message, absl::Base64Escape(*flat));
      return;
    }
    SetStringValue(message,
                   absl::Base64Escape(static_cast<std::string>(value)));
  }

  virtual absl::Nonnull<google::protobuf::MessageLite*> MutableListValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const = 0;

  virtual absl::Nonnull<google::protobuf::MessageLite*> MutableStructValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const = 0;

  virtual void ReserveValues(absl::Nonnull<google::protobuf::MessageLite*> message,
                             int capacity) const = 0;

  virtual absl::Nonnull<google::protobuf::MessageLite*> AddValues(
      absl::Nonnull<google::protobuf::MessageLite*> message) const = 0;

  virtual absl::Nonnull<google::protobuf::MessageLite*> InsertField(
      absl::Nonnull<google::protobuf::MessageLite*> message,
      absl::string_view name) const = 0;

  absl::Nonnull<const google::protobuf::DescriptorPool*> const descriptor_pool_;
  absl::Nonnull<google::protobuf::MessageFactory*> const message_factory_;
  std::string scratch_;
  Reflection reflection_;
};

class GeneratedMessageToJsonState final : public MessageToJsonState {
 public:
  using MessageToJsonState::MessageToJsonState;

  absl::Status Initialize(
      absl::Nonnull<google::protobuf::MessageLite*> message) override {
    // Nothing to do.
    return absl::OkStatus();
  }

 private:
  void SetNullValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const override {
    ValueReflection::SetNullValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message));
  }

  void SetBoolValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                    bool value) const override {
    ValueReflection::SetBoolValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message), value);
  }

  void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      double value) const override {
    ValueReflection::SetNumberValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message), value);
  }

  void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      int64_t value) const override {
    ValueReflection::SetNumberValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message), value);
  }

  void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      uint64_t value) const override {
    ValueReflection::SetNumberValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message), value);
  }

  void SetStringValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      absl::string_view value) const override {
    ValueReflection::SetStringValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message), value);
  }

  void SetStringValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      const absl::Cord& value) const override {
    ValueReflection::SetStringValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message), value);
  }

  absl::Nonnull<google::protobuf::MessageLite*> MutableListValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const override {
    return ValueReflection::MutableListValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message));
  }

  absl::Nonnull<google::protobuf::MessageLite*> MutableStructValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const override {
    return ValueReflection::MutableStructValue(
        google::protobuf::DownCastMessage<google::protobuf::Value>(message));
  }

  void ReserveValues(absl::Nonnull<google::protobuf::MessageLite*> message,
                     int capacity) const override {
    ListValueReflection::ReserveValues(
        google::protobuf::DownCastMessage<google::protobuf::ListValue>(message),
        capacity);
  }

  absl::Nonnull<google::protobuf::MessageLite*> AddValues(
      absl::Nonnull<google::protobuf::MessageLite*> message) const override {
    return ListValueReflection::AddValues(
        google::protobuf::DownCastMessage<google::protobuf::ListValue>(message));
  }

  absl::Nonnull<google::protobuf::MessageLite*> InsertField(
      absl::Nonnull<google::protobuf::MessageLite*> message,
      absl::string_view name) const override {
    return StructReflection::InsertField(
        google::protobuf::DownCastMessage<google::protobuf::Struct>(message), name);
  }
};

class DynamicMessageToJsonState final : public MessageToJsonState {
 public:
  using MessageToJsonState::MessageToJsonState;

  absl::Status Initialize(
      absl::Nonnull<google::protobuf::MessageLite*> message) override {
    CEL_RETURN_IF_ERROR(value_reflection_.Initialize(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message)->GetDescriptor()));
    CEL_RETURN_IF_ERROR(list_value_reflection_.Initialize(
        value_reflection_.GetListValueDescriptor()));
    CEL_RETURN_IF_ERROR(
        struct_reflection_.Initialize(value_reflection_.GetStructDescriptor()));
    return absl::OkStatus();
  }

 private:
  void SetNullValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const override {
    value_reflection_.SetNullValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message));
  }

  void SetBoolValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                    bool value) const override {
    value_reflection_.SetBoolValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message), value);
  }

  void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      double value) const override {
    value_reflection_.SetNumberValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message), value);
  }

  void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      int64_t value) const override {
    value_reflection_.SetNumberValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message), value);
  }

  void SetNumberValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      uint64_t value) const override {
    value_reflection_.SetNumberValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message), value);
  }

  void SetStringValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      absl::string_view value) const override {
    value_reflection_.SetStringValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message), value);
  }

  void SetStringValue(absl::Nonnull<google::protobuf::MessageLite*> message,
                      const absl::Cord& value) const override {
    value_reflection_.SetStringValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message), value);
  }

  absl::Nonnull<google::protobuf::MessageLite*> MutableListValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const override {
    return value_reflection_.MutableListValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message));
  }

  absl::Nonnull<google::protobuf::MessageLite*> MutableStructValue(
      absl::Nonnull<google::protobuf::MessageLite*> message) const override {
    return value_reflection_.MutableStructValue(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message));
  }

  void ReserveValues(absl::Nonnull<google::protobuf::MessageLite*> message,
                     int capacity) const override {
    list_value_reflection_.ReserveValues(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message), capacity);
  }

  absl::Nonnull<google::protobuf::MessageLite*> AddValues(
      absl::Nonnull<google::protobuf::MessageLite*> message) const override {
    return list_value_reflection_.AddValues(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message));
  }

  absl::Nonnull<google::protobuf::MessageLite*> InsertField(
      absl::Nonnull<google::protobuf::MessageLite*> message,
      absl::string_view name) const override {
    return struct_reflection_.InsertField(
        google::protobuf::DownCastMessage<google::protobuf::Message>(message), name);
  }

  ValueReflection value_reflection_;
  ListValueReflection list_value_reflection_;
  StructReflection struct_reflection_;
};

}  // namespace

absl::Status MessageToJson(
    const google::protobuf::Message& message,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Value*> result) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(result != nullptr);
  auto state = std::make_unique<GeneratedMessageToJsonState>(descriptor_pool,
                                                             message_factory);
  CEL_RETURN_IF_ERROR(state->Initialize(result));
  return state->ToJson(message, result);
}

absl::Status MessageToJson(
    const google::protobuf::Message& message,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Message*> result) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(result != nullptr);
  auto state = std::make_unique<DynamicMessageToJsonState>(descriptor_pool,
                                                           message_factory);
  CEL_RETURN_IF_ERROR(state->Initialize(result));
  return state->ToJson(message, result);
}

absl::Status MessageFieldToJson(
    const google::protobuf::Message& message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Value*> result) {
  ABSL_DCHECK_EQ(field->containing_type(), message.GetDescriptor());
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(result != nullptr);
  auto state = std::make_unique<GeneratedMessageToJsonState>(descriptor_pool,
                                                             message_factory);
  CEL_RETURN_IF_ERROR(state->Initialize(result));
  return state->FieldToJson(message, field, result);
}

absl::Status MessageFieldToJson(
    const google::protobuf::Message& message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Message*> result) {
  ABSL_DCHECK_EQ(field->containing_type(), message.GetDescriptor());
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(result != nullptr);
  auto state = std::make_unique<DynamicMessageToJsonState>(descriptor_pool,
                                                           message_factory);
  CEL_RETURN_IF_ERROR(state->Initialize(result));
  return state->FieldToJson(message, field, result);
}

namespace {

struct DynamicProtoJsonToNativeJsonState {
  ValueReflection value_reflection;
  ListValueReflection list_value_reflection;
  StructReflection struct_reflection;
  std::string scratch;

  absl::Status Initialize(const google::protobuf::Message& proto) {
    CEL_RETURN_IF_ERROR(value_reflection.Initialize(proto.GetDescriptor()));
    CEL_RETURN_IF_ERROR(list_value_reflection.Initialize(
        value_reflection.GetListValueDescriptor()));
    CEL_RETURN_IF_ERROR(
        struct_reflection.Initialize(value_reflection.GetStructDescriptor()));
    return absl::OkStatus();
  }

  absl::Status InitializeListValue(const google::protobuf::Message& proto) {
    CEL_RETURN_IF_ERROR(
        list_value_reflection.Initialize(proto.GetDescriptor()));
    CEL_RETURN_IF_ERROR(value_reflection.Initialize(
        list_value_reflection.GetValueDescriptor()));
    CEL_RETURN_IF_ERROR(
        struct_reflection.Initialize(value_reflection.GetStructDescriptor()));
    return absl::OkStatus();
  }

  absl::Status InitializeStruct(const google::protobuf::Message& proto) {
    CEL_RETURN_IF_ERROR(struct_reflection.Initialize(proto.GetDescriptor()));
    CEL_RETURN_IF_ERROR(
        value_reflection.Initialize(struct_reflection.GetValueDescriptor()));
    CEL_RETURN_IF_ERROR(list_value_reflection.Initialize(
        value_reflection.GetListValueDescriptor()));
    return absl::OkStatus();
  }

  absl::StatusOr<Json> ToNativeJson(const google::protobuf::Message& proto) {
    const auto kind_case = value_reflection.GetKindCase(proto);
    switch (kind_case) {
      case google::protobuf::Value::KIND_NOT_SET:
        ABSL_FALLTHROUGH_INTENDED;
      case google::protobuf::Value::kNullValue:
        return kJsonNull;
      case google::protobuf::Value::kBoolValue:
        return JsonBool(value_reflection.GetBoolValue(proto));
      case google::protobuf::Value::kNumberValue:
        return JsonNumber(value_reflection.GetNumberValue(proto));
      case google::protobuf::Value::kStringValue:
        return absl::visit(
            absl::Overload(
                [](absl::string_view string) -> JsonString {
                  return JsonString(string);
                },
                [](absl::Cord&& cord) -> JsonString { return cord; }),
            AsVariant(value_reflection.GetStringValue(proto, scratch)));
      case google::protobuf::Value::kListValue:
        return ToNativeJsonList(value_reflection.GetListValue(proto));
      case google::protobuf::Value::kStructValue:
        return ToNativeJsonMap(value_reflection.GetStructValue(proto));
      default:
        return absl::InvalidArgumentError(
            absl::StrCat("unexpected value kind case: ", kind_case));
    }
  }

  absl::StatusOr<JsonArray> ToNativeJsonList(const google::protobuf::Message& proto) {
    const int proto_size = list_value_reflection.ValuesSize(proto);
    JsonArrayBuilder builder;
    builder.reserve(static_cast<size_t>(proto_size));
    for (int i = 0; i < proto_size; ++i) {
      CEL_ASSIGN_OR_RETURN(
          auto value, ToNativeJson(list_value_reflection.Values(proto, i)));
      builder.push_back(std::move(value));
    }
    return std::move(builder).Build();
  }

  absl::StatusOr<JsonObject> ToNativeJsonMap(const google::protobuf::Message& proto) {
    const int proto_size = struct_reflection.FieldsSize(proto);
    JsonObjectBuilder builder;
    builder.reserve(static_cast<size_t>(proto_size));
    auto struct_proto_begin = struct_reflection.BeginFields(proto);
    auto struct_proto_end = struct_reflection.EndFields(proto);
    for (; struct_proto_begin != struct_proto_end; ++struct_proto_begin) {
      CEL_ASSIGN_OR_RETURN(
          auto value,
          ToNativeJson(struct_proto_begin.GetValueRef().GetMessageValue()));
      builder.insert_or_assign(
          JsonString(struct_proto_begin.GetKey().GetStringValue()),
          std::move(value));
    }
    return std::move(builder).Build();
  }
};

}  // namespace

absl::StatusOr<Json> ProtoJsonToNativeJson(const google::protobuf::Message& proto) {
  DynamicProtoJsonToNativeJsonState state;
  CEL_RETURN_IF_ERROR(state.Initialize(proto));
  return state.ToNativeJson(proto);
}

absl::StatusOr<Json> ProtoJsonToNativeJson(
    const google::protobuf::Value& proto) {
  const auto kind_case = ValueReflection::GetKindCase(proto);
  switch (kind_case) {
    case google::protobuf::Value::KIND_NOT_SET:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Value::kNullValue:
      return kJsonNull;
    case google::protobuf::Value::kBoolValue:
      return JsonBool(ValueReflection::GetBoolValue(proto));
    case google::protobuf::Value::kNumberValue:
      return JsonNumber(ValueReflection::GetNumberValue(proto));
    case google::protobuf::Value::kStringValue:
      return JsonString(ValueReflection::GetStringValue(proto));
    case google::protobuf::Value::kListValue:
      return ProtoJsonListToNativeJsonList(
          ValueReflection::GetListValue(proto));
    case google::protobuf::Value::kStructValue:
      return ProtoJsonMapToNativeJsonMap(
          ValueReflection::GetStructValue(proto));
    default:
      return absl::InvalidArgumentError(
          absl::StrCat("unexpected value kind case: ", kind_case));
  }
}
absl::StatusOr<JsonArray> ProtoJsonListToNativeJsonList(
    const google::protobuf::Message& proto) {
  DynamicProtoJsonToNativeJsonState state;
  CEL_RETURN_IF_ERROR(state.InitializeListValue(proto));
  return state.ToNativeJsonList(proto);
}

absl::StatusOr<JsonArray> ProtoJsonListToNativeJsonList(
    const google::protobuf::ListValue& proto) {
  const int proto_size = ListValueReflection::ValuesSize(proto);
  JsonArrayBuilder builder;
  builder.reserve(static_cast<size_t>(proto_size));
  for (int i = 0; i < proto_size; ++i) {
    CEL_ASSIGN_OR_RETURN(
        auto value,
        ProtoJsonToNativeJson(ListValueReflection::Values(proto, i)));
    builder.push_back(std::move(value));
  }
  return std::move(builder).Build();
}

absl::StatusOr<JsonObject> ProtoJsonMapToNativeJsonMap(
    const google::protobuf::Message& proto) {
  DynamicProtoJsonToNativeJsonState state;
  CEL_RETURN_IF_ERROR(state.InitializeStruct(proto));
  return state.ToNativeJsonMap(proto);
}

absl::StatusOr<JsonObject> ProtoJsonMapToNativeJsonMap(
    const google::protobuf::Struct& proto) {
  const int proto_size = StructReflection::FieldsSize(proto);
  JsonObjectBuilder builder;
  builder.reserve(static_cast<size_t>(proto_size));
  auto struct_proto_begin = StructReflection::BeginFields(proto);
  auto struct_proto_end = StructReflection::EndFields(proto);
  for (; struct_proto_begin != struct_proto_end; ++struct_proto_begin) {
    CEL_ASSIGN_OR_RETURN(auto value,
                         ProtoJsonToNativeJson(struct_proto_begin->second));
    builder.insert_or_assign(JsonString(struct_proto_begin->first),
                             std::move(value));
  }
  return std::move(builder).Build();
}

namespace {

struct NativeJsonToProtoJsonState {
  ValueReflection value_reflection;
  ListValueReflection list_value_reflection;
  StructReflection struct_reflection;
  std::string scratch;

  absl::Status Initialize(absl::Nonnull<google::protobuf::Message*> proto) {
    CEL_RETURN_IF_ERROR(value_reflection.Initialize(proto->GetDescriptor()));
    CEL_RETURN_IF_ERROR(list_value_reflection.Initialize(
        value_reflection.GetListValueDescriptor()));
    CEL_RETURN_IF_ERROR(
        struct_reflection.Initialize(value_reflection.GetStructDescriptor()));
    return absl::OkStatus();
  }

  absl::Status ToProtoJson(const Json& json,
                           absl::Nonnull<google::protobuf::Message*> proto) {
    return absl::visit(
        absl::Overload(
            [&](JsonNull) -> absl::Status {
              value_reflection.SetNullValue(proto);
              return absl::OkStatus();
            },
            [&](JsonBool value) -> absl::Status {
              value_reflection.SetBoolValue(proto, value);
              return absl::OkStatus();
            },
            [&](JsonNumber value) -> absl::Status {
              value_reflection.SetNumberValue(proto, value);
              return absl::OkStatus();
            },
            [&](const JsonString& value) -> absl::Status {
              value_reflection.SetStringValue(proto, value);
              return absl::OkStatus();
            },
            [&](const JsonArray& value) -> absl::Status {
              auto* list_proto = value_reflection.MutableListValue(proto);
              list_value_reflection.ReserveValues(
                  list_proto, static_cast<int>(value.size()));
              for (const auto& element : value) {
                CEL_RETURN_IF_ERROR(ToProtoJson(
                    element, list_value_reflection.AddValues(list_proto)));
              }
              return absl::OkStatus();
            },
            [&](const JsonObject& value) -> absl::Status {
              auto* struct_proto = value_reflection.MutableStructValue(proto);
              for (const auto& entry : value) {
                CEL_RETURN_IF_ERROR(ToProtoJson(
                    entry.second,
                    struct_reflection.InsertField(
                        struct_proto, static_cast<std::string>(entry.first))));
              }
              return absl::OkStatus();
            }),
        json);
  }
};

}  // namespace

absl::Status NativeJsonToProtoJson(const Json& json,
                                   absl::Nonnull<google::protobuf::Message*> proto) {
  NativeJsonToProtoJsonState state;
  CEL_RETURN_IF_ERROR(state.Initialize(proto));
  return state.ToProtoJson(json, proto);
}

}  // namespace cel::internal
