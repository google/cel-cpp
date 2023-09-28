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

#include "base/values/null_value.h"

#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "base/handle.h"
#include "base/kind.h"
#include "base/type.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "common/any.h"
#include "common/json.h"

namespace cel {

CEL_INTERNAL_VALUE_IMPL(NullValue);

std::string NullValue::DebugString() { return "null"; }

absl::StatusOr<Any> NullValue::ConvertToAny(ValueFactory&) const {
  return MakeAny(MakeTypeUrl("google.protobuf.Value"),
                 absl::Cord(absl::string_view("\x08\x00", 2)));
}

absl::StatusOr<Json> NullValue::ConvertToJson(ValueFactory&) const {
  return kJsonNull;
}

absl::StatusOr<Handle<Value>> NullValue::ConvertToType(
    ValueFactory& value_factory, const Handle<Type>& type) const {
  switch (type->kind()) {
    case TypeKind::kNull:
      return handle_from_this();
    case TypeKind::kType:
      return value_factory.CreateTypeValue(this->type());
    case TypeKind::kString:
      return value_factory.CreateStringValue("null");
    default:
      return value_factory.CreateErrorValue(
          base_internal::TypeConversionError(*this->type(), *type));
  }
}

absl::StatusOr<Handle<Value>> NullValue::Equals(ValueFactory& value_factory,
                                                const Value& other) const {
  return value_factory.CreateBoolValue(other.Is<NullValue>());
}

}  // namespace cel
