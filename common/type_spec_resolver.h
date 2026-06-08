// Copyright 2026 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPE_SPEC_RESOLVER_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPE_SPEC_RESOLVER_H_

#include "absl/status/statusor.h"
#include "common/ast.h"
#include "common/type.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"

namespace cel {

// Resolves a `cel::TypeSpec` to a `cel::Type`.
//
// TypeSpec only specifies a type while Type provides support for inspecting
// properties of the type when used in CEL. Returns a status with code
// `InvalidArgument` if the input cannot be resolved to a type.
absl::StatusOr<Type> ConvertTypeSpecToType(const TypeSpec& type_spec,
                                           google::protobuf::Arena* arena,
                                           const google::protobuf::DescriptorPool& pool);

// Resolves a `cel::Type` to a `cel::TypeSpec`.
absl::StatusOr<TypeSpec> ConvertTypeToTypeSpec(const Type& type);

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPE_SPEC_RESOLVER_H_
