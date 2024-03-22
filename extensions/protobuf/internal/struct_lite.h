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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_STRUCT_LITE_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_STRUCT_LITE_H_

#include "google/protobuf/struct.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/json.h"

namespace cel::extensions::protobuf_internal {

absl::StatusOr<Json> GeneratedValueProtoToJson(
    const google::protobuf::Value& message);

absl::StatusOr<Json> GeneratedListValueProtoToJson(
    const google::protobuf::ListValue& message);

absl::StatusOr<Json> GeneratedStructProtoToJson(
    const google::protobuf::Struct& message);

absl::Status GeneratedValueProtoFromJson(const Json& json,
                                         google::protobuf::Value& message);

absl::Status GeneratedListValueProtoFromJson(
    const JsonArray& json, google::protobuf::ListValue& message);

absl::Status GeneratedStructProtoFromJson(const JsonObject& json,
                                          google::protobuf::Struct& message);

}  // namespace cel::extensions::protobuf_internal

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_STRUCT_LITE_H_
