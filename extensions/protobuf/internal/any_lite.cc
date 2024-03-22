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

#include "extensions/protobuf/internal/any_lite.h"

#include <string>

#include "google/protobuf/any.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/any.h"

namespace cel::extensions::protobuf_internal {

absl::StatusOr<Any> UnwrapGeneratedAnyProto(
    const google::protobuf::Any& message) {
  return MakeAny(std::string(message.type_url()), absl::Cord(message.value()));
}

absl::Status WrapGeneratedAnyProto(absl::string_view type_url,
                                   const absl::Cord& value,
                                   google::protobuf::Any& message) {
  message.set_type_url(type_url);
  message.set_value(static_cast<std::string>(value));
  return absl::OkStatus();
}

}  // namespace cel::extensions::protobuf_internal
