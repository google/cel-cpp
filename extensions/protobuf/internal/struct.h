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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_STRUCT_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_STRUCT_H_

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/json.h"
#include "google/protobuf/message.h"

namespace cel::extensions::protobuf_internal {

absl::StatusOr<Json> DynamicValueProtoToJson(const google::protobuf::Message& message);

absl::StatusOr<Json> DynamicListValueProtoToJson(
    const google::protobuf::Message& message);

absl::StatusOr<Json> DynamicStructProtoToJson(const google::protobuf::Message& message);

absl::Status DynamicValueProtoFromJson(const Json& json,
                                       google::protobuf::Message& message);

absl::Status DynamicListValueProtoFromJson(const JsonArray& json,
                                           google::protobuf::Message& message);

absl::Status DynamicStructProtoFromJson(const JsonObject& json,
                                        google::protobuf::Message& message);

absl::Status DynamicValueProtoSetNullValue(google::protobuf::Message* message);

absl::Status DynamicValueProtoSetBoolValue(bool value,
                                           google::protobuf::Message* message);

absl::Status DynamicValueProtoSetNumberValue(double value,
                                             google::protobuf::Message* message);

absl::Status DynamicValueProtoSetStringValue(absl::string_view value,
                                             google::protobuf::Message* message);

absl::StatusOr<google::protobuf::Message*> DynamicValueProtoMutableListValue(
    google::protobuf::Message* message);

absl::StatusOr<google::protobuf::Message*> DynamicValueProtoMutableStructValue(
    google::protobuf::Message* message);

absl::StatusOr<google::protobuf::Message*> DynamicListValueProtoAddElement(
    google::protobuf::Message* message);

absl::StatusOr<google::protobuf::Message*> DynamicStructValueProtoAddField(
    absl::string_view name, google::protobuf::Message* message);

}  // namespace cel::extensions::protobuf_internal

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_STRUCT_H_
