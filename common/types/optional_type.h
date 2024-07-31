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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_OPTIONAL_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_OPTIONAL_TYPE_H_

#include <memory>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/log/absl_check.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/types/opaque_type.h"

namespace cel {

class OptionalType;
class Type;

class OptionalType final : public OpaqueType {
 public:
  static constexpr absl::string_view kName = "optional_type";

  using OpaqueType::OpaqueType;

  // By default, this type is `optional(dyn)`. Unless you can help it, you
  // should choose a more specific optional type.
  OptionalType();

  OptionalType(MemoryManagerRef, absl::string_view,
               absl::Span<const Type>) = delete;

  OptionalType(MemoryManagerRef memory_manager, const Type& parameter);

  absl::string_view name() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    auto name = OpaqueType::name();
    ABSL_DCHECK_EQ(name, kName);
    return name;
  }

  absl::Span<const Type> parameters() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    auto parameters = OpaqueType::parameters();
    ABSL_DCHECK_EQ(static_cast<int>(parameters.size()), 1);
    return parameters;
  }

  const Type& parameter() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

 private:
  // Used by SubsumptionTraits to downcast OpaqueType rvalue references.
  explicit OptionalType(OpaqueType&& type) noexcept
      : OpaqueType(std::move(type)) {}

  friend struct SubsumptionTraits<OptionalType>;
};

bool operator==(const OptionalType& lhs, const OptionalType& rhs);

inline bool operator!=(const OptionalType& lhs, const OptionalType& rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, const OptionalType& type);

template <>
struct SubsumptionTraits<OptionalType> final {
  static bool IsA(const OpaqueType& type) {
    return type.name() == OptionalType::kName && type.parameters().size() == 1;
  }

  static const OptionalType& DownCast(const OpaqueType& type) {
    ABSL_DCHECK(IsA(type));
    return *reinterpret_cast<const OptionalType*>(std::addressof(type));
  }

  static OptionalType& DownCast(OpaqueType& type) {
    ABSL_DCHECK(IsA(type));
    return *reinterpret_cast<OptionalType*>(std::addressof(type));
  }

  static OptionalType DownCast(OpaqueType&& type) {
    ABSL_DCHECK(IsA(type));
    return OptionalType(std::move(type));
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_OPTIONAL_TYPE_H_
