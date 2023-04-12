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

using base_internal::HandleFactory;

}  // namespace

Handle<NullType> TypeFactory::GetNullType() { return NullType::Get(); }

Handle<ErrorType> TypeFactory::GetErrorType() { return ErrorType::Get(); }

Handle<DynType> TypeFactory::GetDynType() { return DynType::Get(); }

Handle<AnyType> TypeFactory::GetAnyType() { return AnyType::Get(); }

Handle<BoolType> TypeFactory::GetBoolType() { return BoolType::Get(); }

Handle<IntType> TypeFactory::GetIntType() { return IntType::Get(); }

Handle<UintType> TypeFactory::GetUintType() { return UintType::Get(); }

Handle<DoubleType> TypeFactory::GetDoubleType() { return DoubleType::Get(); }

Handle<StringType> TypeFactory::GetStringType() { return StringType::Get(); }

Handle<BytesType> TypeFactory::GetBytesType() { return BytesType::Get(); }

Handle<DurationType> TypeFactory::GetDurationType() {
  return DurationType::Get();
}

Handle<TimestampType> TypeFactory::GetTimestampType() {
  return TimestampType::Get();
}

Handle<TypeType> TypeFactory::GetTypeType() { return TypeType::Get(); }

Handle<UnknownType> TypeFactory::GetUnknownType() { return UnknownType::Get(); }

absl::StatusOr<Handle<ListType>> TypeFactory::CreateListType(
    const Handle<Type>& element) {
  absl::MutexLock lock(&list_types_mutex_);
  auto existing = list_types_.find(element);
  if (existing != list_types_.end()) {
    return existing->second;
  }
  auto list_type = HandleFactory<ListType>::Make<base_internal::ModernListType>(
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

absl::StatusOr<Handle<MapType>> TypeFactory::CreateMapType(
    const Handle<Type>& key, const Handle<Type>& value) {
  auto key_and_value = std::make_pair(key, value);
  absl::MutexLock lock(&map_types_mutex_);
  auto existing = map_types_.find(key_and_value);
  if (existing != map_types_.end()) {
    return existing->second;
  }
  auto map_type = HandleFactory<MapType>::Make<base_internal::ModernMapType>(
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

absl::StatusOr<Handle<OptionalType>> TypeFactory::CreateOptionalType(
    Handle<Type> type) {
  {
    absl::ReaderMutexLock lock(&optional_types_mutex_);
    auto existing = optional_types_.find(type);
    if (existing != optional_types_.end()) {
      return existing->second;
    }
  }
  absl::WriterMutexLock lock(&optional_types_mutex_);
  auto optional_type = HandleFactory<OptionalType>::Make<OptionalType>(
      memory_manager(), std::move(type));
  return optional_types_
      .insert({Handle<Type>(optional_type->type()), std::move(optional_type)})
      .first->second;
}

}  // namespace cel
