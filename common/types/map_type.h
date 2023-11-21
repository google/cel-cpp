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

// IWYU pragma: private, include "common/type.h"
// IWYU pragma: friend "common/type.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_MAP_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_MAP_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type_kind.h"

namespace cel {

class Type;
class TypeView;
class MapType;
class MapTypeView;

namespace common_internal {
struct MapTypeData;
}  // namespace common_internal

class MapType final {
 public:
  using view_alternative_type = MapTypeView;

  static constexpr TypeKind kKind = TypeKind::kMap;
  static constexpr absl::string_view kName = "map";

  explicit MapType(MapTypeView other);

  MapType(MemoryManagerRef memory_manager, Type key, Type value);

  MapType() = delete;
  MapType(const MapType&) = default;
  MapType(MapType&&) = default;
  MapType& operator=(const MapType&) = default;
  MapType& operator=(MapType&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  std::string DebugString() const;

  void swap(MapType& other) noexcept {
    using std::swap;
    swap(data_, other.data_);
  }

  TypeView key() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  TypeView value() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

 private:
  friend class MapTypeView;
  friend struct NativeTypeTraits<MapType>;

  Shared<const common_internal::MapTypeData> data_;
};

inline void swap(MapType& lhs, MapType& rhs) noexcept { lhs.swap(rhs); }

bool operator==(const MapType& lhs, const MapType& rhs);

inline bool operator!=(const MapType& lhs, const MapType& rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, const MapType& type);

inline std::ostream& operator<<(std::ostream& out, const MapType& type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<MapType> final {
  static bool SkipDestructor(const MapType& type) {
    return NativeType::SkipDestructor(type.data_);
  }
};

class MapTypeView final {
 public:
  using alternative_type = MapType;

  static constexpr TypeKind kKind = MapType::kKind;
  static constexpr absl::string_view kName = MapType::kName;

  // NOLINTNEXTLINE(google-explicit-constructor)
  MapTypeView(const MapType& type ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept;

  MapTypeView() = delete;
  MapTypeView(const MapTypeView&) = default;
  MapTypeView(MapTypeView&&) = default;
  MapTypeView& operator=(const MapTypeView&) = default;
  MapTypeView& operator=(MapTypeView&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  std::string DebugString() const;

  void swap(MapTypeView& other) noexcept {
    using std::swap;
    swap(data_, other.data_);
  }

  TypeView key() const;

  TypeView value() const;

 private:
  friend class MapType;

  SharedView<const common_internal::MapTypeData> data_;
};

inline void swap(MapTypeView& lhs, MapTypeView& rhs) noexcept { lhs.swap(rhs); }

bool operator==(MapTypeView lhs, MapTypeView rhs);

inline bool operator!=(MapTypeView lhs, MapTypeView rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, MapTypeView type);

inline std::ostream& operator<<(std::ostream& out, MapTypeView type) {
  return out << type.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_MAP_TYPE_H_
