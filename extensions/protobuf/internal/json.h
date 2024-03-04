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

// This header exposes utilities for converting `google::protobuf::Message` to `Json`.

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_JSON_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_JSON_H_

#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "common/json.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::extensions::protobuf_internal {

// `value_manager` is used if `message` is `google.protobuf.Any`.
absl::StatusOr<Json> ProtoMessageToJson(AnyToJsonConverter& converter,
                                        const google::protobuf::Message& message);

// Convert a protocol buffer enum to JSON. Prefers the name, but will fallback
// to stringifying the number if the name is unavailable.
Json ProtoEnumToJson(absl::Nonnull<const google::protobuf::EnumDescriptor*> descriptor,
                     int value);
Json ProtoEnumToJson(
    absl::Nonnull<const google::protobuf::EnumValueDescriptor*> descriptor);

}  // namespace cel::extensions::protobuf_internal

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_JSON_H_
