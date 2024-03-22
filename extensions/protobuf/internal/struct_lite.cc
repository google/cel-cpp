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

#include "extensions/protobuf/internal/struct_lite.h"

#include <string>
#include <utility>

#include "google/protobuf/struct.pb.h"
#include "absl/base/attributes.h"
#include "absl/functional/overload.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/types/variant.h"
#include "common/json.h"
#include "internal/status_macros.h"

namespace cel::extensions::protobuf_internal {

absl::StatusOr<Json> GeneratedValueProtoToJson(
    const google::protobuf::Value& message) {
  switch (message.kind_case()) {
    case google::protobuf::Value::KIND_NOT_SET:
      ABSL_FALLTHROUGH_INTENDED;
    case google::protobuf::Value::kNullValue:
      return kJsonNull;
    case google::protobuf::Value::kBoolValue:
      return message.bool_value();
    case google::protobuf::Value::kNumberValue:
      return message.number_value();
    case google::protobuf::Value::kStringValue:
      return absl::Cord(message.string_value());
    case google::protobuf::Value::kStructValue:
      return GeneratedStructProtoToJson(message.struct_value());
    case google::protobuf::Value::kListValue:
      return GeneratedListValueProtoToJson(message.list_value());
    default:
      return absl::InternalError(
          absl::StrCat("unexpected google.protobuf.Value oneof kind: ",
                       message.kind_case()));
  }
}

absl::StatusOr<Json> GeneratedListValueProtoToJson(
    const google::protobuf::ListValue& message) {
  JsonArrayBuilder builder;
  builder.reserve(message.values_size());
  for (const auto& element : message.values()) {
    CEL_ASSIGN_OR_RETURN(auto value, GeneratedValueProtoToJson(element));
    builder.push_back(std::move(value));
  }
  return std::move(builder).Build();
}

absl::StatusOr<Json> GeneratedStructProtoToJson(
    const google::protobuf::Struct& message) {
  JsonObjectBuilder builder;
  builder.reserve(message.fields_size());
  for (const auto& field : message.fields()) {
    CEL_ASSIGN_OR_RETURN(auto value, GeneratedValueProtoToJson(field.second));
    builder.insert_or_assign(absl::Cord(field.first), std::move(value));
  }
  return std::move(builder).Build();
}

absl::Status GeneratedValueProtoFromJson(const Json& json,
                                         google::protobuf::Value& message) {
  return absl::visit(
      absl::Overload(
          [&message](JsonNull) {
            message.set_null_value(google::protobuf::NULL_VALUE);
            return absl::OkStatus();
          },
          [&message](JsonBool value) {
            message.set_bool_value(value);
            return absl::OkStatus();
          },
          [&message](JsonNumber value) {
            message.set_number_value(value);
            return absl::OkStatus();
          },
          [&message](const JsonString& value) {
            message.set_string_value(static_cast<std::string>(value));
            return absl::OkStatus();
          },
          [&message](const JsonArray& value) {
            return GeneratedListValueProtoFromJson(
                value, *message.mutable_list_value());
          },
          [&message](const JsonObject& value) {
            return GeneratedStructProtoFromJson(
                value, *message.mutable_struct_value());
          }),
      json);
}

absl::Status GeneratedListValueProtoFromJson(
    const JsonArray& json, google::protobuf::ListValue& message) {
  auto* elements = message.mutable_values();
  elements->Clear();
  elements->Reserve(static_cast<int>(json.size()));
  for (const auto& element : json) {
    CEL_RETURN_IF_ERROR(GeneratedValueProtoFromJson(element, *elements->Add()));
  }
  return absl::OkStatus();
}

absl::Status GeneratedStructProtoFromJson(const JsonObject& json,
                                          google::protobuf::Struct& message) {
  auto* fields = message.mutable_fields();
  fields->clear();
  for (const auto& field : json) {
    CEL_RETURN_IF_ERROR(GeneratedValueProtoFromJson(
        field.second, (*fields)[static_cast<std::string>(field.first)]));
  }
  return absl::OkStatus();
}

}  // namespace cel::extensions::protobuf_internal
