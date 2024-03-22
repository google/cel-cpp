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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_WRAPPERS_LITE_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_WRAPPERS_LITE_H_

#include <cstdint>

#include "google/protobuf/wrappers.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"

namespace cel::extensions::protobuf_internal {

absl::StatusOr<bool> UnwrapGeneratedBoolValueProto(
    const google::protobuf::BoolValue& message);

absl::StatusOr<absl::Cord> UnwrapGeneratedBytesValueProto(
    const google::protobuf::BytesValue& message);

absl::StatusOr<double> UnwrapGeneratedFloatValueProto(
    const google::protobuf::FloatValue& message);

absl::StatusOr<double> UnwrapGeneratedDoubleValueProto(
    const google::protobuf::DoubleValue& message);

absl::StatusOr<int64_t> UnwrapGeneratedInt32ValueProto(
    const google::protobuf::Int32Value& message);

absl::StatusOr<int64_t> UnwrapGeneratedInt64ValueProto(
    const google::protobuf::Int64Value& message);

absl::StatusOr<absl::Cord> UnwrapGeneratedStringValueProto(
    const google::protobuf::StringValue& message);

absl::StatusOr<uint64_t> UnwrapGeneratedUInt32ValueProto(
    const google::protobuf::UInt32Value& message);

absl::StatusOr<uint64_t> UnwrapGeneratedUInt64ValueProto(
    const google::protobuf::UInt64Value& message);

absl::Status WrapGeneratedBoolValueProto(bool value,
                                         google::protobuf::BoolValue& message);

absl::Status WrapGeneratedBytesValueProto(
    const absl::Cord& value, google::protobuf::BytesValue& message);

absl::Status WrapGeneratedFloatValueProto(
    float value, google::protobuf::FloatValue& message);

absl::Status WrapGeneratedDoubleValueProto(
    double value, google::protobuf::DoubleValue& message);

absl::Status WrapGeneratedInt32ValueProto(
    int32_t value, google::protobuf::Int32Value& message);

absl::Status WrapGeneratedInt64ValueProto(
    int64_t value, google::protobuf::Int64Value& message);

absl::Status WrapGeneratedStringValueProto(
    const absl::Cord& value, google::protobuf::StringValue& message);

absl::Status WrapGeneratedUInt32ValueProto(
    uint32_t value, google::protobuf::UInt32Value& message);

absl::Status WrapGeneratedUInt64ValueProto(
    uint64_t value, google::protobuf::UInt64Value& message);

}  // namespace cel::extensions::protobuf_internal

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_WRAPPERS_LITE_H_
