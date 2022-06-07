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

#ifndef THIRD_PARTY_CEL_CPP_BASE_INTERNAL_TYPE_PRE_H_
#define THIRD_PARTY_CEL_CPP_BASE_INTERNAL_TYPE_PRE_H_

#include <cstdint>

#include "base/handle.h"
#include "internal/rtti.h"

namespace cel {

class EnumType;
class StructType;

namespace base_internal {

class TypeHandleBase;
template <HandleType H>
class TypeHandle;

// Convenient aliases.
using PersistentTypeHandle = TypeHandle<HandleType::kPersistent>;

// As all objects should be aligned to at least 4 bytes, we can use the lower
// two bits for our own purposes.
inline constexpr uintptr_t kTypeHandleUnmanaged = 1 << 0;
inline constexpr uintptr_t kTypeHandleReserved = 1 << 1;
inline constexpr uintptr_t kTypeHandleBits =
    kTypeHandleUnmanaged | kTypeHandleReserved;
inline constexpr uintptr_t kTypeHandleMask = ~kTypeHandleBits;

class ListTypeImpl;
class MapTypeImpl;

internal::TypeInfo GetEnumTypeTypeId(const EnumType& enum_type);

internal::TypeInfo GetStructTypeTypeId(const StructType& struct_type);

}  // namespace base_internal

}  // namespace cel

#define CEL_INTERNAL_DECLARE_TYPE(base, derived)                               \
 private:                                                                      \
  friend class ::cel::base_internal::TypeHandleBase;                           \
                                                                               \
  static bool Is(const ::cel::Type& type);                                     \
                                                                               \
  ::std::pair<::std::size_t, ::std::size_t> SizeAndAlignment() const override; \
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
               ::cel::internal::down_cast<const ::cel::base##Type&>(type)) == \
               ::cel::internal::TypeId<derived>();                            \
  }                                                                           \
                                                                              \
  ::std::pair<::std::size_t, ::std::size_t> derived::SizeAndAlignment()       \
      const {                                                                 \
    static_assert(                                                            \
        ::std::is_same_v<derived,                                             \
                         ::std::remove_const_t<                               \
                             ::std::remove_reference_t<decltype(*this)>>>,    \
        "this must be the same as " #derived);                                \
    return ::std::pair<::std::size_t, ::std::size_t>(sizeof(derived),         \
                                                     alignof(derived));       \
  }                                                                           \
                                                                              \
  ::cel::internal::TypeInfo derived::TypeId() const {                         \
    return ::cel::internal::TypeId<derived>();                                \
  }

#endif  // THIRD_PARTY_CEL_CPP_BASE_INTERNAL_TYPE_PRE_H_
