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
#include "common/casting.h"
#include "common/json.h"
#include "common/value.h"
#include "internal/number.h"
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

absl::StatusOr<size_t> DoubleValue::GetSerializedSize(ValueManager&) const {
  return internal::SerializedDoubleValueSize(NativeValue());
}

absl::Status DoubleValue::SerializeTo(ValueManager&, absl::Cord& value) const {
  return internal::SerializeDoubleValue(NativeValue(), value);
}

absl::StatusOr<absl::Cord> DoubleValue::Serialize(
    ValueManager& value_manager) const {
  absl::Cord value;
  CEL_RETURN_IF_ERROR(SerializeTo(value_manager, value));
  return value;
}

absl::StatusOr<std::string> DoubleValue::GetTypeUrl(
    absl::string_view prefix) const {
  return MakeTypeUrlWithPrefix(prefix, "google.protobuf.DoubleValue");
}

absl::StatusOr<Any> DoubleValue::ConvertToAny(ValueManager& value_manager,
                                              absl::string_view prefix) const {
  CEL_ASSIGN_OR_RETURN(auto value, Serialize(value_manager));
  CEL_ASSIGN_OR_RETURN(auto type_url, GetTypeUrl(prefix));
  return MakeAny(std::move(type_url), std::move(value));
}

absl::StatusOr<Json> DoubleValue::ConvertToJson(ValueManager&) const {
  return NativeValue();
}

absl::StatusOr<ValueView> DoubleValue::Equal(ValueManager&, ValueView other,
                                             Value&) const {
  if (auto other_value = As<DoubleValueView>(other); other_value.has_value()) {
    return BoolValueView{NativeValue() == other_value->NativeValue()};
  }
  if (auto other_value = As<IntValueView>(other); other_value.has_value()) {
    return BoolValueView{
        internal::Number::FromDouble(NativeValue()) ==
        internal::Number::FromInt64(other_value->NativeValue())};
  }
  if (auto other_value = As<UintValueView>(other); other_value.has_value()) {
    return BoolValueView{
        internal::Number::FromDouble(NativeValue()) ==
        internal::Number::FromUint64(other_value->NativeValue())};
  }
  return BoolValueView{false};
}

std::string DoubleValueView::DebugString() const {
  return DoubleDebugString(NativeValue());
}

absl::StatusOr<size_t> DoubleValueView::GetSerializedSize(ValueManager&) const {
  return internal::SerializedDoubleValueSize(NativeValue());
}

absl::Status DoubleValueView::SerializeTo(ValueManager&,
                                          absl::Cord& value) const {
  return internal::SerializeDoubleValue(NativeValue(), value);
}

absl::StatusOr<absl::Cord> DoubleValueView::Serialize(
    ValueManager& value_manager) const {
  absl::Cord value;
  CEL_RETURN_IF_ERROR(SerializeTo(value_manager, value));
  return value;
}

absl::StatusOr<std::string> DoubleValueView::GetTypeUrl(
    absl::string_view prefix) const {
  return MakeTypeUrlWithPrefix(prefix, "google.protobuf.DoubleValue");
}

absl::StatusOr<Any> DoubleValueView::ConvertToAny(
    ValueManager& value_manager, absl::string_view prefix) const {
  CEL_ASSIGN_OR_RETURN(auto value, Serialize(value_manager));
  CEL_ASSIGN_OR_RETURN(auto type_url, GetTypeUrl(prefix));
  return MakeAny(std::move(type_url), std::move(value));
}

absl::StatusOr<Json> DoubleValueView::ConvertToJson(ValueManager&) const {
  return NativeValue();
}

absl::StatusOr<ValueView> DoubleValueView::Equal(ValueManager&, ValueView other,
                                                 Value&) const {
  if (auto other_value = As<DoubleValueView>(other); other_value.has_value()) {
    return BoolValueView{NativeValue() == other_value->NativeValue()};
  }
  if (auto other_value = As<IntValueView>(other); other_value.has_value()) {
    return BoolValueView{
        internal::Number::FromDouble(NativeValue()) ==
        internal::Number::FromInt64(other_value->NativeValue())};
  }
  if (auto other_value = As<UintValueView>(other); other_value.has_value()) {
    return BoolValueView{
        internal::Number::FromDouble(NativeValue()) ==
        internal::Number::FromUint64(other_value->NativeValue())};
  }
  return BoolValueView{false};
}

}  // namespace cel
