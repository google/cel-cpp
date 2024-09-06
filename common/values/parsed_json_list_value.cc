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

#include "common/values/parsed_json_list_value.h"

#include <cstddef>
#include <memory>
#include <string>

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

ParsedJsonListValue::ParsedJsonListValue()
    : ParsedJsonListValue(common_internal::WrapEternal(
          &google::protobuf::ListValue::default_instance())) {}

std::string ParsedJsonListValue::DebugString() const {
  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    return "";
  }
  if (const auto* generated_message =
          google::protobuf::DynamicCastMessage<google::protobuf::ListValue>(&*value_);
      generated_message != nullptr) {
    return absl::StrCat(*generated_message);
  }
  const auto& dynamic_message =
      google::protobuf::DownCastMessage<google::protobuf::Message>(*value_);
  std::string result;
  google::protobuf::TextFormat::PrintToString(dynamic_message, &result);
  return result;
}

absl::Status ParsedJsonListValue::SerializeTo(AnyToJsonConverter& converter,
                                              absl::Cord& value) const {
  return absl::UnimplementedError("SerializeTo is not yet implemented");
}

absl::StatusOr<Json> ParsedJsonListValue::ConvertToJson(
    AnyToJsonConverter& converter) const {
  return absl::UnimplementedError("ConvertToJson is not yet implemented");
}

absl::StatusOr<JsonArray> ParsedJsonListValue::ConvertToJsonArray(
    AnyToJsonConverter& converter) const {
  return absl::UnimplementedError("ConvertToJsonArray is not yet implemented");
}

absl::Status ParsedJsonListValue::Equal(ValueManager& value_manager,
                                        const Value& other,
                                        Value& result) const {
  return absl::UnimplementedError("Equal is not yet implemented");
}

absl::StatusOr<Value> ParsedJsonListValue::Equal(ValueManager& value_manager,
                                                 const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

bool ParsedJsonListValue::IsZeroValue() const { return IsEmpty(); }

bool ParsedJsonListValue::IsEmpty() const { return Size() == 0; }

size_t ParsedJsonListValue::Size() const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    return 0;
  }
  if (const auto* generated_message =
          google::protobuf::DynamicCastMessage<google::protobuf::ListValue>(&*value_);
      generated_message != nullptr) {
    return static_cast<size_t>(generated_message->values().size());
  }
  const auto& dynamic_message =
      google::protobuf::DownCastMessage<google::protobuf::Message>(*value_);
  const auto* descriptor = dynamic_message.GetDescriptor();
  const auto* values_field = descriptor->FindFieldByNumber(1);
  const auto* reflection = dynamic_message.GetReflection();
  return static_cast<size_t>(
      reflection->FieldSize(dynamic_message, values_field));
}

// See ListValueInterface::Get for documentation.
absl::Status ParsedJsonListValue::Get(ValueManager& value_manager, size_t index,
                                      Value& result) const {
  return absl::UnimplementedError("Get is not yet implemented");
}

absl::StatusOr<Value> ParsedJsonListValue::Get(ValueManager& value_manager,
                                               size_t index) const {
  Value result;
  CEL_RETURN_IF_ERROR(Get(value_manager, index, result));
  return result;
}

absl::Status ParsedJsonListValue::ForEach(ValueManager& value_manager,
                                          ForEachCallback callback) const {
  return absl::UnimplementedError("ForEach is not yet implemented");
}

absl::Status ParsedJsonListValue::ForEach(
    ValueManager& value_manager, ForEachWithIndexCallback callback) const {
  return absl::UnimplementedError("ForEach is not yet implemented");
}

absl::StatusOr<absl::Nonnull<std::unique_ptr<ValueIterator>>>
ParsedJsonListValue::NewIterator(ValueManager& value_manager) const {
  return absl::UnimplementedError("NewIterator is not yet implemented");
}

absl::Status ParsedJsonListValue::Contains(ValueManager& value_manager,
                                           const Value& other,
                                           Value& result) const {
  return absl::UnimplementedError("Contains is not yet implemented");
}

absl::StatusOr<Value> ParsedJsonListValue::Contains(ValueManager& value_manager,
                                                    const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Contains(value_manager, other, result));
  return result;
}

}  // namespace cel
