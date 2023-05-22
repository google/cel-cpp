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

#ifndef THIRD_PARTY_CEL_CPP_BASE_TYPES_OPAQUE_TYPE_H_
#define THIRD_PARTY_CEL_CPP_BASE_TYPES_OPAQUE_TYPE_H_

#include <string>

#include "absl/log/absl_check.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/kind.h"
#include "base/type.h"
#include "internal/rtti.h"

namespace cel {

class OpaqueType : public Type, public base_internal::HeapData {
 public:
  static constexpr TypeKind kKind = TypeKind::kOpaque;

  static bool Is(const Type& type) { return type.kind() == kKind; }

  using Type::Is;

  static const OpaqueType& Cast(const Type& type) {
    ABSL_DCHECK(Is(type)) << "cannot cast " << type.name() << " to opaque";
    return static_cast<const OpaqueType&>(type);
  }

  TypeKind kind() const { return kKind; }

  virtual absl::string_view name() const = 0;

  virtual std::string DebugString() const = 0;

  virtual absl::Span<const Handle<Type>> parameters() const = 0;

 protected:
  OpaqueType() : Type(), HeapData(kKind) {}

  static internal::TypeInfo TypeId(const OpaqueType& type) {
    return type.TypeId();
  }

 private:
  friend class Type;
  friend class MemoryManager;
  friend class TypeFactory;
  friend class base_internal::TypeHandle;

  // Called by CEL_IMPLEMENT_STRUCT_TYPE() and Is() to perform type checking.
  virtual internal::TypeInfo TypeId() const = 0;
};

extern template class Handle<OpaqueType>;

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPES_OPAQUE_TYPE_H_
