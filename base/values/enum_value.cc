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

#include "base/values/enum_value.h"

#include <cstdint>
#include <string>
#include <utility>

#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/handle.h"
#include "base/kind.h"
#include "base/type.h"
#include "base/types/enum_type.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "common/any.h"
#include "common/json.h"
#include "internal/proto_wire.h"
#include "internal/status_macros.h"

namespace cel {

CEL_INTERNAL_VALUE_IMPL(EnumValue);

namespace {

using internal::ProtoWireEncoder;
using internal::ProtoWireTag;
using internal::ProtoWireType;

}  // namespace

absl::string_view EnumValue::name() const {
  auto constant = type()->FindConstantByNumber(number());
  if (ABSL_PREDICT_FALSE(!constant.ok() || !constant->has_value())) {
    return absl::string_view();
  }
  return (*constant)->name;
}

std::string EnumValue::DebugString(const EnumType& type, int64_t value) {
  auto status_or_constant = type.FindConstantByNumber(value);
  if (ABSL_PREDICT_FALSE(!status_or_constant.ok() ||
                         !(*status_or_constant).has_value())) {
    return absl::StrCat(type.name(), "(", value, ")");
  }
  return DebugString(type, **status_or_constant);
}

std::string EnumValue::DebugString(const EnumType& type,
                                   const EnumType::Constant& value) {
  if (ABSL_PREDICT_FALSE(value.name.empty())) {
    return absl::StrCat(type.name(), "(", value.number, ")");
  }
  return absl::StrCat(type.name(), ".", value.name);
}

std::string EnumValue::DebugString() const {
  return DebugString(*type(), number());
}

absl::StatusOr<Any> EnumValue::ConvertToAny(ValueFactory&) const {
  static constexpr absl::string_view kTypeName = "google.protobuf.Int64Value";
  const auto value = number();
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

absl::StatusOr<Json> EnumValue::ConvertToJson(ValueFactory&) const {
  return JsonInt(number());
}

absl::StatusOr<Handle<Value>> EnumValue::ConvertToType(
    ValueFactory& value_factory, const Handle<Type>& type) const {
  switch (type->kind()) {
    case TypeKind::kEnum:
      if (this->type() != type) {
        break;
      }
      return handle_from_this();
    case TypeKind::kType:
      return value_factory.CreateTypeValue(this->type());
    case TypeKind::kInt:
      return value_factory.CreateIntValue(number());
    case TypeKind::kString:
      return value_factory.CreateStringValue(absl::StrCat(number()));
    default:
      break;
  }
  return value_factory.CreateErrorValue(absl::InvalidArgumentError(
      absl::StrCat("type conversion error from '", this->type()->DebugString(),
                   "' to '", type->DebugString(), "'")));
}

absl::StatusOr<Handle<Value>> EnumValue::Equals(ValueFactory& value_factory,
                                                const Value& other) const {
  return value_factory.CreateBoolValue(
      other.Is<EnumValue>() && type() == other.As<EnumValue>().type() &&
      number() == other.As<EnumValue>().number());
}

}  // namespace cel
