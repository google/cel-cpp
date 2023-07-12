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

#include "base/types/timestamp_type.h"

#include "absl/strings/cord.h"
#include "absl/time/time.h"
#include "base/value_factory.h"
#include "base/values/timestamp_value.h"
#include "internal/proto_wire.h"
#include "internal/status_macros.h"

namespace cel {

namespace {

using internal::MakeProtoWireTag;
using internal::ProtoWireDecoder;
using internal::ProtoWireType;

}  // namespace

CEL_INTERNAL_TYPE_IMPL(TimestampType);

absl::StatusOr<Handle<TimestampValue>> TimestampType::NewValueFromAny(
    ValueFactory& value_factory, const absl::Cord& value) const {
  // google.protobuf.Timestamp.
  int64_t seconds = 0;
  int32_t nanos = 0;
  ProtoWireDecoder decoder("google.protobuf.Timestamp", value);
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
  return value_factory.CreateTimestampValue(
      absl::UnixEpoch() + absl::Seconds(seconds) + absl::Nanoseconds(nanos));
}

}  // namespace cel
