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

// IWYU pragma: private

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPE_CACHE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPE_CACHE_H_

#include <algorithm>
#include <cstddef>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/hash/hash.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "common/memory.h"
#include "common/sized_input_view.h"
#include "common/type.h"

namespace cel::common_internal {

struct OpaqueTypeKey {
  absl::string_view name;
  absl::Span<const Type> parameters;
};

template <typename H>
H AbslHashValue(H state, const OpaqueTypeKey& key) {
  state = H::combine(std::move(state), key.name);
  for (const auto& parameter : key.parameters) {
    state = H::combine(std::move(state), parameter);
  }
  return std::move(state);
}

struct OpaqueTypeKeyView {
  absl::string_view name;
  const SizedInputView<TypeView>& parameters;
};

template <typename H>
H AbslHashValue(H state, const OpaqueTypeKeyView& key) {
  state = H::combine(std::move(state), key.name);
  for (const auto& parameter : key.parameters) {
    state = H::combine(std::move(state), parameter);
  }
  return std::move(state);
}

struct OpaqueTypeKeyEqualTo {
  using is_transparent = void;

  bool operator()(const OpaqueTypeKey& lhs, const OpaqueTypeKey& rhs) const {
    return lhs.name == rhs.name &&
           lhs.parameters.size() == rhs.parameters.size() &&
           std::equal(lhs.parameters.begin(), lhs.parameters.end(),
                      rhs.parameters.begin(), rhs.parameters.end());
  }

  bool operator()(const OpaqueTypeKey& lhs,
                  const OpaqueTypeKeyView& rhs) const {
    return lhs.name == rhs.name &&
           lhs.parameters.size() == rhs.parameters.size() &&
           std::equal(lhs.parameters.begin(), lhs.parameters.end(),
                      rhs.parameters.begin(), rhs.parameters.end());
  }

  bool operator()(const OpaqueTypeKeyView& lhs,
                  const OpaqueTypeKey& rhs) const {
    return lhs.name == rhs.name &&
           lhs.parameters.size() == rhs.parameters.size() &&
           std::equal(lhs.parameters.begin(), lhs.parameters.end(),
                      rhs.parameters.begin(), rhs.parameters.end());
  }

  bool operator()(const OpaqueTypeKeyView& lhs,
                  const OpaqueTypeKeyView& rhs) const {
    return lhs.name == rhs.name &&
           lhs.parameters.size() == rhs.parameters.size() &&
           std::equal(lhs.parameters.begin(), lhs.parameters.end(),
                      rhs.parameters.begin(), rhs.parameters.end());
  }
};

struct OpaqueTypeKeyHash {
  using is_transparent = void;

  size_t operator()(const OpaqueTypeKey& key) const {
    return absl::HashOf(key);
  }

  size_t operator()(const OpaqueTypeKeyView& key) const {
    return absl::HashOf(key);
  }
};

using ListTypeCacheMap = absl::flat_hash_map<TypeView, ListType>;
using MapTypeCacheMap =
    absl::flat_hash_map<std::pair<TypeView, TypeView>, MapType>;
using OpaqueTypeCacheMap =
    absl::flat_hash_map<OpaqueTypeKey, OpaqueType, OpaqueTypeKeyHash,
                        OpaqueTypeKeyEqualTo>;
using OptionalTypeCacheMap = absl::flat_hash_map<TypeView, OptionalType>;
using StructTypeCacheMap = absl::flat_hash_map<absl::string_view, StructType>;

class ProcessLocalTypeCache final {
 public:
  ABSL_ATTRIBUTE_PURE_FUNCTION static const ProcessLocalTypeCache* Get();

  absl::optional<ListTypeView> FindListType(TypeView element) const;

  SizedInputView<ListTypeView> ListTypes() const;

  absl::optional<MapTypeView> FindMapType(TypeView key, TypeView value) const;

  SizedInputView<MapTypeView> MapTypes() const;

  absl::optional<OpaqueTypeView> FindOpaqueType(
      absl::string_view name, const SizedInputView<TypeView>& parameters) const;

  absl::optional<OptionalTypeView> FindOptionalType(TypeView type) const;

  SizedInputView<OptionalTypeView> OptionalTypes() const;

  ListTypeView GetDynListType() const { return *dyn_list_type_; }

  MapTypeView GetDynDynMapType() const { return *dyn_dyn_map_type_; }

  MapTypeView GetStringDynMapType() const { return *string_dyn_map_type_; }

  OptionalTypeView GetDynOptionalType() const { return *dyn_optional_type_; }

 private:
  friend class absl::NoDestructor<ProcessLocalTypeCache>;

  ProcessLocalTypeCache();

  template <typename... Ts>
  void PopulateTypes(MemoryManagerRef memory_manager);

  template <typename... Ts>
  void PopulateListTypes(MemoryManagerRef memory_manager);

  template <typename... Ts>
  void PopulateMapTypes(MemoryManagerRef memory_manager);

  template <typename... Ts>
  void PopulateOptionalTypes(MemoryManagerRef memory_manager);

  template <typename T, typename... Ts>
  void DoPopulateListTypes(MemoryManagerRef memory_manager);

  void InsertListType(ListType list_type);

  template <typename T, typename... Ts>
  void DoPopulateMapTypes(MemoryManagerRef memory_manager);

  void InsertMapType(MapType map_type);

  template <typename T, typename... Ts>
  void DoPopulateOptionalTypes(MemoryManagerRef memory_manager);

  void InsertOptionalType(OptionalType optional_type);

  ListTypeCacheMap list_types_;
  MapTypeCacheMap map_types_;
  OptionalTypeCacheMap optional_types_;
  OpaqueTypeCacheMap opaque_types_;
  absl::optional<ListTypeView> dyn_list_type_;
  absl::optional<MapTypeView> dyn_dyn_map_type_;
  absl::optional<MapTypeView> string_dyn_map_type_;
  absl::optional<OptionalTypeView> dyn_optional_type_;
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPE_CACHE_H_
