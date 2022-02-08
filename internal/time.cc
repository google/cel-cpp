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

#include "internal/time.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_format.h"
#include "absl/time/time.h"
#include "internal/status_macros.h"

namespace cel::internal {

namespace {

std::string RawFormatTimestamp(absl::Time timestamp) {
  return absl::FormatTime("%Y-%m-%d%ET%H:%M:%E*SZ", timestamp,
                          absl::UTCTimeZone());
}

}  // namespace

absl::Status ValidateDuration(absl::Duration duration) {
  if (duration < MinDuration()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Duration \"", absl::FormatDuration(duration),
                     "\" below minimum allowed duration \"",
                     absl::FormatDuration(MinDuration()), "\""));
  }
  if (duration > MaxDuration()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Duration \"", absl::FormatDuration(duration),
                     "\" above maximum allowed duration \"",
                     absl::FormatDuration(MaxDuration()), "\""));
  }
  return absl::OkStatus();
}

absl::StatusOr<absl::Duration> ParseDuration(absl::string_view input) {
  absl::Duration duration;
  if (!absl::ParseDuration(input, &duration)) {
    return absl::InvalidArgumentError("Failed to parse duration from string");
  }
  return duration;
}

absl::StatusOr<std::string> FormatDuration(absl::Duration duration) {
  CEL_RETURN_IF_ERROR(ValidateDuration(duration));
  return absl::FormatDuration(duration);
}

absl::Status ValidateTimestamp(absl::Time timestamp) {
  if (timestamp < MinTimestamp()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Timestamp \"", RawFormatTimestamp(timestamp),
                     "\" below minimum allowed timestamp \"",
                     RawFormatTimestamp(MinTimestamp()), "\""));
  }
  if (timestamp > MaxTimestamp()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Timestamp \"", RawFormatTimestamp(timestamp),
                     "\" above maximum allowed timestamp \"",
                     RawFormatTimestamp(MaxTimestamp()), "\""));
  }
  return absl::OkStatus();
}

absl::StatusOr<absl::Time> ParseTimestamp(absl::string_view input) {
  absl::Time timestamp;
  std::string err;
  if (!absl::ParseTime(absl::RFC3339_full, input, absl::UTCTimeZone(),
                       &timestamp, &err)) {
    return err.empty() ? absl::InvalidArgumentError(
                             "Failed to parse timestamp from string")
                       : absl::InvalidArgumentError(absl::StrCat(
                             "Failed to parse timestamp from string: ", err));
  }
  CEL_RETURN_IF_ERROR(ValidateTimestamp(timestamp));
  return timestamp;
}

absl::StatusOr<std::string> FormatTimestamp(absl::Time timestamp) {
  CEL_RETURN_IF_ERROR(ValidateTimestamp(timestamp));
  return RawFormatTimestamp(timestamp);
}

}  // namespace cel::internal
