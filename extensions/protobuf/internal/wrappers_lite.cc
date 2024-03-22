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

#include "extensions/protobuf/internal/wrappers_lite.h"

#include <cstdint>
#include <string>

#include "google/protobuf/wrappers.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"

namespace cel::extensions::protobuf_internal {

absl::StatusOr<bool> UnwrapGeneratedBoolValueProto(
    const google::protobuf::BoolValue& message) {
  return message.value();
}

absl::StatusOr<absl::Cord> UnwrapGeneratedBytesValueProto(
    const google::protobuf::BytesValue& message) {
  return absl::Cord(message.value());
}

absl::StatusOr<double> UnwrapGeneratedFloatValueProto(
    const google::protobuf::FloatValue& message) {
  return message.value();
}

absl::StatusOr<double> UnwrapGeneratedDoubleValueProto(
    const google::protobuf::DoubleValue& message) {
  return message.value();
}

absl::StatusOr<int64_t> UnwrapGeneratedInt32ValueProto(
    const google::protobuf::Int32Value& message) {
  return message.value();
}

absl::StatusOr<int64_t> UnwrapGeneratedInt64ValueProto(
    const google::protobuf::Int64Value& message) {
  return message.value();
}

absl::StatusOr<absl::Cord> UnwrapGeneratedStringValueProto(
    const google::protobuf::StringValue& message) {
  return absl::Cord(message.value());
}

absl::StatusOr<uint64_t> UnwrapGeneratedUInt32ValueProto(
    const google::protobuf::UInt32Value& message) {
  return message.value();
}

absl::StatusOr<uint64_t> UnwrapGeneratedUInt64ValueProto(
    const google::protobuf::UInt64Value& message) {
  return message.value();
}

absl::Status WrapGeneratedBoolValueProto(bool value,
                                         google::protobuf::BoolValue& message) {
  message.set_value(value);
  return absl::OkStatus();
}

absl::Status WrapGeneratedBytesValueProto(
    const absl::Cord& value, google::protobuf::BytesValue& message) {
  message.set_value(static_cast<std::string>(value));
  return absl::OkStatus();
}

absl::Status WrapGeneratedFloatValueProto(
    float value, google::protobuf::FloatValue& message) {
  message.set_value(value);
  return absl::OkStatus();
}

absl::Status WrapGeneratedDoubleValueProto(
    double value, google::protobuf::DoubleValue& message) {
  message.set_value(value);
  return absl::OkStatus();
}

absl::Status WrapGeneratedInt32ValueProto(
    int32_t value, google::protobuf::Int32Value& message) {
  message.set_value(value);
  return absl::OkStatus();
}

absl::Status WrapGeneratedInt64ValueProto(
    int64_t value, google::protobuf::Int64Value& message) {
  message.set_value(value);
  return absl::OkStatus();
}

absl::Status WrapGeneratedStringValueProto(
    const absl::Cord& value, google::protobuf::StringValue& message) {
  message.set_value(static_cast<std::string>(value));
  return absl::OkStatus();
}

absl::Status WrapGeneratedUInt32ValueProto(
    uint32_t value, google::protobuf::UInt32Value& message) {
  message.set_value(value);
  return absl::OkStatus();
}

absl::Status WrapGeneratedUInt64ValueProto(
    uint64_t value, google::protobuf::UInt64Value& message) {
  message.set_value(value);
  return absl::OkStatus();
}

}  // namespace cel::extensions::protobuf_internal
