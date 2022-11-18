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
#include "base/memory_manager.h"
#include "extensions/protobuf/memory_manager.h"

namespace cel::interop_internal {

using ::cel::extensions::ProtoMemoryManager;
using ::google::protobuf::Arena;

const absl::Status* DurationOverflowError() {
  static const auto* const kDurationOverflow = new absl::Status(
      absl::StatusCode::kInvalidArgument, "Duration is out of range");
  return kDurationOverflow;
}

const absl::Status* CreateNoMatchingOverloadError(cel::MemoryManager& manager,
                                                  absl::string_view fn) {
  return CreateNoMatchingOverloadError(
      ProtoMemoryManager::CastToProtoArena(manager), fn);
}

const absl::Status* CreateNoMatchingOverloadError(google::protobuf::Arena* arena,
                                                  absl::string_view fn) {
  return Arena::Create<absl::Status>(
      arena, absl::StatusCode::kUnknown,
      absl::StrCat(kErrNoMatchingOverload, fn.empty() ? "" : " : ", fn));
}

const absl::Status* CreateNoSuchFieldError(cel::MemoryManager& manager,
                                           absl::string_view field) {
  return CreateNoSuchFieldError(
      extensions::ProtoMemoryManager::CastToProtoArena(manager), field);
}

const absl::Status* CreateNoSuchFieldError(google::protobuf::Arena* arena,
                                           absl::string_view field) {
  return Arena::Create<absl::Status>(
      arena, absl::StatusCode::kNotFound,
      absl::StrCat(kErrNoSuchField, field.empty() ? "" : " : ", field));
}

const absl::Status* CreateNoSuchKeyError(cel::MemoryManager& manager,
                                         absl::string_view key) {
  return CreateNoSuchKeyError(
      extensions::ProtoMemoryManager::CastToProtoArena(manager), key);
}

const absl::Status* CreateNoSuchKeyError(google::protobuf::Arena* arena,
                                         absl::string_view key) {
  return Arena::Create<absl::Status>(arena, absl::StatusCode::kNotFound,
                                     absl::StrCat(kErrNoSuchKey, " : ", key));
}

const absl::Status* CreateUnknownValueError(google::protobuf::Arena* arena,
                                            absl::string_view unknown_path) {
  auto* error =
      Arena::Create<absl::Status>(arena, absl::StatusCode::kUnavailable,
                                  absl::StrCat(kErrUnknownValue, unknown_path));
  error->SetPayload(kPayloadUrlUnknownPath, absl::Cord(unknown_path));
  return error;
}

const absl::Status* CreateMissingAttributeError(
    google::protobuf::Arena* arena, absl::string_view missing_attribute_path) {
  auto* error = Arena::Create<absl::Status>(
      arena, absl::StatusCode::kInvalidArgument,
      absl::StrCat(kErrMissingAttribute, missing_attribute_path));
  error->SetPayload(kPayloadUrlMissingAttributePath,
                    absl::Cord(missing_attribute_path));
  return error;
}

const absl::Status* CreateMissingAttributeError(
    cel::MemoryManager& manager, absl::string_view missing_attribute_path) {
  // TODO(issues/5): assume arena-style allocator while migrating
  // to new value type.
  return CreateMissingAttributeError(
      extensions::ProtoMemoryManager::CastToProtoArena(manager),
      missing_attribute_path);
}

const absl::Status* CreateUnknownFunctionResultError(
    cel::MemoryManager& manager, absl::string_view help_message) {
  return CreateUnknownFunctionResultError(
      extensions::ProtoMemoryManager::CastToProtoArena(manager), help_message);
}

const absl::Status* CreateUnknownFunctionResultError(
    google::protobuf::Arena* arena, absl::string_view help_message) {
  auto* error = Arena::Create<absl::Status>(
      arena, absl::StatusCode::kUnavailable,
      absl::StrCat("Unknown function result: ", help_message));
  error->SetPayload(kPayloadUrlUnknownFunctionResult, absl::Cord("true"));
  return error;
}

}  // namespace cel::interop_internal