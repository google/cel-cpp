// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_PROTO_UTIL_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_PROTO_UTIL_H_

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/util/message_differencer.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"

namespace google {
namespace api {
namespace expr {
namespace internal {

struct DefaultProtoEqual {
  inline bool operator()(const google::protobuf::Message& lhs,
                         const google::protobuf::Message& rhs) const {
    return google::protobuf::util::MessageDifferencer::Equals(lhs, rhs);
  }
};

/** Helper function to encode a duration in a google::protobuf::Duration. */
absl::Status EncodeDuration(absl::Duration duration,
                            google::protobuf::Duration* proto);

/** Helper function to encode an absl::Duration to a JSON-formatted string. */
absl::StatusOr<std::string> EncodeDurationToString(absl::Duration duration);

/** Helper function to encode a time in a google::protobuf::Timestamp. */
absl::Status EncodeTime(absl::Time time, google::protobuf::Timestamp* proto);

/** Helper function to encode an absl::Time to a JSON-formatted string. */
absl::StatusOr<std::string> EncodeTimeToString(absl::Time time);

/** Helper function to decode a duration from a google::protobuf::Duration. */
absl::Duration DecodeDuration(const google::protobuf::Duration& proto);

/** Helper function to decode a time from a google::protobuf::Timestamp. */
absl::Time DecodeTime(const google::protobuf::Timestamp& proto);

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_PROTO_UTIL_H_
