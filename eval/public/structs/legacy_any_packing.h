// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_LEGACY_ANY_PACKING_H_
#define THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_LEGACY_ANY_PACKING_H_

#include "google/protobuf/any.pb.h"
#include "google/protobuf/message_lite.h"
#include "absl/status/statusor.h"

namespace google::api::expr::runtime {

// Interface for packing/unpacking google::protobuf::Any messages apis.
class LegacyAnyPackingApis {
 public:
  virtual ~LegacyAnyPackingApis() = default;
  // Return MessageLite pointer to the unpacked message from provided
  // `any_message`.
  virtual absl::StatusOr<google::protobuf::MessageLite*> Unpack(
      const google::protobuf::Any& any_message, google::protobuf::Arena* arena) const = 0;
  // Pack provided 'message' into given 'any_message'.
  virtual absl::Status Pack(const google::protobuf::MessageLite* message,
                            google::protobuf::Any& any_message) const = 0;
};
}  // namespace google::api::expr::runtime

#endif  // THIRD_PARTY_CEL_CPP_EVAL_PUBLIC_STRUCTS_LEGACY_ANY_PACKING_H_
