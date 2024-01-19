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
using LegacyStructValue_GetFieldByName = absl::StatusOr<ValueView> (*)(
    uintptr_t, uintptr_t, ValueManager&, absl::string_view, Value&,
    ProtoWrapperTypeOptions);
using LegacyStructValue_GetFieldByNumber =
    absl::StatusOr<ValueView> (*)(uintptr_t, uintptr_t, ValueManager&, int64_t,
                                  Value&, ProtoWrapperTypeOptions);
using LegacyStructValue_HasFieldByName =
    absl::StatusOr<bool> (*)(uintptr_t, uintptr_t, absl::string_view);
using LegacyStructValue_HasFieldByNumber = absl::StatusOr<bool> (*)(uintptr_t,
                                                                    uintptr_t,
                                                                    int64_t);
using LegacyStructValue_Equal = absl::StatusOr<ValueView> (*)(
    uintptr_t, uintptr_t, ValueManager&, ValueView, Value&);
using LegacyStructValue_ForEachField =
    absl::Status (*)(uintptr_t, uintptr_t, ValueManager&,
                     LegacyStructValue::ForEachFieldCallback);
using LegacyStructValue_Qualify = absl::StatusOr<std::pair<ValueView, int>> (*)(
    uintptr_t, uintptr_t, ValueManager&, absl::Span<const SelectQualifier>,
    bool, Value&);

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
  LegacyStructValue_ForEachField for_each_field = nullptr;
  LegacyStructValue_Qualify qualify = nullptr;
} legacy_struct_value_vtable;

void InitializeLegacyStructValue() {
  absl::call_once(legacy_struct_value_vtable.init_once, []() -> void {
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
    legacy_struct_value_vtable.for_each_field = symbol_finder.FindSymbolOrDie(
        "cel_common_internal_LegacyStructValue_ForEachField");
    legacy_struct_value_vtable.qualify = symbol_finder.FindSymbolOrDie(
        "cel_common_internal_LegacyStructValue_Qualify");
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

absl::StatusOr<size_t> LegacyStructValue::GetSerializedSize() const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.get_serialized_size)(message_ptr_,
                                                           type_info_);
}

absl::Status LegacyStructValue::SerializeTo(absl::Cord& value) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.serialize_to)(message_ptr_, type_info_,
                                                    value);
}

absl::StatusOr<absl::Cord> LegacyStructValue::Serialize() const {
  absl::Cord serialized_value;
  CEL_RETURN_IF_ERROR(SerializeTo(serialized_value));
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
    absl::string_view prefix) const {
  CEL_ASSIGN_OR_RETURN(auto serialized_value, Serialize());
  CEL_ASSIGN_OR_RETURN(auto type_url, GetTypeUrl(prefix));
  return MakeAny(std::move(type_url), std::move(serialized_value));
}

absl::StatusOr<Json> LegacyStructValue::ConvertToJson() const {
  return ConvertToJsonObject();
}

absl::StatusOr<JsonObject> LegacyStructValue::ConvertToJsonObject() const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.convert_to_json_object)(message_ptr_,
                                                              type_info_);
}

absl::StatusOr<ValueView> LegacyStructValue::Equal(ValueManager& value_manager,
                                                   ValueView other,
                                                   Value& scratch) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.equal)(message_ptr_, type_info_,
                                             value_manager, other, scratch);
}

absl::StatusOr<ValueView> LegacyStructValue::GetFieldByName(
    ValueManager& value_manager, absl::string_view name, Value& scratch,
    ProtoWrapperTypeOptions unboxing_options) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.get_field_by_name)(
      message_ptr_, type_info_, value_manager, name, scratch, unboxing_options);
}

absl::StatusOr<ValueView> LegacyStructValue::GetFieldByNumber(
    ValueManager& value_manager, int64_t number, Value& scratch,
    ProtoWrapperTypeOptions unboxing_options) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.get_field_by_number)(
      message_ptr_, type_info_, value_manager, number, scratch,
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

absl::StatusOr<std::pair<ValueView, int>> LegacyStructValue::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test, Value& scratch) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.qualify)(message_ptr_, type_info_,
                                               value_manager, qualifiers,
                                               presence_test, scratch);
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

absl::StatusOr<size_t> LegacyStructValueView::GetSerializedSize() const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.get_serialized_size)(message_ptr_,
                                                           type_info_);
}

absl::Status LegacyStructValueView::SerializeTo(absl::Cord& value) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.serialize_to)(message_ptr_, type_info_,
                                                    value);
}

absl::StatusOr<absl::Cord> LegacyStructValueView::Serialize() const {
  absl::Cord serialized_value;
  CEL_RETURN_IF_ERROR(SerializeTo(serialized_value));
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
    absl::string_view prefix) const {
  CEL_ASSIGN_OR_RETURN(auto serialized_value, Serialize());
  CEL_ASSIGN_OR_RETURN(auto type_url, GetTypeUrl(prefix));
  return MakeAny(std::move(type_url), std::move(serialized_value));
}

absl::StatusOr<Json> LegacyStructValueView::ConvertToJson() const {
  return ConvertToJsonObject();
}

absl::StatusOr<JsonObject> LegacyStructValueView::ConvertToJsonObject() const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.convert_to_json_object)(message_ptr_,
                                                              type_info_);
}

absl::StatusOr<ValueView> LegacyStructValueView::Equal(
    ValueManager& value_manager, ValueView other, Value& scratch) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.equal)(message_ptr_, type_info_,
                                             value_manager, other, scratch);
}

absl::StatusOr<ValueView> LegacyStructValueView::GetFieldByName(
    ValueManager& value_manager, absl::string_view name, Value& scratch,
    ProtoWrapperTypeOptions unboxing_options) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.get_field_by_name)(
      message_ptr_, type_info_, value_manager, name, scratch, unboxing_options);
}

absl::StatusOr<ValueView> LegacyStructValueView::GetFieldByNumber(
    ValueManager& value_manager, int64_t number, Value& scratch,
    ProtoWrapperTypeOptions unboxing_options) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.get_field_by_number)(
      message_ptr_, type_info_, value_manager, number, scratch,
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

absl::StatusOr<std::pair<ValueView, int>> LegacyStructValueView::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test, Value& scratch) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.qualify)(message_ptr_, type_info_,
                                               value_manager, qualifiers,
                                               presence_test, scratch);
}

}  // namespace cel::common_internal
