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

// Utilities for converting to and from the well known protocol buffer message
// types in `google/protobuf/any.proto`.

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_ANY_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_ANY_H_

#include "google/protobuf/any.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "google/protobuf/message.h"

namespace cel::extensions::protobuf_internal {

// Converts `google.protobuf.Any` to `Any`. No validation is performed. The type
// name of `message` must be `google.protobuf.Any`.
absl::StatusOr<google::protobuf::Any> UnwrapDynamicAnyProto(
    const google::protobuf::Message& message);

// Converts `Any` to `google.protobuf.Any`. No validation is performed. The type
// name of `message` must be `google.protobuf.Any`.
absl::Status WrapDynamicAnyProto(absl::string_view type_url,
                                 const absl::Cord& value,
                                 google::protobuf::Message& message);
inline absl::Status WrapDynamicAnyProto(const google::protobuf::Any& any,
                                        google::protobuf::Message& message) {
  return WrapDynamicAnyProto(any.type_url(), GetAnyValueAsCord(any), message);
}

}  // namespace cel::extensions::protobuf_internal

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_ANY_H_
