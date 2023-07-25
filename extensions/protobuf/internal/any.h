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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_ANY_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_ANY_H_

#include "google/protobuf/any.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "base/value_factory.h"
#include "common/any.h"
#include "common/json.h"
#include "google/protobuf/message.h"

namespace cel::extensions::protobuf_internal {

absl::Status WrapGeneratedAnyProto(absl::string_view type_url,
                                   const absl::Cord& value,
                                   google::protobuf::Any& message);

absl::Status WrapDynamicAnyProto(absl::string_view type_url,
                                 const absl::Cord& value,
                                 google::protobuf::Message& message);

absl::StatusOr<Any> UnwrapGeneratedAnyProto(
    const google::protobuf::Any& message);

absl::StatusOr<Any> UnwrapDynamicAnyProto(const google::protobuf::Message& message);

absl::StatusOr<Json> AnyToJson(ValueFactory& value_factory,
                               absl::string_view type_url,
                               const absl::Cord& value);

absl::StatusOr<Json> AnyToJson(ValueFactory& value_factory, const Any& any);

absl::StatusOr<Json> GeneratedAnyProtoToJson(
    ValueFactory& value_factory, const google::protobuf::Any& message);

absl::StatusOr<Json> DynamicAnyProtoToJson(ValueFactory& value_factory,
                                           const google::protobuf::Message& message);

}  // namespace cel::extensions::protobuf_internal

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_ANY_H_
