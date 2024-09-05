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

#include "common/values/parsed_repeated_field_value.h"

#include <cstddef>
#include <memory>
#include <string>

#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/log/die_if_null.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "common/json.h"
#include "common/value.h"
#include "internal/status_macros.h"
#include "google/protobuf/message.h"

namespace cel {

std::string ParsedRepeatedFieldValue::DebugString() const {
  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    return "INVALID";
  }
  return "VALID";
}

absl::Status ParsedRepeatedFieldValue::SerializeTo(
    AnyToJsonConverter& converter, absl::Cord& value) const {
  return absl::UnimplementedError("SerializeTo is not yet implemented");
}

absl::StatusOr<Json> ParsedRepeatedFieldValue::ConvertToJson(
    AnyToJsonConverter& converter) const {
  return absl::UnimplementedError("ConvertToJson is not yet implemented");
}

absl::StatusOr<JsonArray> ParsedRepeatedFieldValue::ConvertToJsonArray(
    AnyToJsonConverter& converter) const {
  return absl::UnimplementedError("ConvertToJsonArray is not yet implemented");
}

absl::Status ParsedRepeatedFieldValue::Equal(ValueManager& value_manager,
                                             const Value& other,
                                             Value& result) const {
  return absl::UnimplementedError("Equal is not yet implemented");
}

absl::StatusOr<Value> ParsedRepeatedFieldValue::Equal(
    ValueManager& value_manager, const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

bool ParsedRepeatedFieldValue::IsZeroValue() const { return IsEmpty(); }

bool ParsedRepeatedFieldValue::IsEmpty() const { return Size() == 0; }

size_t ParsedRepeatedFieldValue::Size() const {
  ABSL_DCHECK(*this);
  if (ABSL_PREDICT_FALSE(field_ == nullptr)) {
    return 0;
  }
  return static_cast<size_t>(
      GetReflectionOrDie()->FieldSize(*message_, field_));
}

// See ListValueInterface::Get for documentation.
absl::Status ParsedRepeatedFieldValue::Get(ValueManager& value_manager,
                                           size_t index, Value& result) const {
  return absl::UnimplementedError("Get is not yet implemented");
}

absl::StatusOr<Value> ParsedRepeatedFieldValue::Get(ValueManager& value_manager,
                                                    size_t index) const {
  Value result;
  CEL_RETURN_IF_ERROR(Get(value_manager, index, result));
  return result;
}

absl::Status ParsedRepeatedFieldValue::ForEach(ValueManager& value_manager,
                                               ForEachCallback callback) const {
  return absl::UnimplementedError("ForEach is not yet implemented");
}

absl::Status ParsedRepeatedFieldValue::ForEach(
    ValueManager& value_manager, ForEachWithIndexCallback callback) const {
  return absl::UnimplementedError("ForEach is not yet implemented");
}

absl::StatusOr<absl::Nonnull<std::unique_ptr<ValueIterator>>>
ParsedRepeatedFieldValue::NewIterator(ValueManager& value_manager) const {
  return absl::UnimplementedError("NewIterator is not yet implemented");
}

absl::Status ParsedRepeatedFieldValue::Contains(ValueManager& value_manager,
                                                const Value& other,
                                                Value& result) const {
  return absl::UnimplementedError("Contains is not yet implemented");
}

absl::StatusOr<Value> ParsedRepeatedFieldValue::Contains(
    ValueManager& value_manager, const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Contains(value_manager, other, result));
  return result;
}

absl::Nonnull<const google::protobuf::Reflection*>
ParsedRepeatedFieldValue::GetReflectionOrDie() const {
  return ABSL_DIE_IF_NULL(message_->GetReflection());  // Crash OK
}

}  // namespace cel
