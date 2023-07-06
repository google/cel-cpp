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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_FIELD_MASK_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_FIELD_MASK_H_

#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/json.h"
#include "google/protobuf/message.h"

namespace cel::extensions::protobuf_internal {

// Extracts the paths from `google.protobuf.FieldMask`.
absl::StatusOr<std::vector<std::string>> FieldMaskFromProto(
    const google::protobuf::Message& message);

// Formats `google.protobuf.FieldMask` according to
// https://protobuf.dev/programming-guides/proto3/#json.
absl::Status FieldMaskToJson(const google::protobuf::Message& message,
                             JsonMutator& mutator);

}  // namespace cel::extensions::protobuf_internal

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_INTERNAL_FIELD_MASK_H_
