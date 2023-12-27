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
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/call_once.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "common/json.h"
#include "common/type.h"
#include "common/value.h"
#include "internal/status_macros.h"

#ifdef _WIN32
#include <windows.h>
#else
#include <dlfcn.h>
#endif

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
using LegacyStructValue_GetTypeName = absl::string_view (*)(uintptr_t,
                                                            uintptr_t);
using LegacyStructValue_ConvertToJsonObject =
    absl::StatusOr<JsonObject> (*)(uintptr_t, uintptr_t);
using LegacyStructValue_GetFieldByName = absl::StatusOr<ValueView> (*)(
    uintptr_t, uintptr_t, ValueManager&, absl::string_view, Value&);
using LegacyStructValue_GetFieldByNumber = absl::StatusOr<ValueView> (*)(
    uintptr_t, uintptr_t, ValueManager&, int64_t, Value&);
using LegacyStructValue_HasFieldByName =
    absl::StatusOr<bool> (*)(uintptr_t, uintptr_t, absl::string_view);
using LegacyStructValue_HasFieldByNumber = absl::StatusOr<bool> (*)(uintptr_t,
                                                                    uintptr_t,
                                                                    int64_t);

ABSL_CONST_INIT struct {
  absl::once_flag init_once;
  LegacyStructValue_DebugString debug_string = nullptr;
  LegacyStructValue_GetSerializedSize get_serialized_size = nullptr;
  LegacyStructValue_SerializeTo serialize_to = nullptr;
  LegacyStructValue_GetTypeName get_type_name = nullptr;
  LegacyStructValue_ConvertToJsonObject convert_to_json_object = nullptr;
  LegacyStructValue_GetFieldByName get_field_by_name = nullptr;
  LegacyStructValue_GetFieldByNumber get_field_by_number = nullptr;
  LegacyStructValue_HasFieldByName has_field_by_name = nullptr;
  LegacyStructValue_HasFieldByNumber has_field_by_number = nullptr;
} legacy_struct_value_vtable;

struct DynamicLibrarySymbol {
  void* address;

  template <typename T, typename = std::enable_if_t<std::is_pointer_v<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  operator T() const {
    return reinterpret_cast<T>(address);
  }
};

class DynamicLibrarySymbolFinder {
 public:
  DynamicLibrarySymbolFinder() {
#ifdef _WIN32
    DWORD modules_capacity = 16;
    modules_ = new HMODULE[modules_capacity];
    while (true) {
      if (!::EnumProcessModulesEx(::GetCurrentProcessHandle(), &modules_,
                                  modules_capacity * sizeof(HMODULE),
                                  &modules_size_, LIST_MODULES_DEFAULT)) {
        // Give up. Fallback to check fail.
        delete[] modules_;
        modules_size_ = 0;
        break;
      }
      modules_size_ /= sizeof(HMODULE);
      if (modules_size_ <= modules_capacity) {
        break;
      }
      modules_capacity *= 2;
      delete[] modules_;
      modules_ = new HMODULE[modules_capacity];
    }
#endif
  }

  ~DynamicLibrarySymbolFinder() {
#ifdef _WIN32
    delete[] modules_;
#endif
  }

  DynamicLibrarySymbolFinder(const DynamicLibrarySymbolFinder&) = delete;
  DynamicLibrarySymbolFinder(DynamicLibrarySymbolFinder&&) = delete;
  DynamicLibrarySymbolFinder& operator=(const DynamicLibrarySymbolFinder&) =
      delete;
  DynamicLibrarySymbolFinder& operator=(DynamicLibrarySymbolFinder&&) = delete;

  DynamicLibrarySymbol FindSymbolOrDie(const char* name) const {
    void* address = nullptr;
#ifdef _WIN32
    for (DWORD i = 0; address == nullptr && i < modules_size_; ++i) {
      address = ::GetProcAddress(modules_[i], name);
    }
#else
    address = ::dlsym(handle_, name);
#endif
    ABSL_CHECK(address != nullptr)  // Crash OK
        << "failed to find dynamic library symbol: " << name;
    return DynamicLibrarySymbol{address};
  }

 private:
#ifdef _WIN32
  HMODULE* modules_ = nullptr;
  DWORD modules_size_ = 0;
#else
  void* handle_ = RTLD_DEFAULT;
#endif
};

void InitializeLegacyStructValue() {
  absl::call_once(legacy_struct_value_vtable.init_once, []() -> void {
    DynamicLibrarySymbolFinder symbol_finder;
    legacy_struct_value_vtable.debug_string = symbol_finder.FindSymbolOrDie(
        "cel_common_internal_LegacyStructValue_DebugString");
    legacy_struct_value_vtable.get_serialized_size =
        symbol_finder.FindSymbolOrDie(
            "cel_common_internal_LegacyStructValue_GetSerializedSize");
    legacy_struct_value_vtable.serialize_to = symbol_finder.FindSymbolOrDie(
        "cel_common_internal_LegacyStructValue_SerializeTo");
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
  });
}

}  // namespace

StructType LegacyStructValue::GetType(TypeManager& type_manager) const {
  return type_manager.CreateStructType(GetTypeName());
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

absl::StatusOr<ValueView> LegacyStructValue::GetFieldByName(
    ValueManager& value_manager, absl::string_view name, Value& scratch) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.get_field_by_name)(
      message_ptr_, type_info_, value_manager, name, scratch);
}

absl::StatusOr<ValueView> LegacyStructValue::GetFieldByNumber(
    ValueManager& value_manager, int64_t number, Value& scratch) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.get_field_by_number)(
      message_ptr_, type_info_, value_manager, number, scratch);
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

StructType LegacyStructValueView::GetType(TypeManager& type_manager) const {
  return type_manager.CreateStructType(GetTypeName());
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

absl::StatusOr<ValueView> LegacyStructValueView::GetFieldByName(
    ValueManager& value_manager, absl::string_view name, Value& scratch) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.get_field_by_name)(
      message_ptr_, type_info_, value_manager, name, scratch);
}

absl::StatusOr<ValueView> LegacyStructValueView::GetFieldByNumber(
    ValueManager& value_manager, int64_t number, Value& scratch) const {
  InitializeLegacyStructValue();
  return (*legacy_struct_value_vtable.get_field_by_number)(
      message_ptr_, type_info_, value_manager, number, scratch);
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

}  // namespace cel::common_internal
