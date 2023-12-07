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

#include "common/type_factory.h"

#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/sized_input_view.h"
#include "common/type.h"
#include "common/type_kind.h"
#include "common/types/type_cache.h"
#include "internal/names.h"

namespace cel {

namespace {

using common_internal::ListTypeCacheMap;
using common_internal::MapTypeCacheMap;
using common_internal::OpaqueTypeCacheMap;
using common_internal::OpaqueTypeKey;
using common_internal::OpaqueTypeKeyView;
using common_internal::ProcessLocalTypeCache;

using StructTypeCacheMap = absl::flat_hash_map<absl::string_view, StructType>;

class ThreadCompatibleTypeFactory final : public TypeFactory {
 public:
  explicit ThreadCompatibleTypeFactory(MemoryManagerRef memory_manager)
      : memory_manager_(memory_manager) {}

  MemoryManagerRef memory_manager() const override { return memory_manager_; }

  ListType CreateListTypeImpl(TypeView element) override {
    if (auto list_type = list_types_.find(element);
        list_type != list_types_.end()) {
      return list_type->second;
    }
    ListType list_type(memory_manager(), Type(element));
    return list_types_.insert({list_type.element(), list_type}).first->second;
  }

  MapType CreateMapTypeImpl(TypeView key, TypeView value) override {
    if (auto map_type = map_types_.find(std::make_pair(key, value));
        map_type != map_types_.end()) {
      return map_type->second;
    }
    MapType map_type(memory_manager(), Type(key), Type(value));
    return map_types_
        .insert({std::make_pair(map_type.key(), map_type.value()), map_type})
        .first->second;
  }

  StructType CreateStructTypeImpl(absl::string_view name) override {
    if (auto struct_type = struct_types_.find(name);
        struct_type != struct_types_.end()) {
      return struct_type->second;
    }
    StructType struct_type(memory_manager(), name);
    return struct_types_.insert({struct_type.name(), struct_type})
        .first->second;
  }

  OpaqueType CreateOpaqueTypeImpl(
      absl::string_view name,
      const SizedInputView<TypeView>& parameters) override {
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

 private:
  MemoryManagerRef memory_manager_;
  ListTypeCacheMap list_types_;
  MapTypeCacheMap map_types_;
  StructTypeCacheMap struct_types_;
  OpaqueTypeCacheMap opaque_types_;
};

class ThreadSafeTypeFactory final : public TypeFactory {
 public:
  explicit ThreadSafeTypeFactory(MemoryManagerRef memory_manager)
      : memory_manager_(memory_manager) {}

  MemoryManagerRef memory_manager() const override { return memory_manager_; }

  ListType CreateListTypeImpl(TypeView element) override {
    {
      absl::ReaderMutexLock lock(&list_types_mutex_);
      if (auto list_type = list_types_.find(element);
          list_type != list_types_.end()) {
        return list_type->second;
      }
    }
    ListType list_type(memory_manager(), Type(element));
    absl::WriterMutexLock lock(&list_types_mutex_);
    return list_types_.insert({list_type.element(), list_type}).first->second;
  }

  MapType CreateMapTypeImpl(TypeView key, TypeView value) override {
    {
      absl::ReaderMutexLock lock(&map_types_mutex_);
      if (auto map_type = map_types_.find(std::make_pair(key, value));
          map_type != map_types_.end()) {
        return map_type->second;
      }
    }
    MapType map_type(memory_manager(), Type(key), Type(value));
    absl::WriterMutexLock lock(&map_types_mutex_);
    return map_types_
        .insert({std::make_pair(map_type.key(), map_type.value()), map_type})
        .first->second;
  }

  StructType CreateStructTypeImpl(absl::string_view name) override {
    {
      absl::ReaderMutexLock lock(&struct_types_mutex_);
      if (auto struct_type = struct_types_.find(name);
          struct_type != struct_types_.end()) {
        return struct_type->second;
      }
    }
    StructType struct_type(memory_manager(), name);
    absl::WriterMutexLock lock(&struct_types_mutex_);
    return struct_types_.insert({struct_type.name(), struct_type})
        .first->second;
  }

  OpaqueType CreateOpaqueTypeImpl(
      absl::string_view name,
      const SizedInputView<TypeView>& parameters) override {
    if (auto opaque_type =
            ProcessLocalTypeCache::Get()->FindOpaqueType(name, parameters);
        opaque_type.has_value()) {
      return *opaque_type;
    }
    {
      absl::ReaderMutexLock lock(&opaque_types_mutex_);
      if (auto opaque_type = opaque_types_.find(
              OpaqueTypeKeyView{.name = name, .parameters = parameters});
          opaque_type != opaque_types_.end()) {
        return opaque_type->second;
      }
    }
    OpaqueType opaque_type(memory_manager(), name, parameters);
    absl::WriterMutexLock lock(&opaque_types_mutex_);
    return opaque_types_
        .insert({OpaqueTypeKey{.name = opaque_type.name(),
                               .parameters = opaque_type.parameters()},
                 opaque_type})
        .first->second;
  }

 private:
  MemoryManagerRef memory_manager_;
  mutable absl::Mutex list_types_mutex_;
  ListTypeCacheMap list_types_ ABSL_GUARDED_BY(list_types_mutex_);
  mutable absl::Mutex map_types_mutex_;
  MapTypeCacheMap map_types_ ABSL_GUARDED_BY(map_types_mutex_);
  mutable absl::Mutex struct_types_mutex_;
  StructTypeCacheMap struct_types_ ABSL_GUARDED_BY(struct_types_mutex_);
  mutable absl::Mutex opaque_types_mutex_;
  OpaqueTypeCacheMap opaque_types_ ABSL_GUARDED_BY(opaque_types_mutex_);
};

bool IsValidMapKeyType(TypeView type) {
  switch (type.kind()) {
    case TypeKind::kDyn:
      ABSL_FALLTHROUGH_INTENDED;
    case TypeKind::kError:
      ABSL_FALLTHROUGH_INTENDED;
    case TypeKind::kBool:
      ABSL_FALLTHROUGH_INTENDED;
    case TypeKind::kInt:
      ABSL_FALLTHROUGH_INTENDED;
    case TypeKind::kUint:
      ABSL_FALLTHROUGH_INTENDED;
    case TypeKind::kString:
      return true;
    default:
      return false;
  }
}

}  // namespace

ListType TypeFactory::CreateListType(TypeView element) {
  if (auto list_type = ProcessLocalTypeCache::Get()->FindListType(element);
      list_type.has_value()) {
    return *list_type;
  }
  return CreateListTypeImpl(element);
}

MapType TypeFactory::CreateMapType(TypeView key, TypeView value) {
  ABSL_DCHECK(IsValidMapKeyType(key)) << key;
  if (auto map_type = ProcessLocalTypeCache::Get()->FindMapType(key, value);
      map_type.has_value()) {
    return *map_type;
  }
  return CreateMapTypeImpl(key, value);
}

StructType TypeFactory::CreateStructType(absl::string_view name) {
  ABSL_DCHECK(internal::IsValidRelativeName(name)) << name;
  return CreateStructTypeImpl(name);
}

OpaqueType TypeFactory::CreateOpaqueType(
    absl::string_view name, const SizedInputView<TypeView>& parameters) {
  ABSL_DCHECK(internal::IsValidRelativeName(name)) << name;
  if (auto opaque_type =
          ProcessLocalTypeCache::Get()->FindOpaqueType(name, parameters);
      opaque_type.has_value()) {
    return *opaque_type;
  }
  return CreateOpaqueTypeImpl(name, parameters);
}

OptionalType TypeFactory::CreateOptionalType(TypeView parameter) {
  return Cast<OptionalType>(CreateOpaqueType(OptionalType::kName, {parameter}));
}

Shared<TypeFactory> NewThreadCompatibleTypeFactory(
    MemoryManagerRef memory_manager) {
  return memory_manager.MakeShared<ThreadCompatibleTypeFactory>(memory_manager);
}

Shared<TypeFactory> NewThreadSafeTypeFactory(MemoryManagerRef memory_manager) {
  return memory_manager.MakeShared<ThreadSafeTypeFactory>(memory_manager);
}

}  // namespace cel
