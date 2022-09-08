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

// IWYU pragma: private, include "base/type.h"

#ifndef THIRD_PARTY_CEL_CPP_BASE_INTERNAL_TYPE_H_
#define THIRD_PARTY_CEL_CPP_BASE_INTERNAL_TYPE_H_

#include <cstdint>

#include "base/handle.h"
#include "base/internal/data.h"
#include "base/kind.h"
#include "internal/rtti.h"

namespace cel {

class EnumType;
class StructType;

namespace base_internal {

class PersistentTypeHandle;

class ListTypeImpl;
class MapTypeImpl;
class LegacyStructType;
class AbstractStructType;
class LegacyStructValue;
class AbstractStructValue;

template <Kind K>
class SimpleType;
template <typename T, typename U>
class SimpleValue;

internal::TypeInfo GetEnumTypeTypeId(const EnumType& enum_type);

internal::TypeInfo GetStructTypeTypeId(const StructType& struct_type);

inline constexpr size_t kTypeInlineSize = sizeof(void*);
inline constexpr size_t kTypeInlineAlign = alignof(void*);

struct AnyType final : public AnyData<kTypeInlineSize, kTypeInlineAlign> {};

}  // namespace base_internal

}  // namespace cel

#define CEL_INTERNAL_TYPE_DECL(name)      \
  extern template class Persistent<name>; \
  extern template class Persistent<const name>

#define CEL_INTERNAL_TYPE_IMPL(name) \
  template class Persistent<name>;   \
  template class Persistent<const name>

#define CEL_INTERNAL_DECLARE_TYPE(base, derived)           \
 public:                                                   \
  static bool Is(const ::cel::Type& type);                 \
                                                           \
 private:                                                  \
  friend class ::cel::base_internal::PersistentTypeHandle; \
                                                           \
  ::cel::internal::TypeInfo TypeId() const override;

#define CEL_INTERNAL_IMPLEMENT_TYPE(base, derived)                            \
  static_assert(::std::is_base_of_v<::cel::base##Type, derived>,              \
                #derived " must inherit from cel::" #base "Type");            \
  static_assert(!::std::is_abstract_v<derived>, "this must not be abstract"); \
                                                                              \
  bool derived::Is(const ::cel::Type& type) {                                 \
    return type.kind() == ::cel::Kind::k##base &&                             \
           ::cel::base_internal::Get##base##TypeTypeId(                       \
               static_cast<const ::cel::base##Type&>(type)) ==                \
               ::cel::internal::TypeId<derived>();                            \
  }                                                                           \
                                                                              \
  ::cel::internal::TypeInfo derived::TypeId() const {                         \
    return ::cel::internal::TypeId<derived>();                                \
  }

#endif  // THIRD_PARTY_CEL_CPP_BASE_INTERNAL_TYPE_H_
