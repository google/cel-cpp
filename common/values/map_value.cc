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
#include <tuple>

#include "absl/base/attributes.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "common/casting.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "internal/status_macros.h"

namespace cel {

namespace {

absl::Status InvalidMapKeyTypeError(ValueKind kind) {
  return absl::InvalidArgumentError(
      absl::StrCat("Invalid map key type: '", ValueKindToString(kind), "'"));
}

}  // namespace

MapType MapValue::GetType(TypeManager& type_manager) const {
  return absl::visit(
      [&type_manager](const auto& alternative) -> MapType {
        return alternative.GetType(type_manager);
      },
      variant_);
}

absl::string_view MapValue::GetTypeName() const {
  return absl::visit(
      [](const auto& alternative) -> absl::string_view {
        return alternative.GetTypeName();
      },
      variant_);
}

std::string MapValue::DebugString() const {
  return absl::visit(
      [](const auto& alternative) -> std::string {
        return alternative.DebugString();
      },
      variant_);
}

absl::StatusOr<size_t> MapValue::GetSerializedSize(
    AnyToJsonConverter& converter) const {
  return absl::visit(
      [&converter](const auto& alternative) -> absl::StatusOr<size_t> {
        return alternative.GetSerializedSize(converter);
      },
      variant_);
}

absl::Status MapValue::SerializeTo(AnyToJsonConverter& converter,
                                   absl::Cord& value) const {
  return absl::visit(
      [&converter, &value](const auto& alternative) -> absl::Status {
        return alternative.SerializeTo(converter, value);
      },
      variant_);
}

absl::StatusOr<absl::Cord> MapValue::Serialize(
    AnyToJsonConverter& converter) const {
  return absl::visit(
      [&converter](const auto& alternative) -> absl::StatusOr<absl::Cord> {
        return alternative.Serialize(converter);
      },
      variant_);
}

absl::StatusOr<std::string> MapValue::GetTypeUrl(
    absl::string_view prefix) const {
  return absl::visit(
      [prefix](const auto& alternative) -> absl::StatusOr<std::string> {
        return alternative.GetTypeUrl(prefix);
      },
      variant_);
}

absl::StatusOr<Any> MapValue::ConvertToAny(AnyToJsonConverter& converter,
                                           absl::string_view prefix) const {
  return absl::visit(
      [&converter, prefix](const auto& alternative) -> absl::StatusOr<Any> {
        return alternative.ConvertToAny(converter, prefix);
      },
      variant_);
}

absl::StatusOr<Json> MapValue::ConvertToJson(
    AnyToJsonConverter& converter) const {
  return absl::visit(
      [&converter](const auto& alternative) -> absl::StatusOr<Json> {
        return alternative.ConvertToJson(converter);
      },
      variant_);
}

absl::StatusOr<JsonObject> MapValue::ConvertToJsonObject(
    AnyToJsonConverter& converter) const {
  return absl::visit(
      [&converter](const auto& alternative) -> absl::StatusOr<JsonObject> {
        return alternative.ConvertToJsonObject(converter);
      },
      variant_);
}

bool MapValue::IsZeroValue() const {
  return absl::visit(
      [](const auto& alternative) -> bool { return alternative.IsZeroValue(); },
      variant_);
}

bool MapValue::IsEmpty() const {
  return absl::visit(
      [](const auto& alternative) -> bool { return alternative.IsEmpty(); },
      variant_);
}

size_t MapValue::Size() const {
  return absl::visit(
      [](const auto& alternative) -> size_t { return alternative.Size(); },
      variant_);
}

common_internal::MapValueViewVariant MapValue::ToViewVariant() const {
  return absl::visit(
      [](const auto& alternative) -> common_internal::MapValueViewVariant {
        return common_internal::MapValueViewVariant{
            absl::in_place_type<typename absl::remove_cvref_t<
                decltype(alternative)>::view_alternative_type>,
            alternative};
      },
      variant_);
}

MapType MapValueView::GetType(TypeManager& type_manager) const {
  return absl::visit(
      [&type_manager](auto alternative) -> MapType {
        return alternative.GetType(type_manager);
      },
      variant_);
}

absl::string_view MapValueView::GetTypeName() const {
  return absl::visit(
      [](auto alternative) -> absl::string_view {
        return alternative.GetTypeName();
      },
      variant_);
}

std::string MapValueView::DebugString() const {
  return absl::visit(
      [](auto alternative) -> std::string { return alternative.DebugString(); },
      variant_);
}

absl::StatusOr<size_t> MapValueView::GetSerializedSize(
    AnyToJsonConverter& converter) const {
  return absl::visit(
      [&converter](auto alternative) -> absl::StatusOr<size_t> {
        return alternative.GetSerializedSize(converter);
      },
      variant_);
}

absl::Status MapValueView::SerializeTo(AnyToJsonConverter& converter,
                                       absl::Cord& value) const {
  return absl::visit(
      [&converter, &value](auto alternative) -> absl::Status {
        return alternative.SerializeTo(converter, value);
      },
      variant_);
}

absl::StatusOr<absl::Cord> MapValueView::Serialize(
    AnyToJsonConverter& converter) const {
  return absl::visit(
      [&converter](auto alternative) -> absl::StatusOr<absl::Cord> {
        return alternative.Serialize(converter);
      },
      variant_);
}

absl::StatusOr<std::string> MapValueView::GetTypeUrl(
    absl::string_view prefix) const {
  return absl::visit(
      [prefix](auto alternative) -> absl::StatusOr<std::string> {
        return alternative.GetTypeUrl(prefix);
      },
      variant_);
}

absl::StatusOr<Any> MapValueView::ConvertToAny(AnyToJsonConverter& converter,
                                               absl::string_view prefix) const {
  return absl::visit(
      [&converter, prefix](auto alternative) -> absl::StatusOr<Any> {
        return alternative.ConvertToAny(converter, prefix);
      },
      variant_);
}

absl::StatusOr<Json> MapValueView::ConvertToJson(
    AnyToJsonConverter& converter) const {
  return absl::visit(
      [&converter](auto alternative) -> absl::StatusOr<Json> {
        return alternative.ConvertToJson(converter);
      },
      variant_);
}

absl::StatusOr<JsonObject> MapValueView::ConvertToJsonObject(
    AnyToJsonConverter& converter) const {
  return absl::visit(
      [&converter](auto alternative) -> absl::StatusOr<JsonObject> {
        return alternative.ConvertToJsonObject(converter);
      },
      variant_);
}

bool MapValueView::IsZeroValue() const {
  return absl::visit(
      [](auto alternative) -> bool { return alternative.IsZeroValue(); },
      variant_);
}

bool MapValueView::IsEmpty() const {
  return absl::visit(
      [](auto alternative) -> bool { return alternative.IsEmpty(); }, variant_);
}

size_t MapValueView::Size() const {
  return absl::visit(
      [](auto alternative) -> size_t { return alternative.Size(); }, variant_);
}

common_internal::MapValueVariant MapValueView::ToVariant() const {
  return absl::visit(
      [](auto alternative) -> common_internal::MapValueVariant {
        return common_internal::MapValueVariant{
            absl::in_place_type<typename absl::remove_cvref_t<
                decltype(alternative)>::alternative_type>,
            alternative};
      },
      variant_);
}

namespace common_internal {

absl::StatusOr<ValueView> MapValueEqual(ValueManager& value_manager,
                                        MapValueView lhs, MapValueView rhs,
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
  Value lhs_key_scratch;
  Value rhs_value_scratch;
  ValueView rhs_value;
  Value lhs_value_scratch;
  for (size_t index = 0; index < lhs_size; ++index) {
    ABSL_CHECK(lhs_iterator->HasNext());  // Crash OK
    CEL_ASSIGN_OR_RETURN(auto lhs_key,
                         lhs_iterator->Next(value_manager, lhs_key_scratch));
    bool rhs_value_found;
    CEL_ASSIGN_OR_RETURN(std::tie(rhs_value, rhs_value_found),
                         rhs.Find(value_manager, lhs_key, rhs_value_scratch));
    if (!rhs_value_found) {
      return BoolValueView{false};
    }
    CEL_ASSIGN_OR_RETURN(auto lhs_value,
                         lhs.Get(value_manager, lhs_key, lhs_value_scratch));
    CEL_ASSIGN_OR_RETURN(auto result,
                         lhs_value.Equal(value_manager, rhs_value, scratch));
    if (auto bool_value = As<BoolValueView>(result);
        bool_value.has_value() && !bool_value->NativeValue()) {
      return result;
    }
  }
  ABSL_DCHECK(!lhs_iterator->HasNext());
  return BoolValueView{true};
}

absl::StatusOr<ValueView> MapValueEqual(ValueManager& value_manager,
                                        const ParsedMapValueInterface& lhs,
                                        MapValueView rhs, Value& scratch) {
  const auto lhs_size = lhs.Size();
  const auto rhs_size = rhs.Size();
  if (lhs_size != rhs_size) {
    return BoolValueView{false};
  }
  CEL_ASSIGN_OR_RETURN(auto lhs_iterator, lhs.NewIterator(value_manager));
  Value lhs_key_scratch;
  Value rhs_value_scratch;
  ValueView rhs_value;
  Value lhs_value_scratch;
  for (size_t index = 0; index < lhs_size; ++index) {
    ABSL_CHECK(lhs_iterator->HasNext());  // Crash OK
    CEL_ASSIGN_OR_RETURN(auto lhs_key,
                         lhs_iterator->Next(value_manager, lhs_key_scratch));
    bool rhs_value_found;
    CEL_ASSIGN_OR_RETURN(std::tie(rhs_value, rhs_value_found),
                         rhs.Find(value_manager, lhs_key, rhs_value_scratch));
    if (!rhs_value_found) {
      return BoolValueView{false};
    }
    CEL_ASSIGN_OR_RETURN(auto lhs_value,
                         lhs.Get(value_manager, lhs_key, lhs_value_scratch));
    CEL_ASSIGN_OR_RETURN(auto result,
                         lhs_value.Equal(value_manager, rhs_value, scratch));
    if (auto bool_value = As<BoolValueView>(result);
        bool_value.has_value() && !bool_value->NativeValue()) {
      return result;
    }
  }
  ABSL_DCHECK(!lhs_iterator->HasNext());
  return BoolValueView{true};
}

}  // namespace common_internal

absl::Status CheckMapKey(ValueView key) {
  switch (key.kind()) {
    case ValueKind::kBool:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kInt:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUint:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kString:
      return absl::OkStatus();
    default:
      return InvalidMapKeyTypeError(key.kind());
  }
}

}  // namespace cel
