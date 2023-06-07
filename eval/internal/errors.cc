// Copyright 2022 Google LLC
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

#include "eval/internal/errors.h"

#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "google/protobuf/arena.h"

namespace cel {
namespace runtime_internal {

const absl::Status* DurationOverflowError() {
  static const auto* const kDurationOverflow = new absl::Status(
      absl::StatusCode::kInvalidArgument, "Duration is out of range");
  return kDurationOverflow;
}

absl::Status CreateNoMatchingOverloadError(absl::string_view fn) {
  return absl::UnknownError(
      absl::StrCat(kErrNoMatchingOverload, fn.empty() ? "" : " : ", fn));
}

absl::Status CreateNoSuchFieldError(absl::string_view field) {
  return absl::Status(
      absl::StatusCode::kNotFound,
      absl::StrCat(kErrNoSuchField, field.empty() ? "" : " : ", field));
}

absl::Status CreateMissingAttributeError(
    absl::string_view missing_attribute_path) {
  absl::Status result = absl::InvalidArgumentError(
      absl::StrCat(kErrMissingAttribute, missing_attribute_path));
  result.SetPayload(kPayloadUrlMissingAttributePath,
                    absl::Cord(missing_attribute_path));
  return result;
}

absl::Status CreateNoSuchKeyError(absl::string_view key) {
  return absl::NotFoundError(absl::StrCat(kErrNoSuchKey, " : ", key));
}

absl::Status CreateUnknownFunctionResultError(absl::string_view help_message) {
  absl::Status result = absl::UnavailableError(
      absl::StrCat("Unknown function result: ", help_message));
  result.SetPayload(kPayloadUrlUnknownFunctionResult, absl::Cord("true"));
  return result;
}

absl::Status CreateError(absl::string_view message, absl::StatusCode code) {
  return absl::Status(code, message);
}

}  // namespace runtime_internal

namespace interop_internal {

using ::google::protobuf::Arena;

const absl::Status* CreateNoMatchingOverloadError(google::protobuf::Arena* arena,
                                                  absl::string_view fn) {
  return Arena::Create<absl::Status>(
      arena, runtime_internal::CreateNoMatchingOverloadError(fn));
}

const absl::Status* CreateNoSuchFieldError(google::protobuf::Arena* arena,
                                           absl::string_view field) {
  return Arena::Create<absl::Status>(
      arena, runtime_internal::CreateNoSuchFieldError(field));
}

const absl::Status* CreateNoSuchKeyError(google::protobuf::Arena* arena,
                                         absl::string_view key) {
  return Arena::Create<absl::Status>(
      arena, runtime_internal::CreateNoSuchKeyError(key));
}

const absl::Status* CreateMissingAttributeError(
    google::protobuf::Arena* arena, absl::string_view missing_attribute_path) {
  return Arena::Create<absl::Status>(
      arena,
      runtime_internal::CreateMissingAttributeError(missing_attribute_path));
}

const absl::Status* CreateUnknownFunctionResultError(
    google::protobuf::Arena* arena, absl::string_view help_message) {
  return Arena::Create<absl::Status>(
      arena, runtime_internal::CreateUnknownFunctionResultError(help_message));
}

const absl::Status* CreateError(google::protobuf::Arena* arena, absl::string_view message,
                                absl::StatusCode code) {
  return Arena::Create<absl::Status>(arena, code, message);
}

}  // namespace interop_internal
}  // namespace cel
