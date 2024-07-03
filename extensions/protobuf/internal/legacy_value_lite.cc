// Copyright 2024 Google LLC
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

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/value.h"
#include "eval/public/message_wrapper.h"
#include "extensions/protobuf/internal/legacy_value.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"

namespace cel::extensions::protobuf_internal {

absl::StatusOr<google::api::expr::runtime::MessageWrapper>
MessageWrapperFromValue(const Value&, google::protobuf::Arena*) {
  return absl::InvalidArgumentError(
      "cannot reinterpret cel::ValueView as "
      "google::api::expr::runtime::MessageWrapper when using proto-lite");
}

}  // namespace cel::extensions::protobuf_internal
