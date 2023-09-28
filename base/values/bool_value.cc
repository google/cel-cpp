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

#include "base/values/bool_value.h"

#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/handle.h"
#include "base/kind.h"
#include "base/type.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "common/any.h"
#include "common/json.h"
#include "internal/proto_wire.h"
#include "internal/status_macros.h"

namespace cel {

namespace {

using internal::ProtoWireEncoder;
using internal::ProtoWireTag;
using internal::ProtoWireType;

}  // namespace

CEL_INTERNAL_VALUE_IMPL(BoolValue);

std::string BoolValue::DebugString(bool value) {
  return value ? "true" : "false";
}

std::string BoolValue::DebugString() const { return DebugString(value()); }

absl::StatusOr<Any> BoolValue::ConvertToAny(ValueFactory&) const {
  static constexpr absl::string_view kTypeName = "google.protobuf.BoolValue";
  const auto value = this->value();
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

absl::StatusOr<Json> BoolValue::ConvertToJson(ValueFactory&) const {
  return value();
}

absl::StatusOr<Handle<Value>> BoolValue::ConvertToType(
    ValueFactory& value_factory, const Handle<Type>& type) const {
  switch (type->kind()) {
    case TypeKind::kBool:
      return handle_from_this();
    case TypeKind::kType:
      return value_factory.CreateTypeValue(this->type());
    case TypeKind::kString:
      return value_factory.CreateStringValue(value() ? "true" : "false");
    default:
      return value_factory.CreateErrorValue(
          absl::InvalidArgumentError(absl::StrCat(
              "type conversion error from '", this->type()->DebugString(),
              "' to '", type->DebugString(), "'")));
  }
}

absl::StatusOr<Handle<Value>> BoolValue::Equals(ValueFactory& value_factory,
                                                const Value& other) const {
  return value_factory.CreateBoolValue(
      other.Is<BoolValue>() && value() == other.As<BoolValue>().value());
}

}  // namespace cel
