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

#include "common/values/parsed_json_map_value.h"

#include <cstddef>
#include <memory>
#include <string>
#include <utility>

#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/value.h"
#include "internal/status_macros.h"
#include "google/protobuf/message.h"
#include "google/protobuf/message_lite.h"
#include "google/protobuf/text_format.h"

namespace cel {

ParsedJsonMapValue::ParsedJsonMapValue()
    : ParsedJsonMapValue(common_internal::WrapEternal(
          &google::protobuf::Struct::default_instance())) {}

std::string ParsedJsonMapValue::DebugString() const {
  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    return "";
  }
  if (const auto* generated_message =
          google::protobuf::DynamicCastMessage<google::protobuf::Struct>(&*value_);
      generated_message != nullptr) {
    return absl::StrCat(*generated_message);
  }
  const auto& dynamic_message =
      google::protobuf::DownCastMessage<google::protobuf::Message>(*value_);
  std::string result;
  google::protobuf::TextFormat::PrintToString(dynamic_message, &result);
  return result;
}

absl::Status ParsedJsonMapValue::SerializeTo(AnyToJsonConverter& converter,
                                             absl::Cord& value) const {
  return absl::UnimplementedError("SerializeTo is not yet implemented");
}

absl::StatusOr<Json> ParsedJsonMapValue::ConvertToJson(
    AnyToJsonConverter& converter) const {
  return absl::UnimplementedError("ConvertToJson is not yet implemented");
}

absl::StatusOr<JsonObject> ParsedJsonMapValue::ConvertToJsonObject(
    AnyToJsonConverter& converter) const {
  return absl::UnimplementedError("ConvertToJsonObject is not yet implemented");
}

absl::Status ParsedJsonMapValue::Equal(ValueManager& value_manager,
                                       const Value& other,
                                       Value& result) const {
  return absl::UnimplementedError("Equal is not yet implemented");
}

absl::StatusOr<Value> ParsedJsonMapValue::Equal(ValueManager& value_manager,
                                                const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

bool ParsedJsonMapValue::IsZeroValue() const { return IsEmpty(); }

bool ParsedJsonMapValue::IsEmpty() const { return Size() == 0; }

size_t ParsedJsonMapValue::Size() const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    return 0;
  }
  if (const auto* generated_message =
          google::protobuf::DynamicCastMessage<google::protobuf::Struct>(&*value_);
      generated_message != nullptr) {
    return static_cast<size_t>(generated_message->fields().size());
  }
  const auto& dynamic_message =
      google::protobuf::DownCastMessage<google::protobuf::Message>(*value_);
  const auto* descriptor = dynamic_message.GetDescriptor();
  const auto* fields_field = descriptor->FindFieldByNumber(1);
  const auto* reflection = dynamic_message.GetReflection();
  return static_cast<size_t>(
      reflection->FieldSize(dynamic_message, fields_field));
}

absl::Status ParsedJsonMapValue::Get(ValueManager& value_manager,
                                     const Value& key, Value& result) const {
  return absl::UnimplementedError("Get is not yet implemented");
}

absl::StatusOr<Value> ParsedJsonMapValue::Get(ValueManager& value_manager,
                                              const Value& key) const {
  Value result;
  CEL_RETURN_IF_ERROR(Get(value_manager, key, result));
  return result;
}

absl::StatusOr<bool> ParsedJsonMapValue::Find(ValueManager& value_manager,
                                              const Value& key,
                                              Value& result) const {
  return absl::UnimplementedError("Find is not yet implemented");
}

absl::StatusOr<std::pair<Value, bool>> ParsedJsonMapValue::Find(
    ValueManager& value_manager, const Value& key) const {
  Value result;
  CEL_ASSIGN_OR_RETURN(auto found, Find(value_manager, key, result));
  if (found) {
    return std::pair{std::move(result), found};
  }
  return std::pair{NullValue(), found};
}

absl::Status ParsedJsonMapValue::Has(ValueManager& value_manager,
                                     const Value& key, Value& result) const {
  return absl::UnimplementedError("Has is not yet implemented");
}

absl::StatusOr<Value> ParsedJsonMapValue::Has(ValueManager& value_manager,
                                              const Value& key) const {
  Value result;
  CEL_RETURN_IF_ERROR(Has(value_manager, key, result));
  return result;
}

absl::Status ParsedJsonMapValue::ListKeys(ValueManager& value_manager,
                                          ListValue& result) const {
  return absl::UnimplementedError("ListKeys is not yet implemented");
}

absl::StatusOr<ListValue> ParsedJsonMapValue::ListKeys(
    ValueManager& value_manager) const {
  ListValue result;
  CEL_RETURN_IF_ERROR(ListKeys(value_manager, result));
  return result;
}

absl::Status ParsedJsonMapValue::ForEach(ValueManager& value_manager,
                                         ForEachCallback callback) const {
  return absl::UnimplementedError("ForEach is not yet implemented");
}

absl::StatusOr<absl::Nonnull<std::unique_ptr<ValueIterator>>>
ParsedJsonMapValue::NewIterator(ValueManager& value_manager) const {
  return absl::UnimplementedError("NewIterator is not yet implemented");
}

}  // namespace cel
