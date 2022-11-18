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

#include "base/types/struct_type.h"

#include <string>
#include <utility>

#include "absl/base/macros.h"
#include "absl/hash/hash.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "base/internal/message_wrapper.h"

namespace cel {

CEL_INTERNAL_TYPE_IMPL(StructType);

absl::string_view StructType::name() const {
  if (base_internal::Metadata::IsStoredInline(*this)) {
    return static_cast<const base_internal::LegacyStructType*>(this)->name();
  }
  return static_cast<const base_internal::AbstractStructType*>(this)->name();
}

std::string StructType::DebugString() const {
  if (base_internal::Metadata::IsStoredInline(*this)) {
    return static_cast<const base_internal::LegacyStructType*>(this)
        ->DebugString();
  }
  return static_cast<const base_internal::AbstractStructType*>(this)
      ->DebugString();
}

internal::TypeInfo StructType::TypeId() const {
  if (base_internal::Metadata::IsStoredInline(*this)) {
    return static_cast<const base_internal::LegacyStructType*>(this)->TypeId();
  }
  return static_cast<const base_internal::AbstractStructType*>(this)->TypeId();
}

absl::StatusOr<absl::optional<StructType::Field>> StructType::FindFieldByName(
    TypeManager& type_manager, absl::string_view name) const {
  if (base_internal::Metadata::IsStoredInline(*this)) {
    return static_cast<const base_internal::LegacyStructType*>(this)
        ->FindFieldByName(type_manager, name);
  }
  return static_cast<const base_internal::AbstractStructType*>(this)
      ->FindFieldByName(type_manager, name);
}

absl::StatusOr<absl::optional<StructType::Field>> StructType::FindFieldByNumber(
    TypeManager& type_manager, int64_t number) const {
  if (base_internal::Metadata::IsStoredInline(*this)) {
    return static_cast<const base_internal::LegacyStructType*>(this)
        ->FindFieldByNumber(type_manager, number);
  }
  return static_cast<const base_internal::AbstractStructType*>(this)
      ->FindFieldByNumber(type_manager, number);
}

struct StructType::FindFieldVisitor final {
  const StructType& struct_type;
  TypeManager& type_manager;

  absl::StatusOr<absl::optional<Field>> operator()(
      absl::string_view name) const {
    return struct_type.FindFieldByName(type_manager, name);
  }

  absl::StatusOr<absl::optional<Field>> operator()(int64_t number) const {
    return struct_type.FindFieldByNumber(type_manager, number);
  }
};

absl::StatusOr<absl::optional<StructType::Field>> StructType::FindField(
    TypeManager& type_manager, FieldId id) const {
  return absl::visit(FindFieldVisitor{*this, type_manager}, id.data_);
}

namespace base_internal {

absl::string_view LegacyStructType::name() const {
  return MessageTypeName(msg_);
}

// Always returns an error.
absl::StatusOr<absl::optional<StructType::Field>>
LegacyStructType::FindFieldByName(TypeManager& type_manager,
                                  absl::string_view name) const {
  return absl::UnimplementedError(
      "Legacy struct type does not support type introspection");
}

// Always returns an error.
absl::StatusOr<absl::optional<StructType::Field>>
LegacyStructType::FindFieldByNumber(TypeManager& type_manager,
                                    int64_t number) const {
  return absl::UnimplementedError(
      "Legacy struct type does not support type introspection");
}

AbstractStructType::AbstractStructType()
    : StructType(), base_internal::HeapData(kKind) {
  // Ensure `Type*` and `base_internal::HeapData*` are not thunked.
  ABSL_ASSERT(
      reinterpret_cast<uintptr_t>(static_cast<Type*>(this)) ==
      reinterpret_cast<uintptr_t>(static_cast<base_internal::HeapData*>(this)));
}

}  // namespace base_internal

}  // namespace cel
