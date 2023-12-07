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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_STRUCT_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_STRUCT_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type_kind.h"

namespace cel {

class StructType;
class StructTypeView;

namespace common_internal {
struct StructTypeData;
}  // namespace common_internal

class StructType final {
 public:
  using view_alternative_type = StructTypeView;

  static constexpr TypeKind kKind = TypeKind::kStruct;

  explicit StructType(StructTypeView other);

  StructType(MemoryManagerRef memory_manager, absl::string_view name);

  StructType() = delete;
  StructType(const StructType&) = default;
  StructType(StructType&&) = default;
  StructType& operator=(const StructType&) = default;
  StructType& operator=(StructType&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  absl::string_view name() const;

  std::string DebugString() const { return std::string(name()); }

  void swap(StructType& other) noexcept {
    using std::swap;
    swap(data_, other.data_);
  }

 private:
  friend class StructTypeView;
  friend struct NativeTypeTraits<StructType>;

  Shared<const common_internal::StructTypeData> data_;
};

inline void swap(StructType& lhs, StructType& rhs) noexcept { lhs.swap(rhs); }

inline bool operator==(const StructType& lhs, const StructType& rhs) {
  return lhs.name() == rhs.name();
}

inline bool operator!=(const StructType& lhs, const StructType& rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, const StructType& type) {
  return H::combine(std::move(state), type.name());
}

inline std::ostream& operator<<(std::ostream& out, const StructType& type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<StructType> final {
  static bool SkipDestructor(const StructType& type) {
    return NativeType::SkipDestructor(type.data_);
  }
};

class StructTypeView final {
 public:
  using alternative_type = StructType;

  static constexpr TypeKind kKind = StructType::kKind;

  // NOLINTNEXTLINE(google-explicit-constructor)
  StructTypeView(const StructType& type ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept;

  // NOLINTNEXTLINE(google-explicit-constructor)
  StructTypeView& operator=(
      const StructType& type ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    data_ = type.data_;
    return *this;
  }

  StructTypeView& operator=(StructType&&) = delete;

  StructTypeView() = delete;
  StructTypeView(const StructTypeView&) = default;
  StructTypeView(StructTypeView&&) = default;
  StructTypeView& operator=(const StructTypeView&) = default;
  StructTypeView& operator=(StructTypeView&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  absl::string_view name() const;

  std::string DebugString() const { return std::string(name()); }

  void swap(StructTypeView& other) noexcept {
    using std::swap;
    swap(data_, other.data_);
  }

 private:
  friend class StructType;

  SharedView<const common_internal::StructTypeData> data_;
};

inline void swap(StructTypeView& lhs, StructTypeView& rhs) noexcept {
  lhs.swap(rhs);
}

inline bool operator==(StructTypeView lhs, StructTypeView rhs) {
  return lhs.name() == rhs.name();
}

inline bool operator!=(StructTypeView lhs, StructTypeView rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, StructTypeView type) {
  return H::combine(std::move(state), type.name());
}

inline std::ostream& operator<<(std::ostream& out, StructTypeView type) {
  return out << type.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_STRUCT_TYPE_H_
