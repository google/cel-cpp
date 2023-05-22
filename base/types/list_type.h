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

#ifndef THIRD_PARTY_CEL_CPP_BASE_TYPES_LIST_TYPE_H_
#define THIRD_PARTY_CEL_CPP_BASE_TYPES_LIST_TYPE_H_

#include <cstddef>
#include <cstdint>
#include <string>

#include "absl/base/attributes.h"
#include "absl/log/absl_check.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "base/handle.h"
#include "base/internal/data.h"
#include "base/kind.h"
#include "base/memory.h"
#include "base/type.h"

namespace cel {

class MemoryManager;
class ListValue;
class ValueFactory;
class ListValueBuilderInterface;

// ListType represents a list type. A list is a sequential container where each
// element is the same type.
class ListType
    : public Type,
      public base_internal::EnableHandleFromThis<ListType, ListType> {
 public:
  static constexpr TypeKind kKind = TypeKind::kList;

  static bool Is(const Type& type) { return type.kind() == kKind; }

  TypeKind kind() const { return kKind; }

  absl::string_view name() const { return TypeKindToString(kind()); }

  std::string DebugString() const;

  // Returns the type of the elements in the list.
  const Handle<Type>& element() const;

  using Type::Is;

  static const ListType& Cast(const Type& type) {
    ABSL_DCHECK(Is(type)) << "cannot cast " << type.name() << " to list";
    return static_cast<const ListType&>(type);
  }

  absl::StatusOr<UniqueRef<ListValueBuilderInterface>> NewValueBuilder(
      ValueFactory& value_factory
          ABSL_ATTRIBUTE_LIFETIME_BOUND) const ABSL_ATTRIBUTE_LIFETIME_BOUND;

 private:
  friend class Type;
  friend class MemoryManager;
  friend class TypeFactory;
  friend class base_internal::TypeHandle;
  friend class base_internal::LegacyListType;
  friend class base_internal::ModernListType;

  // See Type::aliases().
  absl::Span<const absl::string_view> aliases() const;

  ListType() = default;
};

CEL_INTERNAL_TYPE_DECL(ListType);

namespace base_internal {

// LegacyListType is used by LegacyListValue for compatibility with the legacy
// API. It's element is always the dynamic type regardless of whether the the
// expression is checked or not.
class LegacyListType final : public ListType, public InlineData {
 public:
  // Returns the type of the elements in the list.
  const Handle<Type>& element() const;

 private:
  friend class MemoryManager;
  friend class TypeFactory;
  friend class cel::ListType;
  friend class base_internal::TypeHandle;
  template <size_t Size, size_t Align>
  friend struct AnyData;

  static constexpr uintptr_t kMetadata =
      base_internal::kStoredInline | base_internal::kTrivial |
      (static_cast<uintptr_t>(kKind) << base_internal::kKindShift);

  LegacyListType() : ListType(), InlineData(kMetadata) {}
};

class ModernListType final : public ListType, public HeapData {
 public:
  // Returns the type of the elements in the list.
  const Handle<Type>& element() const { return element_; }

 private:
  friend class cel::MemoryManager;
  friend class TypeFactory;
  friend class cel::ListType;
  friend class base_internal::TypeHandle;

  // Called by Arena-based memory managers to determine whether we actually need
  // our destructor called.
  CEL_INTERNAL_IS_DESTRUCTOR_SKIPPABLE() {
    return Metadata::IsDestructorSkippable(*element());
  }

  explicit ModernListType(Handle<Type> element);

  const Handle<Type> element_;
};

}  // namespace base_internal

namespace base_internal {

template <>
struct TypeTraits<ListType> {
  using value_type = ListValue;
};

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPES_LIST_TYPE_H_
