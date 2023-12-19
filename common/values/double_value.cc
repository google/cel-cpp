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

#include <cmath>
#include <cstddef>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "common/json.h"
#include "common/value.h"
#include "internal/serialize.h"
#include "internal/status_macros.h"

namespace cel {

namespace {

std::string DoubleDebugString(double value) {
  if (std::isfinite(value)) {
    if (std::floor(value) != value) {
      // The double is not representable as a whole number, so use
      // absl::StrCat which will add decimal places.
      return absl::StrCat(value);
    }
    // absl::StrCat historically would represent 0.0 as 0, and we want the
    // decimal places so ZetaSQL correctly assumes the type as double
    // instead of int64_t.
    std::string stringified = absl::StrCat(value);
    if (!absl::StrContains(stringified, '.')) {
      absl::StrAppend(&stringified, ".0");
    } else {
      // absl::StrCat has a decimal now? Use it directly.
    }
    return stringified;
  }
  if (std::isnan(value)) {
    return "nan";
  }
  if (std::signbit(value)) {
    return "-infinity";
  }
  return "+infinity";
}

}  // namespace

std::string DoubleValue::DebugString() const {
  return DoubleDebugString(NativeValue());
}

absl::StatusOr<size_t> DoubleValue::GetSerializedSize() const {
  return internal::SerializedDoubleValueSize(NativeValue());
}

absl::Status DoubleValue::SerializeTo(absl::Cord& value) const {
  return internal::SerializeDoubleValue(NativeValue(), value);
}

absl::StatusOr<absl::Cord> DoubleValue::Serialize() const {
  absl::Cord value;
  CEL_RETURN_IF_ERROR(SerializeTo(value));
  return value;
}

absl::StatusOr<std::string> DoubleValue::GetTypeUrl(
    absl::string_view prefix) const {
  return MakeTypeUrlWithPrefix(prefix, "google.protobuf.DoubleValue");
}

absl::StatusOr<Any> DoubleValue::ConvertToAny(absl::string_view prefix) const {
  CEL_ASSIGN_OR_RETURN(auto value, Serialize());
  CEL_ASSIGN_OR_RETURN(auto type_url, GetTypeUrl(prefix));
  return MakeAny(std::move(type_url), std::move(value));
}

absl::StatusOr<Json> DoubleValue::ConvertToJson() const {
  return NativeValue();
}

std::string DoubleValueView::DebugString() const {
  return DoubleDebugString(NativeValue());
}

absl::StatusOr<size_t> DoubleValueView::GetSerializedSize() const {
  return internal::SerializedDoubleValueSize(NativeValue());
}

absl::Status DoubleValueView::SerializeTo(absl::Cord& value) const {
  return internal::SerializeDoubleValue(NativeValue(), value);
}

absl::StatusOr<absl::Cord> DoubleValueView::Serialize() const {
  absl::Cord value;
  CEL_RETURN_IF_ERROR(SerializeTo(value));
  return value;
}

absl::StatusOr<std::string> DoubleValueView::GetTypeUrl(
    absl::string_view prefix) const {
  return MakeTypeUrlWithPrefix(prefix, "google.protobuf.DoubleValue");
}

absl::StatusOr<Any> DoubleValueView::ConvertToAny(
    absl::string_view prefix) const {
  CEL_ASSIGN_OR_RETURN(auto value, Serialize());
  CEL_ASSIGN_OR_RETURN(auto type_url, GetTypeUrl(prefix));
  return MakeAny(std::move(type_url), std::move(value));
}

absl::StatusOr<Json> DoubleValueView::ConvertToJson() const {
  return NativeValue();
}

}  // namespace cel
