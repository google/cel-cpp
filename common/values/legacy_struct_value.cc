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

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/call_once.h"
#include "absl/log/die_if_null.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/any.h"
#include "common/json.h"
#include "common/type.h"
#include "common/value.h"
#include "internal/dynamic_loader.h"
#include "internal/status_macros.h"
#include "runtime/runtime_options.h"

namespace cel::common_internal {

namespace {

// Weak symbols are not supported on all platforms, namely Windows. Instead we
// implement them ourselves using the dynamic linker. This is here to implement
// `StructValue` on top of the legacy `google::api::expr::runtime::CelValue`. We
// cannot take a strong dependency due to circular dependency issues and we also
// don't want the protobuf library to be a core dependency. Once
// `google::api::expr::runtime::CelValue` is dead, this can be removed.

using LegacyStructValue_DebugString = std::string (*)(uintptr_t, uintptr_t);
using LegacyStructValue_GetSerializedSize =
    absl::StatusOr<size_t> (*)(uintptr_t, uintptr_t);
using LegacyStructValue_SerializeTo = absl::Status (*)(uintptr_t, uintptr_t,
                                                       absl::Cord&);
using LegacyStructValue_GetType = std::string (*)(uintptr_t, uintptr_t);
using LegacyStructValue_GetTypeName = absl::string_view (*)(uintptr_t,
                                                            uintptr_t);
using LegacyStructValue_ConvertToJsonObject =
    absl::StatusOr<JsonObject> (*)(uintptr_t, uintptr_t);
using LegacyStructValue_GetFieldByName =
    absl::Status (*)(uintptr_t, uintptr_t, ValueManager&, absl::string_view,
                     Value&, ProtoWrapperTypeOptions);
using LegacyStructValue_GetFieldByNumber =
    absl::Status (*)(uintptr_t, uintptr_t, ValueManager&, int64_t, Value&,
                     ProtoWrapperTypeOptions);
using LegacyStructValue_HasFieldByName =
    absl::StatusOr<bool> (*)(uintptr_t, uintptr_t, absl::string_view);
using LegacyStructValue_HasFieldByNumber = absl::StatusOr<bool> (*)(uintptr_t,
                                                                    uintptr_t,
                                                                    int64_t);
using LegacyStructValue_Equal = absl::Status (*)(uintptr_t, uintptr_t,
                                                 ValueManager&, const Value&,
                                                 Value&);
using LegacyStructValue_IsZeroValue = bool (*)(uintptr_t, uintptr_t);
using LegacyStructValue_ForEachField =
    absl::Status (*)(uintptr_t, uintptr_t, ValueManager&,
                     LegacyStructValue::ForEachFieldCallback);
using LegacyStructValue_Qualify =
    absl::StatusOr<int> (*)(uintptr_t, uintptr_t, ValueManager&,
                            absl::Span<const SelectQualifier>, bool, Value&);

ABSL_CONST_INIT struct {
  absl::once_flag init_once;
  LegacyStructValue_DebugString debug_string = nullptr;
  LegacyStructValue_GetSerializedSize get_serialized_size = nullptr;
  LegacyStructValue_SerializeTo serialize_to = nullptr;
  LegacyStructValue_GetType get_type = nullptr;
  LegacyStructValue_GetTypeName get_type_name = nullptr;
  LegacyStructValue_ConvertToJsonObject convert_to_json_object = nullptr;
  LegacyStructValue_GetFieldByName get_field_by_name = nullptr;
  LegacyStructValue_GetFieldByNumber get_field_by_number = nullptr;
  LegacyStructValue_HasFieldByName has_field_by_name = nullptr;
  LegacyStructValue_HasFieldByNumber has_field_by_number = nullptr;
  LegacyStructValue_Equal equal = nullptr;
  LegacyStructValue_IsZeroValue is_zero_value = nullptr;
  LegacyStructValue_ForEachField for_each_field = nullptr;
  LegacyStructValue_Qualify qualify = nullptr;
} legacy_struct_value_vtable;

#if ABSL_HAVE_ATTRIBUTE_WEAK
extern "C" ABSL_ATTRIBUTE_WEAK std::string
cel_common_internal_LegacyStructValue_DebugString(uintptr_t message_ptr,
                                                  uintptr_t type_info);
extern "C" ABSL_ATTRIBUTE_WEAK absl::StatusOr<size_t>
cel_common_internal_LegacyStructValue_GetSerializedSize(uintptr_t message_ptr,
                                                        uintptr_t type_info);
extern "C" ABSL_ATTRIBUTE_WEAK absl::Status
cel_common_internal_LegacyStructValue_SerializeTo(uintptr_t message_ptr,
                                                  uintptr_t type_info,
                                                  absl::Cord& value);
extern "C" ABSL_ATTRIBUTE_WEAK std::string
cel_common_internal_LegacyStructValue_GetType(uintptr_t message_ptr,
                                              uintptr_t type_info);
extern "C" ABSL_ATTRIBUTE_WEAK absl::string_view
cel_common_internal_LegacyStructValue_GetTypeName(uintptr_t message_ptr,
                                                  uintptr_t type_info);
extern "C" ABSL_ATTRIBUTE_WEAK absl::StatusOr<JsonObject>
cel_common_internal_LegacyStructValue_ConvertToJsonObject(uintptr_t message_ptr,
                                                          uintptr_t type_info);
extern "C" ABSL_ATTRIBUTE_WEAK absl::Status
cel_common_internal_LegacyStructValue_GetFieldByName(
    uintptr_t message_ptr, uintptr_t type_info, ValueManager& value_manager,
    absl::string_view name, Value& result,
    ProtoWrapperTypeOptions unboxing_options);
extern "C" ABSL_ATTRIBUTE_WEAK absl::Status
cel_common_internal_LegacyStructValue_GetFieldByNumber(uintptr_t, uintptr_t,
                                                       ValueManager&, int64_t,
                                                       Value&,
                                                       ProtoWrapperTypeOptions);
extern "C" ABSL_ATTRIBUTE_WEAK absl::StatusOr<bool>
cel_common_internal_LegacyStructValue_HasFieldByName(uintptr_t message_ptr,
                                                     uintptr_t type_info,
                                                     absl::string_view name);
extern "C" ABSL_ATTRIBUTE_WEAK absl::StatusOr<bool>
cel_common_internal_LegacyStructValue_HasFieldByNumber(uintptr_t, uintptr_t,
                                                       int64_t);
extern "C" ABSL_ATTRIBUTE_WEAK absl::Status
cel_common_internal_LegacyStructValue_Equal(uintptr_t message_ptr,
                                            uintptr_t type_info,
                                            ValueManager& value_manager,
                                            const Value& other, Value& result);
extern "C" ABSL_ATTRIBUTE_WEAK bool
cel_common_internal_LegacyStructValue_IsZeroValue(uintptr_t message_ptr,
                                                  uintptr_t type_info);
extern "C" ABSL_ATTRIBUTE_WEAK absl::Status
cel_common_internal_LegacyStructValue_ForEachField(
    uintptr_t message_ptr, uintptr_t type_info, ValueManager& value_manager,
    StructValue::ForEachFieldCallback callback);
extern "C" ABSL_ATTRIBUTE_WEAK absl::StatusOr<int>
cel_common_internal_LegacyStructValue_Qualify(
    uintptr_t message_ptr, uintptr_t type_info, ValueManager& value_manager,
    absl::Span<const SelectQualifier> qualifiers, bool presence_test,
    Value& result);
#endif

void InitializeLegacyStructValue() {
  absl::call_once(legacy_struct_value_vtable.init_once, []() -> void {
#if ABSL_HAVE_ATTRIBUTE_WEAK
    legacy_struct_value_vtable.debug_string = ABSL_DIE_IF_NULL(  // Crash OK
        cel_common_internal_LegacyStructValue_DebugString);
    legacy_struct_value_vtable.get_serialized_size =
        ABSL_DIE_IF_NULL(  // Crash OK
            cel_common_internal_LegacyStructValue_GetSerializedSize);
    legacy_struct_value_vtable.serialize_to = ABSL_DIE_IF_NULL(  // Crash OK
        cel_common_internal_LegacyStructValue_SerializeTo);
    legacy_struct_value_vtable.get_type = ABSL_DIE_IF_NULL(  // Crash OK
        cel_common_internal_LegacyStructValue_GetType);
    legacy_struct_value_vtable.get_type_name = ABSL_DIE_IF_NULL(  // Crash OK
        cel_common_internal_LegacyStructValue_GetTypeName);
    legacy_struct_value_vtable.convert_to_json_object =
        ABSL_DIE_IF_NULL(  // Crash OK
            cel_common_internal_LegacyStructValue_ConvertToJsonObject);
    legacy_struct_value_vtable.get_field_by_name =
        ABSL_DIE_IF_NULL(  // Crash OK
            cel_common_internal_LegacyStructValue_GetFieldByName);
    legacy_struct_value_vtable.get_field_by_number =
        ABSL_DIE_IF_NULL(  // Crash OK
            cel_common_internal_LegacyStructValue_GetFieldByNumber);
    legacy_struct_value_vtable.has_field_by_name =
        ABSL_DIE_IF_NULL(  // Crash OK
            cel_common_internal_LegacyStructValue_HasFieldByName);
    legacy_struct_value_vtable.has_field_by_number =
        ABSL_DIE_IF_NULL(  // Crash OK
            cel_common_internal_LegacyStructValue_HasFieldByNumber);
    legacy_struct_value_vtable.equal = ABSL_DIE_IF_NULL(  // Crash OK
        cel_common_internal_LegacyStructValue_Equal);
    legacy_struct_value_vtable.is_zero_value = ABSL_DIE_IF_NULL(  // Crash OK
        cel_common_internal_LegacyStructValue_IsZeroValue);
    legacy_struct_value_vtable.for_each_field = ABSL_DIE_IF_NULL(  // Crash OK
        cel_common_internal_LegacyStructValue_ForEachField);
    legacy_struct_value_vtable.qualify = ABSL_DIE_IF_NULL(  // Crash OK
        cel_common_internal_LegacyStructValue_Qualify);
#else
    internal::DynamicLoader symbol_finder;
    legacy_struct_value_vtable.debug_string = symbol_finder.FindSymbolOrDie(
        "cel_common_internal_LegacyStructValue_DebugString");
    legacy_struct_value_vtable.get_serialized_size =
        symbol_finder.FindSymbolOrDie(
            "cel_common_internal_LegacyStructValue_GetSerializedSize");
    legacy_struct_value_vtable.serialize_to = symbol_finder.FindSymbolOrDie(
        "cel_common_internal_LegacyStructValue_SerializeTo");
    legacy_struct_value_vtable.get_type = symbol_finder.FindSymbolOrDie(
        "cel_common_internal_LegacyStructValue_GetType");
    legacy_struct_value_vtable.get_type_name = symbol_finder.FindSymbolOrDie(
        "cel_common_internal_LegacyStructValue_GetTypeName");
    legacy_struct_value_vtable.convert_to_json_object =
        symbol_finder.FindSymbolOrDie(
            "cel_common_internal_LegacyStructValue_ConvertToJsonObject");
    legacy_struct_value_vtable.get_field_by_name =
        symbol_finder.FindSymbolOrDie(
            "cel_common_internal_LegacyStructValue_GetFieldByName");
    legacy_struct_value_vtable.get_field_by_number =
        symbol_finder.FindSymbolOrDie(
            "cel_common_internal_LegacyStructValue_GetFieldByNumber");
    legacy_struct_value_vtable.has_field_by_name =
        symbol_finder.FindSymbolOrDie(
            "cel_common_internal_LegacyStructValue_HasFieldByName");
    legacy_struct_value_vtable.has_field_by_number =
        symbol_finder.FindSymbolOrDie(
            "cel_common_internal_LegacyStructValue_HasFieldByNumber");
    legacy_struct_value_vtable.equal = symbol_finder.FindSymbolOrDie(
        "cel_common_internal_LegacyStructValue_Equal");
    legacy_struct_value_vtable.is_zero_value = symbol_finder.FindSymbolOrDie(
        "cel_common_internal_LegacyStructValue_IsZeroValue");
    legacy_struct_value_vtable.for_each_field = symbol_finder.FindSymbolOrDie(
        "cel_common_internal_LegacyStructValue_ForEachField");
    legacy_struct_value_vtable.qualify = symbol_finder.FindSymbolOrDie(
        "cel_common_internal_LegacyStructValue_Qualify");
#endif
  });
}

}  // namespace

StructType LegacyStructValue::GetType(TypeManager& type_manager) const {
  InitializeLegacyStructValue();
  return type_manager.CreateStructType(
      (*legacy_struct_value_vtable.get_type)(message_ptr_, type_info_));
}

absl::string_view LegacyStructValue::GetTypeName() const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.get_type_name)(message_ptr_, type_info_);
}

std::string LegacyStructValue::DebugString() const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.debug_string)(message_ptr_, type_info_);
}

absl::StatusOr<size_t> LegacyStructValue::GetSerializedSize(
    AnyToJsonConverter&) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.get_serialized_size)(message_ptr_,
                                                           type_info_);
}

absl::Status LegacyStructValue::SerializeTo(AnyToJsonConverter&,
                                            absl::Cord& value) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.serialize_to)(message_ptr_, type_info_,
                                                    value);
}

absl::StatusOr<absl::Cord> LegacyStructValue::Serialize(
    AnyToJsonConverter& value_manager) const {
  absl::Cord serialized_value;
  CEL_RETURN_IF_ERROR(SerializeTo(value_manager, serialized_value));
  return serialized_value;
}

absl::StatusOr<std::string> LegacyStructValue::GetTypeUrl(
    absl::string_view prefix) const {
  InitializeLegacyStructValue();
  return MakeTypeUrlWithPrefix(
      prefix,
      (*legacy_struct_value_vtable.get_type_name)(message_ptr_, type_info_));
}

absl::StatusOr<Any> LegacyStructValue::ConvertToAny(
    AnyToJsonConverter& value_manager, absl::string_view prefix) const {
  CEL_ASSIGN_OR_RETURN(auto serialized_value, Serialize(value_manager));
  CEL_ASSIGN_OR_RETURN(auto type_url, GetTypeUrl(prefix));
  return MakeAny(std::move(type_url), std::move(serialized_value));
}

absl::StatusOr<Json> LegacyStructValue::ConvertToJson(
    AnyToJsonConverter& value_manager) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.convert_to_json_object)(message_ptr_,
                                                              type_info_);
}

absl::Status LegacyStructValue::Equal(ValueManager& value_manager,
                                      const Value& other, Value& result) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.equal)(message_ptr_, type_info_,
                                             value_manager, other, result);
}

bool LegacyStructValue::IsZeroValue() const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.is_zero_value)(message_ptr_, type_info_);
}

absl::Status LegacyStructValue::GetFieldByName(
    ValueManager& value_manager, absl::string_view name, Value& result,
    ProtoWrapperTypeOptions unboxing_options) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.get_field_by_name)(
      message_ptr_, type_info_, value_manager, name, result, unboxing_options);
}

absl::Status LegacyStructValue::GetFieldByNumber(
    ValueManager& value_manager, int64_t number, Value& result,
    ProtoWrapperTypeOptions unboxing_options) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.get_field_by_number)(
      message_ptr_, type_info_, value_manager, number, result,
      unboxing_options);
}

absl::StatusOr<bool> LegacyStructValue::HasFieldByName(
    absl::string_view name) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.has_field_by_name)(message_ptr_,
                                                         type_info_, name);
}

absl::StatusOr<bool> LegacyStructValue::HasFieldByNumber(int64_t number) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.has_field_by_number)(message_ptr_,
                                                           type_info_, number);
}

absl::Status LegacyStructValue::ForEachField(
    ValueManager& value_manager, ForEachFieldCallback callback) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.for_each_field)(message_ptr_, type_info_,
                                                      value_manager, callback);
}

absl::StatusOr<int> LegacyStructValue::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test, Value& result) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.qualify)(message_ptr_, type_info_,
                                               value_manager, qualifiers,
                                               presence_test, result);
}

StructType LegacyStructValueView::GetType(TypeManager& type_manager) const {
  InitializeLegacyStructValue();
  return type_manager.CreateStructType(
      (*legacy_struct_value_vtable.get_type)(message_ptr_, type_info_));
}

absl::string_view LegacyStructValueView::GetTypeName() const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.get_type_name)(message_ptr_, type_info_);
}

std::string LegacyStructValueView::DebugString() const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.debug_string)(message_ptr_, type_info_);
}

absl::StatusOr<size_t> LegacyStructValueView::GetSerializedSize(
    AnyToJsonConverter&) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.get_serialized_size)(message_ptr_,
                                                           type_info_);
}

absl::Status LegacyStructValueView::SerializeTo(AnyToJsonConverter&,
                                                absl::Cord& value) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.serialize_to)(message_ptr_, type_info_,
                                                    value);
}

absl::StatusOr<absl::Cord> LegacyStructValueView::Serialize(
    AnyToJsonConverter& value_manager) const {
  absl::Cord serialized_value;
  CEL_RETURN_IF_ERROR(SerializeTo(value_manager, serialized_value));
  return serialized_value;
}

absl::StatusOr<std::string> LegacyStructValueView::GetTypeUrl(
    absl::string_view prefix) const {
  InitializeLegacyStructValue();
  return MakeTypeUrlWithPrefix(
      prefix,
      (*legacy_struct_value_vtable.get_type_name)(message_ptr_, type_info_));
}

absl::StatusOr<Any> LegacyStructValueView::ConvertToAny(
    AnyToJsonConverter& value_manager, absl::string_view prefix) const {
  CEL_ASSIGN_OR_RETURN(auto serialized_value, Serialize(value_manager));
  CEL_ASSIGN_OR_RETURN(auto type_url, GetTypeUrl(prefix));
  return MakeAny(std::move(type_url), std::move(serialized_value));
}

absl::StatusOr<Json> LegacyStructValueView::ConvertToJson(
    AnyToJsonConverter& value_manager) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.convert_to_json_object)(message_ptr_,
                                                              type_info_);
}

absl::Status LegacyStructValueView::Equal(ValueManager& value_manager,
                                          ValueView other,
                                          Value& result) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.equal)(
      message_ptr_, type_info_, value_manager, Value(other), result);
}

bool LegacyStructValueView::IsZeroValue() const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.is_zero_value)(message_ptr_, type_info_);
}

absl::Status LegacyStructValueView::GetFieldByName(
    ValueManager& value_manager, absl::string_view name, Value& result,
    ProtoWrapperTypeOptions unboxing_options) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.get_field_by_name)(
      message_ptr_, type_info_, value_manager, name, result, unboxing_options);
}

absl::Status LegacyStructValueView::GetFieldByNumber(
    ValueManager& value_manager, int64_t number, Value& result,
    ProtoWrapperTypeOptions unboxing_options) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.get_field_by_number)(
      message_ptr_, type_info_, value_manager, number, result,
      unboxing_options);
}

absl::StatusOr<bool> LegacyStructValueView::HasFieldByName(
    absl::string_view name) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.has_field_by_name)(message_ptr_,
                                                         type_info_, name);
}

absl::StatusOr<bool> LegacyStructValueView::HasFieldByNumber(
    int64_t number) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.has_field_by_number)(message_ptr_,
                                                           type_info_, number);
}

absl::Status LegacyStructValueView::ForEachField(
    ValueManager& value_manager, ForEachFieldCallback callback) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.for_each_field)(message_ptr_, type_info_,
                                                      value_manager, callback);
}

absl::StatusOr<int> LegacyStructValueView::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test, Value& result) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.qualify)(message_ptr_, type_info_,
                                               value_manager, qualifiers,
                                               presence_test, result);
}

}  // namespace cel::common_internal
