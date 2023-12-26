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
#include "common/json.h"
#include "common/type.h"
#include "common/value.h"

namespace cel {

absl::StatusOr<size_t> TypeValue::GetSerializedSize() const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::Status TypeValue::SerializeTo(absl::Cord&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<absl::Cord> TypeValue::Serialize() const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<std::string> TypeValue::GetTypeUrl(absl::string_view) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<Any> TypeValue::ConvertToAny(absl::string_view) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<Json> TypeValue::ConvertToJson() const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is not convertable to JSON"));
}

absl::StatusOr<size_t> TypeValueView::GetSerializedSize() const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::Status TypeValueView::SerializeTo(absl::Cord&) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<absl::Cord> TypeValueView::Serialize() const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<std::string> TypeValueView::GetTypeUrl(absl::string_view) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<Any> TypeValueView::ConvertToAny(absl::string_view) const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is unserializable"));
}

absl::StatusOr<Json> TypeValueView::ConvertToJson() const {
  return absl::FailedPreconditionError(
      absl::StrCat(GetTypeName(), " is not convertable to JSON"));
}

}  // namespace cel
