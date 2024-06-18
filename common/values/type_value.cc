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

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/type.h"
#include "common/value.h"

namespace cel {

absl::StatusOr<size_t> TypeValue::GetSerializedSize(AnyToJsonConverter&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::Status TypeValue::SerializeTo(AnyToJsonConverter&, absl::Cord&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<absl::Cord> TypeValue::Serialize(AnyToJsonConverter&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<std::string> TypeValue::GetTypeUrl(absl::string_view) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<Any> TypeValue::ConvertToAny(AnyToJsonConverter&,
                                            absl::string_view) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<Json> TypeValue::ConvertToJson(AnyToJsonConverter&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is not convertable to JSON"));
}

absl::Status TypeValue::Equal(ValueManager&, ValueView other,
                              Value& result) const {
  if (auto other_value = As<TypeValueView>(other); other_value.has_value()) {
    result = BoolValueView{NativeValue() == other_value->NativeValue()};
    return absl::OkStatus();
  }
  result = BoolValueView{false};
  return absl::OkStatus();
}

absl::StatusOr<size_t> TypeValueView::GetSerializedSize(
    AnyToJsonConverter&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::Status TypeValueView::SerializeTo(AnyToJsonConverter&,
                                        absl::Cord&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<absl::Cord> TypeValueView::Serialize(AnyToJsonConverter&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<std::string> TypeValueView::GetTypeUrl(absl::string_view) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<Any> TypeValueView::ConvertToAny(AnyToJsonConverter&,
                                                absl::string_view) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<Json> TypeValueView::ConvertToJson(AnyToJsonConverter&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is not convertable to JSON"));
}

absl::Status TypeValueView::Equal(ValueManager&, ValueView other,
                                  Value& result) const {
  if (auto other_value = As<TypeValueView>(other); other_value.has_value()) {
    result = BoolValueView{NativeValue() == other_value->NativeValue()};
    return absl::OkStatus();
  }
  result = BoolValueView{false};
  return absl::OkStatus();
}

}  // namespace cel
