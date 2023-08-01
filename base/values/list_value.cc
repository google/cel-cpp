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
#include <string>
#include <utility>

#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/status/statusor.h"
#include "base/handle.h"
#include "base/internal/data.h"
#include "base/type.h"
#include "base/types/list_type.h"
#include "base/value_factory.h"
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

size_t ListValue::size() const {
  return CEL_INTERNAL_LIST_VALUE_DISPATCH(size);
}

bool ListValue::empty() const {
  return CEL_INTERNAL_LIST_VALUE_DISPATCH(empty);
}

absl::StatusOr<Handle<Value>> ListValue::Get(const GetContext& context,
                                             size_t index) const {
  return CEL_INTERNAL_LIST_VALUE_DISPATCH(Get, context, index);
}

absl::StatusOr<UniqueRef<ListValue::Iterator>> ListValue::NewIterator(
    MemoryManager& memory_manager) const {
  return CEL_INTERNAL_LIST_VALUE_DISPATCH(NewIterator, memory_manager);
}

internal::TypeInfo ListValue::TypeId() const {
  return CEL_INTERNAL_LIST_VALUE_DISPATCH(TypeId);
}

#undef CEL_INTERNAL_LIST_VALUE_DISPATCH

absl::StatusOr<size_t> ListValue::Iterator::NextIndex(
    const ListValue::GetContext& context) {
  CEL_ASSIGN_OR_RETURN(auto element, Next(context));
  return element.index;
}

absl::StatusOr<Handle<Value>> ListValue::Iterator::NextValue(
    const ListValue::GetContext& context) {
  CEL_ASSIGN_OR_RETURN(auto element, Next(context));
  return std::move(element.value);
}

namespace base_internal {

namespace {

class LegacyListValueIterator final : public ListValue::Iterator {
 public:
  explicit LegacyListValueIterator(uintptr_t impl)
      : impl_(impl), size_(LegacyListValueSize(impl_)) {}

  bool HasNext() override { return index_ < size_; }

  absl::StatusOr<Element> Next(const ListValue::GetContext& context) override {
    if (ABSL_PREDICT_FALSE(index_ >= size_)) {
      return absl::FailedPreconditionError(
          "ListValue::Iterator::Next() called when "
          "ListValue::Iterator::HasNext() returns false");
    }
    CEL_ASSIGN_OR_RETURN(
        auto value, LegacyListValueGet(impl_, context.value_factory(), index_));
    return Element(index_++, std::move(value));
  }

  absl::StatusOr<size_t> NextIndex(
      const ListValue::GetContext& context) override {
    if (ABSL_PREDICT_FALSE(index_ >= size_)) {
      return absl::FailedPreconditionError(
          "ListValue::Iterator::Next() called when "
          "ListValue::Iterator::HasNext() returns false");
    }
    return index_++;
  }

 private:
  const uintptr_t impl_;
  const size_t size_;
  size_t index_ = 0;
};

class AbstractListValueIterator final : public ListValue::Iterator {
 public:
  explicit AbstractListValueIterator(const AbstractListValue* value)
      : value_(value), size_(value_->size()) {}

  bool HasNext() override { return index_ < size_; }

  absl::StatusOr<Element> Next(const ListValue::GetContext& context) override {
    if (ABSL_PREDICT_FALSE(index_ >= size_)) {
      return absl::FailedPreconditionError(
          "ListValue::Iterator::Next() called when "
          "ListValue::Iterator::HasNext() returns false");
    }
    CEL_ASSIGN_OR_RETURN(auto value, value_->Get(context, index_));
    return Element(index_++, std::move(value));
  }

  absl::StatusOr<size_t> NextIndex(
      const ListValue::GetContext& context) override {
    if (ABSL_PREDICT_FALSE(index_ >= size_)) {
      return absl::FailedPreconditionError(
          "ListValue::Iterator::Next() called when "
          "ListValue::Iterator::HasNext() returns false");
    }
    return index_++;
  }

 private:
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
  CEL_ASSIGN_OR_RETURN(auto iterator,
                       value.NewIterator(value_factory.memory_manager()));
  while (iterator->HasNext()) {
    CEL_ASSIGN_OR_RETURN(
        auto element,
        iterator->NextValue(ListValue::GetContext(value_factory)));
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
    MemoryManager& memory_manager) const {
  return MakeUnique<LegacyListValueIterator>(memory_manager, impl_);
}

absl::StatusOr<Handle<Value>> LegacyListValue::Get(const GetContext& context,
                                                   size_t index) const {
  return LegacyListValueGet(impl_, context.value_factory(), index);
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
    MemoryManager& memory_manager) const {
  return MakeUnique<AbstractListValueIterator>(memory_manager, this);
}

}  // namespace base_internal

}  // namespace cel
