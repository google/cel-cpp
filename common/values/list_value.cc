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
#include "absl/status/status.h"
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
    AnyToJsonConverter& converter) const {
  return absl::visit(
      [&converter](const auto& alternative) -> absl::StatusOr<size_t> {
        return alternative.GetSerializedSize(converter);
      },
      variant_);
}

absl::Status ListValue::SerializeTo(AnyToJsonConverter& converter,
                                    absl::Cord& value) const {
  return absl::visit(
      [&converter, &value](const auto& alternative) -> absl::Status {
        return alternative.SerializeTo(converter, value);
      },
      variant_);
}

absl::StatusOr<absl::Cord> ListValue::Serialize(
    AnyToJsonConverter& converter) const {
  return absl::visit(
      [&converter](const auto& alternative) -> absl::StatusOr<absl::Cord> {
        return alternative.Serialize(converter);
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

absl::StatusOr<Any> ListValue::ConvertToAny(AnyToJsonConverter& converter,
                                            absl::string_view prefix) const {
  return absl::visit(
      [&converter, prefix](const auto& alternative) -> absl::StatusOr<Any> {
        return alternative.ConvertToAny(converter, prefix);
      },
      variant_);
}

absl::StatusOr<Json> ListValue::ConvertToJson(
    AnyToJsonConverter& converter) const {
  return absl::visit(
      [&converter](const auto& alternative) -> absl::StatusOr<Json> {
        return alternative.ConvertToJson(converter);
      },
      variant_);
}

absl::StatusOr<JsonArray> ListValue::ConvertToJsonArray(
    AnyToJsonConverter& converter) const {
  return absl::visit(
      [&converter](const auto& alternative) -> absl::StatusOr<JsonArray> {
        return alternative.ConvertToJsonArray(converter);
      },
      variant_);
}

bool ListValue::IsZeroValue() const {
  return absl::visit(
      [](const auto& alternative) -> bool { return alternative.IsZeroValue(); },
      variant_);
}

absl::StatusOr<bool> ListValue::IsEmpty() const {
  return absl::visit(
      [](const auto& alternative) -> bool { return alternative.IsEmpty(); },
      variant_);
}

absl::StatusOr<size_t> ListValue::Size() const {
  return absl::visit(
      [](const auto& alternative) -> size_t { return alternative.Size(); },
      variant_);
}

namespace common_internal {

absl::Status ListValueEqual(ValueManager& value_manager, const ListValue& lhs,
                            const ListValue& rhs, Value& result) {
  if (Is(lhs, rhs)) {
    result = BoolValue{true};
    return absl::OkStatus();
  }
  CEL_ASSIGN_OR_RETURN(auto lhs_size, lhs.Size());
  CEL_ASSIGN_OR_RETURN(auto rhs_size, rhs.Size());
  if (lhs_size != rhs_size) {
    result = BoolValue{false};
    return absl::OkStatus();
  }
  CEL_ASSIGN_OR_RETURN(auto lhs_iterator, lhs.NewIterator(value_manager));
  CEL_ASSIGN_OR_RETURN(auto rhs_iterator, rhs.NewIterator(value_manager));
  Value lhs_element;
  Value rhs_element;
  for (size_t index = 0; index < lhs_size; ++index) {
    ABSL_CHECK(lhs_iterator->HasNext());  // Crash OK
    ABSL_CHECK(rhs_iterator->HasNext());  // Crash OK
    CEL_RETURN_IF_ERROR(lhs_iterator->Next(value_manager, lhs_element));
    CEL_RETURN_IF_ERROR(rhs_iterator->Next(value_manager, rhs_element));
    CEL_RETURN_IF_ERROR(lhs_element.Equal(value_manager, rhs_element, result));
    if (auto bool_value = As<BoolValue>(result);
        bool_value.has_value() && !bool_value->NativeValue()) {
      return absl::OkStatus();
    }
  }
  ABSL_DCHECK(!lhs_iterator->HasNext());
  ABSL_DCHECK(!rhs_iterator->HasNext());
  result = BoolValue{true};
  return absl::OkStatus();
}

absl::Status ListValueEqual(ValueManager& value_manager,
                            const ParsedListValueInterface& lhs,
                            const ListValue& rhs, Value& result) {
  auto lhs_size = lhs.Size();
  CEL_ASSIGN_OR_RETURN(auto rhs_size, rhs.Size());
  if (lhs_size != rhs_size) {
    result = BoolValue{false};
    return absl::OkStatus();
  }
  CEL_ASSIGN_OR_RETURN(auto lhs_iterator, lhs.NewIterator(value_manager));
  CEL_ASSIGN_OR_RETURN(auto rhs_iterator, rhs.NewIterator(value_manager));
  Value lhs_element;
  Value rhs_element;
  for (size_t index = 0; index < lhs_size; ++index) {
    ABSL_CHECK(lhs_iterator->HasNext());  // Crash OK
    ABSL_CHECK(rhs_iterator->HasNext());  // Crash OK
    CEL_RETURN_IF_ERROR(lhs_iterator->Next(value_manager, lhs_element));
    CEL_RETURN_IF_ERROR(rhs_iterator->Next(value_manager, rhs_element));
    CEL_RETURN_IF_ERROR(lhs_element.Equal(value_manager, rhs_element, result));
    if (auto bool_value = As<BoolValue>(result);
        bool_value.has_value() && !bool_value->NativeValue()) {
      return absl::OkStatus();
    }
  }
  ABSL_DCHECK(!lhs_iterator->HasNext());
  ABSL_DCHECK(!rhs_iterator->HasNext());
  result = BoolValue{true};
  return absl::OkStatus();
}

}  // namespace common_internal

}  // namespace cel
