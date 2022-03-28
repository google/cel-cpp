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
using TransientTypeHandle = TypeHandle<HandleType::kTransient>;
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

#endif  // THIRD_PARTY_CEL_CPP_BASE_INTERNAL_TYPE_PRE_H_
