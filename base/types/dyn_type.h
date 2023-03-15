// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_BASE_TYPES_DYN_TYPE_H_
#define THIRD_PARTY_CEL_CPP_BASE_TYPES_DYN_TYPE_H_

#include "base/kind.h"
#include "base/type.h"

namespace cel {

class DynValue;

class DynType final : public base_internal::SimpleType<Kind::kDyn> {
 private:
  using Base = base_internal::SimpleType<Kind::kDyn>;

 public:
  using Base::kKind;

  using Base::kName;

  using Base::Is;

  static const DynType& Cast(const Type& type) {
    ABSL_ASSERT(Is(type));
    return static_cast<const DynType&>(type);
  }

  using Base::kind;

  using Base::name;

  using Base::DebugString;

 private:
  friend class base_internal::LegacyListType;
  friend class base_internal::LegacyMapType;

  CEL_INTERNAL_SIMPLE_TYPE_MEMBERS(DynType, DynValue);
};

CEL_INTERNAL_SIMPLE_TYPE_STANDALONES(DynType);

namespace base_internal {

template <>
struct TypeTraits<DynType> {
  using value_type = DynValue;
};

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPES_DYN_TYPE_H_
