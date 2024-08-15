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

#include "common/values/legacy_map_value.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/call_once.h"
#include "absl/base/nullability.h"
#include "absl/log/die_if_null.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/type.h"
#include "common/value_manager.h"
#include "common/values/map_value_interface.h"
#include "common/values/values.h"
#include "internal/dynamic_loader.h"  // IWYU pragma: keep
#include "internal/status_macros.h"

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type-c-linkage"
#endif

namespace cel::common_internal {

namespace {

using LegacyMapValue_GetType = MapType (*)(uintptr_t, TypeManager&);
using LegacyMapValue_DebugString = std::string (*)(uintptr_t);
using LegacyMapValue_GetSerializedSize = absl::StatusOr<size_t> (*)(uintptr_t);
using LegacyMapValue_SerializeTo = absl::Status (*)(uintptr_t, absl::Cord&);
using LegacyMapValue_ConvertToJsonObject =
    absl::StatusOr<JsonObject> (*)(uintptr_t);
using LegacyMapValue_IsEmpty = bool (*)(uintptr_t);
using LegacyMapValue_Size = size_t (*)(uintptr_t);
using LegacyMapValue_Get = absl::Status (*)(uintptr_t, ValueManager&,
                                            const Value&, Value&);
using LegacyMapValue_Find = absl::StatusOr<bool> (*)(uintptr_t, ValueManager&,
                                                     const Value&, Value&);
using LegacyMapValue_Has = absl::Status (*)(uintptr_t, ValueManager&,
                                            const Value&, Value&);
using LegacyMapValue_ListKeys = absl::Status (*)(uintptr_t, ValueManager&,
                                                 ListValue&);
using LegacyMapValue_ForEach =
    absl::Status (*)(uintptr_t, ValueManager&, LegacyMapValue::ForEachCallback);
using LegacyMapValue_NewIterator =
    absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> (*)(uintptr_t,
                                                        ValueManager&);

ABSL_CONST_INIT struct {
  absl::once_flag init_once;
  LegacyMapValue_GetType get_type = nullptr;
  LegacyMapValue_DebugString debug_string = nullptr;
  LegacyMapValue_GetSerializedSize get_serialized_size = nullptr;
  LegacyMapValue_SerializeTo serialize_to = nullptr;
  LegacyMapValue_ConvertToJsonObject convert_to_json_object = nullptr;
  LegacyMapValue_IsEmpty is_empty = nullptr;
  LegacyMapValue_Size size = nullptr;
  LegacyMapValue_Get get = nullptr;
  LegacyMapValue_Find find = nullptr;
  LegacyMapValue_Has has = nullptr;
  LegacyMapValue_ListKeys list_keys = nullptr;
  LegacyMapValue_ForEach for_each = nullptr;
  LegacyMapValue_NewIterator new_iterator = nullptr;
} legacy_map_value_vtable;

#if ABSL_HAVE_ATTRIBUTE_WEAK
extern "C" ABSL_ATTRIBUTE_WEAK MapType
cel_common_internal_LegacyMapValue_GetType(uintptr_t impl,
                                           TypeManager& type_manager);
extern "C" ABSL_ATTRIBUTE_WEAK std::string
cel_common_internal_LegacyMapValue_DebugString(uintptr_t impl);
extern "C" ABSL_ATTRIBUTE_WEAK absl::StatusOr<size_t>
cel_common_internal_LegacyMapValue_GetSerializedSize(uintptr_t impl);
extern "C" ABSL_ATTRIBUTE_WEAK absl::Status
cel_common_internal_LegacyMapValue_SerializeTo(uintptr_t impl,
                                               absl::Cord& serialized_value);
extern "C" ABSL_ATTRIBUTE_WEAK absl::StatusOr<JsonObject>
cel_common_internal_LegacyMapValue_ConvertToJsonObject(uintptr_t impl);
extern "C" ABSL_ATTRIBUTE_WEAK bool cel_common_internal_LegacyMapValue_IsEmpty(
    uintptr_t impl);
extern "C" ABSL_ATTRIBUTE_WEAK size_t
cel_common_internal_LegacyMapValue_Size(uintptr_t impl);
extern "C" ABSL_ATTRIBUTE_WEAK absl::StatusOr<bool>
cel_common_internal_LegacyMapValue_Find(uintptr_t impl,
                                        ValueManager& value_manager,
                                        const Value& key, Value& result);
extern "C" ABSL_ATTRIBUTE_WEAK absl::Status
cel_common_internal_LegacyMapValue_Get(uintptr_t impl,
                                       ValueManager& value_manager,
                                       const Value& key, Value& result);
extern "C" ABSL_ATTRIBUTE_WEAK absl::Status
cel_common_internal_LegacyMapValue_Has(uintptr_t impl,
                                       ValueManager& value_manager,
                                       const Value& key, Value& result);
extern "C" ABSL_ATTRIBUTE_WEAK absl::Status
cel_common_internal_LegacyMapValue_ListKeys(uintptr_t impl,
                                            ValueManager& value_manager,
                                            ListValue& result);
extern "C" ABSL_ATTRIBUTE_WEAK absl::Status
cel_common_internal_LegacyMapValue_ForEach(uintptr_t impl,
                                           ValueManager& value_manager,
                                           MapValue::ForEachCallback callback);
extern "C" ABSL_ATTRIBUTE_WEAK absl::StatusOr<absl::Nonnull<ValueIteratorPtr>>
cel_common_internal_LegacyMapValue_NewIterator(uintptr_t impl,
                                               ValueManager& value_manager);
#endif

void InitializeLegacyMapValue() {
  absl::call_once(legacy_map_value_vtable.init_once, []() -> void {
#if ABSL_HAVE_ATTRIBUTE_WEAK
    legacy_map_value_vtable.get_type = ABSL_DIE_IF_NULL(  // Crash OK
        cel_common_internal_LegacyMapValue_GetType);
    legacy_map_value_vtable.debug_string = ABSL_DIE_IF_NULL(  // Crash OK
        cel_common_internal_LegacyMapValue_DebugString);
    legacy_map_value_vtable.get_serialized_size = ABSL_DIE_IF_NULL(  // Crash OK
        cel_common_internal_LegacyMapValue_GetSerializedSize);
    legacy_map_value_vtable.serialize_to = ABSL_DIE_IF_NULL(  // Crash OK
        cel_common_internal_LegacyMapValue_SerializeTo);
    legacy_map_value_vtable.convert_to_json_object =
        ABSL_DIE_IF_NULL(  // Crash OK
            cel_common_internal_LegacyMapValue_ConvertToJsonObject);
    legacy_map_value_vtable.is_empty = ABSL_DIE_IF_NULL(  // Crash OK
        cel_common_internal_LegacyMapValue_IsEmpty);
    legacy_map_value_vtable.size =
        ABSL_DIE_IF_NULL(cel_common_internal_LegacyMapValue_Size);  // Crash OK
    legacy_map_value_vtable.get =
        ABSL_DIE_IF_NULL(cel_common_internal_LegacyMapValue_Get);  // Crash OK
    legacy_map_value_vtable.find =
        ABSL_DIE_IF_NULL(cel_common_internal_LegacyMapValue_Find);  // Crash OK
    legacy_map_value_vtable.has =
        ABSL_DIE_IF_NULL(cel_common_internal_LegacyMapValue_Has);  // Crash OK
    legacy_map_value_vtable.list_keys = ABSL_DIE_IF_NULL(          // Crash OK
        cel_common_internal_LegacyMapValue_ListKeys);
    legacy_map_value_vtable.for_each = ABSL_DIE_IF_NULL(  // Crash OK
        cel_common_internal_LegacyMapValue_ForEach);
    legacy_map_value_vtable.new_iterator = ABSL_DIE_IF_NULL(  // Crash OK
        cel_common_internal_LegacyMapValue_NewIterator);
#else
    internal::DynamicLoader dynamic_loader;
    legacy_map_value_vtable.get_type = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyMapValue_GetType");
    legacy_map_value_vtable.debug_string = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyMapValue_DebugString");
    legacy_map_value_vtable.get_serialized_size =
        dynamic_loader.FindSymbolOrDie(
            "cel_common_internal_LegacyMapValue_GetSerializedSize");
    legacy_map_value_vtable.serialize_to = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyMapValue_SerializeTo");
    legacy_map_value_vtable.convert_to_json_object =
        dynamic_loader.FindSymbolOrDie(
            "cel_common_internal_LegacyMapValue_ConvertToJsonObject");
    legacy_map_value_vtable.is_empty = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyMapValue_IsEmpty");
    legacy_map_value_vtable.size = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyMapValue_Size");
    legacy_map_value_vtable.get = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyMapValue_Get");
    legacy_map_value_vtable.find = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyMapValue_Find");
    legacy_map_value_vtable.has = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyMapValue_Has");
    legacy_map_value_vtable.list_keys = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyMapValue_ListKeys");
    legacy_map_value_vtable.for_each = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyMapValue_ForEach");
    legacy_map_value_vtable.new_iterator = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyMapValue_NewIterator");
#endif
  });
}

}  // namespace

std::string LegacyMapValue::DebugString() const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.debug_string)(impl_);
}

absl::Status LegacyMapValue::SerializeTo(AnyToJsonConverter&,
                                         absl::Cord& value) const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.serialize_to)(impl_, value);
}

absl::StatusOr<JsonObject> LegacyMapValue::ConvertToJsonObject(
    AnyToJsonConverter&) const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.convert_to_json_object)(impl_);
}

bool LegacyMapValue::IsEmpty() const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.is_empty)(impl_);
}

size_t LegacyMapValue::Size() const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.size)(impl_);
}

absl::Status LegacyMapValue::Get(ValueManager& value_manager, const Value& key,
                                 Value& result) const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.get)(impl_, value_manager, key, result);
}

absl::StatusOr<bool> LegacyMapValue::Find(ValueManager& value_manager,
                                          const Value& key,
                                          Value& result) const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.find)(impl_, value_manager, key, result);
}

absl::Status LegacyMapValue::Has(ValueManager& value_manager, const Value& key,
                                 Value& result) const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.has)(impl_, value_manager, key, result);
}

absl::Status LegacyMapValue::ListKeys(ValueManager& value_manager,
                                      ListValue& result) const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.list_keys)(impl_, value_manager, result);
}

absl::Status LegacyMapValue::ForEach(ValueManager& value_manager,
                                     ForEachCallback callback) const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.for_each)(impl_, value_manager, callback);
}

absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> LegacyMapValue::NewIterator(
    ValueManager& value_manager) const {
  InitializeLegacyMapValue();
  return (*legacy_map_value_vtable.new_iterator)(impl_, value_manager);
}

absl::Status LegacyMapValue::Equal(ValueManager& value_manager,
                                   const Value& other, Value& result) const {
  if (auto map_value = As<MapValue>(other); map_value.has_value()) {
    return MapValueEqual(value_manager, *this, *map_value, result);
  }
  result = BoolValue{false};
  return absl::OkStatus();
}

}  // namespace cel::common_internal

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
