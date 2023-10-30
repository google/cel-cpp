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

#include <string>
#include <utility>

#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/handle.h"
#include "base/internal/data.h"
#include "base/internal/message_wrapper.h"
#include "base/kind.h"
#include "base/type.h"
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
#include "base/values/opaque_value.h"
#include "base/values/string_value.h"
#include "base/values/struct_value.h"
#include "base/values/timestamp_value.h"
#include "base/values/type_value.h"
#include "base/values/uint_value.h"
#include "base/values/unknown_value.h"
#include "common/any.h"
#include "common/json.h"

namespace cel {

CEL_INTERNAL_VALUE_IMPL(Value);

Handle<Type> Value::type() const {
  switch (kind()) {
    case ValueKind::kNullType:
      return static_cast<const NullValue*>(this)->type().As<Type>();
    case ValueKind::kError:
      return static_cast<const ErrorValue*>(this)->type().As<Type>();
    case ValueKind::kType:
      return static_cast<const TypeValue*>(this)->type().As<Type>();
    case ValueKind::kBool:
      return static_cast<const BoolValue*>(this)->type().As<Type>();
    case ValueKind::kInt:
      return static_cast<const IntValue*>(this)->type().As<Type>();
    case ValueKind::kUint:
      return static_cast<const UintValue*>(this)->type().As<Type>();
    case ValueKind::kDouble:
      return static_cast<const DoubleValue*>(this)->type().As<Type>();
    case ValueKind::kString:
      return static_cast<const StringValue*>(this)->type().As<Type>();
    case ValueKind::kBytes:
      return static_cast<const BytesValue*>(this)->type().As<Type>();
    case ValueKind::kEnum:
      return static_cast<const EnumValue*>(this)->type().As<Type>();
    case ValueKind::kDuration:
      return static_cast<const DurationValue*>(this)->type().As<Type>();
    case ValueKind::kTimestamp:
      return static_cast<const TimestampValue*>(this)->type().As<Type>();
    case ValueKind::kList:
      return static_cast<const ListValue*>(this)->type().As<Type>();
    case ValueKind::kMap:
      return static_cast<const MapValue*>(this)->type().As<Type>();
    case ValueKind::kStruct:
      return static_cast<const StructValue*>(this)->type().As<Type>();
    case ValueKind::kUnknown:
      return static_cast<const UnknownValue*>(this)->type().As<Type>();
    case ValueKind::kOpaque:
      return static_cast<const OpaqueValue*>(this)->type().As<Type>();
    default:
      ABSL_UNREACHABLE();
  }
}

std::string Value::DebugString() const {
  switch (kind()) {
    case ValueKind::kNullType:
      return static_cast<const NullValue*>(this)->DebugString();
    case ValueKind::kError:
      return static_cast<const ErrorValue*>(this)->DebugString();
    case ValueKind::kType:
      return static_cast<const TypeValue*>(this)->DebugString();
    case ValueKind::kBool:
      return static_cast<const BoolValue*>(this)->DebugString();
    case ValueKind::kInt:
      return static_cast<const IntValue*>(this)->DebugString();
    case ValueKind::kUint:
      return static_cast<const UintValue*>(this)->DebugString();
    case ValueKind::kDouble:
      return static_cast<const DoubleValue*>(this)->DebugString();
    case ValueKind::kString:
      return static_cast<const StringValue*>(this)->DebugString();
    case ValueKind::kBytes:
      return static_cast<const BytesValue*>(this)->DebugString();
    case ValueKind::kEnum:
      return static_cast<const EnumValue*>(this)->DebugString();
    case ValueKind::kDuration:
      return static_cast<const DurationValue*>(this)->DebugString();
    case ValueKind::kTimestamp:
      return static_cast<const TimestampValue*>(this)->DebugString();
    case ValueKind::kList:
      return static_cast<const ListValue*>(this)->DebugString();
    case ValueKind::kMap:
      return static_cast<const MapValue*>(this)->DebugString();
    case ValueKind::kStruct:
      return static_cast<const StructValue*>(this)->DebugString();
    case ValueKind::kUnknown:
      return static_cast<const UnknownValue*>(this)->DebugString();
    case ValueKind::kOpaque:
      return static_cast<const OpaqueValue*>(this)->DebugString();
    default:
      ABSL_UNREACHABLE();
  }
}

absl::StatusOr<Any> Value::ConvertToAny(ValueFactory& value_factory) const {
  switch (kind()) {
    case ValueKind::kNullType:
      return static_cast<const NullValue*>(this)->ConvertToAny(value_factory);
    case ValueKind::kError:
      return static_cast<const ErrorValue*>(this)->ConvertToAny(value_factory);
    case ValueKind::kType:
      return static_cast<const TypeValue*>(this)->ConvertToAny(value_factory);
    case ValueKind::kBool:
      return static_cast<const BoolValue*>(this)->ConvertToAny(value_factory);
    case ValueKind::kInt:
      return static_cast<const IntValue*>(this)->ConvertToAny(value_factory);
    case ValueKind::kUint:
      return static_cast<const UintValue*>(this)->ConvertToAny(value_factory);
    case ValueKind::kDouble:
      return static_cast<const DoubleValue*>(this)->ConvertToAny(value_factory);
    case ValueKind::kString:
      return static_cast<const StringValue*>(this)->ConvertToAny(value_factory);
    case ValueKind::kBytes:
      return static_cast<const BytesValue*>(this)->ConvertToAny(value_factory);
    case ValueKind::kEnum:
      return static_cast<const EnumValue*>(this)->ConvertToAny(value_factory);
    case ValueKind::kDuration:
      return static_cast<const DurationValue*>(this)->ConvertToAny(
          value_factory);
    case ValueKind::kTimestamp:
      return static_cast<const TimestampValue*>(this)->ConvertToAny(
          value_factory);
    case ValueKind::kList:
      return static_cast<const ListValue*>(this)->ConvertToAny(value_factory);
    case ValueKind::kMap:
      return static_cast<const MapValue*>(this)->ConvertToAny(value_factory);
    case ValueKind::kStruct:
      return static_cast<const StructValue*>(this)->ConvertToAny(value_factory);
    case ValueKind::kUnknown:
      return static_cast<const UnknownValue*>(this)->ConvertToAny(
          value_factory);
    case ValueKind::kOpaque:
      return static_cast<const OpaqueValue*>(this)->ConvertToAny(value_factory);
    default:
      ABSL_UNREACHABLE();
  }
}

absl::StatusOr<Json> Value::ConvertToJson(ValueFactory& value_factory) const {
  switch (kind()) {
    case ValueKind::kNullType:
      return static_cast<const NullValue*>(this)->ConvertToJson(value_factory);
    case ValueKind::kError:
      return static_cast<const ErrorValue*>(this)->ConvertToJson(value_factory);
    case ValueKind::kType:
      return static_cast<const TypeValue*>(this)->ConvertToJson(value_factory);
    case ValueKind::kBool:
      return static_cast<const BoolValue*>(this)->ConvertToJson(value_factory);
    case ValueKind::kInt:
      return static_cast<const IntValue*>(this)->ConvertToJson(value_factory);
    case ValueKind::kUint:
      return static_cast<const UintValue*>(this)->ConvertToJson(value_factory);
    case ValueKind::kDouble:
      return static_cast<const DoubleValue*>(this)->ConvertToJson(
          value_factory);
    case ValueKind::kString:
      return static_cast<const StringValue*>(this)->ConvertToJson(
          value_factory);
    case ValueKind::kBytes:
      return static_cast<const BytesValue*>(this)->ConvertToJson(value_factory);
    case ValueKind::kEnum:
      return static_cast<const EnumValue*>(this)->ConvertToJson(value_factory);
    case ValueKind::kDuration:
      return static_cast<const DurationValue*>(this)->ConvertToJson(
          value_factory);
    case ValueKind::kTimestamp:
      return static_cast<const TimestampValue*>(this)->ConvertToJson(
          value_factory);
    case ValueKind::kList:
      return static_cast<const ListValue*>(this)->ConvertToJson(value_factory);
    case ValueKind::kMap:
      return static_cast<const MapValue*>(this)->ConvertToJson(value_factory);
    case ValueKind::kStruct:
      return static_cast<const StructValue*>(this)->ConvertToJson(
          value_factory);
    case ValueKind::kUnknown:
      return static_cast<const UnknownValue*>(this)->ConvertToJson(
          value_factory);
    case ValueKind::kOpaque:
      return static_cast<const OpaqueValue*>(this)->ConvertToJson(
          value_factory);
    default:
      ABSL_UNREACHABLE();
  }
}

absl::StatusOr<Handle<Value>> Value::ConvertToType(
    ValueFactory& value_factory, const Handle<Type>& type) const {
  switch (kind()) {
    case ValueKind::kNullType:
      return static_cast<const NullValue*>(this)->ConvertToType(value_factory,
                                                                type);
    case ValueKind::kError:
      return static_cast<const ErrorValue*>(this)->ConvertToType(value_factory,
                                                                 type);
    case ValueKind::kType:
      return static_cast<const TypeValue*>(this)->ConvertToType(value_factory,
                                                                type);
    case ValueKind::kBool:
      return static_cast<const BoolValue*>(this)->ConvertToType(value_factory,
                                                                type);
    case ValueKind::kInt:
      return static_cast<const IntValue*>(this)->ConvertToType(value_factory,
                                                               type);
    case ValueKind::kUint:
      return static_cast<const UintValue*>(this)->ConvertToType(value_factory,
                                                                type);
    case ValueKind::kDouble:
      return static_cast<const DoubleValue*>(this)->ConvertToType(value_factory,
                                                                  type);
    case ValueKind::kString:
      return static_cast<const StringValue*>(this)->ConvertToType(value_factory,
                                                                  type);
    case ValueKind::kBytes:
      return static_cast<const BytesValue*>(this)->ConvertToType(value_factory,
                                                                 type);
    case ValueKind::kEnum:
      return static_cast<const EnumValue*>(this)->ConvertToType(value_factory,
                                                                type);
    case ValueKind::kDuration:
      return static_cast<const DurationValue*>(this)->ConvertToType(
          value_factory, type);
    case ValueKind::kTimestamp:
      return static_cast<const TimestampValue*>(this)->ConvertToType(
          value_factory, type);
    case ValueKind::kList:
      return static_cast<const ListValue*>(this)->ConvertToType(value_factory,
                                                                type);
    case ValueKind::kMap:
      return static_cast<const MapValue*>(this)->ConvertToType(value_factory,
                                                               type);
    case ValueKind::kStruct:
      return static_cast<const StructValue*>(this)->ConvertToType(value_factory,
                                                                  type);
    case ValueKind::kUnknown:
      return static_cast<const UnknownValue*>(this)->ConvertToType(
          value_factory, type);
    case ValueKind::kOpaque:
      return static_cast<const OpaqueValue*>(this)->ConvertToType(value_factory,
                                                                  type);
    default:
      ABSL_UNREACHABLE();
  }
}

bool Value::IsRuntimeConvertible(const Type& from, const Type& to) {
  if (from.kind() == TypeKind::kDyn) {
    return to.kind() == TypeKind::kDyn;
  }
  return to.kind() == TypeKind::kDyn || Type::Equals(from, to);
}

absl::StatusOr<Handle<Value>> Value::Equals(ValueFactory& value_factory,
                                            const Value& other) const {
  switch (kind()) {
    case ValueKind::kNullType:
      return static_cast<const NullValue*>(this)->Equals(value_factory, other);
    case ValueKind::kError:
      return static_cast<const ErrorValue*>(this)->Equals(value_factory, other);
    case ValueKind::kType:
      return static_cast<const TypeValue*>(this)->Equals(value_factory, other);
    case ValueKind::kBool:
      return static_cast<const BoolValue*>(this)->Equals(value_factory, other);
    case ValueKind::kInt:
      return static_cast<const IntValue*>(this)->Equals(value_factory, other);
    case ValueKind::kUint:
      return static_cast<const UintValue*>(this)->Equals(value_factory, other);
    case ValueKind::kDouble:
      return static_cast<const DoubleValue*>(this)->Equals(value_factory,
                                                           other);
    case ValueKind::kString:
      return static_cast<const StringValue*>(this)->Equals(value_factory,
                                                           other);
    case ValueKind::kBytes:
      return static_cast<const BytesValue*>(this)->Equals(value_factory, other);
    case ValueKind::kEnum:
      return static_cast<const EnumValue*>(this)->Equals(value_factory, other);
    case ValueKind::kDuration:
      return static_cast<const DurationValue*>(this)->Equals(value_factory,
                                                             other);
    case ValueKind::kTimestamp:
      return static_cast<const TimestampValue*>(this)->Equals(value_factory,
                                                              other);
    case ValueKind::kList:
      return static_cast<const ListValue*>(this)->Equals(value_factory, other);
    case ValueKind::kMap:
      return static_cast<const MapValue*>(this)->Equals(value_factory, other);
    case ValueKind::kStruct:
      return static_cast<const StructValue*>(this)->Equals(value_factory,
                                                           other);
    case ValueKind::kUnknown:
      return static_cast<const UnknownValue*>(this)->Equals(value_factory,
                                                            other);
    case ValueKind::kOpaque:
      return static_cast<const OpaqueValue*>(this)->Equals(value_factory,
                                                           other);
    default:
      ABSL_UNREACHABLE();
  }
}

namespace base_internal {

bool ValueHandle::Equals(const Value& lhs, const Value& rhs, ValueKind kind) {
  switch (kind) {
    case ValueKind::kNullType:
      return true;
    case ValueKind::kError:
      return static_cast<const ErrorValue&>(lhs).NativeValue() ==
             static_cast<const ErrorValue&>(rhs).NativeValue();
    case ValueKind::kType:
      return static_cast<const TypeValue&>(lhs).name() ==
             static_cast<const TypeValue&>(rhs).name();
    case ValueKind::kBool:
      return static_cast<const BoolValue&>(lhs).NativeValue() ==
             static_cast<const BoolValue&>(rhs).NativeValue();
    case ValueKind::kInt:
      return static_cast<const IntValue&>(lhs).NativeValue() ==
             static_cast<const IntValue&>(rhs).NativeValue();
    case ValueKind::kUint:
      return static_cast<const UintValue&>(lhs).NativeValue() ==
             static_cast<const UintValue&>(rhs).NativeValue();
    case ValueKind::kDouble:
      return static_cast<const DoubleValue&>(lhs).NativeValue() ==
             static_cast<const DoubleValue&>(rhs).NativeValue();
    case ValueKind::kString:
      return static_cast<const StringValue&>(lhs).Equals(
          static_cast<const StringValue&>(rhs));
    case ValueKind::kBytes:
      return static_cast<const BytesValue&>(lhs).Equals(
          static_cast<const BytesValue&>(rhs));
    case ValueKind::kEnum:
      return static_cast<const EnumValue&>(lhs).number() ==
                 static_cast<const EnumValue&>(rhs).number() &&
             static_cast<const EnumValue&>(lhs).type() ==
                 static_cast<const EnumValue&>(rhs).type();
    case ValueKind::kDuration:
      return static_cast<const DurationValue&>(lhs).NativeValue() ==
             static_cast<const DurationValue&>(rhs).NativeValue();
    case ValueKind::kTimestamp:
      return static_cast<const TimestampValue&>(lhs).NativeValue() ==
             static_cast<const TimestampValue&>(rhs).NativeValue();
    case ValueKind::kList: {
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
    case ValueKind::kMap: {
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
    case ValueKind::kStruct: {
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
    case ValueKind::kUnknown:
      return static_cast<const UnknownValue&>(lhs).attribute_set() ==
                 static_cast<const UnknownValue&>(rhs).attribute_set() &&
             static_cast<const UnknownValue&>(lhs).function_result_set() ==
                 static_cast<const UnknownValue&>(rhs).function_result_set();
    case ValueKind::kOpaque:
      return &lhs == &rhs;
    default:
      ABSL_UNREACHABLE();
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
  ValueKind kind = self->kind();
  return kind == that->kind() && Equals(*self, *that, kind);
}

void ValueHandle::CopyFrom(const ValueHandle& other) {
  // data_ is currently uninitialized.
  auto locality = other.data_.locality();
  if (locality == DataLocality::kStoredInline) {
    if (!other.data_.IsTrivial()) {
      switch (KindToValueKind(other.data_.kind_inline())) {
        case ValueKind::kError:
          data_.ConstructInline<ErrorValue>(
              *static_cast<const ErrorValue*>(other.data_.get_inline()));
          return;
        case ValueKind::kUnknown:
          data_.ConstructInline<UnknownValue>(
              *static_cast<const UnknownValue*>(other.data_.get_inline()));
          return;
        case ValueKind::kString:
          switch (other.data_.inline_variant<InlinedStringValueVariant>()) {
            case InlinedStringValueVariant::kCord:
              data_.ConstructInline<InlinedCordStringValue>(
                  *static_cast<const InlinedCordStringValue*>(
                      other.data_.get_inline()));
              break;
            case InlinedStringValueVariant::kStringView:
              data_.ConstructInline<InlinedStringViewStringValue>(
                  *static_cast<const InlinedStringViewStringValue*>(
                      other.data_.get_inline()));
              break;
          }
          return;
        case ValueKind::kBytes:
          switch (other.data_.inline_variant<InlinedBytesValueVariant>()) {
            case InlinedBytesValueVariant::kCord:
              data_.ConstructInline<InlinedCordBytesValue>(
                  *static_cast<const InlinedCordBytesValue*>(
                      other.data_.get_inline()));
              break;
            case InlinedBytesValueVariant::kStringView:
              data_.ConstructInline<InlinedStringViewBytesValue>(
                  *static_cast<const InlinedStringViewBytesValue*>(
                      other.data_.get_inline()));
              break;
          }
          return;
        case ValueKind::kType:
          data_.ConstructInline<base_internal::ModernTypeValue>(
              *static_cast<const base_internal::ModernTypeValue*>(
                  other.data_.get_inline()));
          return;
        case ValueKind::kEnum:
          data_.ConstructInline<EnumValue>(
              *static_cast<const EnumValue*>(other.data_.get_inline()));
          return;
        default:
          ABSL_UNREACHABLE();
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
      switch (KindToValueKind(other.data_.kind_inline())) {
        case ValueKind::kError:
          data_.ConstructInline<ErrorValue>(
              std::move(*static_cast<ErrorValue*>(other.data_.get_inline())));
          other.data_.Destruct<ErrorValue>();
          break;
        case ValueKind::kUnknown:
          data_.ConstructInline<UnknownValue>(
              std::move(*static_cast<UnknownValue*>(other.data_.get_inline())));
          other.data_.Destruct<UnknownValue>();
          break;
        case ValueKind::kString:
          switch (other.data_.inline_variant<InlinedStringValueVariant>()) {
            case InlinedStringValueVariant::kCord:
              data_.ConstructInline<InlinedCordStringValue>(
                  std::move(*static_cast<InlinedCordStringValue*>(
                      other.data_.get_inline())));
              other.data_.Destruct<InlinedCordStringValue>();
              break;
            case InlinedStringValueVariant::kStringView:
              data_.ConstructInline<InlinedStringViewStringValue>(
                  std::move(*static_cast<InlinedStringViewStringValue*>(
                      other.data_.get_inline())));
              other.data_.Destruct<InlinedStringViewStringValue>();
              break;
          }
          break;
        case ValueKind::kBytes:
          switch (other.data_.inline_variant<InlinedBytesValueVariant>()) {
            case InlinedBytesValueVariant::kCord:
              data_.ConstructInline<InlinedCordBytesValue>(
                  std::move(*static_cast<InlinedCordBytesValue*>(
                      other.data_.get_inline())));
              other.data_.Destruct<InlinedCordBytesValue>();
              break;
            case InlinedBytesValueVariant::kStringView:
              data_.ConstructInline<InlinedStringViewBytesValue>(
                  std::move(*static_cast<InlinedStringViewBytesValue*>(
                      other.data_.get_inline())));
              other.data_.Destruct<InlinedStringViewBytesValue>();
              break;
          }
          break;
        case ValueKind::kType:
          data_.ConstructInline<ModernTypeValue>(std::move(
              *static_cast<const ModernTypeValue*>(other.data_.get_inline())));
          other.data_.Destruct<ModernTypeValue>();
          break;
        case ValueKind::kEnum:
          data_.ConstructInline<EnumValue>(std::move(
              *static_cast<const EnumValue*>(other.data_.get_inline())));
          other.data_.Destruct<EnumValue>();
          break;
        default:
          ABSL_UNREACHABLE();
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
        switch (KindToValueKind(data_.kind_inline())) {
          case ValueKind::kError:
            data_.Destruct<ErrorValue>();
            return;
          case ValueKind::kUnknown:
            data_.Destruct<UnknownValue>();
            return;
          case ValueKind::kString:
            switch (data_.inline_variant<InlinedStringValueVariant>()) {
              case InlinedStringValueVariant::kCord:
                data_.Destruct<InlinedCordStringValue>();
                break;
              case InlinedStringValueVariant::kStringView:
                data_.Destruct<InlinedStringViewStringValue>();
                break;
            }
            return;
          case ValueKind::kBytes:
            switch (data_.inline_variant<InlinedBytesValueVariant>()) {
              case InlinedBytesValueVariant::kCord:
                data_.Destruct<InlinedCordBytesValue>();
                break;
              case InlinedBytesValueVariant::kStringView:
                data_.Destruct<InlinedStringViewBytesValue>();
                break;
            }
            return;
          case ValueKind::kType:
            data_.Destruct<ModernTypeValue>();
            return;
          case ValueKind::kEnum:
            data_.Destruct<EnumValue>();
            return;
          default:
            ABSL_UNREACHABLE();
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
  Delete(KindToValueKind(data_.kind_heap()),
         *static_cast<const Value*>(data_.get_heap()));
}

void ValueHandle::Delete(ValueKind kind, const Value& value) {
  switch (kind) {
    case ValueKind::kList:
      delete static_cast<const AbstractListValue*>(&value);
      return;
    case ValueKind::kMap:
      delete static_cast<const AbstractMapValue*>(&value);
      return;
    case ValueKind::kStruct:
      delete static_cast<const AbstractStructValue*>(&value);
      return;
    case ValueKind::kString:
      delete static_cast<const StringStringValue*>(&value);
      return;
    case ValueKind::kBytes:
      delete static_cast<const StringBytesValue*>(&value);
      return;
    case ValueKind::kOpaque:
      delete static_cast<const OpaqueValue*>(&value);
      return;
    default:
      ABSL_UNREACHABLE();
  }
}

void ValueMetadata::Unref(const Value& value) {
  if (Metadata::Unref(value)) {
    ValueHandle::Delete(KindToValueKind(Metadata::KindHeap(value)), value);
  }
}

}  // namespace base_internal

}  // namespace cel
