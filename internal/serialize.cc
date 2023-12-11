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

#include "internal/serialize.h"

#include <cstdint>

#include "absl/base/casts.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "common/json.h"
#include "internal/proto_wire.h"
#include "internal/status_macros.h"

namespace cel::internal {

namespace {

absl::Status SerializeDurationOrTimestamp(absl::string_view name,
                                          absl::Duration value,
                                          absl::Cord& serialized_value) {
  if (value != absl::ZeroDuration()) {
    auto seconds = absl::IDivDuration(value, absl::Seconds(1), &value);
    auto nanos = static_cast<int32_t>(
        absl::IDivDuration(value, absl::Nanoseconds(1), &value));
    ProtoWireEncoder encoder(name, serialized_value);
    CEL_RETURN_IF_ERROR(
        encoder.WriteTag(ProtoWireTag(1, ProtoWireType::kVarint)));
    CEL_RETURN_IF_ERROR(encoder.WriteVarint(seconds));
    CEL_RETURN_IF_ERROR(
        encoder.WriteTag(ProtoWireTag(2, ProtoWireType::kVarint)));
    CEL_RETURN_IF_ERROR(encoder.WriteVarint(nanos));
    encoder.EnsureFullyEncoded();
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status SerializeDuration(absl::Duration value,
                               absl::Cord& serialized_value) {
  return SerializeDurationOrTimestamp("google.protobuf.Duration", value,
                                      serialized_value);
}

absl::Status SerializeTimestamp(absl::Time value,
                                absl::Cord& serialized_value) {
  return SerializeDurationOrTimestamp(
      "google.protobuf.Timestamp", value - absl::UnixEpoch(), serialized_value);
}

namespace {

template <typename Value>
absl::Status SerializeBytesValueOrStringValue(absl::string_view name,
                                              Value&& value,
                                              absl::Cord& serialized_value) {
  if (!value.empty()) {
    ProtoWireEncoder encoder(name, serialized_value);
    CEL_RETURN_IF_ERROR(
        encoder.WriteTag(ProtoWireTag(1, ProtoWireType::kLengthDelimited)));
    CEL_RETURN_IF_ERROR(
        encoder.WriteLengthDelimited(std::forward<Value>(value)));
    encoder.EnsureFullyEncoded();
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status SerializeBytesValue(const absl::Cord& value,
                                 absl::Cord& serialized_value) {
  return SerializeBytesValueOrStringValue("google.protobuf.BytesValue", value,
                                          serialized_value);
}

absl::Status SerializeBytesValue(absl::string_view value,
                                 absl::Cord& serialized_value) {
  return SerializeBytesValueOrStringValue("google.protobuf.BytesValue", value,
                                          serialized_value);
}

absl::Status SerializeStringValue(const absl::Cord& value,
                                  absl::Cord& serialized_value) {
  return SerializeBytesValueOrStringValue("google.protobuf.StringValue", value,
                                          serialized_value);
}

absl::Status SerializeStringValue(absl::string_view value,
                                  absl::Cord& serialized_value) {
  return SerializeBytesValueOrStringValue("google.protobuf.StringValue", value,
                                          serialized_value);
}

namespace {

template <typename Value>
absl::Status SerializeVarintValue(absl::string_view name, Value value,
                                  absl::Cord& serialized_value) {
  if (value) {
    ProtoWireEncoder encoder(name, serialized_value);
    CEL_RETURN_IF_ERROR(
        encoder.WriteTag(ProtoWireTag(1, ProtoWireType::kVarint)));
    CEL_RETURN_IF_ERROR(encoder.WriteVarint(value));
    encoder.EnsureFullyEncoded();
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status SerializeBoolValue(bool value, absl::Cord& serialized_value) {
  return SerializeVarintValue("google.protobuf.BoolValue", value,
                              serialized_value);
}

absl::Status SerializeInt32Value(int32_t value, absl::Cord& serialized_value) {
  return SerializeVarintValue("google.protobuf.Int32Value", value,
                              serialized_value);
}

absl::Status SerializeInt64Value(int64_t value, absl::Cord& serialized_value) {
  return SerializeVarintValue("google.protobuf.Int64Value", value,
                              serialized_value);
}

absl::Status SerializeUInt32Value(uint32_t value,
                                  absl::Cord& serialized_value) {
  return SerializeVarintValue("google.protobuf.UInt32Value", value,
                              serialized_value);
}

absl::Status SerializeUInt64Value(uint64_t value,
                                  absl::Cord& serialized_value) {
  return SerializeVarintValue("google.protobuf.UInt64Value", value,
                              serialized_value);
}

absl::Status SerializeFloatValue(float value, absl::Cord& serialized_value) {
  if (absl::bit_cast<uint32_t>(value) != 0) {
    ProtoWireEncoder encoder("google.protobuf.FloatValue", serialized_value);
    CEL_RETURN_IF_ERROR(
        encoder.WriteTag(ProtoWireTag(1, ProtoWireType::kFixed32)));
    CEL_RETURN_IF_ERROR(encoder.WriteFixed32(value));
    encoder.EnsureFullyEncoded();
  }
  return absl::OkStatus();
}

absl::Status SerializeDoubleValue(double value, absl::Cord& serialized_value) {
  if (absl::bit_cast<uint64_t>(value) != 0) {
    ProtoWireEncoder encoder("google.protobuf.FloatValue", serialized_value);
    CEL_RETURN_IF_ERROR(
        encoder.WriteTag(ProtoWireTag(1, ProtoWireType::kFixed64)));
    CEL_RETURN_IF_ERROR(encoder.WriteFixed64(value));
    encoder.EnsureFullyEncoded();
  }
  return absl::OkStatus();
}

absl::Status SerializeValue(const Json& value, absl::Cord& serialized_value) {
  return JsonToAnyValue(value, serialized_value);
}

absl::Status SerializeListValue(const JsonArray& value,
                                absl::Cord& serialized_value) {
  return JsonArrayToAnyValue(value, serialized_value);
}

absl::Status SerializeStruct(const JsonObject& value,
                             absl::Cord& serialized_value) {
  return JsonObjectToAnyValue(value, serialized_value);
}

}  // namespace cel::internal
