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

#include "extensions/protobuf/type.h"

#include <utility>

#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "extensions/protobuf/enum_type.h"
#include "internal/status_macros.h"

namespace cel::extensions {

namespace {

bool IsJsonMap(const Type& type) {
  return type.Is<MapType>() && type.As<MapType>().key()->Is<StringType>() &&
         type.As<MapType>().value()->Is<DynType>();
}

bool IsJsonList(const Type& type) {
  return type.Is<ListType>() && type.As<ListType>().element()->Is<DynType>();
}

}  // namespace

absl::StatusOr<Handle<Type>> ProtoType::Resolve(
    TypeManager& type_manager, const google::protobuf::EnumDescriptor& descriptor) {
  CEL_ASSIGN_OR_RETURN(auto type,
                       type_manager.ResolveType(descriptor.full_name()));
  if (!type.has_value()) {
    return absl::NotFoundError(
        absl::StrCat("Missing protocol buffer type implementation for \"",
                     descriptor.full_name(), "\""));
  }
  if (ABSL_PREDICT_FALSE(!(*type)->Is<ProtoEnumType>() &&
                         !(*type)->Is<NullType>())) {
    return absl::FailedPreconditionError(
        absl::StrCat("Unexpected protocol buffer type implementation for \"",
                     descriptor.full_name(), "\": ", (*type)->DebugString()));
  }
  return std::move(type).value();
}

absl::StatusOr<Handle<Type>> ProtoType::Resolve(
    TypeManager& type_manager, const google::protobuf::Descriptor& descriptor) {
  CEL_ASSIGN_OR_RETURN(auto type,
                       type_manager.ResolveType(descriptor.full_name()));
  if (!type.has_value()) {
    return absl::NotFoundError(
        absl::StrCat("Missing protocol buffer type implementation for \"",
                     descriptor.full_name(), "\""));
  }
  if (ABSL_PREDICT_FALSE(
          !(*type)->Is<ProtoStructType>() && !(*type)->Is<DurationType>() &&
          !(*type)->Is<TimestampType>() && !(*type)->Is<WrapperType>() &&
          !IsJsonList(**type) && !IsJsonMap(**type) &&
          !(*type)->Is<DynType>() && !(*type)->Is<AnyType>())) {
    return absl::FailedPreconditionError(
        absl::StrCat("Unexpected protocol buffer type implementation for \"",
                     descriptor.full_name(), "\": ", (*type)->DebugString()));
  }
  return std::move(type).value();
}

}  // namespace cel::extensions
