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

#include "common/type_pool.h"

#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/type.h"
#include "google/protobuf/descriptor.h"

namespace cel {

ListType TypePool::MakeListType(const Type& element) {
  return list_type_pool_.InternListType(element);
}

MapType TypePool::MakeMapType(const Type& key, const Type& value) {
  return map_type_pool_.InternMapType(key, value);
}

StructType TypePool::MakeStructType(absl::string_view name) {
  if (descriptor_pool_ != nullptr) {
    const google::protobuf::Descriptor* descriptor =
        descriptor_pool_->FindMessageTypeByName(name);
    if (descriptor != nullptr) {
      return MessageType(descriptor);
    }
  }
  return common_internal::MakeBasicStructType(string_pool_->InternString(name));
}

FunctionType TypePool::MakeFunctionType(const Type& result,
                                        absl::Span<const Type> args) {
  return function_type_pool_.InternFunctionType(result, args);
}

OpaqueType TypePool::MakeOpaqueType(absl::string_view name,
                                    absl::Span<const Type> params) {
  return opaque_type_pool_.InternOpaqueType(string_pool_->InternString(name),
                                            params);
}

TypeParamType TypePool::MakeTypeParamType(absl::string_view name) {
  return TypeParamType(string_pool_->InternString(name));
}

TypeType TypePool::MakeTypeType(const Type& type) {
  return type_type_pool_.InternTypeType(type);
}

}  // namespace cel
