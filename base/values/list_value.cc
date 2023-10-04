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

#include "base/values/list_value.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/handle.h"
#include "base/internal/data.h"
#include "base/kind.h"
#include "base/memory.h"
#include "base/type.h"
#include "base/types/list_type.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "base/values/bool_value.h"
#include "common/any.h"
#include "common/json.h"
#include "internal/rtti.h"
#include "internal/status_macros.h"

namespace cel {

CEL_INTERNAL_VALUE_IMPL(ListValue);

#define CEL_INTERNAL_LIST_VALUE_DISPATCH(method, ...)                       \
  base_internal::Metadata::IsStoredInline(*this)                            \
      ? static_cast<const base_internal::LegacyListValue&>(*this).method(   \
            __VA_ARGS__)                                                    \
      : static_cast<const base_internal::AbstractListValue&>(*this).method( \
            __VA_ARGS__)

Handle<ListType> ListValue::type() const {
  return CEL_INTERNAL_LIST_VALUE_DISPATCH(type);
}

std::string ListValue::DebugString() const {
  return CEL_INTERNAL_LIST_VALUE_DISPATCH(DebugString);
}

absl::StatusOr<Any> ListValue::ConvertToAny(ValueFactory& value_factory) const {
  return CEL_INTERNAL_LIST_VALUE_DISPATCH(ConvertToAny, value_factory);
}

absl::StatusOr<Json> ListValue::ConvertToJson(
    ValueFactory& value_factory) const {
  return ConvertToJsonArray(value_factory);
}

absl::StatusOr<JsonArray> ListValue::ConvertToJsonArray(
    ValueFactory& value_factory) const {
  return CEL_INTERNAL_LIST_VALUE_DISPATCH(ConvertToJsonArray, value_factory);
}

absl::StatusOr<Handle<Value>> ListValue::ConvertToType(
    ValueFactory& value_factory, const Handle<Type>& type) const {
  switch (type->kind()) {
    case TypeKind::kList:
      // At runtime, a list is a list. In C++ we currently attempt to propagate
      // the type if we know all the values have the same type, otherwise we use
      // dyn.
      if (!Value::IsRuntimeConvertible(*this->type()->element(),
                                       *type->As<ListType>().element())) {
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

size_t ListValue::size() const {
  return CEL_INTERNAL_LIST_VALUE_DISPATCH(size);
}

bool ListValue::empty() const {
  return CEL_INTERNAL_LIST_VALUE_DISPATCH(empty);
}

absl::StatusOr<Handle<Value>> ListValue::Get(ValueFactory& value_factory,
                                             size_t index) const {
  return CEL_INTERNAL_LIST_VALUE_DISPATCH(Get, value_factory, index);
}

absl::StatusOr<UniqueRef<ListValue::Iterator>> ListValue::NewIterator(
    ValueFactory& value_factory) const {
  return CEL_INTERNAL_LIST_VALUE_DISPATCH(NewIterator, value_factory);
}

absl::StatusOr<Handle<Value>> ListValue::Equals(ValueFactory& value_factory,
                                                const Value& other) const {
  return CEL_INTERNAL_LIST_VALUE_DISPATCH(Equals, value_factory, other);
}

absl::StatusOr<Handle<Value>> ListValue::Contains(
    ValueFactory& value_factory, const Handle<Value>& other) const {
  return CEL_INTERNAL_LIST_VALUE_DISPATCH(Contains, value_factory, other);
}

absl::StatusOr<bool> ListValue::AnyOf(ValueFactory& value_factory,
                                      AnyOfCallback cb) const {
  return CEL_INTERNAL_LIST_VALUE_DISPATCH(AnyOf, value_factory, cb);
}

internal::TypeInfo ListValue::TypeId() const {
  return CEL_INTERNAL_LIST_VALUE_DISPATCH(TypeId);
}

#undef CEL_INTERNAL_LIST_VALUE_DISPATCH

namespace {

absl::StatusOr<Handle<Value>> GenericListValueEquals(
    ValueFactory& value_factory, const ListValue& lhs, const ListValue& rhs) {
  if (&lhs == &rhs) {
    return value_factory.CreateBoolValue(true);
  }
  const auto lhs_size = lhs.size();
  if (lhs_size != rhs.size()) {
    return value_factory.CreateBoolValue(false);
  }
  if (lhs_size == 0) {
    return value_factory.CreateBoolValue(true);
  }
  CEL_ASSIGN_OR_RETURN(auto lhs_iterator, lhs.NewIterator(value_factory));
  CEL_ASSIGN_OR_RETURN(auto rhs_iterator, rhs.NewIterator(value_factory));
  for (size_t index = 0; index < lhs_size; ++index) {
    if (ABSL_PREDICT_FALSE(!lhs_iterator->HasNext() ||
                           !rhs_iterator->HasNext())) {
      return absl::InternalError(
          "ListValue::Iterator has less elements than ListValue");
    }
    CEL_ASSIGN_OR_RETURN(auto lhs_element, lhs_iterator->Next());
    CEL_ASSIGN_OR_RETURN(auto rhs_element, rhs_iterator->Next());
    CEL_ASSIGN_OR_RETURN(auto equal,
                         lhs_element->Equals(value_factory, *rhs_element));
    if (equal->Is<BoolValue>() && !equal->As<BoolValue>().value()) {
      return equal;
    }
  }
  if (ABSL_PREDICT_FALSE(lhs_iterator->HasNext() || rhs_iterator->HasNext())) {
    return absl::InternalError(
        "ListValue::Iterator has more elements than ListValue");
  }
  return value_factory.CreateBoolValue(true);
}

absl::StatusOr<Handle<Value>> GenericListValueContains(
    ValueFactory& value_factory, const ListValue& haystack,
    const Value& needle) {
  CEL_ASSIGN_OR_RETURN(
      bool found,
      haystack.AnyOf(value_factory,
                     [&needle, &value_factory](
                         const Handle<Value>& element) -> absl::StatusOr<bool> {
                       CEL_ASSIGN_OR_RETURN(
                           auto equal, needle.Equals(value_factory, *element));
                       if (equal->Is<BoolValue>() &&
                           equal->As<BoolValue>().value()) {
                         return true;
                       }
                       return false;
                     }));

  return value_factory.CreateBoolValue(found);
}

}  // namespace

namespace base_internal {

namespace {

class LegacyListValueIterator final : public ListValue::Iterator {
 public:
  LegacyListValueIterator(ValueFactory& value_factory, uintptr_t impl)
      : value_factory_(value_factory),
        impl_(impl),
        size_(LegacyListValueSize(impl_)) {}

  bool HasNext() override { return index_ < size_; }

  absl::StatusOr<Handle<Value>> Next() override {
    if (ABSL_PREDICT_FALSE(index_ >= size_)) {
      return absl::FailedPreconditionError(
          "ListValue::Iterator::Next() called when "
          "ListValue::Iterator::HasNext() returns false");
    }
    return LegacyListValueGet(impl_, value_factory_, index_++);
  }

 private:
  ValueFactory& value_factory_;
  const uintptr_t impl_;
  const size_t size_;
  size_t index_ = 0;
};

class AbstractListValueIterator final : public ListValue::Iterator {
 public:
  AbstractListValueIterator(ValueFactory& value_factory,
                            const AbstractListValue* value)
      : value_factory_(value_factory), value_(value), size_(value_->size()) {}

  bool HasNext() override { return index_ < size_; }

  absl::StatusOr<Handle<Value>> Next() override {
    if (ABSL_PREDICT_FALSE(index_ >= size_)) {
      return absl::FailedPreconditionError(
          "ListValue::Iterator::Next() called when "
          "ListValue::Iterator::HasNext() returns false");
    }
    return value_->Get(value_factory_, index_++);
  }

 private:
  ValueFactory& value_factory_;
  const AbstractListValue* const value_;
  const size_t size_;
  size_t index_ = 0;
};

absl::StatusOr<Any> GenericListValueConvertToAny(const ListValue& value,
                                                 ValueFactory& value_factory) {
  CEL_ASSIGN_OR_RETURN(auto json, value.ConvertToJsonArray(value_factory));
  return JsonArrayToAny(json);
}

absl::StatusOr<JsonArray> GenericListValueConvertToJsonArray(
    const ListValue& value, ValueFactory& value_factory) {
  JsonArrayBuilder builder;
  builder.reserve(value.size());
  CEL_ASSIGN_OR_RETURN(auto iterator, value.NewIterator(value_factory));
  while (iterator->HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto element, iterator->Next());
    CEL_ASSIGN_OR_RETURN(auto element_json,
                         element->ConvertToJson(value_factory));
    builder.push_back(std::move(element_json));
  }
  return std::move(builder).Build();
}

}  // namespace

Handle<ListType> LegacyListValue::type() const {
  return HandleFactory<ListType>::Make<LegacyListType>();
}

std::string LegacyListValue::DebugString() const { return "list"; }

absl::StatusOr<Any> LegacyListValue::ConvertToAny(
    ValueFactory& value_factory) const {
  return GenericListValueConvertToAny(*this, value_factory);
}

absl::StatusOr<JsonArray> LegacyListValue::ConvertToJsonArray(
    ValueFactory& value_factory) const {
  return GenericListValueConvertToJsonArray(*this, value_factory);
}

size_t LegacyListValue::size() const { return LegacyListValueSize(impl_); }

bool LegacyListValue::empty() const { return LegacyListValueEmpty(impl_); }

absl::StatusOr<UniqueRef<ListValue::Iterator>> LegacyListValue::NewIterator(
    ValueFactory& value_factory) const {
  return MakeUnique<LegacyListValueIterator>(value_factory.memory_manager(),
                                             value_factory, impl_);
}

absl::StatusOr<Handle<Value>> LegacyListValue::Get(ValueFactory& value_factory,
                                                   size_t index) const {
  return LegacyListValueGet(impl_, value_factory, index);
}

absl::StatusOr<Handle<Value>> LegacyListValue::Equals(
    ValueFactory& value_factory, const Value& other) const {
  if (!other.Is<ListValue>()) {
    return value_factory.CreateBoolValue(false);
  }
  return GenericListValueEquals(value_factory, *this, other.As<ListValue>());
}

absl::StatusOr<Handle<Value>> LegacyListValue::Contains(
    ValueFactory& value_factory, const Handle<Value>& other) const {
  return LegacyListValueContains(value_factory, impl_, other);
}

absl::StatusOr<bool> LegacyListValue::AnyOf(ValueFactory& value_factory,
                                            AnyOfCallback cb) const {
  return LegacyListValueAnyOf(value_factory, impl_, cb);
}

AbstractListValue::AbstractListValue(Handle<ListType> type)
    : HeapData(kKind), type_(std::move(type)) {
  // Ensure `Value*` and `HeapData*` are not thunked.
  ABSL_ASSERT(reinterpret_cast<uintptr_t>(static_cast<Value*>(this)) ==
              reinterpret_cast<uintptr_t>(static_cast<HeapData*>(this)));
}

absl::StatusOr<Any> AbstractListValue::ConvertToAny(
    ValueFactory& value_factory) const {
  return GenericListValueConvertToAny(*this, value_factory);
}

absl::StatusOr<JsonArray> AbstractListValue::ConvertToJsonArray(
    ValueFactory& value_factory) const {
  return GenericListValueConvertToJsonArray(*this, value_factory);
}

absl::StatusOr<UniqueRef<ListValue::Iterator>> AbstractListValue::NewIterator(
    ValueFactory& value_factory) const {
  return MakeUnique<AbstractListValueIterator>(value_factory.memory_manager(),
                                               value_factory, this);
}

absl::StatusOr<Handle<Value>> AbstractListValue::Equals(
    ValueFactory& value_factory, const Value& other) const {
  if (!other.Is<ListValue>()) {
    return value_factory.CreateBoolValue(false);
  }
  return GenericListValueEquals(value_factory, *this, other.As<ListValue>());
}

absl::StatusOr<Handle<Value>> AbstractListValue::Contains(
    ValueFactory& value_factory, const Handle<Value>& other) const {
  return GenericListValueContains(value_factory, *this, *other);
}

absl::StatusOr<bool> AbstractListValue::AnyOf(ValueFactory& value_factory,
                                              AnyOfCallback cb) const {
  for (size_t i = 0; i < size(); ++i) {
    absl::StatusOr<Handle<Value>> value = Get(value_factory, i);
    auto condition = cb(*value);
    if (!condition.ok() || *condition) {
      return condition;
    }
  }
  return false;
}

absl::StatusOr<bool> DynamicListValue::AnyOf(ValueFactory& value_factory,
                                             AnyOfCallback cb) const {
  for (size_t i = 0; i < storage_.size(); i++) {
    auto condition = cb(storage_[i]);
    if (!condition.ok() || *condition) return condition;
  }
  return false;
}

}  // namespace base_internal

}  // namespace cel
