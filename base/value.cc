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

#include "base/value.h"

#include <cstddef>
#include <string>
#include <utility>

#include "absl/base/macros.h"
#include "base/values/bool_value.h"
#include "base/values/bytes_value.h"
#include "base/values/double_value.h"
#include "base/values/duration_value.h"
#include "base/values/enum_value.h"
#include "base/values/error_value.h"
#include "base/values/int_value.h"
#include "base/values/list_value.h"
#include "base/values/map_value.h"
#include "base/values/null_value.h"
#include "base/values/string_value.h"
#include "base/values/struct_value.h"
#include "base/values/timestamp_value.h"
#include "base/values/type_value.h"
#include "base/values/uint_value.h"
#include "base/values/unknown_value.h"
#include "internal/unreachable.h"

namespace cel {

CEL_INTERNAL_VALUE_IMPL(Value);

Persistent<const Type> Value::type() const {
  switch (kind()) {
    case Kind::kNullType:
      return static_cast<const NullValue*>(this)->type().As<const Type>();
    case Kind::kError:
      return static_cast<const ErrorValue*>(this)->type().As<const Type>();
    case Kind::kType:
      return static_cast<const TypeValue*>(this)->type().As<const Type>();
    case Kind::kBool:
      return static_cast<const BoolValue*>(this)->type().As<const Type>();
    case Kind::kInt:
      return static_cast<const IntValue*>(this)->type().As<const Type>();
    case Kind::kUint:
      return static_cast<const UintValue*>(this)->type().As<const Type>();
    case Kind::kDouble:
      return static_cast<const DoubleValue*>(this)->type().As<const Type>();
    case Kind::kString:
      return static_cast<const StringValue*>(this)->type().As<const Type>();
    case Kind::kBytes:
      return static_cast<const BytesValue*>(this)->type().As<const Type>();
    case Kind::kEnum:
      return static_cast<const EnumValue*>(this)->type().As<const Type>();
    case Kind::kDuration:
      return static_cast<const DurationValue*>(this)->type().As<const Type>();
    case Kind::kTimestamp:
      return static_cast<const TimestampValue*>(this)->type().As<const Type>();
    case Kind::kList:
      return static_cast<const ListValue*>(this)->type().As<const Type>();
    case Kind::kMap:
      return static_cast<const MapValue*>(this)->type().As<const Type>();
    case Kind::kStruct:
      return static_cast<const StructValue*>(this)->type().As<const Type>();
    case Kind::kUnknown:
      return static_cast<const UnknownValue*>(this)->type().As<const Type>();
    default:
      internal::unreachable();
  }
}

std::string Value::DebugString() const {
  switch (kind()) {
    case Kind::kNullType:
      return static_cast<const NullValue*>(this)->DebugString();
    case Kind::kError:
      return static_cast<const ErrorValue*>(this)->DebugString();
    case Kind::kType:
      return static_cast<const TypeValue*>(this)->DebugString();
    case Kind::kBool:
      return static_cast<const BoolValue*>(this)->DebugString();
    case Kind::kInt:
      return static_cast<const IntValue*>(this)->DebugString();
    case Kind::kUint:
      return static_cast<const UintValue*>(this)->DebugString();
    case Kind::kDouble:
      return static_cast<const DoubleValue*>(this)->DebugString();
    case Kind::kString:
      return static_cast<const StringValue*>(this)->DebugString();
    case Kind::kBytes:
      return static_cast<const BytesValue*>(this)->DebugString();
    case Kind::kEnum:
      return static_cast<const EnumValue*>(this)->DebugString();
    case Kind::kDuration:
      return static_cast<const DurationValue*>(this)->DebugString();
    case Kind::kTimestamp:
      return static_cast<const TimestampValue*>(this)->DebugString();
    case Kind::kList:
      return static_cast<const ListValue*>(this)->DebugString();
    case Kind::kMap:
      return static_cast<const MapValue*>(this)->DebugString();
    case Kind::kStruct:
      return static_cast<const StructValue*>(this)->DebugString();
    case Kind::kUnknown:
      return static_cast<const UnknownValue*>(this)->DebugString();
    default:
      internal::unreachable();
  }
}

void Value::HashValue(absl::HashState state) const {
  switch (kind()) {
    case Kind::kNullType:
      return static_cast<const NullValue*>(this)->HashValue(std::move(state));
    case Kind::kError:
      return static_cast<const ErrorValue*>(this)->HashValue(std::move(state));
    case Kind::kType:
      return static_cast<const TypeValue*>(this)->HashValue(std::move(state));
    case Kind::kBool:
      return static_cast<const BoolValue*>(this)->HashValue(std::move(state));
    case Kind::kInt:
      return static_cast<const IntValue*>(this)->HashValue(std::move(state));
    case Kind::kUint:
      return static_cast<const UintValue*>(this)->HashValue(std::move(state));
    case Kind::kDouble:
      return static_cast<const DoubleValue*>(this)->HashValue(std::move(state));
    case Kind::kString:
      return static_cast<const StringValue*>(this)->HashValue(std::move(state));
    case Kind::kBytes:
      return static_cast<const BytesValue*>(this)->HashValue(std::move(state));
    case Kind::kEnum:
      return static_cast<const EnumValue*>(this)->HashValue(std::move(state));
    case Kind::kDuration:
      return static_cast<const DurationValue*>(this)->HashValue(
          std::move(state));
    case Kind::kTimestamp:
      return static_cast<const TimestampValue*>(this)->HashValue(
          std::move(state));
    case Kind::kList:
      return static_cast<const ListValue*>(this)->HashValue(std::move(state));
    case Kind::kMap:
      return static_cast<const MapValue*>(this)->HashValue(std::move(state));
    case Kind::kStruct:
      return static_cast<const StructValue*>(this)->HashValue(std::move(state));
    case Kind::kUnknown:
      return static_cast<const UnknownValue*>(this)->HashValue(
          std::move(state));
    default:
      internal::unreachable();
  }
}

bool Value::Equals(const Value& other) const {
  if (this == &other) {
    return true;
  }
  switch (kind()) {
    case Kind::kNullType:
      return static_cast<const NullValue*>(this)->Equals(other);
    case Kind::kError:
      return static_cast<const ErrorValue*>(this)->Equals(other);
    case Kind::kType:
      return static_cast<const TypeValue*>(this)->Equals(other);
    case Kind::kBool:
      return static_cast<const BoolValue*>(this)->Equals(other);
    case Kind::kInt:
      return static_cast<const IntValue*>(this)->Equals(other);
    case Kind::kUint:
      return static_cast<const UintValue*>(this)->Equals(other);
    case Kind::kDouble:
      return static_cast<const DoubleValue*>(this)->Equals(other);
    case Kind::kString:
      return static_cast<const StringValue*>(this)->Equals(other);
    case Kind::kBytes:
      return static_cast<const BytesValue*>(this)->Equals(other);
    case Kind::kEnum:
      return static_cast<const EnumValue*>(this)->Equals(other);
    case Kind::kDuration:
      return static_cast<const DurationValue*>(this)->Equals(other);
    case Kind::kTimestamp:
      return static_cast<const TimestampValue*>(this)->Equals(other);
    case Kind::kList:
      return static_cast<const ListValue*>(this)->Equals(other);
    case Kind::kMap:
      return static_cast<const MapValue*>(this)->Equals(other);
    case Kind::kStruct:
      return static_cast<const StructValue*>(this)->Equals(other);
    case Kind::kUnknown:
      return static_cast<const UnknownValue*>(this)->Equals(other);
    default:
      internal::unreachable();
  }
}

namespace base_internal {

bool PersistentValueHandle::Equals(const PersistentValueHandle& other) const {
  const auto* self = static_cast<const Value*>(data_.get());
  const auto* that = static_cast<const Value*>(other.data_.get());
  if (self == that) {
    return true;
  }
  if (self == nullptr || that == nullptr) {
    return false;
  }
  return *self == *that;
}

void PersistentValueHandle::HashValue(absl::HashState state) const {
  if (const auto* pointer = static_cast<const Value*>(data_.get());
      ABSL_PREDICT_TRUE(pointer != nullptr)) {
    pointer->HashValue(std::move(state));
  }
}

void PersistentValueHandle::CopyFrom(const PersistentValueHandle& other) {
  // data_ is currently uninitialized.
  auto locality = other.data_.locality();
  if (locality == DataLocality::kStoredInline &&
      !other.data_.IsTriviallyCopyable()) {
    switch (other.data_.kind()) {
      case Kind::kError:
        data_.ConstructInline<ErrorValue>(
            *static_cast<const ErrorValue*>(other.data_.get()));
        break;
      case Kind::kString:
        data_.ConstructInline<InlinedCordStringValue>(
            *static_cast<const InlinedCordStringValue*>(other.data_.get()));
        break;
      case Kind::kBytes:
        data_.ConstructInline<InlinedCordBytesValue>(
            *static_cast<const InlinedCordBytesValue*>(other.data_.get()));
        break;
      case Kind::kType:
        data_.ConstructInline<TypeValue>(
            *static_cast<const TypeValue*>(other.data_.get()));
        break;
      case Kind::kEnum:
        data_.ConstructInline<EnumValue>(
            *static_cast<const EnumValue*>(other.data_.get()));
        break;
      default:
        internal::unreachable();
    }
  } else {
    // We can simply just copy the bytes.
    data_.CopyFrom(other.data_);
    if (locality == DataLocality::kReferenceCounted) {
      Ref();
    }
  }
}

void PersistentValueHandle::MoveFrom(PersistentValueHandle& other) {
  // data_ is currently uninitialized.
  auto locality = other.data_.locality();
  if (locality == DataLocality::kStoredInline &&
      !other.data_.IsTriviallyCopyable()) {
    switch (other.data_.kind()) {
      case Kind::kError:
        data_.ConstructInline<ErrorValue>(
            std::move(*static_cast<ErrorValue*>(other.data_.get())));
        break;
      case Kind::kString:
        data_.ConstructInline<InlinedCordStringValue>(std::move(
            *static_cast<InlinedCordStringValue*>(other.data_.get())));
        break;
      case Kind::kBytes:
        data_.ConstructInline<InlinedCordBytesValue>(
            std::move(*static_cast<InlinedCordBytesValue*>(other.data_.get())));
        break;
      case Kind::kType:
        data_.ConstructInline<TypeValue>(
            std::move(*static_cast<const TypeValue*>(other.data_.get())));
        break;
      case Kind::kEnum:
        data_.ConstructInline<EnumValue>(
            std::move(*static_cast<const EnumValue*>(other.data_.get())));
        break;
      default:
        internal::unreachable();
    }
    other.Destruct();
    other.data_.Clear();
  } else {
    // We can simply just copy the bytes.
    data_.MoveFrom(other.data_);
  }
}

void PersistentValueHandle::CopyAssign(const PersistentValueHandle& other) {
  // data_ is initialized.
  Destruct();
  CopyFrom(other);
}

void PersistentValueHandle::MoveAssign(PersistentValueHandle& other) {
  // data_ is initialized.
  Destruct();
  MoveFrom(other);
}

void PersistentValueHandle::Destruct() {
  switch (data_.locality()) {
    case DataLocality::kNull:
      break;
    case DataLocality::kStoredInline:
      if (!data_.IsTriviallyDestructible()) {
        switch (data_.kind()) {
          case Kind::kError:
            data_.Destruct<ErrorValue>();
            break;
          case Kind::kString:
            data_.Destruct<InlinedCordStringValue>();
            break;
          case Kind::kBytes:
            data_.Destruct<InlinedCordBytesValue>();
            break;
          case Kind::kType:
            data_.Destruct<TypeValue>();
            break;
          case Kind::kEnum:
            data_.Destruct<EnumValue>();
            break;
          default:
            internal::unreachable();
        }
      }
      break;
    case DataLocality::kReferenceCounted:
      Unref();
      break;
    case DataLocality::kArenaAllocated:
      break;
  }
}

void PersistentValueHandle::Delete() const {
  switch (data_.kind()) {
    case Kind::kList:
      delete static_cast<ListValue*>(static_cast<Value*>(data_.get()));
      break;
    case Kind::kMap:
      delete static_cast<MapValue*>(static_cast<Value*>(data_.get()));
      break;
    case Kind::kStruct:
      delete static_cast<AbstractStructValue*>(
          static_cast<Value*>(data_.get()));
      break;
    case Kind::kString:
      delete static_cast<StringStringValue*>(static_cast<Value*>(data_.get()));
      break;
    case Kind::kBytes:
      delete static_cast<StringBytesValue*>(static_cast<Value*>(data_.get()));
      break;
    case Kind::kUnknown:
      delete static_cast<UnknownValue*>(static_cast<Value*>(data_.get()));
      break;
    default:
      internal::unreachable();
  }
}

}  // namespace base_internal

}  // namespace cel
