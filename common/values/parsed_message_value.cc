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

#include "common/values/parsed_message_value.h"

#include <cstdint>
#include <string>
#include <utility>

#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/attribute.h"
#include "common/json.h"
#include "common/value.h"
#include "internal/status_macros.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

bool ParsedMessageValue::IsZeroValue() const {
  ABSL_DCHECK(*this);
  return ABSL_PREDICT_TRUE(value_ != nullptr) ? value_->ByteSizeLong() == 0
                                              : true;
}

std::string ParsedMessageValue::DebugString() const {
  if (ABSL_PREDICT_FALSE(value_ == nullptr)) {
    return "INVALID";
  }
  return value_->DebugString();
}

absl::Status ParsedMessageValue::SerializeTo(AnyToJsonConverter& converter,
                                             absl::Cord& value) const {
  return absl::UnimplementedError("SerializeTo is not yet implemented");
}

absl::StatusOr<Json> ParsedMessageValue::ConvertToJson(
    AnyToJsonConverter& converter) const {
  return absl::UnimplementedError("ConvertToJson is not yet implemented");
}

absl::Status ParsedMessageValue::Equal(ValueManager& value_manager,
                                       const Value& other,
                                       Value& result) const {
  return absl::UnimplementedError("Equal is not yet implemented");
}

absl::StatusOr<Value> ParsedMessageValue::Equal(ValueManager& value_manager,
                                                const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::Status ParsedMessageValue::GetFieldByName(
    ValueManager& value_manager, absl::string_view name, Value& result,
    ProtoWrapperTypeOptions unboxing_options) const {
  return absl::UnimplementedError("GetFieldByName is not yet implemented");
}

absl::StatusOr<Value> ParsedMessageValue::GetFieldByName(
    ValueManager& value_manager, absl::string_view name,
    ProtoWrapperTypeOptions unboxing_options) const {
  Value result;
  CEL_RETURN_IF_ERROR(
      GetFieldByName(value_manager, name, result, unboxing_options));
  return result;
}

absl::Status ParsedMessageValue::GetFieldByNumber(
    ValueManager& value_manager, int64_t number, Value& result,
    ProtoWrapperTypeOptions unboxing_options) const {
  return absl::UnimplementedError("GetFieldByNumber is not yet implemented");
}

absl::StatusOr<Value> ParsedMessageValue::GetFieldByNumber(
    ValueManager& value_manager, int64_t number,
    ProtoWrapperTypeOptions unboxing_options) const {
  Value result;
  CEL_RETURN_IF_ERROR(
      GetFieldByNumber(value_manager, number, result, unboxing_options));
  return result;
}

absl::StatusOr<bool> ParsedMessageValue::HasFieldByName(
    absl::string_view name) const {
  return absl::UnimplementedError("HasFieldByName is not yet implemented");
}

absl::StatusOr<bool> ParsedMessageValue::HasFieldByNumber(
    int64_t number) const {
  return absl::UnimplementedError("HasFieldByNumber is not yet implemented");
}

absl::Status ParsedMessageValue::ForEachField(
    ValueManager& value_manager, ForEachFieldCallback callback) const {
  return absl::UnimplementedError("ForEachField is not yet implemented");
}

absl::StatusOr<int> ParsedMessageValue::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test, Value& result) const {
  return absl::UnimplementedError("Qualify is not yet implemented");
}

absl::StatusOr<std::pair<Value, int>> ParsedMessageValue::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test) const {
  Value result;
  CEL_ASSIGN_OR_RETURN(
      auto count, Qualify(value_manager, qualifiers, presence_test, result));
  return std::pair{std::move(result), count};
}

}  // namespace cel
