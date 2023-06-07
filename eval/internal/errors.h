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
//
// Factories and constants for well-known CEL errors.
#ifndef THIRD_PARTY_CEL_CPP_EVAL_INTERNAL_ERRORS_H_
#define THIRD_PARTY_CEL_CPP_EVAL_INTERNAL_ERRORS_H_

#include "google/protobuf/arena.h"
#include "absl/status/status.h"

namespace cel {
namespace runtime_internal {

constexpr absl::string_view kErrNoMatchingOverload =
    "No matching overloads found";
constexpr absl::string_view kErrNoSuchField = "no_such_field";
constexpr absl::string_view kErrNoSuchKey = "Key not found in map";
// Error name for MissingAttributeError indicating that evaluation has
// accessed an attribute whose value is undefined. go/terminal-unknown
constexpr absl::string_view kErrMissingAttribute = "MissingAttributeError: ";
constexpr absl::string_view kPayloadUrlMissingAttributePath =
    "missing_attribute_path";
constexpr absl::string_view kPayloadUrlUnknownFunctionResult =
    "cel_is_unknown_function_result";

const absl::Status* DurationOverflowError();

// Exclusive bounds for valid duration values.
constexpr absl::Duration kDurationHigh = absl::Seconds(315576000001);
constexpr absl::Duration kDurationLow = absl::Seconds(-315576000001);

// At runtime, no matching overload could be found for a function invocation.
absl::Status CreateNoMatchingOverloadError(absl::string_view fn);

// No such field for struct access.
absl::Status CreateNoSuchFieldError(absl::string_view field);

// No such key for map access.
absl::Status CreateNoSuchKeyError(absl::string_view key);

// A missing attribute was accessed. Attributes may be declared as missing to
// they are not well defined at evaluation time.
absl::Status CreateMissingAttributeError(
    absl::string_view missing_attribute_path);

// Function result is unknown. The evaluator may convert this to an
// UnknownValue if enabled.
absl::Status CreateUnknownFunctionResultError(absl::string_view help_message);

// The default error type uses absl::StatusCode::kUnknown. In general, a more
// specific error should be used.
absl::Status CreateError(absl::string_view message,
                         absl::StatusCode code = absl::StatusCode::kUnknown);

}  // namespace runtime_internal

namespace interop_internal {
// Factories for interop error values.
// const pointer Results are arena allocated to support interop with cel::Handle
// and expr::runtime::CelValue.
const absl::Status* CreateNoMatchingOverloadError(google::protobuf::Arena* arena,
                                                  absl::string_view fn);

const absl::Status* CreateNoSuchFieldError(google::protobuf::Arena* arena,
                                           absl::string_view field);

const absl::Status* CreateNoSuchKeyError(google::protobuf::Arena* arena,
                                         absl::string_view key);

const absl::Status* CreateUnknownValueError(google::protobuf::Arena* arena,
                                            absl::string_view unknown_path);

const absl::Status* CreateMissingAttributeError(
    google::protobuf::Arena* arena, absl::string_view missing_attribute_path);

const absl::Status* CreateUnknownFunctionResultError(
    google::protobuf::Arena* arena, absl::string_view help_message);

const absl::Status* CreateError(
    google::protobuf::Arena* arena, absl::string_view message,
    absl::StatusCode code = absl::StatusCode::kUnknown);

}  // namespace interop_internal
}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_EVAL_INTERNAL_ERRORS_H_
