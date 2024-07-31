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

#include "common/types/type_cache.h"

#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/functional/function_ref.h"
#include "absl/log/absl_check.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "common/memory.h"
#include "common/type.h"

namespace cel::common_internal {

absl::Nonnull<const ProcessLocalTypeCache*> ProcessLocalTypeCache::Get() {
  static const absl::NoDestructor<ProcessLocalTypeCache> type_cache;
  return &*type_cache;
}

absl::optional<ListType> ProcessLocalTypeCache::FindListType(
    const Type& element) const {
  if (auto list_type = list_types_.find(element);
      list_type != list_types_.end()) {
    return list_type->second;
  }
  return absl::nullopt;
}

namespace {

struct MapValueTransformer {
  template <typename Pair>
  decltype(auto) operator()(Pair& to_transformer) const {
    return to_transformer.second;
  }
};

}  // namespace

void ProcessLocalTypeCache::ListTypes(
    absl::FunctionRef<void(const ListType&)> callback) const {
  for (const auto& list_type : list_types_) {
    callback(list_type.second);
  }
}

absl::optional<MapType> ProcessLocalTypeCache::FindMapType(
    const Type& key, const Type& value) const {
  if (auto map_type = map_types_.find(std::make_pair(key, value));
      map_type != map_types_.end()) {
    return map_type->second;
  }
  return absl::nullopt;
}

void ProcessLocalTypeCache::MapTypes(
    absl::FunctionRef<void(const MapType&)> callback) const {
  for (const auto& map_type : map_types_) {
    callback(map_type.second);
  }
}

absl::optional<OpaqueType> ProcessLocalTypeCache::FindOpaqueType(
    absl::string_view name, absl::Span<const Type> parameters) const {
  if (name == OptionalType::kName && parameters.size() == 1) {
    return FindOptionalType(*parameters.begin());
  }
  if (auto opaque_type = opaque_types_.find(
          OpaqueTypeKey{.name = name, .parameters = parameters});
      opaque_type != opaque_types_.end()) {
    return opaque_type->second;
  }
  return absl::nullopt;
}

absl::optional<OptionalType> ProcessLocalTypeCache::FindOptionalType(
    const Type& type) const {
  if (auto optional_type = optional_types_.find(type);
      optional_type != optional_types_.end()) {
    return optional_type->second;
  }
  return absl::nullopt;
}

void ProcessLocalTypeCache::OptionalTypes(
    absl::FunctionRef<void(const OptionalType&)> callback) const {
  for (const auto& optional_type : optional_types_) {
    callback(optional_type.second);
  }
}

ProcessLocalTypeCache::ProcessLocalTypeCache() {
  PopulateTypes<AnyType, BoolType, BoolWrapperType, BytesType, BytesWrapperType,
                DoubleType, DoubleWrapperType, DurationType, DynType, ErrorType,
                IntType, IntWrapperType, NullType, StringType,
                StringWrapperType, TimestampType, TypeType, UintType,
                UintWrapperType, UnknownType>(MemoryManagerRef::Unmanaged());
  dyn_list_type_ = FindListType(DynType());
  ABSL_DCHECK(dyn_list_type_.has_value());
  dyn_dyn_map_type_ = FindMapType(DynType(), DynType());
  ABSL_DCHECK(dyn_dyn_map_type_.has_value());
  string_dyn_map_type_ = FindMapType(StringType(), DynType());
  ABSL_DCHECK(string_dyn_map_type_.has_value());
  dyn_optional_type_ = FindOptionalType(DynType());
  ABSL_DCHECK(dyn_optional_type_.has_value());
}

template <typename... Ts>
void ProcessLocalTypeCache::PopulateTypes(MemoryManagerRef memory_manager) {
  PopulateListTypes<Ts...>(memory_manager);
  PopulateMapTypes<Ts...>(memory_manager);
  PopulateOptionalTypes<Ts...>(memory_manager);
}

template <typename... Ts>
void ProcessLocalTypeCache::PopulateListTypes(MemoryManagerRef memory_manager) {
  list_types_.reserve(sizeof...(Ts));
  DoPopulateListTypes<Ts...>(memory_manager);
}

template <typename... Ts>
void ProcessLocalTypeCache::PopulateMapTypes(MemoryManagerRef memory_manager) {
  map_types_.reserve(sizeof...(Ts) * 5);
  DoPopulateMapTypes<Ts...>(memory_manager);
}

template <typename... Ts>
void ProcessLocalTypeCache::PopulateOptionalTypes(
    MemoryManagerRef memory_manager) {
  // Reserve space for optionals of each primitive type, optionals of each
  // list type, and optionals of each map type.
  optional_types_.reserve(sizeof...(Ts) + list_types_.size() +
                          map_types_.size());
  DoPopulateOptionalTypes<Ts...>(memory_manager);
  for (const auto& list_type : list_types_) {
    InsertOptionalType(OptionalType(memory_manager, Type(list_type.second)));
  }
  for (const auto& map_type : map_types_) {
    InsertOptionalType(OptionalType(memory_manager, Type(map_type.second)));
  }
}

template <typename T, typename... Ts>
void ProcessLocalTypeCache::DoPopulateListTypes(
    MemoryManagerRef memory_manager) {
  InsertListType(ListType(memory_manager, T()));
  if constexpr (sizeof...(Ts) != 0) {
    DoPopulateListTypes<Ts...>(memory_manager);
  }
}

void ProcessLocalTypeCache::InsertListType(ListType list_type) {
  auto element = list_type.element();
  auto inserted =
      list_types_.insert_or_assign(element, std::move(list_type)).second;
  ABSL_DCHECK(inserted);
}

template <typename T, typename... Ts>
void ProcessLocalTypeCache::DoPopulateMapTypes(
    MemoryManagerRef memory_manager) {
  InsertMapType(MapType(memory_manager, DynType(), T()));
  InsertMapType(MapType(memory_manager, BoolType(), T()));
  InsertMapType(MapType(memory_manager, IntType(), T()));
  InsertMapType(MapType(memory_manager, UintType(), T()));
  InsertMapType(MapType(memory_manager, StringType(), T()));
  if constexpr (sizeof...(Ts) != 0) {
    DoPopulateMapTypes<Ts...>(memory_manager);
  }
}

void ProcessLocalTypeCache::InsertMapType(MapType map_type) {
  auto key = map_type.key();
  auto value = map_type.value();
  auto inserted =
      map_types_
          .insert_or_assign(std::make_pair(key, value), std::move(map_type))
          .second;
  ABSL_DCHECK(inserted);
}

template <typename T, typename... Ts>
void ProcessLocalTypeCache::DoPopulateOptionalTypes(
    MemoryManagerRef memory_manager) {
  InsertOptionalType(OptionalType(memory_manager, T()));
  if constexpr (sizeof...(Ts) != 0) {
    DoPopulateOptionalTypes<Ts...>(memory_manager);
  }
}

void ProcessLocalTypeCache::InsertOptionalType(OptionalType optional_type) {
  auto parameter = optional_type.parameter();
  auto inserted =
      optional_types_.insert_or_assign(parameter, std::move(optional_type))
          .second;
  ABSL_DCHECK(inserted);
}

ListType GetDynListType() {
  return ProcessLocalTypeCache::Get()->GetDynListType();
}

MapType GetDynDynMapType() {
  return ProcessLocalTypeCache::Get()->GetDynDynMapType();
}

OptionalType GetDynOptionalType() {
  return ProcessLocalTypeCache::Get()->GetDynOptionalType();
}

}  // namespace cel::common_internal
