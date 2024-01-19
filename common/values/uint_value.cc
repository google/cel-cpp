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

namespace cel {

namespace {

std::string UintDebugString(int64_t value) { return absl::StrCat(value, "u"); }

}  // namespace

std::string UintValue::DebugString() const {
  return UintDebugString(NativeValue());
}

absl::StatusOr<size_t> UintValue::GetSerializedSize() const {
  return internal::SerializedUInt64ValueSize(NativeValue());
}

absl::Status UintValue::SerializeTo(absl::Cord& value) const {
  return internal::SerializeUInt64Value(NativeValue(), value);
}

absl::StatusOr<absl::Cord> UintValue::Serialize() const {
  absl::Cord value;
  CEL_RETURN_IF_ERROR(SerializeTo(value));
  return value;
}

absl::StatusOr<std::string> UintValue::GetTypeUrl(
    absl::string_view prefix) const {
  return MakeTypeUrlWithPrefix(prefix, "google.protobuf.UInt64Value");
}

absl::StatusOr<Any> UintValue::ConvertToAny(absl::string_view prefix) const {
  CEL_ASSIGN_OR_RETURN(auto value, Serialize());
  CEL_ASSIGN_OR_RETURN(auto type_url, GetTypeUrl(prefix));
  return MakeAny(std::move(type_url), std::move(value));
}

absl::StatusOr<Json> UintValue::ConvertToJson() const {
  return JsonUint(NativeValue());
}

absl::StatusOr<ValueView> UintValue::Equal(ValueManager&, ValueView other,
                                           Value&) const {
  if (auto other_value = As<UintValueView>(other); other_value.has_value()) {
    return BoolValueView{NativeValue() == other_value->NativeValue()};
  }
  if (auto other_value = As<DoubleValueView>(other); other_value.has_value()) {
    return BoolValueView{
        internal::Number::FromUint64(NativeValue()) ==
        internal::Number::FromDouble(other_value->NativeValue())};
  }
  if (auto other_value = As<IntValueView>(other); other_value.has_value()) {
    return BoolValueView{
        internal::Number::FromUint64(NativeValue()) ==
        internal::Number::FromInt64(other_value->NativeValue())};
  }
  return BoolValueView{false};
}

std::string UintValueView::DebugString() const {
  return UintDebugString(NativeValue());
}

absl::StatusOr<size_t> UintValueView::GetSerializedSize() const {
  return internal::SerializedUInt64ValueSize(NativeValue());
}

absl::Status UintValueView::SerializeTo(absl::Cord& value) const {
  return internal::SerializeUInt64Value(NativeValue(), value);
}

absl::StatusOr<absl::Cord> UintValueView::Serialize() const {
  absl::Cord value;
  CEL_RETURN_IF_ERROR(SerializeTo(value));
  return value;
}

absl::StatusOr<std::string> UintValueView::GetTypeUrl(
    absl::string_view prefix) const {
  return MakeTypeUrlWithPrefix(prefix, "google.protobuf.UInt64Value");
}

absl::StatusOr<Any> UintValueView::ConvertToAny(
    absl::string_view prefix) const {
  CEL_ASSIGN_OR_RETURN(auto value, Serialize());
  CEL_ASSIGN_OR_RETURN(auto type_url, GetTypeUrl(prefix));
  return MakeAny(std::move(type_url), std::move(value));
}

absl::StatusOr<Json> UintValueView::ConvertToJson() const {
  return JsonUint(NativeValue());
}

absl::StatusOr<ValueView> UintValueView::Equal(ValueManager&, ValueView other,
                                               Value&) const {
  if (auto other_value = As<UintValueView>(other); other_value.has_value()) {
    return BoolValueView{NativeValue() == other_value->NativeValue()};
  }
  if (auto other_value = As<DoubleValueView>(other); other_value.has_value()) {
    return BoolValueView{
        internal::Number::FromUint64(NativeValue()) ==
        internal::Number::FromDouble(other_value->NativeValue())};
  }
  if (auto other_value = As<IntValueView>(other); other_value.has_value()) {
    return BoolValueView{
        internal::Number::FromUint64(NativeValue()) ==
        internal::Number::FromInt64(other_value->NativeValue())};
  }
  return BoolValueView{false};
}

}  // namespace cel
