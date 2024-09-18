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

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_JSON_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_JSON_H_

#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/json.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel::internal {

// Converts the given message to its `google.protobuf.Value` equivalent
// representation. This is similar to `google::protobuf::json::MessageToJsonString()`,
// except that this results in structured serialization.
absl::Status MessageToJson(
    const google::protobuf::Message& message,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Value*> result);
absl::Status MessageToJson(
    const google::protobuf::Message& message,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Message*> result);

// Converts the given message field to its `google.protobuf.Value` equivalent
// representation. This is similar to `google::protobuf::json::MessageToJsonString()`,
// except that this results in structured serialization.
absl::Status MessageFieldToJson(
    const google::protobuf::Message& message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Value*> result);
absl::Status MessageFieldToJson(
    const google::protobuf::Message& message,
    absl::Nonnull<const google::protobuf::FieldDescriptor*> field,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Message*> result);

// Temporary function which converts from `google.protobuf.Value` to
// `cel::Json`. In future `cel::Json` will be killed in favor of pure proto.
absl::StatusOr<Json> ProtoJsonToNativeJson(const google::protobuf::Message& proto);
absl::StatusOr<Json> ProtoJsonToNativeJson(
    const google::protobuf::Value& proto);

// Temporary function which converts from `google.protobuf.ListValue` to
// `cel::JsonArray`. In future `cel::Json` will be killed in favor of pure
// proto.
absl::StatusOr<JsonArray> ProtoJsonListToNativeJsonList(
    const google::protobuf::Message& proto);
absl::StatusOr<JsonArray> ProtoJsonListToNativeJsonList(
    const google::protobuf::ListValue& proto);

// Temporary function which converts from `google.protobuf.Struct` to
// `cel::JsonObject`. In future `cel::Json` will be killed in favor of pure
// proto.
absl::StatusOr<JsonObject> ProtoJsonMapToNativeJsonMap(
    const google::protobuf::Message& proto);
absl::StatusOr<JsonObject> ProtoJsonMapToNativeJsonMap(
    const google::protobuf::Struct& proto);

// Temporary function which converts from `cel::Json` to
// `google.protobuf.Value`. In future `cel::Json` will be killed in favor of
// pure proto.
absl::Status NativeJsonToProtoJson(const Json& json,
                                   absl::Nonnull<google::protobuf::Message*> proto);

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_JSON_H_
