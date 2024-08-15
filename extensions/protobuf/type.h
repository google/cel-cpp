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

#ifndef THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_TYPE_H_
#define THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_TYPE_H_

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/status/statusor.h"
#include "common/type.h"
#include "common/type_factory.h"
#include "google/protobuf/descriptor.h"

namespace cel::extensions {

ABSL_DEPRECATED("Use Type::Message")
absl::StatusOr<Type> ProtoTypeToType(
    TypeFactory& type_factory, absl::Nonnull<const google::protobuf::Descriptor*> desc);

absl::StatusOr<Type> ProtoEnumTypeToType(
    TypeFactory& type_factory,
    absl::Nonnull<const google::protobuf::EnumDescriptor*> desc);

}  // namespace cel::extensions

#endif  // THIRD_PARTY_CEL_CPP_EXTENSIONS_PROTOBUF_TYPE_H_
