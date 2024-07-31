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

#include "common/types/thread_safe_type_manager.h"

#include <utility>

#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "common/type.h"
#include "common/types/type_cache.h"

namespace cel::common_internal {

ListType ThreadSafeTypeManager::CreateListTypeImpl(const Type& element) {
  {
    absl::ReaderMutexLock lock(&list_types_mutex_);
    if (auto list_type = list_types_.find(element);
        list_type != list_types_.end()) {
      return list_type->second;
    }
  }
  ListType list_type(GetMemoryManager(), Type(element));
  absl::WriterMutexLock lock(&list_types_mutex_);
  return list_types_.insert({list_type.element(), list_type}).first->second;
}

MapType ThreadSafeTypeManager::CreateMapTypeImpl(const Type& key,
                                                 const Type& value) {
  {
    absl::ReaderMutexLock lock(&map_types_mutex_);
    if (auto map_type = map_types_.find(std::make_pair(key, value));
        map_type != map_types_.end()) {
      return map_type->second;
    }
  }
  MapType map_type(GetMemoryManager(), Type(key), Type(value));
  absl::WriterMutexLock lock(&map_types_mutex_);
  return map_types_
      .insert({std::make_pair(map_type.key(), map_type.value()), map_type})
      .first->second;
}

StructType ThreadSafeTypeManager::CreateStructTypeImpl(absl::string_view name) {
  {
    absl::ReaderMutexLock lock(&struct_types_mutex_);
    if (auto struct_type = struct_types_.find(name);
        struct_type != struct_types_.end()) {
      return struct_type->second;
    }
  }
  StructType struct_type(GetMemoryManager(), name);
  absl::WriterMutexLock lock(&struct_types_mutex_);
  return struct_types_.insert({struct_type.name(), struct_type}).first->second;
}

OpaqueType ThreadSafeTypeManager::CreateOpaqueTypeImpl(
    absl::string_view name, absl::Span<const Type> parameters) {
  if (auto opaque_type =
          ProcessLocalTypeCache::Get()->FindOpaqueType(name, parameters);
      opaque_type.has_value()) {
    return OpaqueType(*opaque_type);
  }
  {
    absl::ReaderMutexLock lock(&opaque_types_mutex_);
    if (auto opaque_type = opaque_types_.find(
            OpaqueTypeKey{.name = name, .parameters = parameters});
        opaque_type != opaque_types_.end()) {
      return opaque_type->second;
    }
  }
  OpaqueType opaque_type(GetMemoryManager(), name, parameters);
  absl::WriterMutexLock lock(&opaque_types_mutex_);
  return opaque_types_
      .insert({OpaqueTypeKey{.name = opaque_type.name(),
                             .parameters = opaque_type.parameters()},
               opaque_type})
      .first->second;
}

}  // namespace cel::common_internal
