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

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/value.h"
#include "internal/number.h"
#include "internal/serialize.h"
#include "internal/status_macros.h"

namespace cel::common_internal {

namespace {

std::string UintDebugString(int64_t value) { return absl::StrCat(value, "u"); }

}  // namespace

std::string UintValueBase::DebugString() const {
  return UintDebugString(NativeValue());
}

absl::StatusOr<size_t> UintValueBase::GetSerializedSize(
    AnyToJsonConverter&) const {
  return internal::SerializedUInt64ValueSize(NativeValue());
}

absl::Status UintValueBase::SerializeTo(AnyToJsonConverter&,
                                        absl::Cord& value) const {
  return internal::SerializeUInt64Value(NativeValue(), value);
}

absl::StatusOr<absl::Cord> UintValueBase::Serialize(
    AnyToJsonConverter& value_manager) const {
  absl::Cord value;
  CEL_RETURN_IF_ERROR(SerializeTo(value_manager, value));
  return value;
}

absl::StatusOr<std::string> UintValueBase::GetTypeUrl(
    absl::string_view prefix) const {
  return MakeTypeUrlWithPrefix(prefix, "google.protobuf.UInt64Value");
}

absl::StatusOr<Any> UintValueBase::ConvertToAny(
    AnyToJsonConverter& value_manager, absl::string_view prefix) const {
  CEL_ASSIGN_OR_RETURN(auto value, Serialize(value_manager));
  CEL_ASSIGN_OR_RETURN(auto type_url, GetTypeUrl(prefix));
  return MakeAny(std::move(type_url), std::move(value));
}

absl::StatusOr<Json> UintValueBase::ConvertToJson(AnyToJsonConverter&) const {
  return JsonUint(NativeValue());
}

absl::Status UintValueBase::Equal(ValueManager&, const Value& other,
                                  Value& result) const {
  if (auto other_value = As<UintValue>(other); other_value.has_value()) {
    result = BoolValue{NativeValue() == other_value->NativeValue()};
    return absl::OkStatus();
  }
  if (auto other_value = As<DoubleValue>(other); other_value.has_value()) {
    result =
        BoolValue{internal::Number::FromUint64(NativeValue()) ==
                  internal::Number::FromDouble(other_value->NativeValue())};
    return absl::OkStatus();
  }
  if (auto other_value = As<IntValue>(other); other_value.has_value()) {
    result = BoolValue{internal::Number::FromUint64(NativeValue()) ==
                       internal::Number::FromInt64(other_value->NativeValue())};
    return absl::OkStatus();
  }
  result = BoolValue{false};
  return absl::OkStatus();
}

absl::StatusOr<Value> UintValueBase::Equal(ValueManager& value_manager,
                                           const Value& other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

}  // namespace cel::common_internal
