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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_LIST_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_LIST_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type_kind.h"

namespace cel {

class Type;
class TypeView;
class ListType;
class ListTypeView;

namespace common_internal {
struct ListTypeData;
}  // namespace common_internal

class ListType final {
 public:
  using view_alternative_type = ListTypeView;

  static constexpr TypeKind kKind = TypeKind::kList;
  static constexpr absl::string_view kName = "list";

  explicit ListType(ListTypeView other);

  ListType(MemoryManagerRef memory_manager, Type element);

  // By default, this type is `list(dyn)`. Unless you can help it, you should
  // use a more specific list type.
  ListType();
  ListType(const ListType&) = default;
  ListType(ListType&&) = default;
  ListType& operator=(const ListType&) = default;
  ListType& operator=(ListType&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  std::string DebugString() const;

  absl::Span<const Type> parameters() const;

  void swap(ListType& other) noexcept {
    using std::swap;
    swap(data_, other.data_);
  }

  TypeView element() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

 private:
  friend class ListTypeView;
  friend struct NativeTypeTraits<ListType>;

  Shared<const common_internal::ListTypeData> data_;
};

inline void swap(ListType& lhs, ListType& rhs) noexcept { lhs.swap(rhs); }

bool operator==(const ListType& lhs, const ListType& rhs);

inline bool operator!=(const ListType& lhs, const ListType& rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, const ListType& type);

inline std::ostream& operator<<(std::ostream& out, const ListType& type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<ListType> final {
  static bool SkipDestructor(const ListType& type) {
    return NativeType::SkipDestructor(type.data_);
  }
};

class ListTypeView final {
 public:
  using alternative_type = ListType;

  static constexpr TypeKind kKind = ListType::kKind;
  static constexpr absl::string_view kName = ListType::kName;

  // NOLINTNEXTLINE(google-explicit-constructor)
  ListTypeView(const ListType& type ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept;

  // NOLINTNEXTLINE(google-explicit-constructor)
  ListTypeView& operator=(const ListType& type ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    data_ = type.data_;
    return *this;
  }

  ListTypeView& operator=(ListType&&) = delete;

  // By default, this type is `list(dyn)`. Unless you can help it, you should
  // use a more specific list type.
  ListTypeView();
  ListTypeView(const ListTypeView&) = default;
  ListTypeView(ListTypeView&&) = default;
  ListTypeView& operator=(const ListTypeView&) = default;
  ListTypeView& operator=(ListTypeView&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  constexpr absl::string_view name() const { return kName; }

  std::string DebugString() const;

  absl::Span<const Type> parameters() const;

  void swap(ListTypeView& other) noexcept {
    using std::swap;
    swap(data_, other.data_);
  }

  TypeView element() const;

 private:
  friend class ListType;

  SharedView<const common_internal::ListTypeData> data_;
};

inline void swap(ListTypeView& lhs, ListTypeView& rhs) noexcept {
  lhs.swap(rhs);
}

bool operator==(ListTypeView lhs, ListTypeView rhs);

inline bool operator!=(ListTypeView lhs, ListTypeView rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, ListTypeView type);

inline std::ostream& operator<<(std::ostream& out, ListTypeView type) {
  return out << type.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_LIST_TYPE_H_
