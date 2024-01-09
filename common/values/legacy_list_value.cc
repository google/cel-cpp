// Copyright 2023 Google LLC
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

#include "common/values/legacy_list_value.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/call_once.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "common/json.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "internal/dynamic_loader.h"
#include "internal/status_macros.h"

namespace cel::common_internal {

namespace {

using LegacyListValue_DebugString = std::string (*)(uintptr_t);
using LegacyListValue_GetSerializedSize = absl::StatusOr<size_t> (*)(uintptr_t);
using LegacyListValue_SerializeTo = absl::Status (*)(uintptr_t, absl::Cord&);
using LegacyListValue_ConvertToJsonArray =
    absl::StatusOr<JsonArray> (*)(uintptr_t);
using LegacyListValue_IsEmpty = bool (*)(uintptr_t);
using LegacyListValue_Size = size_t (*)(uintptr_t);
using LegacyListValue_Get = absl::StatusOr<ValueView> (*)(uintptr_t,
                                                          ValueManager&, size_t,
                                                          Value&);
using LegacyListValue_ForEach = absl::Status (*)(
    uintptr_t, ValueManager&, LegacyListValue::ForEachCallback);
using LegacyListValue_NewIterator =
    absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> (*)(uintptr_t,
                                                        ValueManager&);

ABSL_CONST_INIT struct {
  absl::once_flag init_once;
  LegacyListValue_DebugString debug_string = nullptr;
  LegacyListValue_GetSerializedSize get_serialized_size = nullptr;
  LegacyListValue_SerializeTo serialize_to = nullptr;
  LegacyListValue_ConvertToJsonArray convert_to_json_array = nullptr;
  LegacyListValue_IsEmpty is_empty = nullptr;
  LegacyListValue_Size size = nullptr;
  LegacyListValue_Get get = nullptr;
  LegacyListValue_ForEach for_each = nullptr;
  LegacyListValue_NewIterator new_iterator = nullptr;
} legacy_list_value_vtable;

void InitializeLegacyListValue() {
  absl::call_once(legacy_list_value_vtable.init_once, []() -> void {
    internal::DynamicLoader dynamic_loader;
    legacy_list_value_vtable.debug_string = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyListValue_DebugString");
    legacy_list_value_vtable.get_serialized_size =
        dynamic_loader.FindSymbolOrDie(
            "cel_common_internal_LegacyListValue_GetSerializedSize");
    legacy_list_value_vtable.serialize_to = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyListValue_SerializeTo");
    legacy_list_value_vtable.convert_to_json_array =
        dynamic_loader.FindSymbolOrDie(
            "cel_common_internal_LegacyListValue_ConvertToJsonArray");
    legacy_list_value_vtable.is_empty = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyListValue_IsEmpty");
    legacy_list_value_vtable.size = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyListValue_Size");
    legacy_list_value_vtable.get = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyListValue_Get");
    legacy_list_value_vtable.for_each = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyListValue_ForEach");
    legacy_list_value_vtable.new_iterator = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyListValue_NewIterator");
  });
}

}  // namespace

ListType LegacyListValue::GetType(TypeManager& type_manager) const {
  return ListType(type_manager.GetDynListType());
}

std::string LegacyListValue::DebugString() const {
  InitializeLegacyListValue();
  return (*legacy_list_value_vtable.debug_string)(impl_);
}

// See `ValueInterface::GetSerializedSize`.
absl::StatusOr<size_t> LegacyListValue::GetSerializedSize() const {
  InitializeLegacyListValue();
  return (*legacy_list_value_vtable.get_serialized_size)(impl_);
}

// See `ValueInterface::SerializeTo`.
absl::Status LegacyListValue::SerializeTo(absl::Cord& value) const {
  InitializeLegacyListValue();
  return (*legacy_list_value_vtable.serialize_to)(impl_, value);
}

// See `ValueInterface::Serialize`.
absl::StatusOr<absl::Cord> LegacyListValue::Serialize() const {
  absl::Cord serialized_value;
  CEL_RETURN_IF_ERROR(SerializeTo(serialized_value));
  return serialized_value;
}

// See `ValueInterface::GetTypeUrl`.
absl::StatusOr<std::string> LegacyListValue::GetTypeUrl(
    absl::string_view prefix) const {
  return MakeTypeUrlWithPrefix(prefix, "google.protobuf.ListValue");
}

// See `ValueInterface::ConvertToAny`.
absl::StatusOr<Any> LegacyListValue::ConvertToAny(
    absl::string_view prefix) const {
  CEL_ASSIGN_OR_RETURN(auto value, Serialize());
  CEL_ASSIGN_OR_RETURN(auto type_url, GetTypeUrl(prefix));
  return MakeAny(std::move(type_url), std::move(value));
}

absl::StatusOr<JsonArray> LegacyListValue::ConvertToJsonArray() const {
  InitializeLegacyListValue();
  return (*legacy_list_value_vtable.convert_to_json_array)(impl_);
}

bool LegacyListValue::IsEmpty() const {
  InitializeLegacyListValue();
  return (*legacy_list_value_vtable.is_empty)(impl_);
}

size_t LegacyListValue::Size() const {
  InitializeLegacyListValue();
  return (*legacy_list_value_vtable.size)(impl_);
}

// See LegacyListValueInterface::Get for documentation.
absl::StatusOr<ValueView> LegacyListValue::Get(ValueManager& value_manager,
                                               size_t index,
                                               Value& scratch) const {
  InitializeLegacyListValue();
  return (*legacy_list_value_vtable.get)(impl_, value_manager, index, scratch);
}

absl::Status LegacyListValue::ForEach(ValueManager& value_manager,
                                      ForEachCallback callback) const {
  InitializeLegacyListValue();
  return (*legacy_list_value_vtable.for_each)(impl_, value_manager, callback);
}

absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> LegacyListValue::NewIterator(
    ValueManager& value_manager) const {
  InitializeLegacyListValue();
  return (*legacy_list_value_vtable.new_iterator)(impl_, value_manager);
}

ListType LegacyListValueView::GetType(TypeManager& type_manager) const {
  return ListType(type_manager.GetDynListType());
}

std::string LegacyListValueView::DebugString() const {
  InitializeLegacyListValue();
  return (*legacy_list_value_vtable.debug_string)(impl_);
}

// See `ValueInterface::GetSerializedSize`.
absl::StatusOr<size_t> LegacyListValueView::GetSerializedSize() const {
  InitializeLegacyListValue();
  return (*legacy_list_value_vtable.get_serialized_size)(impl_);
}

// See `ValueInterface::SerializeTo`.
absl::Status LegacyListValueView::SerializeTo(absl::Cord& value) const {
  InitializeLegacyListValue();
  return (*legacy_list_value_vtable.serialize_to)(impl_, value);
}

// See `ValueInterface::Serialize`.
absl::StatusOr<absl::Cord> LegacyListValueView::Serialize() const {
  absl::Cord serialized_value;
  CEL_RETURN_IF_ERROR(SerializeTo(serialized_value));
  return serialized_value;
}

// See `ValueInterface::GetTypeUrl`.
absl::StatusOr<std::string> LegacyListValueView::GetTypeUrl(
    absl::string_view prefix) const {
  return MakeTypeUrlWithPrefix(prefix, "google.protobuf.ListValue");
}

// See `ValueInterface::ConvertToAny`.
absl::StatusOr<Any> LegacyListValueView::ConvertToAny(
    absl::string_view prefix) const {
  CEL_ASSIGN_OR_RETURN(auto value, Serialize());
  CEL_ASSIGN_OR_RETURN(auto type_url, GetTypeUrl(prefix));
  return MakeAny(std::move(type_url), std::move(value));
}

absl::StatusOr<JsonArray> LegacyListValueView::ConvertToJsonArray() const {
  InitializeLegacyListValue();
  return (*legacy_list_value_vtable.convert_to_json_array)(impl_);
}

bool LegacyListValueView::IsEmpty() const {
  InitializeLegacyListValue();
  return (*legacy_list_value_vtable.is_empty)(impl_);
}

size_t LegacyListValueView::Size() const {
  InitializeLegacyListValue();
  return (*legacy_list_value_vtable.size)(impl_);
}

// See LegacyListValueInterface::Get for documentation.
absl::StatusOr<ValueView> LegacyListValueView::Get(ValueManager& value_manager,
                                                   size_t index,
                                                   Value& scratch) const {
  InitializeLegacyListValue();
  return (*legacy_list_value_vtable.get)(impl_, value_manager, index, scratch);
}

absl::Status LegacyListValueView::ForEach(ValueManager& value_manager,
                                          ForEachCallback callback) const {
  InitializeLegacyListValue();
  return (*legacy_list_value_vtable.for_each)(impl_, value_manager, callback);
}

absl::StatusOr<absl::Nonnull<ValueIteratorPtr>>
LegacyListValueView::NewIterator(ValueManager& value_manager) const {
  InitializeLegacyListValue();
  return (*legacy_list_value_vtable.new_iterator)(impl_, value_manager);
}

}  // namespace cel::common_internal
