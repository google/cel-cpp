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

#include <algorithm>
#include <string>
#include <utility>

#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/internal/data.h"
#include "base/types/any_type.h"
#include "base/types/bool_type.h"
#include "base/types/bytes_type.h"
#include "base/types/double_type.h"
#include "base/types/duration_type.h"
#include "base/types/dyn_type.h"
#include "base/types/error_type.h"
#include "base/types/int_type.h"
#include "base/types/list_type.h"
#include "base/types/map_type.h"
#include "base/types/null_type.h"
#include "base/types/opaque_type.h"
#include "base/types/string_type.h"
#include "base/types/struct_type.h"
#include "base/types/timestamp_type.h"
#include "base/types/type_type.h"
#include "base/types/uint_type.h"
#include "base/types/unknown_type.h"
#include "base/types/wrapper_type.h"
#include "base/value.h"
#include "base/value_factory.h"

namespace cel {

CEL_INTERNAL_TYPE_IMPL(Type);

absl::string_view Type::name() const {
  switch (kind()) {
    case TypeKind::kNullType:
      return static_cast<const NullType*>(this)->name();
    case TypeKind::kError:
      return static_cast<const ErrorType*>(this)->name();
    case TypeKind::kDyn:
      return static_cast<const DynType*>(this)->name();
    case TypeKind::kAny:
      return static_cast<const AnyType*>(this)->name();
    case TypeKind::kType:
      return static_cast<const TypeType*>(this)->name();
    case TypeKind::kBool:
      return static_cast<const BoolType*>(this)->name();
    case TypeKind::kInt:
      return static_cast<const IntType*>(this)->name();
    case TypeKind::kUint:
      return static_cast<const UintType*>(this)->name();
    case TypeKind::kDouble:
      return static_cast<const DoubleType*>(this)->name();
    case TypeKind::kString:
      return static_cast<const StringType*>(this)->name();
    case TypeKind::kBytes:
      return static_cast<const BytesType*>(this)->name();
    case TypeKind::kDuration:
      return static_cast<const DurationType*>(this)->name();
    case TypeKind::kTimestamp:
      return static_cast<const TimestampType*>(this)->name();
    case TypeKind::kList:
      return static_cast<const ListType*>(this)->name();
    case TypeKind::kMap:
      return static_cast<const MapType*>(this)->name();
    case TypeKind::kStruct:
      return static_cast<const StructType*>(this)->name();
    case TypeKind::kUnknown:
      return static_cast<const UnknownType*>(this)->name();
    case TypeKind::kWrapper:
      return static_cast<const WrapperType*>(this)->name();
    case TypeKind::kOpaque:
      return static_cast<const OpaqueType*>(this)->name();
    default:
      return "*unreachable*";
  }
}

absl::Span<const absl::string_view> Type::aliases() const {
  switch (kind()) {
    case TypeKind::kDyn:
      return static_cast<const DynType*>(this)->aliases();
    case TypeKind::kList:
      return static_cast<const ListType*>(this)->aliases();
    case TypeKind::kMap:
      return static_cast<const MapType*>(this)->aliases();
    case TypeKind::kWrapper:
      return static_cast<const WrapperType*>(this)->aliases();
    default:
      // Everything else does not support aliases.
      return absl::Span<const absl::string_view>();
  }
}

std::string Type::DebugString() const {
  switch (kind()) {
    case TypeKind::kNullType:
      return static_cast<const NullType*>(this)->DebugString();
    case TypeKind::kError:
      return static_cast<const ErrorType*>(this)->DebugString();
    case TypeKind::kDyn:
      return static_cast<const DynType*>(this)->DebugString();
    case TypeKind::kAny:
      return static_cast<const AnyType*>(this)->DebugString();
    case TypeKind::kType:
      return static_cast<const TypeType*>(this)->DebugString();
    case TypeKind::kBool:
      return static_cast<const BoolType*>(this)->DebugString();
    case TypeKind::kInt:
      return static_cast<const IntType*>(this)->DebugString();
    case TypeKind::kUint:
      return static_cast<const UintType*>(this)->DebugString();
    case TypeKind::kDouble:
      return static_cast<const DoubleType*>(this)->DebugString();
    case TypeKind::kString:
      return static_cast<const StringType*>(this)->DebugString();
    case TypeKind::kBytes:
      return static_cast<const BytesType*>(this)->DebugString();
    case TypeKind::kDuration:
      return static_cast<const DurationType*>(this)->DebugString();
    case TypeKind::kTimestamp:
      return static_cast<const TimestampType*>(this)->DebugString();
    case TypeKind::kList:
      return static_cast<const ListType*>(this)->DebugString();
    case TypeKind::kMap:
      return static_cast<const MapType*>(this)->DebugString();
    case TypeKind::kStruct:
      return static_cast<const StructType*>(this)->DebugString();
    case TypeKind::kUnknown:
      return static_cast<const UnknownType*>(this)->DebugString();
    case TypeKind::kWrapper:
      return static_cast<const WrapperType*>(this)->DebugString();
    case TypeKind::kOpaque:
      return static_cast<const OpaqueType*>(this)->DebugString();
    default:
      return "*unreachable*";
  }
}

absl::StatusOr<Handle<Value>> Type::NewValueFromAny(
    ValueFactory& value_factory, const absl::Cord& value) const {
  switch (kind()) {
    case TypeKind::kNullType:
      return static_cast<const NullType*>(this)->NewValueFromAny(value_factory,
                                                                 value);
    case TypeKind::kError:
      return static_cast<const ErrorType*>(this)->NewValueFromAny(value_factory,
                                                                  value);
    case TypeKind::kDyn:
      return static_cast<const DynType*>(this)->NewValueFromAny(value_factory,
                                                                value);
    case TypeKind::kAny:
      return static_cast<const AnyType*>(this)->NewValueFromAny(value_factory,
                                                                value);
    case TypeKind::kType:
      return static_cast<const TypeType*>(this)->NewValueFromAny(value_factory,
                                                                 value);
    case TypeKind::kBool:
      return static_cast<const BoolType*>(this)->NewValueFromAny(value_factory,
                                                                 value);
    case TypeKind::kInt:
      return static_cast<const IntType*>(this)->NewValueFromAny(value_factory,
                                                                value);
    case TypeKind::kUint:
      return static_cast<const UintType*>(this)->NewValueFromAny(value_factory,
                                                                 value);
    case TypeKind::kDouble:
      return static_cast<const DoubleType*>(this)->NewValueFromAny(
          value_factory, value);
    case TypeKind::kString:
      return static_cast<const StringType*>(this)->NewValueFromAny(
          value_factory, value);
    case TypeKind::kBytes:
      return static_cast<const BytesType*>(this)->NewValueFromAny(value_factory,
                                                                  value);
    case TypeKind::kDuration:
      return static_cast<const DurationType*>(this)->NewValueFromAny(
          value_factory, value);
    case TypeKind::kTimestamp:
      return static_cast<const TimestampType*>(this)->NewValueFromAny(
          value_factory, value);
    case TypeKind::kList:
      return static_cast<const ListType*>(this)->NewValueFromAny(value_factory,
                                                                 value);
    case TypeKind::kMap:
      return static_cast<const MapType*>(this)->NewValueFromAny(value_factory,
                                                                value);
    case TypeKind::kStruct:
      return static_cast<const StructType*>(this)->NewValueFromAny(
          value_factory, value);
    case TypeKind::kUnknown:
      return static_cast<const UnknownType*>(this)->NewValueFromAny(
          value_factory, value);
    case TypeKind::kWrapper:
      return static_cast<const WrapperType*>(this)->NewValueFromAny(
          value_factory, value);
    case TypeKind::kOpaque:
      return static_cast<const OpaqueType*>(this)->NewValueFromAny(
          value_factory, value);
    default:
      return absl::InternalError(
          absl::StrCat("unexpected type kind: ", TypeKindToString(kind())));
  }
}

bool Type::Equals(const Type& lhs, const Type& rhs, TypeKind kind) {
  if (&lhs == &rhs) {
    return true;
  }
  switch (kind) {
    case TypeKind::kNullType:
      return true;
    case TypeKind::kError:
      return true;
    case TypeKind::kDyn:
      return true;
    case TypeKind::kAny:
      return true;
    case TypeKind::kType:
      return true;
    case TypeKind::kBool:
      return true;
    case TypeKind::kInt:
      return true;
    case TypeKind::kUint:
      return true;
    case TypeKind::kDouble:
      return true;
    case TypeKind::kString:
      return true;
    case TypeKind::kBytes:
      return true;
    case TypeKind::kDuration:
      return true;
    case TypeKind::kTimestamp:
      return true;
    case TypeKind::kList:
      return static_cast<const ListType&>(lhs).element() ==
             static_cast<const ListType&>(rhs).element();
    case TypeKind::kMap:
      return static_cast<const MapType&>(lhs).key() ==
                 static_cast<const MapType&>(rhs).key() &&
             static_cast<const MapType&>(lhs).value() ==
                 static_cast<const MapType&>(rhs).value();
    case TypeKind::kStruct:
      return static_cast<const StructType&>(lhs).name() ==
             static_cast<const StructType&>(rhs).name();
    case TypeKind::kUnknown:
      return true;
    case TypeKind::kWrapper:
      return static_cast<const WrapperType&>(lhs).wrapped() ==
             static_cast<const WrapperType&>(rhs).wrapped();
    case TypeKind::kOpaque: {
      if (static_cast<const OpaqueType&>(lhs).name() !=
          static_cast<const OpaqueType&>(rhs).name()) {
        return false;
      }
      const auto& lhs_parameters =
          static_cast<const OpaqueType&>(lhs).parameters();
      const auto& rhs_parameters =
          static_cast<const OpaqueType&>(rhs).parameters();
      return lhs_parameters.size() == rhs_parameters.size() &&
             std::equal(lhs_parameters.begin(), lhs_parameters.end(),
                        rhs_parameters.begin());
    }
    default:
      return false;
  }
}

void Type::HashValue(const Type& type, TypeKind kind, absl::HashState state) {
  switch (kind) {
    case TypeKind::kNullType:
      absl::HashState::combine(std::move(state), kind,
                               static_cast<const NullType&>(type).name());
      return;
    case TypeKind::kError:
      absl::HashState::combine(std::move(state), kind,
                               static_cast<const ErrorType&>(type).name());
      return;
    case TypeKind::kDyn:
      absl::HashState::combine(std::move(state), kind,
                               static_cast<const DynType&>(type).name());
      return;
    case TypeKind::kAny:
      absl::HashState::combine(std::move(state), kind,
                               static_cast<const AnyType&>(type).name());
      return;
    case TypeKind::kType:
      absl::HashState::combine(std::move(state), kind,
                               static_cast<const TypeType&>(type).name());
      return;
    case TypeKind::kBool:
      absl::HashState::combine(std::move(state), kind,
                               static_cast<const BoolType&>(type).name());
      return;
    case TypeKind::kInt:
      absl::HashState::combine(std::move(state), kind,
                               static_cast<const IntType&>(type).name());
      return;
    case TypeKind::kUint:
      absl::HashState::combine(std::move(state), kind,
                               static_cast<const UintType&>(type).name());
      return;
    case TypeKind::kDouble:
      absl::HashState::combine(std::move(state), kind,
                               static_cast<const DoubleType&>(type).name());
      return;
    case TypeKind::kString:
      absl::HashState::combine(std::move(state), kind,
                               static_cast<const StringType&>(type).name());
      return;
    case TypeKind::kBytes:
      absl::HashState::combine(std::move(state), kind,
                               static_cast<const BytesType&>(type).name());
      return;
      return;
    case TypeKind::kDuration:
      absl::HashState::combine(std::move(state), kind,
                               static_cast<const DurationType&>(type).name());
      return;
    case TypeKind::kTimestamp:
      absl::HashState::combine(std::move(state), kind,
                               static_cast<const TimestampType&>(type).name());
      return;
    case TypeKind::kList:
      absl::HashState::combine(std::move(state),
                               static_cast<const ListType&>(type).element(),
                               kind, static_cast<const ListType&>(type).name());
      return;
    case TypeKind::kMap:
      absl::HashState::combine(std::move(state),
                               static_cast<const MapType&>(type).key(),
                               static_cast<const MapType&>(type).value(), kind,
                               static_cast<const MapType&>(type).name());
      return;
    case TypeKind::kStruct:
      absl::HashState::combine(std::move(state), kind,
                               static_cast<const StructType&>(type).name());
      return;
    case TypeKind::kUnknown:
      absl::HashState::combine(std::move(state), kind,
                               static_cast<const UnknownType&>(type).name());
      return;
    case TypeKind::kWrapper:
      absl::HashState::combine(
          std::move(state), static_cast<const WrapperType&>(type).wrapped(),
          kind, static_cast<const WrapperType&>(type).name());
      return;
    case TypeKind::kOpaque: {
      const auto& parameters =
          static_cast<const OpaqueType&>(type).parameters();
      for (const auto& parameter : parameters) {
        state = absl::HashState::combine(std::move(state), parameter);
      }
      absl::HashState::combine(std::move(state), kind,
                               static_cast<const OpaqueType&>(type).name());
      return;
    }
    default:
      return;
  }
}

bool Type::Equals(const Type& other) const { return Equals(*this, other); }

void Type::HashValue(absl::HashState state) const {
  HashValue(*this, std::move(state));
}

namespace base_internal {

bool TypeHandle::Equals(const TypeHandle& other) const {
  const auto* self = static_cast<const Type*>(data_.get());
  const auto* that = static_cast<const Type*>(other.data_.get());
  if (self == that) {
    return true;
  }
  if (self == nullptr || that == nullptr) {
    return false;
  }
  TypeKind kind = self->kind();
  return kind == that->kind() && Type::Equals(*self, *that, kind);
}

void TypeHandle::HashValue(absl::HashState state) const {
  if (const auto* pointer = static_cast<const Type*>(data_.get());
      ABSL_PREDICT_TRUE(pointer != nullptr)) {
    Type::HashValue(*pointer, pointer->kind(), std::move(state));
  }
}

void TypeHandle::CopyFrom(const TypeHandle& other) {
  // data_ is currently uninitialized.
  auto locality = other.data_.locality();
  if (locality == DataLocality::kStoredInline) {
    if (ABSL_PREDICT_FALSE(!other.data_.IsTrivial())) {
      // Type currently has only trivially copyable inline
      // representations.
      ABSL_UNREACHABLE();
    } else {
      // We can simply just copy the bytes.
      data_.CopyFrom(other.data_);
    }
  } else {
    data_.set_pointer(other.data_.pointer());
    if (locality == DataLocality::kReferenceCounted) {
      Ref();
    }
  }
}

void TypeHandle::MoveFrom(TypeHandle& other) {
  // data_ is currently uninitialized.
  if (other.data_.IsStoredInline()) {
    if (ABSL_PREDICT_FALSE(!other.data_.IsTrivial())) {
      // Type currently has only trivially copyable inline
      // representations.
      ABSL_UNREACHABLE();
    } else {
      // We can simply just copy the bytes.
      data_.CopyFrom(other.data_);
    }
  } else {
    data_.set_pointer(other.data_.pointer());
  }
  other.data_.Clear();
}

void TypeHandle::CopyAssign(const TypeHandle& other) {
  // data_ is initialized.
  Destruct();
  CopyFrom(other);
}

void TypeHandle::MoveAssign(TypeHandle& other) {
  // data_ is initialized.
  Destruct();
  MoveFrom(other);
}

void TypeHandle::Destruct() {
  switch (data_.locality()) {
    case DataLocality::kNull:
      return;
    case DataLocality::kStoredInline:
      if (ABSL_PREDICT_FALSE(!data_.IsTrivial())) {
        // Type currently has only trivially destructible inline
        // representations.
        ABSL_UNREACHABLE();
      }
      return;
    case DataLocality::kReferenceCounted:
      Unref();
      return;
    case DataLocality::kArenaAllocated:
      return;
  }
}

void TypeHandle::Delete() const {
  switch (KindToTypeKind(data_.kind_heap())) {
    case TypeKind::kList:
      delete static_cast<ModernListType*>(
          static_cast<ListType*>(static_cast<Type*>(data_.get_heap())));
      return;
    case TypeKind::kMap:
      delete static_cast<ModernMapType*>(
          static_cast<MapType*>(static_cast<Type*>(data_.get_heap())));
      return;
    case TypeKind::kStruct:
      delete static_cast<AbstractStructType*>(
          static_cast<Type*>(data_.get_heap()));
      return;
    case TypeKind::kOpaque:
      delete static_cast<OpaqueType*>(static_cast<Type*>(data_.get_heap()));
      return;
    default:
      ABSL_UNREACHABLE();
  }
}

absl::Status TypeConversionError(const Type& from, const Type& to) {
  return absl::InvalidArgumentError(absl::StrCat("type conversion error from '",
                                                 from.DebugString(), "' to '",
                                                 to.DebugString(), "'"));
}

absl::Status DuplicateKeyError() {
  return absl::AlreadyExistsError("duplicate key error");
}

}  // namespace base_internal

}  // namespace cel
