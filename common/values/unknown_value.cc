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

#include <cstddef>
#include <string>

#include "absl/base/no_destructor.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "common/json.h"
#include "common/unknown.h"
#include "common/value.h"

namespace cel {

absl::StatusOr<size_t> UnknownValue::GetSerializedSize(
    AnyToJsonConverter&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::Status UnknownValue::SerializeTo(AnyToJsonConverter&, absl::Cord&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<absl::Cord> UnknownValue::Serialize(AnyToJsonConverter&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<std::string> UnknownValue::GetTypeUrl(absl::string_view) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<Any> UnknownValue::ConvertToAny(AnyToJsonConverter&,
                                               absl::string_view) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<Json> UnknownValue::ConvertToJson(AnyToJsonConverter&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is not convertable to JSON"));
}

absl::StatusOr<ValueView> UnknownValue::Equal(ValueManager&, ValueView,
                                              Value&) const {
  return BoolValueView{false};
}

const Unknown& UnknownValueView::Empty() {
  static const absl::NoDestructor<Unknown> empty;
  return *empty;
}

absl::StatusOr<size_t> UnknownValueView::GetSerializedSize(
    AnyToJsonConverter&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::Status UnknownValueView::SerializeTo(AnyToJsonConverter&,
                                           absl::Cord&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<absl::Cord> UnknownValueView::Serialize(
    AnyToJsonConverter&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<std::string> UnknownValueView::GetTypeUrl(
    absl::string_view) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<Any> UnknownValueView::ConvertToAny(AnyToJsonConverter&,
                                                   absl::string_view) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<Json> UnknownValueView::ConvertToJson(
    AnyToJsonConverter&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is not convertable to JSON"));
}

absl::StatusOr<ValueView> UnknownValueView::Equal(ValueManager&, ValueView,
                                                  Value&) const {
  return BoolValueView{false};
}

}  // namespace cel
