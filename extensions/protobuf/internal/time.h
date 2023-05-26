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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_TIME_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_TIME_H_

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "google/protobuf/message.h"

namespace cel::extensions::protobuf_internal {

// Convert google.protobuf.Duration to absl::Duration. Does not perform range
// checking.
absl::StatusOr<absl::Duration> AbslDurationFromDurationProto(
    const google::protobuf::Message& message);

// Convert google.protobuf.Timestamp to absl::Time. Does not perform range
// checking.
absl::StatusOr<absl::Time> AbslTimeFromTimestampProto(
    const google::protobuf::Message& message);

// Convert absl::Duration to google.protobuf.Duration. Does not perform range
// checking.
absl::Status AbslDurationToDurationProto(google::protobuf::Message& message,
                                         absl::Duration duration);

// Convert absl::Time to google.protobuf.Timestamp. Does not perform range
// checking.
absl::Status AbslTimeToTimestampProto(google::protobuf::Message& message,
                                      absl::Time time);

}  // namespace cel::extensions::protobuf_internal

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_TIME_H_
