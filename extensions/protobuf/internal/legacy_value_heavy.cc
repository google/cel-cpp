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

#include "absl/status/statusor.h"
#include "common/value.h"
#include "eval/public/message_wrapper.h"
#include "eval/public/structs/proto_message_type_adapter.h"
#include "extensions/protobuf/internal/legacy_value.h"
#include "extensions/protobuf/value.h"
#include "internal/status_macros.h"
#include "google/protobuf/arena.h"

namespace cel::extensions::protobuf_internal {

absl::StatusOr<google::api::expr::runtime::MessageWrapper>
MessageWrapperFromValue(const Value& value, google::protobuf::Arena* arena) {
  CEL_ASSIGN_OR_RETURN(auto* message,
                       extensions::ProtoMessageFromValue(value, arena));
  return google::api::expr::runtime::MessageWrapper::Builder(message).Build(
      &google::api::expr::runtime::GetGenericProtoTypeInfoInstance());
}

}  // namespace cel::extensions::protobuf_internal
