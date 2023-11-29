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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_OPAQUE_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_OPAQUE_TYPE_H_

#include <ostream>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/sized_input_view.h"
#include "common/type_kind.h"

namespace cel {

class Type;
class TypeView;
class OpaqueType;
class OpaqueTypeView;

namespace common_internal {
struct OpaqueTypeData;
}  // namespace common_internal

class OpaqueType final {
 public:
  using view_alternative_type = OpaqueTypeView;

  static constexpr TypeKind kKind = TypeKind::kOpaque;

  explicit OpaqueType(OpaqueTypeView other);

  OpaqueType(MemoryManagerRef memory_manager, absl::string_view name,
             SizedInputView<TypeView> parameters);

  OpaqueType() = delete;
  OpaqueType(const OpaqueType&) = default;
  OpaqueType(OpaqueType&&) = default;
  OpaqueType& operator=(const OpaqueType&) = default;
  OpaqueType& operator=(OpaqueType&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  absl::string_view name() const;

  std::string DebugString() const;

  void swap(OpaqueType& other) noexcept {
    using std::swap;
    swap(data_, other.data_);
  }

  absl::Span<const Type> parameters() const;

 private:
  friend class OpaqueTypeView;
  friend struct NativeTypeTraits<OpaqueType>;

  Shared<const common_internal::OpaqueTypeData> data_;
};

inline void swap(OpaqueType& lhs, OpaqueType& rhs) noexcept { lhs.swap(rhs); }

bool operator==(const OpaqueType& lhs, const OpaqueType& rhs);

inline bool operator!=(const OpaqueType& lhs, const OpaqueType& rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, const OpaqueType& type);

inline std::ostream& operator<<(std::ostream& out, const OpaqueType& type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<OpaqueType> final {
  static bool SkipDestructor(const OpaqueType& type) {
    return NativeType::SkipDestructor(type.data_);
  }
};

class OpaqueTypeView final {
 public:
  using alternative_type = OpaqueType;

  static constexpr TypeKind kKind = OpaqueType::kKind;

  // NOLINTNEXTLINE(google-explicit-constructor)
  OpaqueTypeView(const OpaqueType& type ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept;

  OpaqueTypeView() = delete;
  OpaqueTypeView(const OpaqueTypeView&) = default;
  OpaqueTypeView(OpaqueTypeView&&) = default;
  OpaqueTypeView& operator=(const OpaqueTypeView&) = default;
  OpaqueTypeView& operator=(OpaqueTypeView&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  absl::string_view name() const;

  std::string DebugString() const;

  void swap(OpaqueTypeView& other) noexcept {
    using std::swap;
    swap(data_, other.data_);
  }

  absl::Span<const Type> parameters() const;

 private:
  friend class OpaqueType;

  SharedView<const common_internal::OpaqueTypeData> data_;
};

inline void swap(OpaqueTypeView& lhs, OpaqueTypeView& rhs) noexcept {
  lhs.swap(rhs);
}

bool operator==(OpaqueTypeView lhs, OpaqueTypeView rhs);

inline bool operator!=(OpaqueTypeView lhs, OpaqueTypeView rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, OpaqueTypeView type);

inline std::ostream& operator<<(std::ostream& out, OpaqueTypeView type) {
  return out << type.DebugString();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_OPAQUE_TYPE_H_
