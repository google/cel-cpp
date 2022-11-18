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

#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "base/handle.h"
#include "base/internal/data.h"
#include "base/types/any_type.h"
#include "base/types/bool_type.h"
#include "base/types/bytes_type.h"
#include "base/types/double_type.h"
#include "base/types/duration_type.h"
#include "base/types/dyn_type.h"
#include "base/types/enum_type.h"
#include "base/types/error_type.h"
#include "base/types/int_type.h"
#include "base/types/list_type.h"
#include "base/types/map_type.h"
#include "base/types/null_type.h"
#include "base/types/string_type.h"
#include "base/types/struct_type.h"
#include "base/types/timestamp_type.h"
#include "base/types/type_type.h"
#include "base/types/uint_type.h"
#include "base/types/unknown_type.h"
#include "internal/unreachable.h"

namespace cel {

CEL_INTERNAL_TYPE_IMPL(Type);

absl::string_view Type::name() const {
  switch (kind()) {
    case Kind::kNullType:
      return static_cast<const NullType*>(this)->name();
    case Kind::kError:
      return static_cast<const ErrorType*>(this)->name();
    case Kind::kDyn:
      return static_cast<const DynType*>(this)->name();
    case Kind::kAny:
      return static_cast<const AnyType*>(this)->name();
    case Kind::kType:
      return static_cast<const TypeType*>(this)->name();
    case Kind::kBool:
      return static_cast<const BoolType*>(this)->name();
    case Kind::kInt:
      return static_cast<const IntType*>(this)->name();
    case Kind::kUint:
      return static_cast<const UintType*>(this)->name();
    case Kind::kDouble:
      return static_cast<const DoubleType*>(this)->name();
    case Kind::kString:
      return static_cast<const StringType*>(this)->name();
    case Kind::kBytes:
      return static_cast<const BytesType*>(this)->name();
    case Kind::kEnum:
      return static_cast<const EnumType*>(this)->name();
    case Kind::kDuration:
      return static_cast<const DurationType*>(this)->name();
    case Kind::kTimestamp:
      return static_cast<const TimestampType*>(this)->name();
    case Kind::kList:
      return static_cast<const ListType*>(this)->name();
    case Kind::kMap:
      return static_cast<const MapType*>(this)->name();
    case Kind::kStruct:
      return static_cast<const StructType*>(this)->name();
    case Kind::kUnknown:
      return static_cast<const UnknownType*>(this)->name();
    default:
      return "*unreachable*";
  }
}

std::string Type::DebugString() const {
  switch (kind()) {
    case Kind::kNullType:
      return static_cast<const NullType*>(this)->DebugString();
    case Kind::kError:
      return static_cast<const ErrorType*>(this)->DebugString();
    case Kind::kDyn:
      return static_cast<const DynType*>(this)->DebugString();
    case Kind::kAny:
      return static_cast<const AnyType*>(this)->DebugString();
    case Kind::kType:
      return static_cast<const TypeType*>(this)->DebugString();
    case Kind::kBool:
      return static_cast<const BoolType*>(this)->DebugString();
    case Kind::kInt:
      return static_cast<const IntType*>(this)->DebugString();
    case Kind::kUint:
      return static_cast<const UintType*>(this)->DebugString();
    case Kind::kDouble:
      return static_cast<const DoubleType*>(this)->DebugString();
    case Kind::kString:
      return static_cast<const StringType*>(this)->DebugString();
    case Kind::kBytes:
      return static_cast<const BytesType*>(this)->DebugString();
    case Kind::kEnum:
      return static_cast<const EnumType*>(this)->DebugString();
    case Kind::kDuration:
      return static_cast<const DurationType*>(this)->DebugString();
    case Kind::kTimestamp:
      return static_cast<const TimestampType*>(this)->DebugString();
    case Kind::kList:
      return static_cast<const ListType*>(this)->DebugString();
    case Kind::kMap:
      return static_cast<const MapType*>(this)->DebugString();
    case Kind::kStruct:
      return static_cast<const StructType*>(this)->DebugString();
    case Kind::kUnknown:
      return static_cast<const UnknownType*>(this)->DebugString();
    default:
      return "*unreachable*";
  }
}

bool Type::Equals(const Type& other) const {
  if (this == &other) {
    return true;
  }
  switch (kind()) {
    case Kind::kNullType:
      return static_cast<const NullType*>(this)->Equals(other);
    case Kind::kError:
      return static_cast<const ErrorType*>(this)->Equals(other);
    case Kind::kDyn:
      return static_cast<const DynType*>(this)->Equals(other);
    case Kind::kAny:
      return static_cast<const AnyType*>(this)->Equals(other);
    case Kind::kType:
      return static_cast<const TypeType*>(this)->Equals(other);
    case Kind::kBool:
      return static_cast<const BoolType*>(this)->Equals(other);
    case Kind::kInt:
      return static_cast<const IntType*>(this)->Equals(other);
    case Kind::kUint:
      return static_cast<const UintType*>(this)->Equals(other);
    case Kind::kDouble:
      return static_cast<const DoubleType*>(this)->Equals(other);
    case Kind::kString:
      return static_cast<const StringType*>(this)->Equals(other);
    case Kind::kBytes:
      return static_cast<const BytesType*>(this)->Equals(other);
    case Kind::kEnum:
      return static_cast<const EnumType*>(this)->Equals(other);
    case Kind::kDuration:
      return static_cast<const DurationType*>(this)->Equals(other);
    case Kind::kTimestamp:
      return static_cast<const TimestampType*>(this)->Equals(other);
    case Kind::kList:
      return static_cast<const ListType*>(this)->Equals(other);
    case Kind::kMap:
      return static_cast<const MapType*>(this)->Equals(other);
    case Kind::kStruct:
      return static_cast<const StructType*>(this)->Equals(other);
    case Kind::kUnknown:
      return static_cast<const UnknownType*>(this)->Equals(other);
    default:
      return kind() == other.kind() && name() == other.name();
  }
}

void Type::HashValue(absl::HashState state) const {
  switch (kind()) {
    case Kind::kNullType:
      return static_cast<const NullType*>(this)->HashValue(std::move(state));
    case Kind::kError:
      return static_cast<const ErrorType*>(this)->HashValue(std::move(state));
    case Kind::kDyn:
      return static_cast<const DynType*>(this)->HashValue(std::move(state));
    case Kind::kAny:
      return static_cast<const AnyType*>(this)->HashValue(std::move(state));
    case Kind::kType:
      return static_cast<const TypeType*>(this)->HashValue(std::move(state));
    case Kind::kBool:
      return static_cast<const BoolType*>(this)->HashValue(std::move(state));
    case Kind::kInt:
      return static_cast<const IntType*>(this)->HashValue(std::move(state));
    case Kind::kUint:
      return static_cast<const UintType*>(this)->HashValue(std::move(state));
    case Kind::kDouble:
      return static_cast<const DoubleType*>(this)->HashValue(std::move(state));
    case Kind::kString:
      return static_cast<const StringType*>(this)->HashValue(std::move(state));
    case Kind::kBytes:
      return static_cast<const BytesType*>(this)->HashValue(std::move(state));
    case Kind::kEnum:
      return static_cast<const EnumType*>(this)->HashValue(std::move(state));
    case Kind::kDuration:
      return static_cast<const DurationType*>(this)->HashValue(
          std::move(state));
    case Kind::kTimestamp:
      return static_cast<const TimestampType*>(this)->HashValue(
          std::move(state));
    case Kind::kList:
      return static_cast<const ListType*>(this)->HashValue(std::move(state));
    case Kind::kMap:
      return static_cast<const MapType*>(this)->HashValue(std::move(state));
    case Kind::kStruct:
      return static_cast<const StructType*>(this)->HashValue(std::move(state));
    case Kind::kUnknown:
      return static_cast<const UnknownType*>(this)->HashValue(std::move(state));
    default:
      absl::HashState::combine(std::move(state), kind(), name());
      return;
  }
}

namespace base_internal {

bool TypeHandle::Equals(const Type& lhs, const Type& rhs, Kind kind) {
  switch (kind) {
    case Kind::kNullType:
      return true;
    case Kind::kError:
      return true;
    case Kind::kDyn:
      return true;
    case Kind::kAny:
      return true;
    case Kind::kType:
      return true;
    case Kind::kBool:
      return true;
    case Kind::kInt:
      return true;
    case Kind::kUint:
      return true;
    case Kind::kDouble:
      return true;
    case Kind::kString:
      return true;
    case Kind::kBytes:
      return true;
    case Kind::kEnum:
      return lhs.name() == rhs.name();
    case Kind::kDuration:
      return true;
    case Kind::kTimestamp:
      return true;
    case Kind::kList:
      return static_cast<const ListType&>(lhs).element() ==
             static_cast<const ListType&>(rhs).element();
    case Kind::kMap:
      return static_cast<const MapType&>(lhs).key() ==
                 static_cast<const MapType&>(rhs).key() &&
             static_cast<const MapType&>(lhs).value() ==
                 static_cast<const MapType&>(rhs).value();
    case Kind::kStruct:
      return lhs.name() == rhs.name();
    case Kind::kUnknown:
      return true;
    default:
      return false;
  }
}

void TypeHandle::HashValue(const Type& type, Kind kind, absl::HashState state) {
  switch (kind) {
    case Kind::kNullType:
      absl::HashState::combine(std::move(state), kind, type.name());
      return;
    case Kind::kError:
      absl::HashState::combine(std::move(state), kind, type.name());
      return;
    case Kind::kDyn:
      absl::HashState::combine(std::move(state), kind, type.name());
      return;
    case Kind::kAny:
      absl::HashState::combine(std::move(state), kind, type.name());
      return;
    case Kind::kType:
      absl::HashState::combine(std::move(state), kind, type.name());
      return;
    case Kind::kBool:
      absl::HashState::combine(std::move(state), kind, type.name());
      return;
    case Kind::kInt:
      absl::HashState::combine(std::move(state), kind, type.name());
      return;
    case Kind::kUint:
      absl::HashState::combine(std::move(state), kind, type.name());
      return;
    case Kind::kDouble:
      absl::HashState::combine(std::move(state), kind, type.name());
      return;
    case Kind::kString:
      absl::HashState::combine(std::move(state), kind, type.name());
      return;
    case Kind::kBytes:
      absl::HashState::combine(std::move(state), kind, type.name());
      return;
    case Kind::kEnum:
      absl::HashState::combine(std::move(state), kind, type.name());
      return;
    case Kind::kDuration:
      absl::HashState::combine(std::move(state), kind, type.name());
      return;
    case Kind::kTimestamp:
      absl::HashState::combine(std::move(state), kind, type.name());
      return;
    case Kind::kList:
      absl::HashState::combine(std::move(state),
                               static_cast<const ListType&>(type).element(),
                               kind, type.name());
      return;
    case Kind::kMap:
      absl::HashState::combine(
          std::move(state), static_cast<const MapType&>(type).key(),
          static_cast<const MapType&>(type).value(), kind, type.name());
      return;
    case Kind::kStruct:
      absl::HashState::combine(std::move(state), kind, type.name());
      return;
    case Kind::kUnknown:
      absl::HashState::combine(std::move(state), kind, type.name());
      return;
    default:
      return;
  }
}

bool TypeHandle::Equals(const TypeHandle& other) const {
  const auto* self = static_cast<const Type*>(data_.get());
  const auto* that = static_cast<const Type*>(other.data_.get());
  if (self == that) {
    return true;
  }
  if (self == nullptr || that == nullptr) {
    return false;
  }
  Kind kind = self->kind();
  return kind == that->kind() && Equals(*self, *that, kind);
}

void TypeHandle::HashValue(absl::HashState state) const {
  if (const auto* pointer = static_cast<const Type*>(data_.get());
      ABSL_PREDICT_TRUE(pointer != nullptr)) {
    HashValue(*pointer, pointer->kind(), std::move(state));
  }
}

void TypeHandle::CopyFrom(const TypeHandle& other) {
  // data_ is currently uninitialized.
  auto locality = other.data_.locality();
  if (locality == DataLocality::kStoredInline) {
    if (ABSL_PREDICT_FALSE(!other.data_.IsTrivial())) {
      // Type currently has only trivially copyable inline
      // representations.
      internal::unreachable();
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
  if (data_.IsStoredInline()) {
    if (ABSL_PREDICT_FALSE(!other.data_.IsTrivial())) {
      // Type currently has only trivially copyable inline
      // representations.
      internal::unreachable();
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
        internal::unreachable();
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
  switch (data_.kind_heap()) {
    case Kind::kList:
      delete static_cast<ModernListType*>(
          static_cast<ListType*>(static_cast<Type*>(data_.get_heap())));
      return;
    case Kind::kMap:
      delete static_cast<ModernMapType*>(
          static_cast<MapType*>(static_cast<Type*>(data_.get_heap())));
      return;
    case Kind::kEnum:
      delete static_cast<EnumType*>(static_cast<Type*>(data_.get_heap()));
      return;
    case Kind::kStruct:
      delete static_cast<AbstractStructType*>(
          static_cast<Type*>(data_.get_heap()));
      return;
    default:
      internal::unreachable();
  }
}

}  // namespace base_internal

}  // namespace cel
