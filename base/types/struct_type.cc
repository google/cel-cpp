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
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "base/value_factory.h"
#include "base/values/struct_value_builder.h"
#include "internal/overloaded.h"
#include "internal/status_macros.h"

namespace cel {

CEL_INTERNAL_TYPE_IMPL(StructType);

bool operator<(const StructType::FieldId& lhs, const StructType::FieldId& rhs) {
  return absl::visit(
      internal::Overloaded{
          [&rhs](absl::string_view lhs_name) {
            return absl::visit(
                internal::Overloaded{// (absl::string_view, absl::string_view)
                                     [lhs_name](absl::string_view rhs_name) {
                                       return lhs_name < rhs_name;
                                     },
                                     // (absl::string_view, int64_t)
                                     [](int64_t rhs_number) { return false; }},
                rhs.data_);
          },
          [&rhs](int64_t lhs_number) {
            return absl::visit(
                internal::Overloaded{
                    // (int64_t, absl::string_view)
                    [](absl::string_view rhs_name) { return true; },
                    // (int64_t, int64_t)
                    [lhs_number](int64_t rhs_number) {
                      return lhs_number < rhs_number;
                    },
                },
                rhs.data_);
          }},
      lhs.data_);
}

std::string StructType::FieldId::DebugString() const {
  return absl::visit(
      internal::Overloaded{
          [](absl::string_view name) { return std::string(name); },
          [](int64_t number) { return absl::StrCat(number); }},
      data_);
}

#define CEL_INTERNAL_STRUCT_TYPE_DISPATCH(method, ...)                       \
  base_internal::Metadata::IsStoredInline(*this)                             \
      ? static_cast<const base_internal::LegacyStructType&>(*this).method(   \
            __VA_ARGS__)                                                     \
      : static_cast<const base_internal::AbstractStructType&>(*this).method( \
            __VA_ARGS__)

absl::string_view StructType::name() const {
  return CEL_INTERNAL_STRUCT_TYPE_DISPATCH(name);
}

size_t StructType::field_count() const {
  return CEL_INTERNAL_STRUCT_TYPE_DISPATCH(field_count);
}

std::string StructType::DebugString() const {
  return CEL_INTERNAL_STRUCT_TYPE_DISPATCH(DebugString);
}

internal::TypeInfo StructType::TypeId() const {
  return CEL_INTERNAL_STRUCT_TYPE_DISPATCH(TypeId);
}

absl::StatusOr<absl::optional<StructType::Field>> StructType::FindFieldByName(
    TypeManager& type_manager, absl::string_view name) const {
  return CEL_INTERNAL_STRUCT_TYPE_DISPATCH(FindFieldByName, type_manager, name);
}

absl::StatusOr<absl::optional<StructType::Field>> StructType::FindFieldByNumber(
    TypeManager& type_manager, int64_t number) const {
  return CEL_INTERNAL_STRUCT_TYPE_DISPATCH(FindFieldByNumber, type_manager,
                                           number);
}

absl::StatusOr<UniqueRef<StructType::FieldIterator>>
StructType::NewFieldIterator(MemoryManager& memory_manager) const {
  return CEL_INTERNAL_STRUCT_TYPE_DISPATCH(NewFieldIterator, memory_manager);
}

absl::StatusOr<UniqueRef<StructValueBuilderInterface>>
StructType::NewValueBuilder(ValueFactory& value_factory) const {
  return CEL_INTERNAL_STRUCT_TYPE_DISPATCH(NewValueBuilder, value_factory);
}

absl::StatusOr<Handle<StructValue>> StructType::NewValueFromAny(
    ValueFactory& value_factory, const absl::Cord& value) const {
  return CEL_INTERNAL_STRUCT_TYPE_DISPATCH(NewValueFromAny, value_factory,
                                           value);
}

#undef CEL_INTERNAL_STRUCT_TYPE_DISPATCH

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

absl::StatusOr<StructType::FieldId> StructType::FieldIterator::NextId(
    TypeManager& type_manager) {
  CEL_ASSIGN_OR_RETURN(auto field, Next(type_manager));
  return field.id;
}

absl::StatusOr<absl::string_view> StructType::FieldIterator::NextName(
    TypeManager& type_manager) {
  CEL_ASSIGN_OR_RETURN(auto field, Next(type_manager));
  return field.name;
}

absl::StatusOr<int64_t> StructType::FieldIterator::NextNumber(
    TypeManager& type_manager) {
  CEL_ASSIGN_OR_RETURN(auto field, Next(type_manager));
  return field.number;
}

absl::StatusOr<Handle<Type>> StructType::FieldIterator::NextType(
    TypeManager& type_manager) {
  CEL_ASSIGN_OR_RETURN(auto field, Next(type_manager));
  return std::move(field.type);
}

namespace base_internal {

absl::string_view LegacyStructType::name() const {
  return MessageTypeName(msg_);
}

size_t LegacyStructType::field_count() const {
  return MessageTypeFieldCount(msg_);
}

absl::StatusOr<UniqueRef<StructType::FieldIterator>>
LegacyStructType::NewFieldIterator(MemoryManager& memory_manager) const {
  return absl::UnimplementedError(
      "StructType::NewFieldIterator is not supported by legacy struct types");
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

absl::StatusOr<UniqueRef<StructValueBuilderInterface>>
LegacyStructType::NewValueBuilder(ValueFactory& value_factory) const {
  return absl::UnimplementedError(
      "StructType::NewValueBuilder is unimplemented. Perhaps the value library "
      "is not linked into your binary or StructType::NewValueBuilder was not "
      "overridden?");
}

absl::StatusOr<Handle<StructValue>> LegacyStructType::NewValueFromAny(
    ValueFactory& value_factory, const absl::Cord& value) const {
  return absl::UnimplementedError(
      "LegacyStructType::NewValueFromAny is unimplemented. Perhaps the value "
      "library "
      "is not linked into your binary or StructType::NewValueFromAny was not "
      "overridden?");
}

AbstractStructType::AbstractStructType()
    : StructType(), base_internal::HeapData(kKind) {
  // Ensure `Type*` and `base_internal::HeapData*` are not thunked.
  ABSL_ASSERT(
      reinterpret_cast<uintptr_t>(static_cast<Type*>(this)) ==
      reinterpret_cast<uintptr_t>(static_cast<base_internal::HeapData*>(this)));
}

absl::StatusOr<UniqueRef<StructValueBuilderInterface>>
AbstractStructType::NewValueBuilder(ValueFactory& value_factory) const {
  return absl::UnimplementedError(
      "StructType::NewValueBuilder is unimplemented. Perhaps the value library "
      "is not linked into your binary or StructType::NewValueBuilder was not "
      "overridden?");
}

absl::StatusOr<Handle<StructValue>> AbstractStructType::NewValueFromAny(
    ValueFactory& value_factory, const absl::Cord& value) const {
  return absl::FailedPreconditionError(
      absl::StrCat("google.protobuf.Any cannot be deserialized as ", name()));
}

}  // namespace base_internal

}  // namespace cel
