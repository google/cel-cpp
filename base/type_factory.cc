// Copyright 2022 Google LLC
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

#include "base/type_factory.h"

#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/status/statusor.h"
#include "absl/synchronization/mutex.h"
#include "base/handle.h"
#include "base/type.h"
#include "base/types/dyn_type.h"
#include "base/types/list_type.h"
#include "base/types/map_type.h"
#include "base/types/optional_type.h"
#include "base/types/string_type.h"
#include "common/memory.h"

namespace cel {

namespace {

using base_internal::HandleFactory;
using base_internal::ModernListType;
using base_internal::ModernMapType;

}  // namespace

TypeFactory::TypeFactory(MemoryManagerRef memory_manager)
    : memory_manager_(memory_manager) {}

const Handle<Type>& TypeFactory::JsonValueType() {
  static absl::NoDestructor<Handle<Type>> kValueType(DynType::Get());
  return *kValueType;
}

const Handle<ListType>& TypeFactory::JsonListType() {
  static absl::NoDestructor<Handle<ListType>> kListType(
      HandleFactory<ListType>::Make<ModernListType>(
          MemoryManagerRef::ReferenceCounting(), JsonValueType()));

  return *kListType;
}

const Handle<MapType>& TypeFactory::JsonMapType() {
  static absl::NoDestructor<Handle<MapType>> kMapType(
      HandleFactory<MapType>::Make<ModernMapType>(
          MemoryManagerRef::ReferenceCounting(), StringType::Get(),
          JsonValueType()));
  return *kMapType;
}

const absl::flat_hash_map<Handle<Type>, Handle<ListType>>&
TypeFactory::BuiltinListTypes() {
  static absl::NoDestructor<absl::flat_hash_map<Handle<Type>, Handle<ListType>>>
      kTypes([]() -> absl::flat_hash_map<Handle<Type>, Handle<ListType>> {
        return {std::make_pair(JsonValueType(), JsonListType())};
      }());

  return *kTypes;
}

const absl::flat_hash_map<std::pair<Handle<Type>, Handle<Type>>,
                          Handle<MapType>>&
TypeFactory::BuiltinMapTypes() {
  static absl::NoDestructor<absl::flat_hash_map<
      std::pair<Handle<Type>, Handle<Type>>, Handle<MapType>>>
      kTypes([]() -> absl::flat_hash_map<std::pair<Handle<Type>, Handle<Type>>,
                                         Handle<MapType>> {
        return {std::make_pair(std::make_pair(StringType::Get().As<Type>(),
                                              JsonValueType()),
                               JsonMapType()),
                std::make_pair(std::make_pair(DynType::Get().As<Type>(),
                                              DynType::Get().As<Type>()),
                               HandleFactory<MapType>::Make<ModernMapType>(
                                   MemoryManagerRef::ReferenceCounting(),
                                   DynType::Get(), DynType::Get()))};
      }());

  return *kTypes;
}

absl::StatusOr<Handle<ListType>> TypeFactory::CreateListType(
    const Handle<Type>& element) {
  ABSL_DCHECK(element) << "handle must not be empty";
  if (auto it = BuiltinListTypes().find(element);
      it != BuiltinListTypes().end()) {
    return it->second;
  }
  {
    absl::ReaderMutexLock lock(&list_types_mutex_);
    if (auto existing = list_types_.find(element);
        existing != list_types_.end()) {
      return existing->second;
    }
  }
  auto list_type = HandleFactory<ListType>::Make<ModernListType>(
      GetMemoryManager(), element);
  absl::WriterMutexLock lock(&list_types_mutex_);
  return list_types_.insert({element, std::move(list_type)}).first->second;
}

absl::StatusOr<Handle<MapType>> TypeFactory::CreateMapType(
    const Handle<Type>& key, const Handle<Type>& value) {
  ABSL_DCHECK(key) << "handle must not be empty";
  ABSL_DCHECK(value) << "handle must not be empty";
  if (auto it = BuiltinMapTypes().find({key, value});
      it != BuiltinMapTypes().end()) {
    return it->second;
  }
  {
    absl::ReaderMutexLock lock(&map_types_mutex_);
    if (auto existing = map_types_.find({key, value});
        existing != map_types_.end()) {
      return existing->second;
    }
  }
  auto map_type = HandleFactory<MapType>::Make<ModernMapType>(
      GetMemoryManager(), key, value);
  absl::WriterMutexLock lock(&map_types_mutex_);
  return map_types_.insert({std::make_pair(key, value), std::move(map_type)})
      .first->second;
}

absl::StatusOr<Handle<OptionalType>> TypeFactory::CreateOptionalType(
    const Handle<Type>& type) {
  ABSL_DCHECK(type) << "handle must not be empty";
  {
    absl::ReaderMutexLock lock(&optional_types_mutex_);
    if (auto existing = optional_types_.find(type);
        existing != optional_types_.end()) {
      return existing->second;
    }
  }
  auto optional_type =
      HandleFactory<OptionalType>::Make<OptionalType>(GetMemoryManager(), type);
  absl::WriterMutexLock lock(&optional_types_mutex_);
  return optional_types_.insert({type, std::move(optional_type)}).first->second;
}

}  // namespace cel
