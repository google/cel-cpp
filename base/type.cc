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

#include "base/type.h"

#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "base/handle.h"
#include "base/type_manager.h"
#include "internal/casts.h"
#include "internal/no_destructor.h"

namespace cel {

#define CEL_INTERNAL_TYPE_IMPL(name)    \
  template class Transient<name>;       \
  template class Transient<const name>; \
  template class Persistent<name>;      \
  template class Persistent<const name>
CEL_INTERNAL_TYPE_IMPL(Type);
CEL_INTERNAL_TYPE_IMPL(NullType);
CEL_INTERNAL_TYPE_IMPL(ErrorType);
CEL_INTERNAL_TYPE_IMPL(DynType);
CEL_INTERNAL_TYPE_IMPL(AnyType);
CEL_INTERNAL_TYPE_IMPL(BoolType);
CEL_INTERNAL_TYPE_IMPL(IntType);
CEL_INTERNAL_TYPE_IMPL(UintType);
CEL_INTERNAL_TYPE_IMPL(DoubleType);
CEL_INTERNAL_TYPE_IMPL(BytesType);
CEL_INTERNAL_TYPE_IMPL(StringType);
CEL_INTERNAL_TYPE_IMPL(DurationType);
CEL_INTERNAL_TYPE_IMPL(TimestampType);
CEL_INTERNAL_TYPE_IMPL(EnumType);
CEL_INTERNAL_TYPE_IMPL(ListType);
CEL_INTERNAL_TYPE_IMPL(MapType);
#undef CEL_INTERNAL_TYPE_IMPL

absl::Span<const Transient<const Type>> Type::parameters() const { return {}; }

std::string Type::DebugString() const { return std::string(name()); }

std::pair<size_t, size_t> Type::SizeAndAlignment() const {
  // Currently no implementation of Type is reference counted. However once we
  // introduce Struct it likely will be. Using 0 here will trigger runtime
  // asserts in case of undefined behavior. Struct should force this to be pure.
  return std::pair<size_t, size_t>(0, 0);
}

bool Type::Equals(const Type& other) const { return kind() == other.kind(); }

void Type::HashValue(absl::HashState state) const {
  absl::HashState::combine(std::move(state), kind(), name());
}

const NullType& NullType::Get() {
  static const internal::NoDestructor<NullType> instance;
  return *instance;
}

const ErrorType& ErrorType::Get() {
  static const internal::NoDestructor<ErrorType> instance;
  return *instance;
}

const DynType& DynType::Get() {
  static const internal::NoDestructor<DynType> instance;
  return *instance;
}

const AnyType& AnyType::Get() {
  static const internal::NoDestructor<AnyType> instance;
  return *instance;
}

const BoolType& BoolType::Get() {
  static const internal::NoDestructor<BoolType> instance;
  return *instance;
}

const IntType& IntType::Get() {
  static const internal::NoDestructor<IntType> instance;
  return *instance;
}

const UintType& UintType::Get() {
  static const internal::NoDestructor<UintType> instance;
  return *instance;
}

const DoubleType& DoubleType::Get() {
  static const internal::NoDestructor<DoubleType> instance;
  return *instance;
}

const StringType& StringType::Get() {
  static const internal::NoDestructor<StringType> instance;
  return *instance;
}

const BytesType& BytesType::Get() {
  static const internal::NoDestructor<BytesType> instance;
  return *instance;
}

const DurationType& DurationType::Get() {
  static const internal::NoDestructor<DurationType> instance;
  return *instance;
}

const TimestampType& TimestampType::Get() {
  static const internal::NoDestructor<TimestampType> instance;
  return *instance;
}

struct EnumType::FindConstantVisitor final {
  const EnumType& enum_type;

  absl::StatusOr<Constant> operator()(absl::string_view name) const {
    return enum_type.FindConstantByName(name);
  }

  absl::StatusOr<Constant> operator()(int64_t number) const {
    return enum_type.FindConstantByNumber(number);
  }
};

absl::StatusOr<EnumType::Constant> EnumType::FindConstant(ConstantId id) const {
  return absl::visit(FindConstantVisitor{*this}, id.data_);
}

struct StructType::FindFieldVisitor final {
  const StructType& struct_type;
  TypeManager& type_manager;

  absl::StatusOr<Field> operator()(absl::string_view name) const {
    return struct_type.FindFieldByName(type_manager, name);
  }

  absl::StatusOr<Field> operator()(int64_t number) const {
    return struct_type.FindFieldByNumber(type_manager, number);
  }
};

absl::StatusOr<StructType::Field> StructType::FindField(
    TypeManager& type_manager, FieldId id) const {
  return absl::visit(FindFieldVisitor{*this, type_manager}, id.data_);
}

std::string ListType::DebugString() const {
  return absl::StrCat(name(), "(", element()->DebugString(), ")");
}

bool ListType::Equals(const Type& other) const {
  if (kind() != other.kind()) {
    return false;
  }
  return element() == internal::down_cast<const ListType&>(other).element();
}

void ListType::HashValue(absl::HashState state) const {
  // We specifically hash the element first and then call the parent method to
  // avoid hash suffix/prefix collisions.
  Type::HashValue(absl::HashState::combine(std::move(state), element()));
}

std::string MapType::DebugString() const {
  return absl::StrCat(name(), "(", key()->DebugString(), ", ",
                      value()->DebugString(), ")");
}

bool MapType::Equals(const Type& other) const {
  if (kind() != other.kind()) {
    return false;
  }
  return key() == internal::down_cast<const MapType&>(other).key() &&
         value() == internal::down_cast<const MapType&>(other).value();
}

void MapType::HashValue(absl::HashState state) const {
  // We specifically hash the element first and then call the parent method to
  // avoid hash suffix/prefix collisions.
  Type::HashValue(absl::HashState::combine(std::move(state), key(), value()));
}

}  // namespace cel
