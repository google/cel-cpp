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

#include "base/values/int_value.h"

#include <cstdint>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "base/handle.h"
#include "base/kind.h"
#include "base/type.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "base/values/double_value.h"
#include "base/values/uint_value.h"
#include "common/any.h"
#include "common/json.h"
#include "internal/number.h"
#include "internal/proto_wire.h"
#include "internal/status_macros.h"

namespace cel {

namespace {

using internal::ProtoWireEncoder;
using internal::ProtoWireTag;
using internal::ProtoWireType;

}  // namespace

CEL_INTERNAL_VALUE_IMPL(IntValue);

std::string IntValue::DebugString(int64_t value) { return absl::StrCat(value); }

std::string IntValue::DebugString() const { return DebugString(NativeValue()); }

absl::StatusOr<Any> IntValue::ConvertToAny(ValueFactory&) const {
  static constexpr absl::string_view kTypeName = "google.protobuf.Int64Value";
  const auto value = this->NativeValue();
  absl::Cord data;
  if (value) {
    ProtoWireEncoder encoder(kTypeName, data);
    CEL_RETURN_IF_ERROR(
        encoder.WriteTag(ProtoWireTag(1, ProtoWireType::kVarint)));
    CEL_RETURN_IF_ERROR(encoder.WriteVarint(value));
    encoder.EnsureFullyEncoded();
  }
  return MakeAny(MakeTypeUrl(kTypeName), std::move(data));
}

absl::StatusOr<Json> IntValue::ConvertToJson(ValueFactory&) const {
  return JsonInt(NativeValue());
}

absl::StatusOr<Handle<Value>> IntValue::ConvertToType(
    ValueFactory& value_factory, const Handle<Type>& type) const {
  switch (type->kind()) {
    case TypeKind::kInt:
      return handle_from_this();
    case TypeKind::kDouble:
      return value_factory.CreateDoubleValue(
          static_cast<double>(NativeValue()));
    case TypeKind::kUint: {
      auto number = internal::Number::FromInt64(NativeValue());
      if (!number.LosslessConvertibleToUint()) {
        return value_factory.CreateErrorValue(
            absl::OutOfRangeError("unsigned integer overflow"));
      }
      return value_factory.CreateUintValue(number.AsUint());
    }
    case TypeKind::kType:
      return value_factory.CreateTypeValue(this->type());
    case TypeKind::kString:
      return value_factory.CreateStringValue(absl::StrCat(NativeValue()));
    case TypeKind::kTimestamp: {
      auto status_or_timestamp = value_factory.CreateTimestampValue(
          absl::FromUnixSeconds(NativeValue()));
      if (!status_or_timestamp.ok()) {
        return value_factory.CreateErrorValue(status_or_timestamp.status());
      }
      return std::move(*status_or_timestamp);
    }
    default:
      return value_factory.CreateErrorValue(
          base_internal::TypeConversionError(*this->type(), *type));
  }
}

absl::StatusOr<Handle<Value>> IntValue::Equals(ValueFactory& value_factory,
                                               const Value& other) const {
  switch (other.kind()) {
    case ValueKind::kInt:
      return value_factory.CreateBoolValue(
          internal::Number::FromInt64(NativeValue()) ==
          internal::Number::FromInt64(other.As<IntValue>().NativeValue()));
    case ValueKind::kUint:
      return value_factory.CreateBoolValue(
          internal::Number::FromInt64(NativeValue()) ==
          internal::Number::FromUint64(other.As<UintValue>().NativeValue()));
    case ValueKind::kDouble:
      return value_factory.CreateBoolValue(
          internal::Number::FromInt64(NativeValue()) ==
          internal::Number::FromDouble(other.As<DoubleValue>().NativeValue()));
    default:
      return value_factory.CreateBoolValue(false);
  }
}

}  // namespace cel
