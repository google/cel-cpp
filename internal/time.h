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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_TIME_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_TIME_H_

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"

namespace cel::internal {

    inline absl::Duration
    MaxDuration() {
  // This currently supports a larger range then the current CEL spec. The
  // intent is to widen the CEL spec to support the larger range and match
  // google.protobuf.Duration from protocol buffer messages, which this
  // implementation currently supports.
  // TODO(google/cel-spec/issues/214): revisit
  return absl::Seconds(315576000000) + absl::Nanoseconds(999999999);
}

    inline absl::Duration
    MinDuration() {
  // This currently supports a larger range then the current CEL spec. The
  // intent is to widen the CEL spec to support the larger range and match
  // google.protobuf.Duration from protocol buffer messages, which this
  // implementation currently supports.
  // TODO(google/cel-spec/issues/214): revisit
  return absl::Seconds(-315576000000) + absl::Nanoseconds(-999999999);
}

    inline absl::Time
    MaxTimestamp() {
  return absl::UnixEpoch() + absl::Seconds(253402300799) +
         absl::Nanoseconds(999999999);
}

    inline absl::Time
    MinTimestamp() {
  return absl::UnixEpoch() + absl::Seconds(-62135596800);
}

absl::Status ValidateDuration(absl::Duration duration);

absl::StatusOr<absl::Duration> ParseDuration(absl::string_view input);

absl::StatusOr<std::string> FormatDuration(absl::Duration duration);

absl::Status ValidateTimestamp(absl::Time timestamp);

absl::StatusOr<absl::Time> ParseTimestamp(absl::string_view input);

absl::StatusOr<std::string> FormatTimestamp(absl::Time timestamp);

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_TIME_H_
