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

#include <string>
#include <utility>

#include "absl/base/macros.h"
#include "base/internal/data.h"
#include "base/type.h"
#include "base/types/list_type.h"

namespace cel {

CEL_INTERNAL_VALUE_IMPL(ListValue);

#define CEL_INTERNAL_LIST_VALUE_DISPATCH(method, ...)                       \
  base_internal::Metadata::IsStoredInline(*this)                            \
      ? static_cast<const base_internal::LegacyListValue&>(*this).method(   \
            __VA_ARGS__)                                                    \
      : static_cast<const base_internal::AbstractListValue&>(*this).method( \
            __VA_ARGS__)

Persistent<ListType> ListValue::type() const {
  return CEL_INTERNAL_LIST_VALUE_DISPATCH(type);
}

std::string ListValue::DebugString() const {
  return CEL_INTERNAL_LIST_VALUE_DISPATCH(DebugString);
}

size_t ListValue::size() const {
  return CEL_INTERNAL_LIST_VALUE_DISPATCH(size);
}

bool ListValue::empty() const {
  return CEL_INTERNAL_LIST_VALUE_DISPATCH(empty);
}

absl::StatusOr<Persistent<Value>> ListValue::Get(ValueFactory& value_factory,
                                                 size_t index) const {
  return CEL_INTERNAL_LIST_VALUE_DISPATCH(Get, value_factory, index);
}

bool ListValue::Equals(const Value& other) const {
  return CEL_INTERNAL_LIST_VALUE_DISPATCH(Equals, other);
}

void ListValue::HashValue(absl::HashState state) const {
  CEL_INTERNAL_LIST_VALUE_DISPATCH(HashValue, std::move(state));
}

internal::TypeInfo ListValue::TypeId() const {
  return CEL_INTERNAL_LIST_VALUE_DISPATCH(TypeId);
}

#undef CEL_INTERNAL_LIST_VALUE_DISPATCH

namespace base_internal {

Persistent<ListType> LegacyListValue::type() const {
  return PersistentHandleFactory<ListType>::Make<LegacyListType>();
}

std::string LegacyListValue::DebugString() const { return "list"; }

size_t LegacyListValue::size() const { return LegacyListValueSize(impl_); }

bool LegacyListValue::empty() const { return LegacyListValueEmpty(impl_); }

absl::StatusOr<Persistent<Value>> LegacyListValue::Get(
    ValueFactory& value_factory, size_t index) const {
  return LegacyListValueGet(impl_, value_factory, index);
}

bool LegacyListValue::Equals(const Value& other) const {
  // Unimplemented.
  return false;
}

void LegacyListValue::HashValue(absl::HashState state) const {
  // Unimplemented.
}

AbstractListValue::AbstractListValue(Persistent<ListType> type)
    : HeapData(kKind), type_(std::move(type)) {
  // Ensure `Value*` and `HeapData*` are not thunked.
  ABSL_ASSERT(reinterpret_cast<uintptr_t>(static_cast<Value*>(this)) ==
              reinterpret_cast<uintptr_t>(static_cast<HeapData*>(this)));
}

}  // namespace base_internal

}  // namespace cel
