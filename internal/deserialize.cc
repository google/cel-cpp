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

#include "internal/deserialize.h"

#include <cstdint>
#include <string>
#include <utility>

#include "google/protobuf/any.pb.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/time/time.h"
#include "common/any.h"
#include "internal/proto_wire.h"
#include "internal/status_macros.h"

namespace cel::internal {

absl::StatusOr<absl::Duration> DeserializeDuration(const absl::Cord& data) {
  int64_t seconds = 0;
  int32_t nanos = 0;
  ProtoWireDecoder decoder("google.protobuf.Duration", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kVarint)) {
      CEL_ASSIGN_OR_RETURN(seconds, decoder.ReadVarint<int64_t>());
      continue;
    }
    if (tag == MakeProtoWireTag(2, ProtoWireType::kVarint)) {
      CEL_ASSIGN_OR_RETURN(nanos, decoder.ReadVarint<int32_t>());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return absl::Seconds(seconds) + absl::Nanoseconds(nanos);
}

absl::StatusOr<absl::Time> DeserializeTimestamp(const absl::Cord& data) {
  int64_t seconds = 0;
  int32_t nanos = 0;
  ProtoWireDecoder decoder("google.protobuf.Timestamp", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kVarint)) {
      CEL_ASSIGN_OR_RETURN(seconds, decoder.ReadVarint<int64_t>());
      continue;
    }
    if (tag == MakeProtoWireTag(2, ProtoWireType::kVarint)) {
      CEL_ASSIGN_OR_RETURN(nanos, decoder.ReadVarint<int32_t>());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return absl::UnixEpoch() + absl::Seconds(seconds) + absl::Nanoseconds(nanos);
}

absl::StatusOr<absl::Cord> DeserializeBytesValue(const absl::Cord& data) {
  absl::Cord primitive;
  ProtoWireDecoder decoder("google.protobuf.BytesValue", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kLengthDelimited)) {
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadLengthDelimited());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return primitive;
}

absl::StatusOr<absl::Cord> DeserializeStringValue(const absl::Cord& data) {
  absl::Cord primitive;
  ProtoWireDecoder decoder("google.protobuf.StringValue", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kLengthDelimited)) {
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadLengthDelimited());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return primitive;
}

absl::StatusOr<bool> DeserializeBoolValue(const absl::Cord& data) {
  bool primitive = false;
  ProtoWireDecoder decoder("google.protobuf.BoolValue", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kVarint)) {
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadVarint<bool>());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return primitive;
}

absl::StatusOr<int32_t> DeserializeInt32Value(const absl::Cord& data) {
  int32_t primitive = 0;
  ProtoWireDecoder decoder("google.protobuf.Int32Value", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kVarint)) {
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadVarint<int32_t>());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return primitive;
}

absl::StatusOr<int64_t> DeserializeInt64Value(const absl::Cord& data) {
  int64_t primitive = 0;
  ProtoWireDecoder decoder("google.protobuf.Int64Value", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kVarint)) {
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadVarint<int64_t>());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return primitive;
}

absl::StatusOr<uint32_t> DeserializeUInt32Value(const absl::Cord& data) {
  uint32_t primitive = 0;
  ProtoWireDecoder decoder("google.protobuf.UInt32Value", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kVarint)) {
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadVarint<uint32_t>());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return primitive;
}

absl::StatusOr<uint64_t> DeserializeUInt64Value(const absl::Cord& data) {
  uint64_t primitive = 0;
  ProtoWireDecoder decoder("google.protobuf.UInt64Value", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kVarint)) {
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadVarint<uint64_t>());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return primitive;
}

absl::StatusOr<float> DeserializeFloatValue(const absl::Cord& data) {
  float primitive = 0.0f;
  ProtoWireDecoder decoder("google.protobuf.FloatValue", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kFixed32)) {
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadFixed32<float>());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return primitive;
}

absl::StatusOr<double> DeserializeDoubleValue(const absl::Cord& data) {
  double primitive = 0.0;
  ProtoWireDecoder decoder("google.protobuf.DoubleValue", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kFixed64)) {
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadFixed64<double>());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return primitive;
}

absl::StatusOr<double> DeserializeFloatValueOrDoubleValue(
    const absl::Cord& data) {
  double primitive = 0.0;
  ProtoWireDecoder decoder("google.protobuf.DoubleValue", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kFixed32)) {
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadFixed32<float>());
      continue;
    }
    if (tag == MakeProtoWireTag(1, ProtoWireType::kFixed64)) {
      CEL_ASSIGN_OR_RETURN(primitive, decoder.ReadFixed64<double>());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return primitive;
}

absl::StatusOr<google::protobuf::Any> DeserializeAny(const absl::Cord& data) {
  absl::Cord type_url;
  absl::Cord value;
  ProtoWireDecoder decoder("google.protobuf.Any", data);
  while (decoder.HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto tag, decoder.ReadTag());
    if (tag == MakeProtoWireTag(1, ProtoWireType::kLengthDelimited)) {
      CEL_ASSIGN_OR_RETURN(type_url, decoder.ReadLengthDelimited());
      continue;
    }
    if (tag == MakeProtoWireTag(2, ProtoWireType::kLengthDelimited)) {
      CEL_ASSIGN_OR_RETURN(value, decoder.ReadLengthDelimited());
      continue;
    }
    CEL_RETURN_IF_ERROR(decoder.SkipLengthValue());
  }
  decoder.EnsureFullyDecoded();
  return MakeAny(static_cast<std::string>(type_url), std::move(value));
}

}  // namespace cel::internal
