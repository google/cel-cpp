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

#include "absl/log/absl_check.h"
#include "absl/synchronization/mutex.h"
#include "base/handle.h"

namespace cel {

namespace {

using base_internal::HandleFactory;
using base_internal::ModernListType;
using base_internal::ModernMapType;

}  // namespace

TypeFactory::TypeFactory(MemoryManager& memory_manager)
    : memory_manager_(memory_manager) {
  json_list_type_ = list_types_
                        .insert({GetJsonValueType(),
                                 HandleFactory<ListType>::Make<ModernListType>(
                                     memory_manager_, GetJsonValueType())})
                        .first->second;
  json_map_type_ =
      map_types_
          .insert({std::make_pair(GetStringType(), GetJsonValueType()),
                   HandleFactory<MapType>::Make<ModernMapType>(
                       memory_manager_, GetStringType(), GetJsonValueType())})
          .first->second;
}

absl::StatusOr<Handle<ListType>> TypeFactory::CreateListType(
    const Handle<Type>& element) {
  ABSL_DCHECK(element) << "handle must not be empty";
  {
    absl::ReaderMutexLock lock(&list_types_mutex_);
    if (auto existing = list_types_.find(element);
        existing != list_types_.end()) {
      return existing->second;
    }
  }
  auto list_type =
      HandleFactory<ListType>::Make<ModernListType>(memory_manager(), element);
  absl::WriterMutexLock lock(&list_types_mutex_);
  return list_types_.insert({element, std::move(list_type)}).first->second;
}

absl::StatusOr<Handle<MapType>> TypeFactory::CreateMapType(
    const Handle<Type>& key, const Handle<Type>& value) {
  ABSL_DCHECK(key) << "handle must not be empty";
  ABSL_DCHECK(value) << "handle must not be empty";
  {
    absl::ReaderMutexLock lock(&map_types_mutex_);
    if (auto existing = map_types_.find({key, value});
        existing != map_types_.end()) {
      return existing->second;
    }
  }
  auto map_type =
      HandleFactory<MapType>::Make<ModernMapType>(memory_manager(), key, value);
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
      HandleFactory<OptionalType>::Make<OptionalType>(memory_manager(), type);
  absl::WriterMutexLock lock(&optional_types_mutex_);
  return optional_types_.insert({type, std::move(optional_type)}).first->second;
}

}  // namespace cel
