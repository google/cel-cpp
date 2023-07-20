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
// types in `google/protobuf/timestamp.proto`.

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_TIMESTAMP_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_TIMESTAMP_H_

#include <cstdint>

#include "google/protobuf/timestamp.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "google/protobuf/message.h"

namespace cel::extensions::protobuf_internal {

absl::StatusOr<absl::Time> UnwrapDynamicTimestampProto(
    const google::protobuf::Message& message);

inline absl::StatusOr<absl::Time> UnwrapGeneratedTimestampProto(
    const google::protobuf::Timestamp& message) {
  return absl::UnixEpoch() + absl::Seconds(message.seconds()) +
         absl::Nanoseconds(message.nanos());
}

absl::Status WrapDynamicTimestampProto(absl::Time value,
                                       google::protobuf::Message& message);

inline absl::Status WrapGeneratedTimestampProto(
    absl::Time value, google::protobuf::Timestamp& message) {
  auto duration = value - absl::UnixEpoch();
  message.set_seconds(
      absl::IDivDuration(duration, absl::Seconds(1), &duration));
  message.set_nanos(static_cast<int32_t>(
      absl::IDivDuration(duration, absl::Nanoseconds(1), &duration)));
  return absl::OkStatus();
}

}  // namespace cel::extensions::protobuf_internal

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_TIMESTAMP_H_
