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
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "common/json.h"
#include "common/value.h"

namespace cel {

namespace {

std::string ErrorDebugString(const absl::Status& value) {
  ABSL_DCHECK(!value.ok()) << "use of moved-from ErrorValue";
  return value.ToString(absl::StatusToStringMode::kWithEverything);
}

}  // namespace

std::string ErrorValue::DebugString() const { return ErrorDebugString(value_); }

absl::StatusOr<size_t> ErrorValue::GetSerializedSize() const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::Status ErrorValue::SerializeTo(absl::Cord&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<absl::Cord> ErrorValue::Serialize() const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<std::string> ErrorValue::GetTypeUrl(absl::string_view) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<Any> ErrorValue::ConvertToAny(absl::string_view) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<Json> ErrorValue::ConvertToJson() const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is not convertable to JSON"));
}

std::string ErrorValueView::DebugString() const {
  return ErrorDebugString(*value_);
}

absl::StatusOr<size_t> ErrorValueView::GetSerializedSize() const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::Status ErrorValueView::SerializeTo(absl::Cord&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<absl::Cord> ErrorValueView::Serialize() const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<std::string> ErrorValueView::GetTypeUrl(
    absl::string_view) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<Any> ErrorValueView::ConvertToAny(absl::string_view) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<Json> ErrorValueView::ConvertToJson() const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is not convertable to JSON"));
}

}  // namespace cel
