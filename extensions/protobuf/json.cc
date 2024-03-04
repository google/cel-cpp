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

#include "extensions/protobuf/json.h"

#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "common/json.h"
#include "extensions/protobuf/internal/json.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::extensions {

absl::StatusOr<Json> ProtoAnyToJsonConverter::ConvertToJson(
    absl::string_view type_url, const absl::Cord& value) {
  absl::string_view type_name;
  if (!ParseTypeUrl(type_url, &type_name)) {
    return absl::InvalidArgumentError(
        absl::StrCat("invalid type URL: ", type_url));
  }
  const auto* descriptor = pool_->FindMessageTypeByName(type_name);
  if (descriptor == nullptr) {
    return absl::NotFoundError(
        absl::StrCat("unable to locate descriptor for `", type_name, "`"));
  }
  const auto* prototype = factory_->GetPrototype(descriptor);
  if (prototype == nullptr) {
    return absl::NotFoundError(
        absl::StrCat("unable to create prototype for `", type_name, "`"));
  }
  auto message = absl::WrapUnique(prototype->New());
  if (!message->ParsePartialFromCord(value)) {
    return absl::InvalidArgumentError(
        absl::StrCat("failed to parse `", type_name, "`"));
  }
  return protobuf_internal::ProtoMessageToJson(*this, *message);
}

}  // namespace cel::extensions
