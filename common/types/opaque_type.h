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
// IWYU pragma: friend "common/types/optional_type.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_OPAQUE_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_OPAQUE_TYPE_H_

#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type_kind.h"

namespace cel {

class Type;
class OpaqueType;
class OptionalType;

namespace common_internal {
struct OpaqueTypeData;
}  // namespace common_internal

class OpaqueType final {
 public:
  static constexpr TypeKind kKind = TypeKind::kOpaque;

  OpaqueType(MemoryManagerRef memory_manager, absl::string_view name,
             absl::Span<const Type> parameters);

  // NOLINTNEXTLINE(google-explicit-constructor)
  OpaqueType(OptionalType type);

  // NOLINTNEXTLINE(google-explicit-constructor)
  OpaqueType& operator=(OptionalType type);

  OpaqueType() = default;
  OpaqueType(const OpaqueType&) = default;
  OpaqueType(OpaqueType&&) = default;
  OpaqueType& operator=(const OpaqueType&) = default;
  OpaqueType& operator=(OpaqueType&&) = default;

  static TypeKind kind() { return kKind; }

  absl::string_view name() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  std::string DebugString() const;

  void swap(OpaqueType& other) noexcept {
    using std::swap;
    swap(data_, other.data_);
  }

  absl::Span<const Type> parameters() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  bool IsOptional() const;

  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalType, T>, bool> Is() const {
    return IsOptional();
  }

  absl::optional<OptionalType> AsOptional() const;

  template <typename T>
  std::enable_if_t<std::is_same_v<OptionalType, T>,
                   absl::optional<OptionalType>>
  As() const;

  explicit operator bool() const { return static_cast<bool>(data_); }

  explicit operator OptionalType() const;

 private:
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

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_OPAQUE_TYPE_H_
