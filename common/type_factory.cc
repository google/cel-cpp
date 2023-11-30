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

#include <algorithm>
#include <cstddef>
#include <utility>

#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/hash/hash.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "common/memory.h"
#include "common/sized_input_view.h"
#include "common/type.h"

namespace cel {

namespace {

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

struct CommonTypes final {
  absl::flat_hash_map<TypeView, ListType> list_types;
  absl::flat_hash_map<std::pair<TypeView, TypeView>, MapType> map_types;

  absl::optional<ListType> FindListType(TypeView element) const {
    if (auto list_type = list_types.find(element);
        list_type != list_types.end()) {
      return list_type->second;
    }
    return absl::nullopt;
  }

  absl::optional<MapType> FindMapType(TypeView key, TypeView value) const {
    if (auto map_type = map_types.find(std::make_pair(key, value));
        map_type != map_types.end()) {
      return map_type->second;
    }
    return absl::nullopt;
  }

  static const CommonTypes* Get() {
    static const CommonTypes* common_types = []() -> const CommonTypes* {
      MemoryManagerRef memory_manager = MemoryManagerRef::Unmanaged();
      CommonTypes* common_types = new CommonTypes();
      common_types->PopulateListTypes<
          BoolType, BytesType, DoubleType, DurationType, DynType, IntType,
          NullType, StringType, TimestampType, TypeType, UintType>(
          memory_manager);
      common_types
          ->PopulateMapTypes<BoolType, BytesType, DoubleType, DurationType,
                             DynType, IntType, NullType, StringType,
                             TimestampType, TypeType, UintType>(memory_manager);
      return common_types;
    }();
    return common_types;
  }

 private:
  template <typename... Ts>
  void PopulateListTypes(MemoryManagerRef memory_manager) {
    list_types.reserve(sizeof...(Ts));
    DoPopulateListTypes<Ts...>(memory_manager);
  }

  template <typename... Ts>
  void PopulateMapTypes(MemoryManagerRef memory_manager) {
    map_types.reserve(sizeof...(Ts) * 4);
    DoPopulateMapTypes<Ts...>(memory_manager);
  }

  template <typename T, typename... Ts>
  void DoPopulateListTypes(MemoryManagerRef memory_manager) {
    list_types.insert_or_assign(typename T::view_alternative_type(),
                                ListType(memory_manager, T()));
    if constexpr (sizeof...(Ts) != 0) {
      DoPopulateListTypes<Ts...>(memory_manager);
    }
  }

  template <typename T, typename... Ts>
  void DoPopulateMapTypes(MemoryManagerRef memory_manager) {
    map_types.insert_or_assign(
        std::make_pair(BoolTypeView(), typename T::view_alternative_type()),
        MapType(memory_manager, BoolType(), T()));
    map_types.insert_or_assign(
        std::make_pair(IntTypeView(), typename T::view_alternative_type()),
        MapType(memory_manager, IntType(), T()));
    map_types.insert_or_assign(
        std::make_pair(UintTypeView(), typename T::view_alternative_type()),
        MapType(memory_manager, UintType(), T()));
    map_types.insert_or_assign(
        std::make_pair(StringTypeView(), typename T::view_alternative_type()),
        MapType(memory_manager, StringType(), T()));
    if constexpr (sizeof...(Ts) != 0) {
      DoPopulateMapTypes<Ts...>(memory_manager);
    }
  }
};

class ThreadCompatibleTypeFactory final : public TypeFactory {
 public:
  explicit ThreadCompatibleTypeFactory(MemoryManagerRef memory_manager)
      : memory_manager_(memory_manager) {}

  MemoryManagerRef memory_manager() const { return memory_manager_; }

  absl::StatusOr<ListType> CreateListType(TypeView element) override {
    if (auto list_type = CommonTypes::Get()->FindListType(element);
        list_type.has_value()) {
      return *list_type;
    }
    if (auto list_type = list_types_.find(element);
        list_type != list_types_.end()) {
      return list_type->second;
    }
    ListType list_type(memory_manager(), Type(element));
    return list_types_.insert({list_type.element(), list_type}).first->second;
  }

  absl::StatusOr<MapType> CreateMapType(TypeView key, TypeView value) override {
    if (auto map_type = CommonTypes::Get()->FindMapType(key, value);
        map_type.has_value()) {
      return *map_type;
    }
    if (auto map_type = map_types_.find(std::make_pair(key, value));
        map_type != map_types_.end()) {
      return map_type->second;
    }
    MapType map_type(memory_manager(), Type(key), Type(value));
    return map_types_
        .insert({std::make_pair(map_type.key(), map_type.value()), map_type})
        .first->second;
  }

  absl::StatusOr<StructType> CreateStructType(absl::string_view name) override {
    if (auto struct_type = struct_types_.find(name);
        struct_type != struct_types_.end()) {
      return struct_type->second;
    }
    StructType struct_type(memory_manager(), name);
    return struct_types_.insert({struct_type.name(), struct_type})
        .first->second;
  }

  absl::StatusOr<OpaqueType> CreateOpaqueType(
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
  absl::flat_hash_map<TypeView, ListType> list_types_;
  absl::flat_hash_map<std::pair<TypeView, TypeView>, MapType> map_types_;
  absl::flat_hash_map<absl::string_view, StructType> struct_types_;
  absl::flat_hash_map<OpaqueTypeKey, OpaqueType, OpaqueTypeKeyHash,
                      OpaqueTypeKeyEqualTo>
      opaque_types_;
};

class ThreadSafeTypeFactory final : public TypeFactory {
 public:
  explicit ThreadSafeTypeFactory(MemoryManagerRef memory_manager)
      : memory_manager_(memory_manager) {}

  MemoryManagerRef memory_manager() const { return memory_manager_; }

  absl::StatusOr<ListType> CreateListType(TypeView element) override {
    if (auto list_type = CommonTypes::Get()->FindListType(element);
        list_type.has_value()) {
      return *list_type;
    }
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

  absl::StatusOr<MapType> CreateMapType(TypeView key, TypeView value) override {
    if (auto map_type = CommonTypes::Get()->FindMapType(key, value);
        map_type.has_value()) {
      return *map_type;
    }
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

  absl::StatusOr<StructType> CreateStructType(absl::string_view name) override {
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

  absl::StatusOr<OpaqueType> CreateOpaqueType(
      absl::string_view name,
      const SizedInputView<TypeView>& parameters) override {
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
  absl::flat_hash_map<TypeView, ListType> list_types_
      ABSL_GUARDED_BY(list_types_mutex_);
  mutable absl::Mutex map_types_mutex_;
  absl::flat_hash_map<std::pair<TypeView, TypeView>, MapType> map_types_
      ABSL_GUARDED_BY(map_types_mutex_);
  mutable absl::Mutex struct_types_mutex_;
  absl::flat_hash_map<absl::string_view, StructType> struct_types_
      ABSL_GUARDED_BY(struct_types_mutex_);
  mutable absl::Mutex opaque_types_mutex_;
  absl::flat_hash_map<OpaqueTypeKey, OpaqueType, OpaqueTypeKeyHash,
                      OpaqueTypeKeyEqualTo>
      opaque_types_ ABSL_GUARDED_BY(opaque_types_mutex_);
};

}  // namespace

Shared<TypeFactory> NewThreadCompatibleTypeFactory(
    MemoryManagerRef memory_manager) {
  return memory_manager.MakeShared<ThreadCompatibleTypeFactory>(memory_manager);
}

Shared<TypeFactory> NewThreadSafeTypeFactory(MemoryManagerRef memory_manager) {
  return memory_manager.MakeShared<ThreadSafeTypeFactory>(memory_manager);
}

}  // namespace cel
