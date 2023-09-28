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

#include "base/values/type_value.h"

#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "base/handle.h"
#include "base/internal/data.h"
#include "base/kind.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "common/any.h"
#include "common/json.h"

namespace cel {

namespace {

using base_internal::InlinedTypeValueVariant;
using base_internal::Metadata;

}  // namespace

CEL_INTERNAL_VALUE_IMPL(TypeValue);

std::string TypeValue::DebugString() const { return std::string(name()); }

absl::StatusOr<Handle<Value>> TypeValue::Equals(ValueFactory& value_factory,
                                                const Value& other) const {
  return value_factory.CreateBoolValue(other.Is<TypeValue>() &&
                                       name() == other.As<TypeValue>().name());
}

absl::string_view TypeValue::name() const {
  switch (Metadata::GetInlineVariant<InlinedTypeValueVariant>(*this)) {
    case InlinedTypeValueVariant::kLegacy:
      return static_cast<const base_internal::LegacyTypeValue&>(*this).name();
    case InlinedTypeValueVariant::kModern:
      return static_cast<const base_internal::ModernTypeValue&>(*this).name();
  }
}

absl::StatusOr<Any> TypeValue::ConvertToAny(ValueFactory&) const {
  return absl::FailedPreconditionError(absl::StrCat(
      type()->name(), " cannot be serialized as google.protobuf.Any"));
}

absl::StatusOr<Json> TypeValue::ConvertToJson(ValueFactory&) const {
  return absl::FailedPreconditionError(absl::StrCat(
      type()->name(), " cannot be serialized as google.protobuf.Value"));
}

absl::StatusOr<Handle<Value>> TypeValue::ConvertToType(
    ValueFactory& value_factory, const Handle<Type>& type) const {
  switch (type->kind()) {
    case TypeKind::kType:
      return value_factory.CreateTypeValue(this->type());
    case TypeKind::kString:
      return value_factory.CreateStringValue(name());
    default:
      return value_factory.CreateErrorValue(
          base_internal::TypeConversionError(*this->type(), *type));
  }
}

}  // namespace cel
