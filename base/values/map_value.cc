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

#include "base/values/map_value.h"

#include <cstddef>
#include <string>
#include <utility>

#include "absl/base/macros.h"
#include "absl/status/statusor.h"
#include "absl/types/optional.h"
#include "base/handle.h"
#include "base/internal/data.h"
#include "base/types/map_type.h"
#include "base/value.h"
#include "base/values/list_value.h"
#include "internal/rtti.h"

namespace cel {

CEL_INTERNAL_VALUE_IMPL(MapValue);

#define CEL_INTERNAL_MAP_VALUE_DISPATCH(method, ...)                       \
  base_internal::Metadata::IsStoredInline(*this)                           \
      ? static_cast<const base_internal::LegacyMapValue&>(*this).method(   \
            __VA_ARGS__)                                                   \
      : static_cast<const base_internal::AbstractMapValue&>(*this).method( \
            __VA_ARGS__)

Handle<MapType> MapValue::type() const {
  return CEL_INTERNAL_MAP_VALUE_DISPATCH(type);
}

std::string MapValue::DebugString() const {
  return CEL_INTERNAL_MAP_VALUE_DISPATCH(DebugString);
}

size_t MapValue::size() const { return CEL_INTERNAL_MAP_VALUE_DISPATCH(size); }

bool MapValue::empty() const { return CEL_INTERNAL_MAP_VALUE_DISPATCH(empty); }

absl::StatusOr<absl::optional<Handle<Value>>> MapValue::Get(
    const GetContext& context, const Handle<Value>& key) const {
  return CEL_INTERNAL_MAP_VALUE_DISPATCH(Get, context, key);
}

absl::StatusOr<bool> MapValue::Has(const HasContext& context,
                                   const Handle<Value>& key) const {
  return CEL_INTERNAL_MAP_VALUE_DISPATCH(Has, context, key);
}

absl::StatusOr<Handle<ListValue>> MapValue::ListKeys(
    const ListKeysContext& context) const {
  return CEL_INTERNAL_MAP_VALUE_DISPATCH(ListKeys, context);
}

internal::TypeInfo MapValue::TypeId() const {
  return CEL_INTERNAL_MAP_VALUE_DISPATCH(TypeId);
}

#undef CEL_INTERNAL_MAP_VALUE_DISPATCH

namespace base_internal {

Handle<MapType> LegacyMapValue::type() const {
  return HandleFactory<MapType>::Make<LegacyMapType>();
}

std::string LegacyMapValue::DebugString() const { return "map"; }

size_t LegacyMapValue::size() const { return LegacyMapValueSize(impl_); }

bool LegacyMapValue::empty() const { return LegacyMapValueEmpty(impl_); }

absl::StatusOr<absl::optional<Handle<Value>>> LegacyMapValue::Get(
    const GetContext& context, const Handle<Value>& key) const {
  return LegacyMapValueGet(impl_, context.value_factory(), key);
}

absl::StatusOr<bool> LegacyMapValue::Has(const HasContext& context,
                                         const Handle<Value>& key) const {
  static_cast<void>(context);
  return LegacyMapValueHas(impl_, key);
}

absl::StatusOr<Handle<ListValue>> LegacyMapValue::ListKeys(
    const ListKeysContext& context) const {
  return LegacyMapValueListKeys(impl_, context.value_factory());
}

AbstractMapValue::AbstractMapValue(Handle<MapType> type)
    : HeapData(kKind), type_(std::move(type)) {
  // Ensure `Value*` and `HeapData*` are not thunked.
  ABSL_ASSERT(reinterpret_cast<uintptr_t>(static_cast<Value*>(this)) ==
              reinterpret_cast<uintptr_t>(static_cast<HeapData*>(this)));
}

}  // namespace base_internal

}  // namespace cel
