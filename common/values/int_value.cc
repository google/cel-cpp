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
#include "common/json.h"
#include "common/value.h"
#include "internal/serialize.h"
#include "internal/status_macros.h"

namespace cel {

namespace {

std::string IntDebugString(int64_t value) { return absl::StrCat(value); }

}  // namespace

std::string IntValue::DebugString() const {
  return IntDebugString(NativeValue());
}

absl::StatusOr<size_t> IntValue::GetSerializedSize() const {
  return internal::SerializedInt64ValueSize(NativeValue());
}

absl::Status IntValue::SerializeTo(absl::Cord& value) const {
  return internal::SerializeInt64Value(NativeValue(), value);
}

absl::StatusOr<absl::Cord> IntValue::Serialize() const {
  absl::Cord value;
  CEL_RETURN_IF_ERROR(SerializeTo(value));
  return value;
}

absl::StatusOr<std::string> IntValue::GetTypeUrl(
    absl::string_view prefix) const {
  return MakeTypeUrlWithPrefix(prefix, "google.protobuf.Int64Value");
}

absl::StatusOr<Any> IntValue::ConvertToAny(absl::string_view prefix) const {
  CEL_ASSIGN_OR_RETURN(auto value, Serialize());
  CEL_ASSIGN_OR_RETURN(auto type_url, GetTypeUrl(prefix));
  return MakeAny(std::move(type_url), std::move(value));
}

absl::StatusOr<Json> IntValue::ConvertToJson() const {
  return JsonInt(NativeValue());
}

std::string IntValueView::DebugString() const {
  return IntDebugString(NativeValue());
}

absl::StatusOr<size_t> IntValueView::GetSerializedSize() const {
  return internal::SerializedInt64ValueSize(NativeValue());
}

absl::Status IntValueView::SerializeTo(absl::Cord& value) const {
  return internal::SerializeInt64Value(NativeValue(), value);
}

absl::StatusOr<absl::Cord> IntValueView::Serialize() const {
  absl::Cord value;
  CEL_RETURN_IF_ERROR(SerializeTo(value));
  return value;
}

absl::StatusOr<std::string> IntValueView::GetTypeUrl(
    absl::string_view prefix) const {
  return MakeTypeUrlWithPrefix(prefix, "google.protobuf.Int64Value");
}

absl::StatusOr<Any> IntValueView::ConvertToAny(absl::string_view prefix) const {
  CEL_ASSIGN_OR_RETURN(auto value, Serialize());
  CEL_ASSIGN_OR_RETURN(auto type_url, GetTypeUrl(prefix));
  return MakeAny(std::move(type_url), std::move(value));
}

absl::StatusOr<Json> IntValueView::ConvertToJson() const {
  return JsonInt(NativeValue());
}

}  // namespace cel
