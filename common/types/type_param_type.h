// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPE_PARAM_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPE_PARAM_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type_kind.h"

namespace cel {

class TypeParamType;
class TypeParamTypeView;

namespace common_internal {
struct TypeParamTypeData;
}  // namespace common_internal

class TypeParamType final {
 public:
  using view_alternative_type = TypeParamTypeView;

  static constexpr TypeKind kKind = TypeKind::kTypeParam;

  explicit TypeParamType(TypeParamTypeView other);

  TypeParamType(MemoryManagerRef memory_manager, absl::string_view name);

  TypeParamType() = delete;
  TypeParamType(const TypeParamType&) = default;
  TypeParamType(TypeParamType&&) = default;
  TypeParamType& operator=(const TypeParamType&) = default;
  TypeParamType& operator=(TypeParamType&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  absl::string_view name() const;

  std::string DebugString() const { return std::string(name()); }

  void swap(TypeParamType& other) noexcept {
    using std::swap;
    swap(data_, other.data_);
  }

 private:
  friend class TypeParamTypeView;
  friend struct NativeTypeTraits<TypeParamType>;

  Shared<const common_internal::TypeParamTypeData> data_;
};

inline void swap(TypeParamType& lhs, TypeParamType& rhs) noexcept {
  lhs.swap(rhs);
}

inline bool operator==(const TypeParamType& lhs, const TypeParamType& rhs) {
  return lhs.name() == rhs.name();
}

inline bool operator!=(const TypeParamType& lhs, const TypeParamType& rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, const TypeParamType& type) {
  return H::combine(std::move(state), type.name());
}

inline std::ostream& operator<<(std::ostream& out, const TypeParamType& type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<TypeParamType> final {
  static bool SkipDestructor(const TypeParamType& type) {
    return NativeType::SkipDestructor(type.data_);
  }
};

class TypeParamTypeView final {
 public:
  using alternative_type = TypeParamType;

  static constexpr TypeKind kKind = TypeParamType::kKind;

  // NOLINTNEXTLINE(google-explicit-constructor)
  TypeParamTypeView(
      const TypeParamType& type ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept;

  // NOLINTNEXTLINE(google-explicit-constructor)
  TypeParamTypeView& operator=(
      const TypeParamType& type ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    data_ = type.data_;
    return *this;
  }

  TypeParamTypeView& operator=(TypeParamType&&) = delete;

  TypeParamTypeView() = delete;
  TypeParamTypeView(const TypeParamTypeView&) = default;
  TypeParamTypeView(TypeParamTypeView&&) = default;
  TypeParamTypeView& operator=(const TypeParamTypeView&) = default;
  TypeParamTypeView& operator=(TypeParamTypeView&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  absl::string_view name() const;

  std::string DebugString() const { return std::string(name()); }

  void swap(TypeParamTypeView& other) noexcept {
    using std::swap;
    swap(data_, other.data_);
  }

 private:
  friend class TypeParamType;

  SharedView<const common_internal::TypeParamTypeData> data_;
};

inline void swap(TypeParamTypeView& lhs, TypeParamTypeView& rhs) noexcept {
  lhs.swap(rhs);
}

inline bool operator==(TypeParamTypeView lhs, TypeParamTypeView rhs) {
  return lhs.name() == rhs.name();
}

inline bool operator!=(TypeParamTypeView lhs, TypeParamTypeView rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, TypeParamTypeView type) {
  return H::combine(std::move(state), type.name());
}

inline std::ostream& operator<<(std::ostream& out, TypeParamTypeView type) {
  return out << type.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_TYPE_PARAM_TYPE_H_
