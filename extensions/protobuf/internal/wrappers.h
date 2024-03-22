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

// Utilities for converting to and from the well known protocol buffer message
// types in `google/protobuf/wrappers.proto`.

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_WRAPPERS_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_WRAPPERS_H_

#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "google/protobuf/message.h"

namespace cel::extensions::protobuf_internal {

absl::StatusOr<bool> UnwrapDynamicBoolValueProto(
    const google::protobuf::Message& message);

absl::StatusOr<absl::Cord> UnwrapDynamicBytesValueProto(
    const google::protobuf::Message& message);

absl::StatusOr<double> UnwrapDynamicFloatValueProto(
    const google::protobuf::Message& message);

absl::StatusOr<double> UnwrapDynamicDoubleValueProto(
    const google::protobuf::Message& message);

absl::StatusOr<int64_t> UnwrapDynamicInt32ValueProto(
    const google::protobuf::Message& message);

absl::StatusOr<int64_t> UnwrapDynamicInt64ValueProto(
    const google::protobuf::Message& message);

absl::StatusOr<absl::Cord> UnwrapDynamicStringValueProto(
    const google::protobuf::Message& message);

absl::StatusOr<uint64_t> UnwrapDynamicUInt32ValueProto(
    const google::protobuf::Message& message);

absl::StatusOr<uint64_t> UnwrapDynamicUInt64ValueProto(
    const google::protobuf::Message& message);

absl::Status WrapDynamicBoolValueProto(bool value, google::protobuf::Message& message);

absl::Status WrapDynamicBytesValueProto(const absl::Cord& value,
                                        google::protobuf::Message& message);

absl::Status WrapDynamicFloatValueProto(float value, google::protobuf::Message& message);

absl::Status WrapDynamicDoubleValueProto(double value,
                                         google::protobuf::Message& message);

absl::Status WrapDynamicInt32ValueProto(int32_t value,
                                        google::protobuf::Message& message);

absl::Status WrapDynamicInt64ValueProto(int64_t value,
                                        google::protobuf::Message& message);

absl::Status WrapDynamicStringValueProto(const absl::Cord& value,
                                         google::protobuf::Message& message);

absl::Status WrapDynamicUInt32ValueProto(uint32_t value,
                                         google::protobuf::Message& message);

absl::Status WrapDynamicUInt64ValueProto(uint64_t value,
                                         google::protobuf::Message& message);

}  // namespace cel::extensions::protobuf_internal

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_WRAPPERS_H_
