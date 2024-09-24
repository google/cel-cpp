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

#include "absl/base/attributes.h"
#include "absl/base/call_once.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/log/die_if_null.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "common/values/values.h"
#include "internal/dynamic_loader.h"  // IWYU pragma: keep

#if defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wreturn-type-c-linkage"
#endif

namespace cel::common_internal {

namespace {

using LegacyListValue_GetType = ListType (*)(uintptr_t, TypeManager&);
using LegacyListValue_DebugString = std::string (*)(uintptr_t);
using LegacyListValue_GetSerializedSize = absl::StatusOr<size_t> (*)(uintptr_t);
using LegacyListValue_SerializeTo = absl::Status (*)(uintptr_t, absl::Cord&);
using LegacyListValue_ConvertToJsonArray =
    absl::StatusOr<JsonArray> (*)(uintptr_t);
using LegacyListValue_IsEmpty = bool (*)(uintptr_t);
using LegacyListValue_Size = size_t (*)(uintptr_t);
using LegacyListValue_Get = absl::Status (*)(uintptr_t, ValueManager&, size_t,
                                             Value&);
using LegacyListValue_ForEach = absl::Status (*)(
    uintptr_t, ValueManager&, LegacyListValue::ForEachWithIndexCallback);
using LegacyListValue_NewIterator =
    absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> (*)(uintptr_t,
                                                        ValueManager&);
using LegacyListValue_Contains = absl::Status (*)(uintptr_t, ValueManager&,
                                                  const Value&, Value&);

ABSL_CONST_INIT struct {
  absl::once_flag init_once;
  LegacyListValue_GetType get_type = nullptr;
  LegacyListValue_DebugString debug_string = nullptr;
  LegacyListValue_GetSerializedSize get_serialized_size = nullptr;
  LegacyListValue_SerializeTo serialize_to = nullptr;
  LegacyListValue_ConvertToJsonArray convert_to_json_array = nullptr;
  LegacyListValue_IsEmpty is_empty = nullptr;
  LegacyListValue_Size size = nullptr;
  LegacyListValue_Get get = nullptr;
  LegacyListValue_ForEach for_each = nullptr;
  LegacyListValue_NewIterator new_iterator = nullptr;
  LegacyListValue_Contains contains = nullptr;
} legacy_list_value_vtable;

#if ABSL_HAVE_ATTRIBUTE_WEAK
extern "C" ABSL_ATTRIBUTE_WEAK ListType
cel_common_internal_LegacyListValue_GetType(uintptr_t impl,
                                            TypeManager& type_manager);
extern "C" ABSL_ATTRIBUTE_WEAK std::string
cel_common_internal_LegacyListValue_DebugString(uintptr_t impl);
extern "C" ABSL_ATTRIBUTE_WEAK absl::StatusOr<size_t>
cel_common_internal_LegacyListValue_GetSerializedSize(uintptr_t impl);
extern "C" ABSL_ATTRIBUTE_WEAK absl::Status
cel_common_internal_LegacyListValue_SerializeTo(uintptr_t impl,
                                                absl::Cord& serialized_value);
extern "C" ABSL_ATTRIBUTE_WEAK absl::StatusOr<JsonArray>
cel_common_internal_LegacyListValue_ConvertToJsonArray(uintptr_t impl);
extern "C" ABSL_ATTRIBUTE_WEAK bool cel_common_internal_LegacyListValue_IsEmpty(
    uintptr_t impl);
extern "C" ABSL_ATTRIBUTE_WEAK size_t
cel_common_internal_LegacyListValue_Size(uintptr_t impl);
extern "C" ABSL_ATTRIBUTE_WEAK absl::Status
cel_common_internal_LegacyListValue_Get(uintptr_t impl,
                                        ValueManager& value_manager,
                                        size_t index, Value& scratch);
extern "C" ABSL_ATTRIBUTE_WEAK absl::Status
cel_common_internal_LegacyListValue_ForEach(
    uintptr_t impl, ValueManager& value_manager,
    ListValue::ForEachWithIndexCallback callback);
extern "C" ABSL_ATTRIBUTE_WEAK absl::StatusOr<absl::Nonnull<ValueIteratorPtr>>
cel_common_internal_LegacyListValue_NewIterator(uintptr_t impl,
                                                ValueManager& value_manager);
extern "C" ABSL_ATTRIBUTE_WEAK absl::Status
cel_common_internal_LegacyListValue_Contains(uintptr_t impl,
                                             ValueManager& value_manager,
                                             const Value& other,
                                             Value& scratch);
#endif

void InitializeLegacyListValue() {
  absl::call_once(legacy_list_value_vtable.init_once, []() -> void {
#if ABSL_HAVE_ATTRIBUTE_WEAK
    legacy_list_value_vtable.get_type = ABSL_DIE_IF_NULL(  // Crash OK
        cel_common_internal_LegacyListValue_GetType);
    legacy_list_value_vtable.debug_string = ABSL_DIE_IF_NULL(  // Crash OK
        cel_common_internal_LegacyListValue_DebugString);
    legacy_list_value_vtable.get_serialized_size =
        ABSL_DIE_IF_NULL(  // Crash OK
            cel_common_internal_LegacyListValue_GetSerializedSize);
    legacy_list_value_vtable.serialize_to = ABSL_DIE_IF_NULL(  // Crash OK
        cel_common_internal_LegacyListValue_SerializeTo);
    legacy_list_value_vtable.convert_to_json_array =
        ABSL_DIE_IF_NULL(  // Crash OK
            cel_common_internal_LegacyListValue_ConvertToJsonArray);
    legacy_list_value_vtable.is_empty = ABSL_DIE_IF_NULL(  // Crash OK
        cel_common_internal_LegacyListValue_IsEmpty);
    legacy_list_value_vtable.size =
        ABSL_DIE_IF_NULL(cel_common_internal_LegacyListValue_Size);  // Crash OK
    legacy_list_value_vtable.get =
        ABSL_DIE_IF_NULL(cel_common_internal_LegacyListValue_Get);  // Crash OK
    legacy_list_value_vtable.for_each = ABSL_DIE_IF_NULL(           // Crash OK
        cel_common_internal_LegacyListValue_ForEach);
    legacy_list_value_vtable.new_iterator = ABSL_DIE_IF_NULL(  // Crash OK
        cel_common_internal_LegacyListValue_NewIterator);
    legacy_list_value_vtable.contains = ABSL_DIE_IF_NULL(  // Crash OK
        cel_common_internal_LegacyListValue_Contains);
#else
    internal::DynamicLoader dynamic_loader;
    legacy_list_value_vtable.get_type = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyListValue_GetType");
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
    legacy_list_value_vtable.contains = dynamic_loader.FindSymbolOrDie(
        "cel_common_internal_LegacyListValue_Contains");
#endif
  });
}

}  // namespace

std::string LegacyListValue::DebugString() const {
  InitializeLegacyListValue();
  return (*legacy_list_value_vtable.debug_string)(impl_);
}

// See `ValueInterface::SerializeTo`.
absl::Status LegacyListValue::SerializeTo(AnyToJsonConverter&,
                                          absl::Cord& value) const {
  InitializeLegacyListValue();
  return (*legacy_list_value_vtable.serialize_to)(impl_, value);
}

absl::StatusOr<JsonArray> LegacyListValue::ConvertToJsonArray(
    AnyToJsonConverter&) const {
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
absl::Status LegacyListValue::Get(ValueManager& value_manager, size_t index,
                                  Value& result) const {
  InitializeLegacyListValue();
  return (*legacy_list_value_vtable.get)(impl_, value_manager, index, result);
}

absl::Status LegacyListValue::ForEach(ValueManager& value_manager,
                                      ForEachCallback callback) const {
  return ForEach(
      value_manager,
      [callback](size_t, const Value& value) -> absl::StatusOr<bool> {
        return callback(value);
      });
}

absl::Status LegacyListValue::ForEach(ValueManager& value_manager,
                                      ForEachWithIndexCallback callback) const {
  InitializeLegacyListValue();
  return (*legacy_list_value_vtable.for_each)(impl_, value_manager, callback);
}

absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> LegacyListValue::NewIterator(
    ValueManager& value_manager) const {
  InitializeLegacyListValue();
  return (*legacy_list_value_vtable.new_iterator)(impl_, value_manager);
}

absl::Status LegacyListValue::Equal(ValueManager& value_manager,
                                    const Value& other, Value& result) const {
  if (auto list_value = As<ListValue>(other); list_value.has_value()) {
    return ListValueEqual(value_manager, *this, *list_value, result);
  }
  result = BoolValue{false};
  return absl::OkStatus();
}

absl::Status LegacyListValue::Contains(ValueManager& value_manager,
                                       const Value& other,
                                       Value& result) const {
  InitializeLegacyListValue();
  return (*legacy_list_value_vtable.contains)(impl_, value_manager, other,
                                              result);
}

bool IsLegacyListValue(const Value& value) {
  return absl::holds_alternative<LegacyListValue>(value.variant_);
}

LegacyListValue GetLegacyListValue(const Value& value) {
  ABSL_DCHECK(IsLegacyListValue(value));
  return absl::get<LegacyListValue>(value.variant_);
}

absl::optional<LegacyListValue> AsLegacyListValue(const Value& value) {
  if (IsLegacyListValue(value)) {
    return GetLegacyListValue(value);
  }
  return absl::nullopt;
}

}  // namespace cel::common_internal

#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
