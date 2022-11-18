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
#include "base/internal/message_wrapper.h"
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

Handle<Type> Value::type() const {
  switch (kind()) {
    case Kind::kNullType:
      return static_cast<const NullValue*>(this)->type().As<Type>();
    case Kind::kError:
      return static_cast<const ErrorValue*>(this)->type().As<Type>();
    case Kind::kType:
      return static_cast<const TypeValue*>(this)->type().As<Type>();
    case Kind::kBool:
      return static_cast<const BoolValue*>(this)->type().As<Type>();
    case Kind::kInt:
      return static_cast<const IntValue*>(this)->type().As<Type>();
    case Kind::kUint:
      return static_cast<const UintValue*>(this)->type().As<Type>();
    case Kind::kDouble:
      return static_cast<const DoubleValue*>(this)->type().As<Type>();
    case Kind::kString:
      return static_cast<const StringValue*>(this)->type().As<Type>();
    case Kind::kBytes:
      return static_cast<const BytesValue*>(this)->type().As<Type>();
    case Kind::kEnum:
      return static_cast<const EnumValue*>(this)->type().As<Type>();
    case Kind::kDuration:
      return static_cast<const DurationValue*>(this)->type().As<Type>();
    case Kind::kTimestamp:
      return static_cast<const TimestampValue*>(this)->type().As<Type>();
    case Kind::kList:
      return static_cast<const ListValue*>(this)->type().As<Type>();
    case Kind::kMap:
      return static_cast<const MapValue*>(this)->type().As<Type>();
    case Kind::kStruct:
      return static_cast<const StructValue*>(this)->type().As<Type>();
    case Kind::kUnknown:
      return static_cast<const UnknownValue*>(this)->type().As<Type>();
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

namespace base_internal {

bool ValueHandle::Equals(const Value& lhs, const Value& rhs, Kind kind) {
  switch (kind) {
    case Kind::kNullType:
      return true;
    case Kind::kError:
      return static_cast<const ErrorValue&>(lhs).value() ==
             static_cast<const ErrorValue&>(rhs).value();
    case Kind::kType:
      return static_cast<const TypeValue&>(lhs).Equals(
          static_cast<const TypeValue&>(rhs));
    case Kind::kBool:
      return static_cast<const BoolValue&>(lhs).value() ==
             static_cast<const BoolValue&>(rhs).value();
    case Kind::kInt:
      return static_cast<const IntValue&>(lhs).value() ==
             static_cast<const IntValue&>(rhs).value();
    case Kind::kUint:
      return static_cast<const UintValue&>(lhs).value() ==
             static_cast<const UintValue&>(rhs).value();
    case Kind::kDouble:
      return static_cast<const DoubleValue&>(lhs).value() ==
             static_cast<const DoubleValue&>(rhs).value();
    case Kind::kString:
      return static_cast<const StringValue&>(lhs).Equals(
          static_cast<const StringValue&>(rhs));
    case Kind::kBytes:
      return static_cast<const BytesValue&>(lhs).Equals(
          static_cast<const BytesValue&>(rhs));
    case Kind::kEnum:
      return static_cast<const EnumValue&>(lhs).number() ==
                 static_cast<const EnumValue&>(rhs).number() &&
             static_cast<const EnumValue&>(lhs).type() ==
                 static_cast<const EnumValue&>(rhs).type();
    case Kind::kDuration:
      return static_cast<const DurationValue&>(lhs).value() ==
             static_cast<const DurationValue&>(rhs).value();
    case Kind::kTimestamp:
      return static_cast<const TimestampValue&>(lhs).value() ==
             static_cast<const TimestampValue&>(rhs).value();
    case Kind::kList: {
      bool stored_inline = Metadata::IsStoredInline(lhs);
      if (stored_inline != Metadata::IsStoredInline(rhs)) {
        return false;
      }
      if (stored_inline) {
        return static_cast<const LegacyListValue&>(lhs).impl_ ==
               static_cast<const LegacyListValue&>(rhs).impl_;
      }
      return &lhs == &rhs;
    }
    case Kind::kMap: {
      bool stored_inline = Metadata::IsStoredInline(lhs);
      if (stored_inline != Metadata::IsStoredInline(rhs)) {
        return false;
      }
      if (stored_inline) {
        return static_cast<const LegacyMapValue&>(lhs).impl_ ==
               static_cast<const LegacyMapValue&>(rhs).impl_;
      }
      return &lhs == &rhs;
    }
    case Kind::kStruct: {
      bool stored_inline = Metadata::IsStoredInline(lhs);
      if (stored_inline != Metadata::IsStoredInline(rhs)) {
        return false;
      }
      if (stored_inline) {
        return (static_cast<const LegacyStructValue&>(lhs).msg_ &
                kMessageWrapperPtrMask) ==
               (static_cast<const LegacyStructValue&>(rhs).msg_ &
                kMessageWrapperPtrMask);
      }
      return &lhs == &rhs;
    }
    case Kind::kUnknown:
      return static_cast<const UnknownValue&>(lhs).attribute_set() ==
                 static_cast<const UnknownValue&>(rhs).attribute_set() &&
             static_cast<const UnknownValue&>(lhs).function_result_set() ==
                 static_cast<const UnknownValue&>(rhs).function_result_set();
    default:
      internal::unreachable();
  }
}

bool ValueHandle::Equals(const ValueHandle& other) const {
  const auto* self = static_cast<const Value*>(data_.get());
  const auto* that = static_cast<const Value*>(other.data_.get());
  if (self == that) {
    return true;
  }
  if (self == nullptr || that == nullptr) {
    return false;
  }
  Kind kind = self->kind();
  return kind == that->kind() && Equals(*self, *that, kind);
}

void ValueHandle::CopyFrom(const ValueHandle& other) {
  // data_ is currently uninitialized.
  auto locality = other.data_.locality();
  if (locality == DataLocality::kStoredInline) {
    if (!other.data_.IsTrivial()) {
      switch (other.data_.kind_inline()) {
        case Kind::kError:
          data_.ConstructInline<ErrorValue>(
              *static_cast<const ErrorValue*>(other.data_.get_inline()));
          return;
        case Kind::kUnknown:
          data_.ConstructInline<UnknownValue>(
              *static_cast<const UnknownValue*>(other.data_.get_inline()));
          return;
        case Kind::kString:
          data_.ConstructInline<InlinedCordStringValue>(
              *static_cast<const InlinedCordStringValue*>(
                  other.data_.get_inline()));
          return;
        case Kind::kBytes:
          data_.ConstructInline<InlinedCordBytesValue>(
              *static_cast<const InlinedCordBytesValue*>(
                  other.data_.get_inline()));
          return;
        case Kind::kType:
          data_.ConstructInline<base_internal::ModernTypeValue>(
              *static_cast<const base_internal::ModernTypeValue*>(
                  other.data_.get_inline()));
          return;
        case Kind::kEnum:
          data_.ConstructInline<EnumValue>(
              *static_cast<const EnumValue*>(other.data_.get_inline()));
          return;
        default:
          internal::unreachable();
      }
    } else {  // trivially copyable
      // We can simply just copy the bytes.
      data_.CopyFrom(other.data_);
    }
  } else {  // not inline
    data_.set_pointer(other.data_.pointer());
    if (locality == DataLocality::kReferenceCounted) {
      Ref();
    }
  }
}

void ValueHandle::MoveFrom(ValueHandle& other) {
  // data_ is currently uninitialized.
  if (other.data_.IsStoredInline()) {
    if (!other.data_.IsTrivial()) {
      switch (other.data_.kind_inline()) {
        case Kind::kError:
          data_.ConstructInline<ErrorValue>(
              std::move(*static_cast<ErrorValue*>(other.data_.get_inline())));
          other.data_.Destruct<ErrorValue>();
          break;
        case Kind::kUnknown:
          data_.ConstructInline<UnknownValue>(
              std::move(*static_cast<UnknownValue*>(other.data_.get_inline())));
          other.data_.Destruct<UnknownValue>();
          break;
        case Kind::kString:
          data_.ConstructInline<InlinedCordStringValue>(std::move(
              *static_cast<InlinedCordStringValue*>(other.data_.get_inline())));
          other.data_.Destruct<InlinedCordStringValue>();
          break;
        case Kind::kBytes:
          data_.ConstructInline<InlinedCordBytesValue>(std::move(
              *static_cast<InlinedCordBytesValue*>(other.data_.get_inline())));
          other.data_.Destruct<InlinedCordBytesValue>();
          break;
        case Kind::kType:
          data_.ConstructInline<ModernTypeValue>(std::move(
              *static_cast<const ModernTypeValue*>(other.data_.get_inline())));
          other.data_.Destruct<ModernTypeValue>();
          break;
        case Kind::kEnum:
          data_.ConstructInline<EnumValue>(std::move(
              *static_cast<const EnumValue*>(other.data_.get_inline())));
          other.data_.Destruct<EnumValue>();
          break;
        default:
          internal::unreachable();
      }
    } else {  // trivially copyable
      // We can simply just copy the bytes.
      data_.CopyFrom(other.data_);
    }
  } else {  // not inline
    data_.set_pointer(other.data_.pointer());
  }
  other.data_.Clear();
}

void ValueHandle::CopyAssign(const ValueHandle& other) {
  // data_ is initialized.
  Destruct();
  CopyFrom(other);
}

void ValueHandle::MoveAssign(ValueHandle& other) {
  // data_ is initialized.
  Destruct();
  MoveFrom(other);
}

void ValueHandle::Destruct() {
  switch (data_.locality()) {
    case DataLocality::kNull:
      return;
    case DataLocality::kStoredInline:
      if (!data_.IsTrivial()) {
        switch (data_.kind_inline()) {
          case Kind::kError:
            data_.Destruct<ErrorValue>();
            return;
          case Kind::kUnknown:
            data_.Destruct<UnknownValue>();
            return;
          case Kind::kString:
            data_.Destruct<InlinedCordStringValue>();
            return;
          case Kind::kBytes:
            data_.Destruct<InlinedCordBytesValue>();
            return;
          case Kind::kType:
            data_.Destruct<ModernTypeValue>();
            return;
          case Kind::kEnum:
            data_.Destruct<EnumValue>();
            return;
          default:
            internal::unreachable();
        }
      }
      return;
    case DataLocality::kReferenceCounted:
      Unref();
      return;
    case DataLocality::kArenaAllocated:
      return;
  }
}

void ValueHandle::Delete() const {
  switch (data_.kind_heap()) {
    case Kind::kList:
      delete static_cast<AbstractListValue*>(
          static_cast<ListValue*>(static_cast<Value*>(data_.get_heap())));
      return;
    case Kind::kMap:
      delete static_cast<AbstractMapValue*>(
          static_cast<MapValue*>(static_cast<Value*>(data_.get_heap())));
      return;
    case Kind::kStruct:
      delete static_cast<AbstractStructValue*>(
          static_cast<Value*>(data_.get_heap()));
      return;
    case Kind::kString:
      delete static_cast<StringStringValue*>(
          static_cast<Value*>(data_.get_heap()));
      return;
    case Kind::kBytes:
      delete static_cast<StringBytesValue*>(
          static_cast<Value*>(data_.get_heap()));
      return;
    default:
      internal::unreachable();
  }
}

}  // namespace base_internal

}  // namespace cel
