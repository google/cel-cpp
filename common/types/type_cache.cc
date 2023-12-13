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

#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/sized_input_view.h"
#include "common/type.h"
#include "internal/no_destructor.h"

namespace cel::common_internal {

const ProcessLocalTypeCache* ProcessLocalTypeCache::Get() {
  static const internal::NoDestructor<ProcessLocalTypeCache> type_cache;
  return &*type_cache;
}

absl::optional<ListTypeView> ProcessLocalTypeCache::FindListType(
    TypeView element) const {
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

SizedInputView<ListTypeView> ProcessLocalTypeCache::ListTypes() const {
  return SizedInputView<ListTypeView>(list_types_, MapValueTransformer{});
}

absl::optional<MapTypeView> ProcessLocalTypeCache::FindMapType(
    TypeView key, TypeView value) const {
  if (auto map_type = map_types_.find(std::make_pair(key, value));
      map_type != map_types_.end()) {
    return map_type->second;
  }
  return absl::nullopt;
}

SizedInputView<MapTypeView> ProcessLocalTypeCache::MapTypes() const {
  return SizedInputView<MapTypeView>(map_types_, MapValueTransformer{});
}

absl::optional<OpaqueTypeView> ProcessLocalTypeCache::FindOpaqueType(
    absl::string_view name, const SizedInputView<TypeView>& parameters) const {
  if (name == OptionalType::kName && parameters.size() == 1) {
    return FindOptionalType(*parameters.begin());
  }
  if (auto opaque_type = opaque_types_.find(
          OpaqueTypeKeyView{.name = name, .parameters = parameters});
      opaque_type != opaque_types_.end()) {
    return opaque_type->second;
  }
  return absl::nullopt;
}

absl::optional<OptionalTypeView> ProcessLocalTypeCache::FindOptionalType(
    TypeView type) const {
  if (auto optional_type = optional_types_.find(type);
      optional_type != optional_types_.end()) {
    return optional_type->second;
  }
  return absl::nullopt;
}

SizedInputView<OptionalTypeView> ProcessLocalTypeCache::OptionalTypes() const {
  return SizedInputView<OptionalTypeView>(optional_types_,
                                          MapValueTransformer{});
}

ProcessLocalTypeCache::ProcessLocalTypeCache() {
  PopulateTypes<AnyType, BoolType, BoolWrapperType, BytesType, BytesWrapperType,
                DoubleType, DoubleWrapperType, DurationType, DynType, ErrorType,
                IntType, IntWrapperType, NullType, StringType,
                StringWrapperType, TimestampType, TypeType, UintType,
                UintWrapperType, UnknownType>(MemoryManagerRef::Unmanaged());
  dyn_list_type_ = FindListType(DynTypeView());
  ABSL_DCHECK(dyn_list_type_.has_value());
  dyn_dyn_map_type_ = FindMapType(DynTypeView(), DynTypeView());
  ABSL_DCHECK(dyn_dyn_map_type_.has_value());
  string_dyn_map_type_ = FindMapType(StringTypeView(), DynTypeView());
  ABSL_DCHECK(string_dyn_map_type_.has_value());
  dyn_optional_type_ = FindOptionalType(DynTypeView());
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
    InsertOptionalType(
        OptionalType(memory_manager, TypeView(list_type.second)));
  }
  for (const auto& map_type : map_types_) {
    InsertOptionalType(OptionalType(memory_manager, TypeView(map_type.second)));
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
  InsertOptionalType(
      OptionalType(memory_manager, typename T::view_alternative_type()));
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

ListTypeView GetDynListType() {
  return ProcessLocalTypeCache::Get()->GetDynListType();
}

MapTypeView GetDynDynMapType() {
  return ProcessLocalTypeCache::Get()->GetDynDynMapType();
}

OptionalTypeView GetDynOptionalType() {
  return ProcessLocalTypeCache::Get()->GetDynOptionalType();
}

}  // namespace cel::common_internal
