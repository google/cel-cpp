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

#include "common/types/thread_compatible_type_factory.h"

#include <utility>

#include "absl/strings/string_view.h"
#include "common/sized_input_view.h"
#include "common/type.h"
#include "common/types/type_cache.h"

namespace cel::common_internal {

ListType ThreadCompatibleTypeFactory::CreateListTypeImpl(TypeView element) {
  if (auto list_type = list_types_.find(element);
      list_type != list_types_.end()) {
    return list_type->second;
  }
  ListType list_type(memory_manager(), Type(element));
  return list_types_.insert({list_type.element(), list_type}).first->second;
}

MapType ThreadCompatibleTypeFactory::CreateMapTypeImpl(TypeView key,
                                                       TypeView value) {
  if (auto map_type = map_types_.find(std::make_pair(key, value));
      map_type != map_types_.end()) {
    return map_type->second;
  }
  MapType map_type(memory_manager(), Type(key), Type(value));
  return map_types_
      .insert({std::make_pair(map_type.key(), map_type.value()), map_type})
      .first->second;
}

StructType ThreadCompatibleTypeFactory::CreateStructTypeImpl(
    absl::string_view name) {
  if (auto struct_type = struct_types_.find(name);
      struct_type != struct_types_.end()) {
    return struct_type->second;
  }
  StructType struct_type(memory_manager(), name);
  return struct_types_.insert({struct_type.name(), struct_type}).first->second;
}

OpaqueType ThreadCompatibleTypeFactory::CreateOpaqueTypeImpl(
    absl::string_view name, const SizedInputView<TypeView>& parameters) {
  if (auto opaque_type = opaque_types_.find(
          OpaqueTypeKeyView{.name = name, .parameters = parameters});
      opaque_type != opaque_types_.end()) {
    return opaque_type->second;
  }
  OpaqueType opaque_type(memory_manager(), name, parameters);
  return opaque_types_
      .insert({OpaqueTypeKey{.name = opaque_type.name(),
                             .parameters = opaque_type.parameters()},
               opaque_type})
      .first->second;
}

}  // namespace cel::common_internal
