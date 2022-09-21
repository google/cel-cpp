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
#include "absl/status/status.h"
#include "base/internal/data.h"
#include "base/types/struct_type.h"

namespace cel {

CEL_INTERNAL_VALUE_IMPL(StructValue);

Persistent<StructType> StructValue::type() const {
  if (base_internal::Metadata::IsStoredInline(*this)) {
    return static_cast<const base_internal::LegacyStructValue*>(this)->type();
  }
  return static_cast<const base_internal::AbstractStructValue*>(this)->type();
}

std::string StructValue::DebugString() const {
  if (base_internal::Metadata::IsStoredInline(*this)) {
    return static_cast<const base_internal::LegacyStructValue*>(this)
        ->DebugString();
  }
  return static_cast<const base_internal::AbstractStructValue*>(this)
      ->DebugString();
}

void StructValue::HashValue(absl::HashState state) const {
  if (base_internal::Metadata::IsStoredInline(*this)) {
    static_cast<const base_internal::LegacyStructValue*>(this)->HashValue(
        std::move(state));
    return;
  }
  static_cast<const base_internal::AbstractStructValue*>(this)->HashValue(
      std::move(state));
}

bool StructValue::Equals(const Value& other) const {
  if (base_internal::Metadata::IsStoredInline(*this)) {
    return static_cast<const base_internal::LegacyStructValue*>(this)->Equals(
        other);
  }
  return static_cast<const base_internal::AbstractStructValue*>(this)->Equals(
      other);
}

absl::StatusOr<Persistent<Value>> StructValue::GetFieldByName(
    ValueFactory& value_factory, absl::string_view name) const {
  if (base_internal::Metadata::IsStoredInline(*this)) {
    return static_cast<const base_internal::LegacyStructValue*>(this)
        ->GetFieldByName(value_factory, name);
  }
  return static_cast<const base_internal::AbstractStructValue*>(this)
      ->GetFieldByName(value_factory, name);
}

absl::StatusOr<Persistent<Value>> StructValue::GetFieldByNumber(
    ValueFactory& value_factory, int64_t number) const {
  if (base_internal::Metadata::IsStoredInline(*this)) {
    return static_cast<const base_internal::LegacyStructValue*>(this)
        ->GetFieldByNumber(value_factory, number);
  }
  return static_cast<const base_internal::AbstractStructValue*>(this)
      ->GetFieldByNumber(value_factory, number);
}

absl::StatusOr<bool> StructValue::HasFieldByName(absl::string_view name) const {
  if (base_internal::Metadata::IsStoredInline(*this)) {
    return static_cast<const base_internal::LegacyStructValue*>(this)
        ->HasFieldByName(name);
  }
  return static_cast<const base_internal::AbstractStructValue*>(this)
      ->HasFieldByName(name);
}

absl::StatusOr<bool> StructValue::HasFieldByNumber(int64_t number) const {
  if (base_internal::Metadata::IsStoredInline(*this)) {
    return static_cast<const base_internal::LegacyStructValue*>(this)
        ->HasFieldByNumber(number);
  }
  return static_cast<const base_internal::AbstractStructValue*>(this)
      ->HasFieldByNumber(number);
}

internal::TypeInfo StructValue::TypeId() const {
  if (base_internal::Metadata::IsStoredInline(*this)) {
    return static_cast<const base_internal::LegacyStructValue*>(this)->TypeId();
  }
  return static_cast<const base_internal::AbstractStructValue*>(this)->TypeId();
}

struct StructValue::GetFieldVisitor final {
  const StructValue& struct_value;
  ValueFactory& value_factory;

  absl::StatusOr<Persistent<Value>> operator()(absl::string_view name) const {
    return struct_value.GetFieldByName(value_factory, name);
  }

  absl::StatusOr<Persistent<Value>> operator()(int64_t number) const {
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

absl::StatusOr<Persistent<Value>> StructValue::GetField(
    ValueFactory& value_factory, FieldId field) const {
  return absl::visit(GetFieldVisitor{*this, value_factory}, field.data_);
}

absl::StatusOr<bool> StructValue::HasField(FieldId field) const {
  return absl::visit(HasFieldVisitor{*this}, field.data_);
}

namespace base_internal {

Persistent<StructType> LegacyStructValue::type() const {
  return PersistentHandleFactory<StructType>::Make<LegacyStructType>(msg_);
}

std::string LegacyStructValue::DebugString() const {
  return type()->DebugString();
}

void LegacyStructValue::HashValue(absl::HashState state) const {
  MessageValueHash(msg_, type_info_, std::move(state));
}

bool LegacyStructValue::Equals(const Value& other) const {
  return MessageValueEquals(msg_, type_info_, other);
}

absl::StatusOr<Persistent<Value>> LegacyStructValue::GetFieldByName(
    ValueFactory& value_factory, absl::string_view name) const {
  return MessageValueGetFieldByName(msg_, type_info_, value_factory, name);
}

absl::StatusOr<Persistent<Value>> LegacyStructValue::GetFieldByNumber(
    ValueFactory& value_factory, int64_t number) const {
  return MessageValueGetFieldByNumber(msg_, type_info_, value_factory, number);
}

absl::StatusOr<bool> LegacyStructValue::HasFieldByName(
    absl::string_view name) const {
  return MessageValueHasFieldByName(msg_, type_info_, name);
}

absl::StatusOr<bool> LegacyStructValue::HasFieldByNumber(int64_t number) const {
  return MessageValueHasFieldByNumber(msg_, type_info_, number);
}

AbstractStructValue::AbstractStructValue(Persistent<StructType> type)
    : StructValue(), base_internal::HeapData(kKind), type_(std::move(type)) {
  // Ensure `Value*` and `base_internal::HeapData*` are not thunked.
  ABSL_ASSERT(
      reinterpret_cast<uintptr_t>(static_cast<Value*>(this)) ==
      reinterpret_cast<uintptr_t>(static_cast<base_internal::HeapData*>(this)));
}

}  // namespace base_internal

}  // namespace cel
