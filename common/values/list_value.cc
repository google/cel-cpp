// Copyright 2024 Google LLC
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

#include "absl/log/absl_check.h"
#include "absl/status/statusor.h"
#include "common/casting.h"
#include "common/type.h"
#include "common/value.h"
#include "internal/status_macros.h"

namespace cel {

ListType ListValue::GetType(TypeManager& type_manager) const {
  return absl::visit(
      [&type_manager](const auto& alternative) -> ListType {
        return alternative.GetType(type_manager);
      },
      variant_);
}

absl::string_view ListValue::GetTypeName() const {
  return absl::visit(
      [](const auto& alternative) -> absl::string_view {
        return alternative.GetTypeName();
      },
      variant_);
}

std::string ListValue::DebugString() const {
  return absl::visit(
      [](const auto& alternative) -> std::string {
        return alternative.DebugString();
      },
      variant_);
}

absl::StatusOr<size_t> ListValue::GetSerializedSize(
    ValueManager& value_manager) const {
  return absl::visit(
      [&value_manager](const auto& alternative) -> absl::StatusOr<size_t> {
        return alternative.GetSerializedSize(value_manager);
      },
      variant_);
}

absl::Status ListValue::SerializeTo(ValueManager& value_manager,
                                    absl::Cord& value) const {
  return absl::visit(
      [&value_manager, &value](const auto& alternative) -> absl::Status {
        return alternative.SerializeTo(value_manager, value);
      },
      variant_);
}

absl::StatusOr<absl::Cord> ListValue::Serialize(
    ValueManager& value_manager) const {
  return absl::visit(
      [&value_manager](const auto& alternative) -> absl::StatusOr<absl::Cord> {
        return alternative.Serialize(value_manager);
      },
      variant_);
}

absl::StatusOr<std::string> ListValue::GetTypeUrl(
    absl::string_view prefix) const {
  return absl::visit(
      [prefix](const auto& alternative) -> absl::StatusOr<std::string> {
        return alternative.GetTypeUrl(prefix);
      },
      variant_);
}

absl::StatusOr<Any> ListValue::ConvertToAny(ValueManager& value_manager,
                                            absl::string_view prefix) const {
  return absl::visit(
      [&value_manager, prefix](const auto& alternative) -> absl::StatusOr<Any> {
        return alternative.ConvertToAny(value_manager, prefix);
      },
      variant_);
}

absl::StatusOr<Json> ListValue::ConvertToJson(
    ValueManager& value_manager) const {
  return absl::visit(
      [&value_manager](const auto& alternative) -> absl::StatusOr<Json> {
        return alternative.ConvertToJson(value_manager);
      },
      variant_);
}

absl::StatusOr<JsonArray> ListValue::ConvertToJsonArray(
    ValueManager& value_manager) const {
  return absl::visit(
      [&value_manager](const auto& alternative) -> absl::StatusOr<JsonArray> {
        return alternative.ConvertToJsonArray(value_manager);
      },
      variant_);
}

bool ListValue::IsZeroValue() const {
  return absl::visit(
      [](const auto& alternative) -> bool { return alternative.IsZeroValue(); },
      variant_);
}

bool ListValue::IsEmpty() const {
  return absl::visit(
      [](const auto& alternative) -> bool { return alternative.IsEmpty(); },
      variant_);
}

size_t ListValue::Size() const {
  return absl::visit(
      [](const auto& alternative) -> size_t { return alternative.Size(); },
      variant_);
}

common_internal::ListValueViewVariant ListValue::ToViewVariant() const {
  return absl::visit(
      [](const auto& alternative) -> common_internal::ListValueViewVariant {
        return common_internal::ListValueViewVariant{
            absl::in_place_type<typename absl::remove_cvref_t<
                decltype(alternative)>::view_alternative_type>,
            alternative};
      },
      variant_);
}

ListType ListValueView::GetType(TypeManager& type_manager) const {
  return absl::visit(
      [&type_manager](auto alternative) -> ListType {
        return alternative.GetType(type_manager);
      },
      variant_);
}

absl::string_view ListValueView::GetTypeName() const {
  return absl::visit(
      [](auto alternative) -> absl::string_view {
        return alternative.GetTypeName();
      },
      variant_);
}

std::string ListValueView::DebugString() const {
  return absl::visit(
      [](auto alternative) -> std::string { return alternative.DebugString(); },
      variant_);
}

absl::StatusOr<size_t> ListValueView::GetSerializedSize(
    ValueManager& value_manager) const {
  return absl::visit(
      [&value_manager](auto alternative) -> absl::StatusOr<size_t> {
        return alternative.GetSerializedSize(value_manager);
      },
      variant_);
}

absl::Status ListValueView::SerializeTo(ValueManager& value_manager,
                                        absl::Cord& value) const {
  return absl::visit(
      [&value_manager, &value](auto alternative) -> absl::Status {
        return alternative.SerializeTo(value_manager, value);
      },
      variant_);
}

absl::StatusOr<absl::Cord> ListValueView::Serialize(
    ValueManager& value_manager) const {
  return absl::visit(
      [&value_manager](auto alternative) -> absl::StatusOr<absl::Cord> {
        return alternative.Serialize(value_manager);
      },
      variant_);
}

absl::StatusOr<std::string> ListValueView::GetTypeUrl(
    absl::string_view prefix) const {
  return absl::visit(
      [prefix](auto alternative) -> absl::StatusOr<std::string> {
        return alternative.GetTypeUrl(prefix);
      },
      variant_);
}

absl::StatusOr<Any> ListValueView::ConvertToAny(
    ValueManager& value_manager, absl::string_view prefix) const {
  return absl::visit(
      [&value_manager, prefix](auto alternative) -> absl::StatusOr<Any> {
        return alternative.ConvertToAny(value_manager, prefix);
      },
      variant_);
}

absl::StatusOr<Json> ListValueView::ConvertToJson(
    ValueManager& value_manager) const {
  return absl::visit(
      [&value_manager](auto alternative) -> absl::StatusOr<Json> {
        return alternative.ConvertToJson(value_manager);
      },
      variant_);
}

absl::StatusOr<JsonArray> ListValueView::ConvertToJsonArray(
    ValueManager& value_manager) const {
  return absl::visit(
      [&value_manager](auto alternative) -> absl::StatusOr<JsonArray> {
        return alternative.ConvertToJsonArray(value_manager);
      },
      variant_);
}

bool ListValueView::IsZeroValue() const {
  return absl::visit(
      [](auto alternative) -> bool { return alternative.IsZeroValue(); },
      variant_);
}

bool ListValueView::IsEmpty() const {
  return absl::visit(
      [](auto alternative) -> bool { return alternative.IsEmpty(); }, variant_);
}

size_t ListValueView::Size() const {
  return absl::visit(
      [](auto alternative) -> size_t { return alternative.Size(); }, variant_);
}

common_internal::ListValueVariant ListValueView::ToVariant() const {
  return absl::visit(
      [](auto alternative) -> common_internal::ListValueVariant {
        return common_internal::ListValueVariant{
            absl::in_place_type<typename absl::remove_cvref_t<
                decltype(alternative)>::alternative_type>,
            alternative};
      },
      variant_);
}

namespace common_internal {

absl::StatusOr<ValueView> ListValueEqual(ValueManager& value_manager,
                                         ListValueView lhs, ListValueView rhs,
                                         Value& scratch) {
  if (Is(lhs, rhs)) {
    return BoolValueView{true};
  }
  const auto lhs_size = lhs.Size();
  const auto rhs_size = rhs.Size();
  if (lhs_size != rhs_size) {
    return BoolValueView{false};
  }
  CEL_ASSIGN_OR_RETURN(auto lhs_iterator, lhs.NewIterator(value_manager));
  CEL_ASSIGN_OR_RETURN(auto rhs_iterator, rhs.NewIterator(value_manager));
  Value lhs_scratch;
  Value rhs_scratch;
  for (size_t index = 0; index < lhs_size; ++index) {
    ABSL_CHECK(lhs_iterator->HasNext());  // Crash OK
    ABSL_CHECK(rhs_iterator->HasNext());  // Crash OK
    CEL_ASSIGN_OR_RETURN(auto lhs_element,
                         lhs_iterator->Next(value_manager, lhs_scratch));
    CEL_ASSIGN_OR_RETURN(auto rhs_element,
                         rhs_iterator->Next(value_manager, lhs_scratch));
    CEL_ASSIGN_OR_RETURN(
        auto result, lhs_element.Equal(value_manager, rhs_element, scratch));
    if (auto bool_value = As<BoolValueView>(result);
        bool_value.has_value() && !bool_value->NativeValue()) {
      return result;
    }
  }
  ABSL_DCHECK(!lhs_iterator->HasNext());
  ABSL_DCHECK(!rhs_iterator->HasNext());
  return BoolValueView{true};
}

absl::StatusOr<ValueView> ListValueEqual(ValueManager& value_manager,
                                         const ParsedListValueInterface& lhs,
                                         ListValueView rhs, Value& scratch) {
  const auto lhs_size = lhs.Size();
  const auto rhs_size = rhs.Size();
  if (lhs_size != rhs_size) {
    return BoolValueView{false};
  }
  CEL_ASSIGN_OR_RETURN(auto lhs_iterator, lhs.NewIterator(value_manager));
  CEL_ASSIGN_OR_RETURN(auto rhs_iterator, rhs.NewIterator(value_manager));
  Value lhs_scratch;
  Value rhs_scratch;
  for (size_t index = 0; index < lhs_size; ++index) {
    ABSL_CHECK(lhs_iterator->HasNext());  // Crash OK
    ABSL_CHECK(rhs_iterator->HasNext());  // Crash OK
    CEL_ASSIGN_OR_RETURN(auto lhs_element,
                         lhs_iterator->Next(value_manager, lhs_scratch));
    CEL_ASSIGN_OR_RETURN(auto rhs_element,
                         rhs_iterator->Next(value_manager, lhs_scratch));
    CEL_ASSIGN_OR_RETURN(
        auto result, lhs_element.Equal(value_manager, rhs_element, scratch));
    if (auto bool_value = As<BoolValueView>(result);
        bool_value.has_value() && !bool_value->NativeValue()) {
      return result;
    }
  }
  ABSL_DCHECK(!lhs_iterator->HasNext());
  ABSL_DCHECK(!rhs_iterator->HasNext());
  return BoolValueView{true};
}

}  // namespace common_internal

}  // namespace cel
