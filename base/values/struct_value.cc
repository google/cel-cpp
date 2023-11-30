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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/macros.h"
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "base/handle.h"
#include "base/internal/data.h"
#include "base/internal/message_wrapper.h"
#include "base/kind.h"
#include "base/memory.h"
#include "base/type.h"
#include "base/types/struct_type.h"
#include "base/types/wrapper_type.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "base/values/bool_value.h"
#include "base/values/null_value.h"
#include "common/any.h"
#include "common/json.h"
#include "common/native_type.h"
#include "internal/status_macros.h"

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

size_t StructValue::field_count() const {
  return CEL_INTERNAL_STRUCT_VALUE_DISPATCH(field_count);
}

std::string StructValue::DebugString() const {
  return CEL_INTERNAL_STRUCT_VALUE_DISPATCH(DebugString);
}

absl::StatusOr<Any> StructValue::ConvertToAny(
    ValueFactory& value_factory) const {
  return CEL_INTERNAL_STRUCT_VALUE_DISPATCH(ConvertToAny, value_factory);
}

absl::StatusOr<Json> StructValue::ConvertToJson(
    ValueFactory& value_factory) const {
  return CEL_INTERNAL_STRUCT_VALUE_DISPATCH(ConvertToJson, value_factory);
}

absl::StatusOr<Handle<Value>> StructValue::ConvertToType(
    ValueFactory& value_factory, const Handle<Type>& type) const {
  switch (type->kind()) {
    case TypeKind::kStruct:
      if (this->type()->name() != type->name()) {
        break;
      }
      return handle_from_this();
    case TypeKind::kType:
      return value_factory.CreateTypeValue(this->type());
    default:
      break;
  }
  return value_factory.CreateErrorValue(
      base_internal::TypeConversionError(*this->type(), *type));
}

absl::StatusOr<Handle<Value>> StructValue::GetFieldByName(
    ValueFactory& value_factory, absl::string_view name) const {
  return CEL_INTERNAL_STRUCT_VALUE_DISPATCH(GetFieldByName, value_factory,
                                            name);
}

absl::StatusOr<Handle<Value>> StructValue::GetFieldByNumber(
    ValueFactory& value_factory, int64_t number) const {
  return CEL_INTERNAL_STRUCT_VALUE_DISPATCH(GetFieldByNumber, value_factory,
                                            number);
}

absl::StatusOr<QualifyResult> StructValue::Qualify(
    ValueFactory& value_factory, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test) const {
  return CEL_INTERNAL_STRUCT_VALUE_DISPATCH(Qualify, value_factory, qualifiers,
                                            presence_test);
}

absl::StatusOr<bool> StructValue::HasFieldByName(TypeManager& type_manager,
                                                 absl::string_view name) const {
  return CEL_INTERNAL_STRUCT_VALUE_DISPATCH(HasFieldByName, type_manager, name);
}

absl::StatusOr<bool> StructValue::HasFieldByNumber(TypeManager& type_manager,
                                                   int64_t number) const {
  return CEL_INTERNAL_STRUCT_VALUE_DISPATCH(HasFieldByNumber, type_manager,
                                            number);
}

absl::StatusOr<absl::Nonnull<std::unique_ptr<StructValue::FieldIterator>>>
StructValue::NewFieldIterator(ValueFactory& value_factory) const {
  return CEL_INTERNAL_STRUCT_VALUE_DISPATCH(NewFieldIterator, value_factory);
}

absl::StatusOr<Handle<Value>> StructValue::Equals(ValueFactory& value_factory,
                                                  const Value& other) const {
  return CEL_INTERNAL_STRUCT_VALUE_DISPATCH(Equals, value_factory, other);
}

absl::StatusOr<Handle<Value>> StructValue::GetWrappedFieldByName(
    ValueFactory& value_factory, absl::string_view name) const {
  return CEL_INTERNAL_STRUCT_VALUE_DISPATCH(GetWrappedFieldByName,
                                            value_factory, name);
}

NativeTypeId StructValue::GetNativeTypeId() const {
  return CEL_INTERNAL_STRUCT_VALUE_DISPATCH(GetNativeTypeId);
}

#undef CEL_INTERNAL_STRUCT_VALUE_DISPATCH

struct StructValue::GetFieldVisitor final {
  const StructValue& struct_value;
  ValueFactory& value_factory;

  absl::StatusOr<Handle<Value>> operator()(absl::string_view name) const {
    return struct_value.GetFieldByName(value_factory, name);
  }

  absl::StatusOr<Handle<Value>> operator()(int64_t number) const {
    return struct_value.GetFieldByNumber(value_factory, number);
  }
};

struct StructValue::HasFieldVisitor final {
  const StructValue& struct_value;
  TypeManager& type_manager;

  absl::StatusOr<bool> operator()(absl::string_view name) const {
    return struct_value.HasFieldByName(type_manager, name);
  }

  absl::StatusOr<bool> operator()(int64_t number) const {
    return struct_value.HasFieldByNumber(type_manager, number);
  }
};

absl::StatusOr<Handle<Value>> StructValue::GetField(ValueFactory& value_factory,
                                                    FieldId field) const {
  return absl::visit(GetFieldVisitor{*this, value_factory}, field.data_);
}

absl::StatusOr<bool> StructValue::HasField(TypeManager& type_manager,
                                           FieldId field) const {
  return absl::visit(HasFieldVisitor{*this, type_manager}, field.data_);
}

absl::StatusOr<StructValue::FieldId> StructValue::FieldIterator::NextId() {
  CEL_ASSIGN_OR_RETURN(auto entry, Next());
  return entry.id;
}

absl::StatusOr<Handle<Value>> StructValue::FieldIterator::NextValue() {
  CEL_ASSIGN_OR_RETURN(auto entry, Next());
  return std::move(entry.value);
}

absl::StatusOr<Handle<Value>> GenericStructValueEquals(
    ValueFactory& value_factory, const StructValue& lhs,
    const StructValue& rhs) {
  if (&lhs == &rhs) {
    return value_factory.CreateBoolValue(true);
  }
  if (!lhs.type()->Equals(*rhs.type())) {
    return value_factory.CreateBoolValue(false);
  }
  const auto lhs_size = lhs.field_count();
  if (lhs_size != rhs.field_count()) {
    return value_factory.CreateBoolValue(false);
  }
  if (lhs_size == 0) {
    return value_factory.CreateBoolValue(true);
  }
  absl::flat_hash_set<StructValue::FieldId> field_ids;
  {
    CEL_ASSIGN_OR_RETURN(auto lhs_iterator,
                         lhs.NewFieldIterator(value_factory));
    CEL_ASSIGN_OR_RETURN(auto rhs_iterator,
                         rhs.NewFieldIterator(value_factory));
    field_ids.reserve(lhs_size);
    for (size_t index = 0; index < lhs_size; ++index) {
      if (ABSL_PREDICT_FALSE(!lhs_iterator->HasNext())) {
        return absl::InternalError(
            "StructValue::FieldIterator has less fields than StructValue");
      }
      CEL_ASSIGN_OR_RETURN(auto field_id, lhs_iterator->NextId());
      field_ids.insert(field_id);
    }
    if (ABSL_PREDICT_FALSE(lhs_iterator->HasNext())) {
      return absl::InternalError(
          "StructValue::FieldIterator has more fields than StructValue");
    }
    for (size_t index = 0; index < lhs_size; ++index) {
      if (ABSL_PREDICT_FALSE(!rhs_iterator->HasNext())) {
        return absl::InternalError(
            "StructValue::FieldIterator has less fields than StructValue");
      }
      CEL_ASSIGN_OR_RETURN(auto field_id, rhs_iterator->NextId());
      if (!field_ids.contains(field_id)) {
        return value_factory.CreateBoolValue(false);
      }
    }
    if (ABSL_PREDICT_FALSE(rhs_iterator->HasNext())) {
      return absl::InternalError(
          "StructValue::FieldIterator has more fields than StructValue");
    }
  }
  for (const auto& field_id : field_ids) {
    CEL_ASSIGN_OR_RETURN(auto lhs_field, lhs.GetField(value_factory, field_id));
    CEL_ASSIGN_OR_RETURN(auto rhs_field, rhs.GetField(value_factory, field_id));
    CEL_ASSIGN_OR_RETURN(auto equal,
                         lhs_field->Equals(value_factory, *rhs_field));
    if (equal->Is<BoolValue>() && !equal->As<BoolValue>().NativeValue()) {
      return equal;
    }
  }
  return value_factory.CreateBoolValue(true);
}

namespace base_internal {

class LegacyStructValueFieldIterator final : public StructValue::FieldIterator {
 public:
  LegacyStructValueFieldIterator(ValueFactory& value_factory, uintptr_t msg,
                                 uintptr_t type_info)
      : value_factory_(value_factory),
        msg_(msg),
        type_info_(type_info),
        field_names_(MessageValueListFields(msg_, type_info_)) {}

  bool HasNext() override { return index_ < field_names_.size(); }

  absl::StatusOr<Field> Next() override {
    if (ABSL_PREDICT_FALSE(index_ >= field_names_.size())) {
      return absl::FailedPreconditionError(
          "StructValue::FieldIterator::Next() called when "
          "StructValue::FieldIterator::HasNext() returns false");
    }
    const auto& field_name = field_names_[index_];
    CEL_ASSIGN_OR_RETURN(
        auto value, MessageValueGetFieldByName(msg_, type_info_, value_factory_,
                                               field_name, true));
    ++index_;
    return Field(LegacyStructType::MakeFieldId(field_name), std::move(value));
  }

  absl::StatusOr<StructValue::FieldId> NextId() override {
    if (ABSL_PREDICT_FALSE(index_ >= field_names_.size())) {
      return absl::FailedPreconditionError(
          "StructValue::FieldIterator::Next() called when "
          "StructValue::FieldIterator::HasNext() returns false");
    }
    return LegacyStructType::MakeFieldId(field_names_[index_++]);
  }

 private:
  ValueFactory& value_factory_;
  const uintptr_t msg_;
  const uintptr_t type_info_;
  const std::vector<absl::string_view> field_names_;
  size_t index_ = 0;
};

Handle<StructType> LegacyStructValue::type() const {
  uintptr_t tag = msg_ & kMessageWrapperTagMask;
  if (tag == kMessageWrapperTagMessageValue) {
    // google::protobuf::Message
    return HandleFactory<StructType>::Make<LegacyStructType>(msg_);
  }
  // LegacyTypeInfoApis
  ABSL_ASSERT(tag == kMessageWrapperTagTypeInfoValue);
  return HandleFactory<StructType>::Make<LegacyStructType>(type_info_);
}

size_t LegacyStructValue::field_count() const {
  return MessageValueFieldCount(msg_, type_info_);
}

std::string LegacyStructValue::DebugString() const {
  return type()->DebugString();
}

absl::StatusOr<Any> LegacyStructValue::ConvertToAny(
    ValueFactory& value_factory) const {
  return absl::UnimplementedError(
      "LegacyStructValue::ConvertToAny is not yet implemented");
}

absl::StatusOr<Json> LegacyStructValue::ConvertToJson(
    ValueFactory& value_factory) const {
  return absl::UnimplementedError(
      "LegacyStructValue::ConvertToJson is not yet implemented");
}

absl::StatusOr<Handle<Value>> LegacyStructValue::GetFieldByName(
    ValueFactory& value_factory, absl::string_view name) const {
  return MessageValueGetFieldByName(msg_, type_info_, value_factory, name,
                                    true);
}

absl::StatusOr<Handle<Value>> LegacyStructValue::GetFieldByNumber(
    ValueFactory& value_factory, int64_t number) const {
  return MessageValueGetFieldByNumber(msg_, type_info_, value_factory, number,
                                      true);
}

absl::StatusOr<QualifyResult> LegacyStructValue::Qualify(
    ValueFactory& value_factory, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test) const {
  return MessageValueQualify(msg_, type_info_, value_factory, qualifiers,
                             presence_test);
}

absl::StatusOr<bool> LegacyStructValue::HasFieldByName(
    TypeManager& type_manager, absl::string_view name) const {
  return MessageValueHasFieldByName(msg_, type_info_, name);
}

absl::StatusOr<bool> LegacyStructValue::HasFieldByNumber(
    TypeManager& type_manager, int64_t number) const {
  return MessageValueHasFieldByNumber(msg_, type_info_, number);
}

absl::StatusOr<Handle<Value>> LegacyStructValue::GetWrappedFieldByName(
    ValueFactory& value_factory, absl::string_view name) const {
  return MessageValueGetFieldByName(msg_, type_info_, value_factory, name,
                                    false);
}

absl::StatusOr<absl::Nonnull<std::unique_ptr<StructValue::FieldIterator>>>
LegacyStructValue::NewFieldIterator(ValueFactory& value_factory) const {
  return std::make_unique<LegacyStructValueFieldIterator>(value_factory, msg_,
                                                          type_info_);
}

absl::StatusOr<Handle<Value>> LegacyStructValue::Equals(
    ValueFactory& value_factory, const Value& other) const {
  if (!other.Is<StructValue>()) {
    return value_factory.CreateBoolValue(false);
  }
  if (other.Is<LegacyStructValue>()) {
    return value_factory.CreateBoolValue(
        MessageValueEquals(msg_, type_info_, other));
  }
  return GenericStructValueEquals(value_factory, *this,
                                  other.As<StructValue>());
}

AbstractStructValue::AbstractStructValue(Handle<StructType> type)
    : StructValue(), base_internal::HeapData(kKind), type_(std::move(type)) {
  // Ensure `Value*` and `base_internal::HeapData*` are not thunked.
  ABSL_ASSERT(
      reinterpret_cast<uintptr_t>(static_cast<Value*>(this)) ==
      reinterpret_cast<uintptr_t>(static_cast<base_internal::HeapData*>(this)));
}

absl::StatusOr<Handle<Value>> AbstractStructValue::GetWrappedFieldByName(
    ValueFactory& value_factory, absl::string_view name) const {
  CEL_ASSIGN_OR_RETURN(auto field, GetFieldByName(value_factory, name));
  if (field->Is<NullValue>()) {
    // Ugh. Somebody is still using the broken behavior of not unboxing wrapper
    // values. We need to synthesize it.
    CEL_ASSIGN_OR_RETURN(
        auto field_type,
        type()->FindFieldByName(value_factory.type_manager(), name));
    if (!field_type.has_value()) {
      return absl::InternalError(absl::StrCat(
          "field exists on value but not on type: ", type()->name(), name));
    }
    if ((*field_type).type->Is<WrapperType>()) {
      switch ((*field_type).type->kind()) {
        case TypeKind::kBool:
          return value_factory.CreateBoolValue(false);
        case TypeKind::kInt:
          return value_factory.CreateIntValue(0);
        case TypeKind::kUint:
          return value_factory.CreateUintValue(0);
        case TypeKind::kDouble:
          return value_factory.CreateDoubleValue(0.0);
        case TypeKind::kString:
          return value_factory.GetStringValue();
        case TypeKind::kBytes:
          return value_factory.GetBytesValue();
        default:
          // There are only 6 wrapper types.
          ABSL_UNREACHABLE();
      }
    }
  }
  return field;
}

absl::StatusOr<Any> AbstractStructValue::ConvertToAny(
    ValueFactory& value_factory) const {
  return absl::UnimplementedError(
      "AbstractStructValue::ConvertToAny is not yet implemented");
}

absl::StatusOr<Json> AbstractStructValue::ConvertToJson(
    ValueFactory& value_factory) const {
  return absl::UnimplementedError(
      "AbstractStructValue::ConvertToJson is not yet implemented");
}

absl::StatusOr<Handle<Value>> AbstractStructValue::Equals(
    ValueFactory& value_factory, const Value& other) const {
  if (!other.Is<StructValue>()) {
    return value_factory.CreateBoolValue(false);
  }
  return GenericStructValueEquals(value_factory, *this,
                                  other.As<StructValue>());
}

}  // namespace base_internal

}  // namespace cel
