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

#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "common/json.h"
#include "common/type.h"
#include "common/value.h"

namespace cel {

namespace {

std::string ErrorDebugString(const absl::Status& value) {
  ABSL_DCHECK(!value.ok()) << "use of moved-from ErrorValue";
  return value.ToString(absl::StatusToStringMode::kWithEverything);
}

}  // namespace

ErrorValue NoSuchFieldError(absl::string_view field) {
  return ErrorValue(absl::NotFoundError(
      absl::StrCat("no_such_field", field.empty() ? "" : " : ", field)));
}

ErrorValue NoSuchKeyError(absl::string_view key) {
  return ErrorValue(
      absl::NotFoundError(absl::StrCat("Key not found in map : ", key)));
}

ErrorValue NoSuchTypeError(absl::string_view type) {
  return ErrorValue(
      absl::NotFoundError(absl::StrCat("type not found: ", type)));
}

ErrorValue DuplicateKeyError() {
  return ErrorValue(absl::AlreadyExistsError("duplicate key in map"));
}

ErrorValue TypeConversionError(absl::string_view from, absl::string_view to) {
  return ErrorValue(absl::InvalidArgumentError(
      absl::StrCat("type conversion error from '", from, "' to '", to, "'")));
}

ErrorValue TypeConversionError(TypeView from, TypeView to) {
  return TypeConversionError(from.DebugString(), to.DebugString());
}

bool IsNoSuchField(ErrorValueView value) {
  return absl::IsNotFound(value.NativeValue()) &&
         absl::StartsWith(value.NativeValue().message(), "no_such_field");
}

bool IsNoSuchKey(ErrorValueView value) {
  return absl::IsNotFound(value.NativeValue()) &&
         absl::StartsWith(value.NativeValue().message(),
                          "Key not found in map");
}

std::string ErrorValue::DebugString() const { return ErrorDebugString(value_); }

absl::StatusOr<size_t> ErrorValue::GetSerializedSize(
    AnyToJsonConverter&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::Status ErrorValue::SerializeTo(AnyToJsonConverter&, absl::Cord&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<absl::Cord> ErrorValue::Serialize(AnyToJsonConverter&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<std::string> ErrorValue::GetTypeUrl(absl::string_view) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<Any> ErrorValue::ConvertToAny(AnyToJsonConverter&,
                                             absl::string_view) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<Json> ErrorValue::ConvertToJson(AnyToJsonConverter&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is not convertable to JSON"));
}

absl::Status ErrorValue::Equal(ValueManager&, ValueView, Value& result) const {
  result = BoolValueView{false};
  return absl::OkStatus();
}

std::string ErrorValueView::DebugString() const {
  return ErrorDebugString(*value_);
}

absl::StatusOr<size_t> ErrorValueView::GetSerializedSize(
    AnyToJsonConverter&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::Status ErrorValueView::SerializeTo(AnyToJsonConverter&,
                                         absl::Cord&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<absl::Cord> ErrorValueView::Serialize(
    AnyToJsonConverter&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<std::string> ErrorValueView::GetTypeUrl(
    absl::string_view) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<Any> ErrorValueView::ConvertToAny(AnyToJsonConverter&,
                                                 absl::string_view) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<Json> ErrorValueView::ConvertToJson(AnyToJsonConverter&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is not convertable to JSON"));
}

absl::Status ErrorValueView::Equal(ValueManager&, ValueView,
                                   Value& result) const {
  result = BoolValueView{false};
  return absl::OkStatus();
}

}  // namespace cel
