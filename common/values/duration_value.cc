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
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "common/any.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/value.h"
#include "internal/serialize.h"
#include "internal/status_macros.h"
#include "internal/time.h"

namespace cel {

namespace {

std::string DurationDebugString(absl::Duration value) {
  return internal::DebugStringDuration(value);
}

}  // namespace

std::string DurationValue::DebugString() const {
  return DurationDebugString(NativeValue());
}

absl::StatusOr<size_t> DurationValue::GetSerializedSize(ValueManager&) const {
  return internal::SerializedDurationSize(NativeValue());
}

absl::Status DurationValue::SerializeTo(ValueManager&,
                                        absl::Cord& value) const {
  return internal::SerializeDuration(NativeValue(), value);
}

absl::StatusOr<absl::Cord> DurationValue::Serialize(
    ValueManager& value_manager) const {
  absl::Cord value;
  CEL_RETURN_IF_ERROR(SerializeTo(value_manager, value));
  return value;
}

absl::StatusOr<std::string> DurationValue::GetTypeUrl(
    absl::string_view prefix) const {
  return MakeTypeUrlWithPrefix(prefix, "google.protobuf.Duration");
}

absl::StatusOr<Any> DurationValue::ConvertToAny(
    ValueManager& value_manager, absl::string_view prefix) const {
  CEL_ASSIGN_OR_RETURN(auto value, Serialize(value_manager));
  CEL_ASSIGN_OR_RETURN(auto type_url, GetTypeUrl(prefix));
  return MakeAny(std::move(type_url), std::move(value));
}

absl::StatusOr<Json> DurationValue::ConvertToJson(ValueManager&) const {
  CEL_ASSIGN_OR_RETURN(auto json,
                       internal::EncodeDurationToJson(NativeValue()));
  return JsonString(std::move(json));
}

absl::StatusOr<ValueView> DurationValue::Equal(ValueManager&, ValueView other,
                                               Value&) const {
  if (auto other_value = As<DurationValueView>(other);
      other_value.has_value()) {
    return BoolValueView{NativeValue() == other_value->NativeValue()};
  }
  return BoolValueView{false};
}

std::string DurationValueView::DebugString() const {
  return DurationDebugString(NativeValue());
}

absl::StatusOr<size_t> DurationValueView::GetSerializedSize(
    ValueManager&) const {
  return internal::SerializedDurationSize(NativeValue());
}

absl::Status DurationValueView::SerializeTo(ValueManager&,
                                            absl::Cord& value) const {
  return internal::SerializeDuration(NativeValue(), value);
}

absl::StatusOr<absl::Cord> DurationValueView::Serialize(
    ValueManager& value_manager) const {
  absl::Cord value;
  CEL_RETURN_IF_ERROR(SerializeTo(value_manager, value));
  return value;
}

absl::StatusOr<std::string> DurationValueView::GetTypeUrl(
    absl::string_view prefix) const {
  return MakeTypeUrlWithPrefix(prefix, "google.protobuf.Duration");
}

absl::StatusOr<Any> DurationValueView::ConvertToAny(
    ValueManager& value_manager, absl::string_view prefix) const {
  CEL_ASSIGN_OR_RETURN(auto value, Serialize(value_manager));
  CEL_ASSIGN_OR_RETURN(auto type_url, GetTypeUrl(prefix));
  return MakeAny(std::move(type_url), std::move(value));
}

absl::StatusOr<Json> DurationValueView::ConvertToJson(ValueManager&) const {
  CEL_ASSIGN_OR_RETURN(auto json,
                       internal::EncodeDurationToJson(NativeValue()));
  return JsonString(std::move(json));
}

absl::StatusOr<ValueView> DurationValueView::Equal(ValueManager&,
                                                   ValueView other,
                                                   Value&) const {
  if (auto other_value = As<DurationValueView>(other);
      other_value.has_value()) {
    return BoolValueView{NativeValue() == other_value->NativeValue()};
  }
  return BoolValueView{false};
}

}  // namespace cel
