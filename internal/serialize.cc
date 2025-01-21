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

#include <cstddef>
#include <cstdint>

#include "absl/base/casts.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/proto_wire.h"
#include "internal/status_macros.h"

namespace cel::internal {

namespace {

size_t SerializedDurationSizeOrTimestampSize(absl::Duration value) {
  size_t serialized_size = 0;
  if (value != absl::ZeroDuration()) {
    auto seconds = absl::IDivDuration(value, absl::Seconds(1), &value);
    auto nanos = static_cast<int32_t>(
        absl::IDivDuration(value, absl::Nanoseconds(1), &value));
    if (seconds != 0) {
      serialized_size +=
          VarintSize(MakeProtoWireTag(1, ProtoWireType::kVarint)) +
          VarintSize(seconds);
    }
    if (nanos != 0) {
      serialized_size +=
          VarintSize(MakeProtoWireTag(2, ProtoWireType::kVarint)) +
          VarintSize(nanos);
    }
  }
  return serialized_size;
}

}  // namespace

size_t SerializedDurationSize(absl::Duration value) {
  return SerializedDurationSizeOrTimestampSize(value);
}

size_t SerializedTimestampSize(absl::Time value) {
  return SerializedDurationSizeOrTimestampSize(value - absl::UnixEpoch());
}

namespace {

template <typename Value>
size_t SerializedBytesValueSizeOrStringValueSize(Value&& value) {
  return !value.empty() ? VarintSize(MakeProtoWireTag(
                              1, ProtoWireType::kLengthDelimited)) +
                              VarintSize(value.size()) + value.size()
                        : 0;
}

}  // namespace

size_t SerializedBytesValueSize(const absl::Cord& value) {
  return SerializedBytesValueSizeOrStringValueSize(value);
}

size_t SerializedBytesValueSize(absl::string_view value) {
  return SerializedBytesValueSizeOrStringValueSize(value);
}

size_t SerializedStringValueSize(const absl::Cord& value) {
  return SerializedBytesValueSizeOrStringValueSize(value);
}

size_t SerializedStringValueSize(absl::string_view value) {
  return SerializedBytesValueSizeOrStringValueSize(value);
}

namespace {

template <typename Value>
size_t SerializedVarintValueSize(Value value) {
  return value ? VarintSize(MakeProtoWireTag(1, ProtoWireType::kVarint)) +
                     VarintSize(value)
               : 0;
}

}  // namespace

size_t SerializedBoolValueSize(bool value) {
  return SerializedVarintValueSize(value);
}

size_t SerializedInt32ValueSize(int32_t value) {
  return SerializedVarintValueSize(value);
}

size_t SerializedInt64ValueSize(int64_t value) {
  return SerializedVarintValueSize(value);
}

size_t SerializedUInt32ValueSize(uint32_t value) {
  return SerializedVarintValueSize(value);
}

size_t SerializedUInt64ValueSize(uint64_t value) {
  return SerializedVarintValueSize(value);
}

size_t SerializedFloatValueSize(float value) {
  return absl::bit_cast<uint32_t>(value) != 0
             ? VarintSize(MakeProtoWireTag(1, ProtoWireType::kFixed32)) + 4
             : 0;
}

size_t SerializedDoubleValueSize(double value) {
  return absl::bit_cast<uint64_t>(value) != 0
             ? VarintSize(MakeProtoWireTag(1, ProtoWireType::kFixed64)) + 8
             : 0;
}

// NOTE: We use ABSL_DCHECK below to assert that the resulting size of
// serializing is the same as the preflighting size calculation functions. They
// must be the same, and ABSL_DCHECK is the cheapest way of ensuring this
// without having to duplicate tests.

namespace {

absl::Status SerializeDurationOrTimestamp(absl::string_view name,
                                          absl::Duration value,
                                          absl::Cord& serialized_value) {
  if (value != absl::ZeroDuration()) {
    auto original_value = value;
    auto seconds = absl::IDivDuration(value, absl::Seconds(1), &value);
    auto nanos = static_cast<int32_t>(
        absl::IDivDuration(value, absl::Nanoseconds(1), &value));
    ProtoWireEncoder encoder(name, serialized_value);
    if (seconds != 0) {
      CEL_RETURN_IF_ERROR(
          encoder.WriteTag(ProtoWireTag(1, ProtoWireType::kVarint)));
      CEL_RETURN_IF_ERROR(encoder.WriteVarint(seconds));
    }
    if (nanos != 0) {
      CEL_RETURN_IF_ERROR(
          encoder.WriteTag(ProtoWireTag(2, ProtoWireType::kVarint)));
      CEL_RETURN_IF_ERROR(encoder.WriteVarint(nanos));
    }
    encoder.EnsureFullyEncoded();
    ABSL_DCHECK_EQ(encoder.size(),
                   SerializedDurationSizeOrTimestampSize(original_value));
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
    ABSL_DCHECK_EQ(encoder.size(),
                   SerializedBytesValueSizeOrStringValueSize(value));
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
    ABSL_DCHECK_EQ(encoder.size(), SerializedVarintValueSize(value));
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
    ABSL_DCHECK_EQ(encoder.size(), SerializedFloatValueSize(value));
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
    ABSL_DCHECK_EQ(encoder.size(), SerializedDoubleValueSize(value));
  }
  return absl::OkStatus();
}

}  // namespace cel::internal
