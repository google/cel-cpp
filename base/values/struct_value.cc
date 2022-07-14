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

#include "base/values/struct_value.h"

#include <string>
#include <utility>

#include "absl/base/macros.h"
#include "base/types/struct_type.h"

namespace cel {

CEL_INTERNAL_VALUE_IMPL(StructValue);

StructValue::StructValue(Persistent<const StructType> type)
    : base_internal::HeapData(kKind), type_(std::move(type)) {
  // Ensure `Value*` and `base_internal::HeapData*` are not thunked.
  ABSL_ASSERT(
      reinterpret_cast<uintptr_t>(static_cast<Value*>(this)) ==
      reinterpret_cast<uintptr_t>(static_cast<base_internal::HeapData*>(this)));
}

struct StructValue::SetFieldVisitor final {
  StructValue& struct_value;
  const Persistent<const Value>& value;

  absl::Status operator()(absl::string_view name) const {
    return struct_value.SetFieldByName(name, value);
  }

  absl::Status operator()(int64_t number) const {
    return struct_value.SetFieldByNumber(number, value);
  }
};

struct StructValue::GetFieldVisitor final {
  const StructValue& struct_value;
  ValueFactory& value_factory;

  absl::StatusOr<Persistent<const Value>> operator()(
      absl::string_view name) const {
    return struct_value.GetFieldByName(value_factory, name);
  }

  absl::StatusOr<Persistent<const Value>> operator()(int64_t number) const {
    return struct_value.GetFieldByNumber(value_factory, number);
  }
};

struct StructValue::HasFieldVisitor final {
  const StructValue& struct_value;

  absl::StatusOr<bool> operator()(absl::string_view name) const {
    return struct_value.HasFieldByName(name);
  }

  absl::StatusOr<bool> operator()(int64_t number) const {
    return struct_value.HasFieldByNumber(number);
  }
};

absl::Status StructValue::SetField(FieldId field,
                                   const Persistent<const Value>& value) {
  return absl::visit(SetFieldVisitor{*this, value}, field.data_);
}

absl::StatusOr<Persistent<const Value>> StructValue::GetField(
    ValueFactory& value_factory, FieldId field) const {
  return absl::visit(GetFieldVisitor{*this, value_factory}, field.data_);
}

absl::StatusOr<bool> StructValue::HasField(FieldId field) const {
  return absl::visit(HasFieldVisitor{*this}, field.data_);
}

}  // namespace cel
