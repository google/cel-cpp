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
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/casting.h"
#include "common/type.h"
#include "common/value.h"
#include "internal/status_macros.h"

namespace cel {

StructType StructValue::GetType(TypeManager& type_manager) const {
  AssertIsValid();
  return absl::visit(
      [&type_manager](const auto& alternative) -> StructType {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          ABSL_UNREACHABLE();
        } else {
          return alternative.GetType(type_manager);
        }
      },
      variant_);
}

absl::string_view StructValue::GetTypeName() const {
  AssertIsValid();
  return absl::visit(
      [](const auto& alternative) -> absl::string_view {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return absl::string_view{};
        } else {
          return alternative.GetTypeName();
        }
      },
      variant_);
}

std::string StructValue::DebugString() const {
  AssertIsValid();
  return absl::visit(
      [](const auto& alternative) -> std::string {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return std::string{};
        } else {
          return alternative.DebugString();
        }
      },
      variant_);
}

absl::StatusOr<size_t> StructValue::GetSerializedSize(
    ValueManager& value_manager) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager](const auto& alternative) -> absl::StatusOr<size_t> {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.GetSerializedSize(value_manager);
        }
      },
      variant_);
}

absl::Status StructValue::SerializeTo(ValueManager& value_manager,
                                      absl::Cord& value) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, &value](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.SerializeTo(value_manager, value);
        }
      },
      variant_);
}

absl::StatusOr<absl::Cord> StructValue::Serialize(
    ValueManager& value_manager) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager](const auto& alternative) -> absl::StatusOr<absl::Cord> {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.Serialize(value_manager);
        }
      },
      variant_);
}

absl::StatusOr<std::string> StructValue::GetTypeUrl(
    absl::string_view prefix) const {
  AssertIsValid();
  return absl::visit(
      [prefix](const auto& alternative) -> absl::StatusOr<std::string> {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.GetTypeUrl(prefix);
        }
      },
      variant_);
}

absl::StatusOr<Any> StructValue::ConvertToAny(ValueManager& value_manager,
                                              absl::string_view prefix) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, prefix](const auto& alternative) -> absl::StatusOr<Any> {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.ConvertToAny(value_manager, prefix);
        }
      },
      variant_);
}

absl::StatusOr<Json> StructValue::ConvertToJson(
    ValueManager& value_manager) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager](const auto& alternative) -> absl::StatusOr<Json> {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.ConvertToJson(value_manager);
        }
      },
      variant_);
}

absl::StatusOr<JsonObject> StructValue::ConvertToJsonObject(
    ValueManager& value_manager) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager](const auto& alternative) -> absl::StatusOr<JsonObject> {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.ConvertToJsonObject(value_manager);
        }
      },
      variant_);
}

bool StructValue::IsZeroValue() const {
  AssertIsValid();
  return absl::visit(
      [](const auto& alternative) -> bool {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return false;
        } else {
          return alternative.IsZeroValue();
        }
      },
      variant_);
}

absl::StatusOr<bool> StructValue::HasFieldByName(absl::string_view name) const {
  AssertIsValid();
  return absl::visit(
      [name](const auto& alternative) -> absl::StatusOr<bool> {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.HasFieldByName(name);
        }
      },
      variant_);
}

absl::StatusOr<bool> StructValue::HasFieldByNumber(int64_t number) const {
  AssertIsValid();
  return absl::visit(
      [number](const auto& alternative) -> absl::StatusOr<bool> {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.HasFieldByNumber(number);
        }
      },
      variant_);
}

common_internal::StructValueViewVariant StructValue::ToViewVariant() const {
  return absl::visit(
      [](const auto& alternative) -> common_internal::StructValueViewVariant {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return common_internal::StructValueViewVariant{};
        } else {
          return common_internal::StructValueViewVariant(
              absl::in_place_type<typename absl::remove_cvref_t<
                  decltype(alternative)>::view_alternative_type>,
              alternative);
        }
      },
      variant_);
}

StructType StructValueView::GetType(TypeManager& type_manager) const {
  AssertIsValid();
  return absl::visit(
      [&type_manager](auto alternative) -> StructType {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          ABSL_UNREACHABLE();
        } else {
          return alternative.GetType(type_manager);
        }
      },
      variant_);
}

absl::string_view StructValueView::GetTypeName() const {
  AssertIsValid();
  return absl::visit(
      [](auto alternative) -> absl::string_view {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return absl::string_view{};
        } else {
          return alternative.GetTypeName();
        }
      },
      variant_);
}

std::string StructValueView::DebugString() const {
  AssertIsValid();
  return absl::visit(
      [](auto alternative) -> std::string {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return std::string{};
        } else {
          return alternative.DebugString();
        }
      },
      variant_);
}

absl::StatusOr<size_t> StructValueView::GetSerializedSize(
    ValueManager& value_manager) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager](auto alternative) -> absl::StatusOr<size_t> {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return absl::InternalError("use of invalid StructValueView");
        } else {
          return alternative.GetSerializedSize(value_manager);
        }
      },
      variant_);
}

absl::Status StructValueView::SerializeTo(ValueManager& value_manager,
                                          absl::Cord& value) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, &value](auto alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return absl::InternalError("use of invalid StructValueView");
        } else {
          return alternative.SerializeTo(value_manager, value);
        }
      },
      variant_);
}

absl::StatusOr<absl::Cord> StructValueView::Serialize(
    ValueManager& value_manager) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager](auto alternative) -> absl::StatusOr<absl::Cord> {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return absl::InternalError("use of invalid StructValueView");
        } else {
          return alternative.Serialize(value_manager);
        }
      },
      variant_);
}

absl::StatusOr<std::string> StructValueView::GetTypeUrl(
    absl::string_view prefix) const {
  AssertIsValid();
  return absl::visit(
      [prefix](auto alternative) -> absl::StatusOr<std::string> {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return absl::InternalError("use of invalid StructValueView");
        } else {
          return alternative.GetTypeUrl(prefix);
        }
      },
      variant_);
}

absl::StatusOr<Any> StructValueView::ConvertToAny(
    ValueManager& value_manager, absl::string_view prefix) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, prefix](auto alternative) -> absl::StatusOr<Any> {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return absl::InternalError("use of invalid StructValueView");
        } else {
          return alternative.ConvertToAny(value_manager, prefix);
        }
      },
      variant_);
}

absl::StatusOr<Json> StructValueView::ConvertToJson(
    ValueManager& value_manager) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager](auto alternative) -> absl::StatusOr<Json> {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return absl::InternalError("use of invalid StructValueView");
        } else {
          return alternative.ConvertToJson(value_manager);
        }
      },
      variant_);
}

absl::StatusOr<JsonObject> StructValueView::ConvertToJsonObject(
    ValueManager& value_manager) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager](auto alternative) -> absl::StatusOr<JsonObject> {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return absl::InternalError("use of invalid StructValueView");
        } else {
          return alternative.ConvertToJsonObject(value_manager);
        }
      },
      variant_);
}

bool StructValueView::IsZeroValue() const {
  AssertIsValid();
  return absl::visit(
      [](auto alternative) -> bool {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return false;
        } else {
          return alternative.IsZeroValue();
        }
      },
      variant_);
}

absl::StatusOr<bool> StructValueView::HasFieldByName(
    absl::string_view name) const {
  AssertIsValid();
  return absl::visit(
      [name](auto alternative) -> absl::StatusOr<bool> {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return absl::InternalError("use of invalid StructValueView");
        } else {
          return alternative.HasFieldByName(name);
        }
      },
      variant_);
}

absl::StatusOr<bool> StructValueView::HasFieldByNumber(int64_t number) const {
  AssertIsValid();
  return absl::visit(
      [number](auto alternative) -> absl::StatusOr<bool> {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          return absl::InternalError("use of invalid StructValueView");
        } else {
          return alternative.HasFieldByNumber(number);
        }
      },
      variant_);
}

common_internal::StructValueVariant StructValueView::ToVariant() const {
  return absl::visit(
      [](auto alternative) -> common_internal::StructValueVariant {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return common_internal::StructValueVariant{};
        } else {
          return common_internal::StructValueVariant(
              absl::in_place_type<typename absl::remove_cvref_t<
                  decltype(alternative)>::alternative_type>,
              alternative);
        }
      },
      variant_);
}

namespace common_internal {

absl::StatusOr<ValueView> StructValueEqual(ValueManager& value_manager,
                                           StructValueView lhs,
                                           StructValueView rhs,
                                           Value& scratch) {
  if (lhs.GetTypeName() != rhs.GetTypeName()) {
    return BoolValueView{false};
  }
  absl::flat_hash_map<std::string, Value> lhs_fields;
  CEL_RETURN_IF_ERROR(lhs.ForEachField(
      value_manager,
      [&lhs_fields](absl::string_view name,
                    ValueView lhs_value) -> absl::StatusOr<bool> {
        lhs_fields.insert_or_assign(std::string(name), Value(lhs_value));
        return true;
      }));
  bool equal = true;
  size_t rhs_fields_count = 0;
  CEL_RETURN_IF_ERROR(rhs.ForEachField(
      value_manager,
      [&value_manager, &scratch, &lhs_fields, &equal, &rhs_fields_count](
          absl::string_view name, ValueView rhs_value) -> absl::StatusOr<bool> {
        auto lhs_field = lhs_fields.find(name);
        if (lhs_field == lhs_fields.end()) {
          equal = false;
          return false;
        }
        CEL_ASSIGN_OR_RETURN(
            auto result,
            lhs_field->second.Equal(value_manager, rhs_value, scratch));
        if (auto bool_value = As<BoolValueView>(result);
            bool_value.has_value() && !bool_value->NativeValue()) {
          equal = false;
          return false;
        }
        ++rhs_fields_count;
        return true;
      }));
  if (!equal || rhs_fields_count != lhs_fields.size()) {
    return BoolValueView{false};
  }
  return BoolValueView{true};
}

absl::StatusOr<ValueView> StructValueEqual(
    ValueManager& value_manager, const ParsedStructValueInterface& lhs,
    StructValueView rhs, Value& scratch) {
  if (lhs.GetTypeName() != rhs.GetTypeName()) {
    return BoolValueView{false};
  }
  absl::flat_hash_map<std::string, Value> lhs_fields;
  CEL_RETURN_IF_ERROR(lhs.ForEachField(
      value_manager,
      [&lhs_fields](absl::string_view name,
                    ValueView lhs_value) -> absl::StatusOr<bool> {
        lhs_fields.insert_or_assign(std::string(name), Value(lhs_value));
        return true;
      }));
  bool equal = true;
  size_t rhs_fields_count = 0;
  CEL_RETURN_IF_ERROR(rhs.ForEachField(
      value_manager,
      [&value_manager, &scratch, &lhs_fields, &equal, &rhs_fields_count](
          absl::string_view name, ValueView rhs_value) -> absl::StatusOr<bool> {
        auto lhs_field = lhs_fields.find(name);
        if (lhs_field == lhs_fields.end()) {
          equal = false;
          return false;
        }
        CEL_ASSIGN_OR_RETURN(
            auto result,
            lhs_field->second.Equal(value_manager, rhs_value, scratch));
        if (auto bool_value = As<BoolValueView>(result);
            bool_value.has_value() && !bool_value->NativeValue()) {
          equal = false;
          return false;
        }
        ++rhs_fields_count;
        return true;
      }));
  if (!equal || rhs_fields_count != lhs_fields.size()) {
    return BoolValueView{false};
  }
  return BoolValueView{true};
}

}  // namespace common_internal

}  // namespace cel
