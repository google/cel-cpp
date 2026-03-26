// Copyright 2026 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_ENV_TYPE_INFO_H_
#define THIRD_PARTY_CEL_CPP_ENV_TYPE_INFO_H_

#include "absl/status/statusor.h"
#include "common/type.h"
#include "env/config.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"

namespace cel {

// Converts a Config::TypeInfo to a cel::Type. Returns an error if the type_info
// cannot be converted to a known cel::Type, a list configured with more than
// one parameter.
absl::StatusOr<Type> TypeInfoToType(
    const Config::TypeInfo& type_info,
    const google::protobuf::DescriptorPool* descriptor_pool, google::protobuf::Arena* arena);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_ENV_TYPE_INFO_H_
