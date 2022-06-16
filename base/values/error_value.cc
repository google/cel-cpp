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

#include "base/values/error_value.h"

#include <algorithm>
#include <string>
#include <utility>

#include "absl/container/inlined_vector.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"

namespace cel {

CEL_INTERNAL_VALUE_IMPL(ErrorValue);

namespace {

struct StatusPayload final {
  std::string key;
  absl::Cord value;
};

void StatusHashValue(absl::HashState state, const absl::Status& status) {
  // absl::Status::operator== compares `raw_code()`, `message()` and the
  // payloads.
  state = absl::HashState::combine(std::move(state), status.raw_code(),
                                   status.message());
  // In order to determistically hash, we need to put the payloads in sorted
  // order. There is no guarantee from `absl::Status` on the order of the
  // payloads returned from `absl::Status::ForEachPayload`.
  //
  // This should be the same inline size as
  // `absl::status_internal::StatusPayloads`.
  absl::InlinedVector<StatusPayload, 1> payloads;
  status.ForEachPayload([&](absl::string_view key, const absl::Cord& value) {
    payloads.push_back(StatusPayload{std::string(key), value});
  });
  std::stable_sort(
      payloads.begin(), payloads.end(),
      [](const StatusPayload& lhs, const StatusPayload& rhs) -> bool {
        return lhs.key < rhs.key;
      });
  for (const auto& payload : payloads) {
    state =
        absl::HashState::combine(std::move(state), payload.key, payload.value);
  }
}

}  // namespace

std::string ErrorValue::DebugString() const { return value().ToString(); }

bool ErrorValue::Equals(const Value& other) const {
  return kind() == other.kind() &&
         value() == static_cast<const ErrorValue&>(other).value();
}

void ErrorValue::HashValue(absl::HashState state) const {
  StatusHashValue(absl::HashState::combine(std::move(state), type()), value());
}

}  // namespace cel
