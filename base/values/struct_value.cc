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

#include <cstdint>
#include <string>
#include <utility>

#include "absl/base/macros.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "base/handle.h"
#include "base/internal/data.h"
#include "base/internal/message_wrapper.h"
#include "base/types/struct_type.h"
#include "base/value.h"
#include "internal/rtti.h"

namespace cel {

CEL_INTERNAL_VALUE_IMPL(StructValue);

#define CEL_INTERNAL_STRUCT_VALUE_DISPATCH(method, ...)                       \
  base_internal::Metadata::IsStoredInline(*this)                              \
      ? static_cast<const base_internal::LegacyStructValue&>(*this).method(   \
            __VA_ARGS__)                                                      \
      : static_cast<const base_internal::AbstractStructValue&>(*this).method( \
            __VA_ARGS__)

Handle<StructType> StructValue::type() const {
  return CEL_INTERNAL_STRUCT_VALUE_DISPATCH(type);
}

std::string StructValue::DebugString() const {
  return CEL_INTERNAL_STRUCT_VALUE_DISPATCH(DebugString);
}

absl::StatusOr<Handle<Value>> StructValue::GetFieldByName(
    const GetFieldContext& context, absl::string_view name) const {
  return CEL_INTERNAL_STRUCT_VALUE_DISPATCH(GetFieldByName, context, name);
}

absl::StatusOr<Handle<Value>> StructValue::GetFieldByNumber(
    const GetFieldContext& context, int64_t number) const {
  return CEL_INTERNAL_STRUCT_VALUE_DISPATCH(GetFieldByNumber, context, number);
}

absl::StatusOr<bool> StructValue::HasFieldByName(const HasFieldContext& context,
                                                 absl::string_view name) const {
  return CEL_INTERNAL_STRUCT_VALUE_DISPATCH(HasFieldByName, context, name);
}

absl::StatusOr<bool> StructValue::HasFieldByNumber(
    const HasFieldContext& context, int64_t number) const {
  return CEL_INTERNAL_STRUCT_VALUE_DISPATCH(HasFieldByNumber, context, number);
}

internal::TypeInfo StructValue::TypeId() const {
  return CEL_INTERNAL_STRUCT_VALUE_DISPATCH(TypeId);
}

#undef CEL_INTERNAL_STRUCT_VALUE_DISPATCH

struct StructValue::GetFieldVisitor final {
  const StructValue& struct_value;
  const GetFieldContext& context;

  absl::StatusOr<Handle<Value>> operator()(absl::string_view name) const {
    return struct_value.GetFieldByName(context, name);
  }

  absl::StatusOr<Handle<Value>> operator()(int64_t number) const {
    return struct_value.GetFieldByNumber(context, number);
  }
};

struct StructValue::HasFieldVisitor final {
  const StructValue& struct_value;
  const HasFieldContext& context;

  absl::StatusOr<bool> operator()(absl::string_view name) const {
    return struct_value.HasFieldByName(context, name);
  }

  absl::StatusOr<bool> operator()(int64_t number) const {
    return struct_value.HasFieldByNumber(context, number);
  }
};

absl::StatusOr<Handle<Value>> StructValue::GetField(
    const GetFieldContext& context, FieldId field) const {
  return absl::visit(GetFieldVisitor{*this, context}, field.data_);
}

absl::StatusOr<bool> StructValue::HasField(const HasFieldContext& context,
                                           FieldId field) const {
  return absl::visit(HasFieldVisitor{*this, context}, field.data_);
}

namespace base_internal {

Handle<StructType> LegacyStructValue::type() const {
  if ((msg_ & kMessageWrapperTagMask) == kMessageWrapperTagMask) {
    // google::protobuf::Message
    return HandleFactory<StructType>::Make<LegacyStructType>(msg_);
  }
  // LegacyTypeInfoApis
  return HandleFactory<StructType>::Make<LegacyStructType>(type_info_);
}

std::string LegacyStructValue::DebugString() const {
  return type()->DebugString();
}

absl::StatusOr<Handle<Value>> LegacyStructValue::GetFieldByName(
    const GetFieldContext& context, absl::string_view name) const {
  return MessageValueGetFieldByName(msg_, type_info_, context.value_factory(),
                                    name, context.unbox_null_wrapper_types());
}

absl::StatusOr<Handle<Value>> LegacyStructValue::GetFieldByNumber(
    const GetFieldContext& context, int64_t number) const {
  return MessageValueGetFieldByNumber(msg_, type_info_, context.value_factory(),
                                      number,
                                      context.unbox_null_wrapper_types());
}

absl::StatusOr<bool> LegacyStructValue::HasFieldByName(
    const HasFieldContext& context, absl::string_view name) const {
  return MessageValueHasFieldByName(msg_, type_info_, name);
}

absl::StatusOr<bool> LegacyStructValue::HasFieldByNumber(
    const HasFieldContext& context, int64_t number) const {
  return MessageValueHasFieldByNumber(msg_, type_info_, number);
}

AbstractStructValue::AbstractStructValue(Handle<StructType> type)
    : StructValue(), base_internal::HeapData(kKind), type_(std::move(type)) {
  // Ensure `Value*` and `base_internal::HeapData*` are not thunked.
  ABSL_ASSERT(
      reinterpret_cast<uintptr_t>(static_cast<Value*>(this)) ==
      reinterpret_cast<uintptr_t>(static_cast<base_internal::HeapData*>(this)));
}

}  // namespace base_internal

}  // namespace cel
