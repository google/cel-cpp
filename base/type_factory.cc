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

#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "base/handle.h"

namespace cel {

namespace {

using base_internal::PersistentHandleFactory;

}  // namespace

Persistent<NullType> TypeFactory::GetNullType() { return NullType::Get(); }

Persistent<ErrorType> TypeFactory::GetErrorType() { return ErrorType::Get(); }

Persistent<DynType> TypeFactory::GetDynType() { return DynType::Get(); }

Persistent<AnyType> TypeFactory::GetAnyType() { return AnyType::Get(); }

Persistent<BoolType> TypeFactory::GetBoolType() { return BoolType::Get(); }

Persistent<IntType> TypeFactory::GetIntType() { return IntType::Get(); }

Persistent<UintType> TypeFactory::GetUintType() { return UintType::Get(); }

Persistent<DoubleType> TypeFactory::GetDoubleType() {
  return DoubleType::Get();
}

Persistent<StringType> TypeFactory::GetStringType() {
  return StringType::Get();
}

Persistent<BytesType> TypeFactory::GetBytesType() { return BytesType::Get(); }

Persistent<DurationType> TypeFactory::GetDurationType() {
  return DurationType::Get();
}

Persistent<TimestampType> TypeFactory::GetTimestampType() {
  return TimestampType::Get();
}

Persistent<TypeType> TypeFactory::GetTypeType() { return TypeType::Get(); }

Persistent<UnknownType> TypeFactory::GetUnknownType() {
  return UnknownType::Get();
}

absl::StatusOr<Persistent<ListType>> TypeFactory::CreateListType(
    const Persistent<Type>& element) {
  absl::MutexLock lock(&list_types_mutex_);
  auto existing = list_types_.find(element);
  if (existing != list_types_.end()) {
    return existing->second;
  }
  auto list_type =
      PersistentHandleFactory<ListType>::Make<base_internal::ModernListType>(
          memory_manager(), element);
  if (ABSL_PREDICT_FALSE(!list_type)) {
    // TODO(issues/5): maybe have the handle factories return statuses as
    // they can add details on the size and alignment more easily and
    // consistently?
    return absl::ResourceExhaustedError("Failed to allocate memory");
  }
  list_types_.insert({element, list_type});
  return list_type;
}

absl::StatusOr<Persistent<MapType>> TypeFactory::CreateMapType(
    const Persistent<Type>& key, const Persistent<Type>& value) {
  auto key_and_value = std::make_pair(key, value);
  absl::MutexLock lock(&map_types_mutex_);
  auto existing = map_types_.find(key_and_value);
  if (existing != map_types_.end()) {
    return existing->second;
  }
  auto map_type =
      PersistentHandleFactory<MapType>::Make<base_internal::ModernMapType>(
          memory_manager(), key, value);
  if (ABSL_PREDICT_FALSE(!map_type)) {
    // TODO(issues/5): maybe have the handle factories return statuses as
    // they can add details on the size and alignment more easily and
    // consistently?
    return absl::ResourceExhaustedError("Failed to allocate memory");
  }
  map_types_.insert({std::move(key_and_value), map_type});
  return map_type;
}

}  // namespace cel
