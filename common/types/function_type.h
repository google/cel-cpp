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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_FUNCTION_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_FUNCTION_TYPE_H_

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
class FunctionType;

namespace common_internal {
struct FunctionTypeData;
}  // namespace common_internal

class FunctionType final {
 public:
  static constexpr TypeKind kKind = TypeKind::kFunction;

  FunctionType(MemoryManagerRef memory_manager, const Type& result,
               absl::Span<const Type> args);

  FunctionType() = delete;
  FunctionType(const FunctionType&) = default;
  FunctionType(FunctionType&&) = default;
  FunctionType& operator=(const FunctionType&) = default;
  FunctionType& operator=(FunctionType&&) = default;

  constexpr TypeKind kind() const { return kKind; }

  absl::string_view name() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return "function";
  }

  absl::Span<const Type> parameters() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  std::string DebugString() const;

  const Type& result() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  absl::Span<const Type> args() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  void swap(FunctionType& other) noexcept {
    using std::swap;
    swap(data_, other.data_);
  }

 private:
  friend struct NativeTypeTraits<FunctionType>;

  Shared<const common_internal::FunctionTypeData> data_;
};

inline void swap(FunctionType& lhs, FunctionType& rhs) noexcept {
  lhs.swap(rhs);
}

bool operator==(const FunctionType& lhs, const FunctionType& rhs);

inline bool operator!=(const FunctionType& lhs, const FunctionType& rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, const FunctionType& type);

inline std::ostream& operator<<(std::ostream& out, const FunctionType& type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<FunctionType> final {
  static bool SkipDestructor(const FunctionType& type) {
    return NativeType::SkipDestructor(type.data_);
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_FUNCTION_TYPE_H_
