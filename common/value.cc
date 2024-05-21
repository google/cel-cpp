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

#include "common/value.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "common/any.h"
#include "common/json.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/values.h"
#include "internal/status_macros.h"
#include "runtime/runtime_options.h"

namespace cel {
namespace {

static constexpr std::array<ValueKind, 20> kValueToKindArray = {
    ValueKind::kError,  ValueKind::kBool,      ValueKind::kBytes,
    ValueKind::kDouble, ValueKind::kDuration,  ValueKind::kError,
    ValueKind::kInt,    ValueKind::kList,      ValueKind::kList,
    ValueKind::kMap,    ValueKind::kMap,       ValueKind::kNull,
    ValueKind::kOpaque, ValueKind::kString,    ValueKind::kStruct,
    ValueKind::kStruct, ValueKind::kTimestamp, ValueKind::kType,
    ValueKind::kUint,   ValueKind::kUnknown};

static_assert(kValueToKindArray.size() ==
                  absl::variant_size<common_internal::ValueVariant>(),
              "Kind indexer must match variant declaration for cel::Value.");

static_assert(
    kValueToKindArray.size() ==
        absl::variant_size<common_internal::ValueViewVariant>(),
    "Kind indexer must match variant declaration for cel::ValueView.");

}  // namespace

Type Value::GetType(TypeManager& type_manager) const {
  AssertIsValid();
  return absl::visit(
      [&type_manager](const auto& alternative) -> Type {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an invalid type. In debug
          // builds we cannot reach here.
          return Type();
        } else {
          return alternative.GetType(type_manager);
        }
      },
      variant_);
}

ValueKind Value::kind() const {
  ABSL_DCHECK_NE(variant_.index(), 0)
      << "kind() called on uninitialized cel::Value.";
  return kValueToKindArray[variant_.index()];
}

absl::string_view Value::GetTypeName() const {
  AssertIsValid();
  return absl::visit(
      [](const auto& alternative) -> absl::string_view {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an empty string. In debug
          // builds we cannot reach here.
          return absl::string_view();
        } else {
          return alternative.GetTypeName();
        }
      },
      variant_);
}

std::string Value::DebugString() const {
  AssertIsValid();
  return absl::visit(
      [](const auto& alternative) -> std::string {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an empty string. In debug
          // builds we cannot reach here.
          return std::string();
        } else {
          return alternative.DebugString();
        }
      },
      variant_);
}

absl::StatusOr<size_t> Value::GetSerializedSize(
    AnyToJsonConverter& value_manager) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager](const auto& alternative) -> absl::StatusOr<size_t> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an error. In debug builds we
          // cannot reach here.
          return absl::InternalError("use of invalid Value");
        } else {
          return alternative.GetSerializedSize(value_manager);
        }
      },
      variant_);
}

absl::Status Value::SerializeTo(AnyToJsonConverter& value_manager,
                                absl::Cord& value) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, &value](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an error. In debug builds we
          // cannot reach here.
          return absl::InternalError("use of invalid Value");
        } else {
          return alternative.SerializeTo(value_manager, value);
        }
      },
      variant_);
}

absl::StatusOr<absl::Cord> Value::Serialize(
    AnyToJsonConverter& value_manager) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager](const auto& alternative) -> absl::StatusOr<absl::Cord> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an error. In debug builds we
          // cannot reach here.
          return absl::InternalError("use of invalid Value");
        } else {
          return alternative.Serialize(value_manager);
        }
      },
      variant_);
}

absl::StatusOr<std::string> Value::GetTypeUrl(absl::string_view prefix) const {
  AssertIsValid();
  return absl::visit(
      [prefix](const auto& alternative) -> absl::StatusOr<std::string> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an error. In debug builds we
          // cannot reach here.
          return absl::InternalError("use of invalid Value");
        } else {
          return alternative.GetTypeUrl(prefix);
        }
      },
      variant_);
}

absl::StatusOr<Any> Value::ConvertToAny(AnyToJsonConverter& value_manager,
                                        absl::string_view prefix) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, prefix](const auto& alternative) -> absl::StatusOr<Any> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an error. In debug builds we
          // cannot reach here.
          return absl::InternalError("use of invalid Value");
        } else {
          return alternative.ConvertToAny(value_manager, prefix);
        }
      },
      variant_);
}

absl::StatusOr<Json> Value::ConvertToJson(
    AnyToJsonConverter& value_manager) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager](const auto& alternative) -> absl::StatusOr<Json> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an error. In debug
          // builds we cannot reach here.
          return absl::InternalError("use of invalid Value");
        } else {
          return alternative.ConvertToJson(value_manager);
        }
      },
      variant_);
}

absl::StatusOr<ValueView> Value::Equal(ValueManager& value_manager,
                                       ValueView other, Value& scratch) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, other,
       &scratch](const auto& alternative) -> absl::StatusOr<ValueView> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an error. In debug
          // builds we cannot reach here.
          return absl::InternalError("use of invalid Value");
        } else {
          return alternative.Equal(value_manager, other, scratch);
        }
      },
      variant_);
}

absl::StatusOr<Value> Value::Equal(ValueManager& value_manager,
                                   ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

bool Value::IsZeroValue() const {
  AssertIsValid();
  return absl::visit(
      [](const auto& alternative) -> bool {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return false. In debug
          // builds we cannot reach here.
          return false;
        } else {
          return alternative.IsZeroValue();
        }
      },
      variant_);
}

std::ostream& operator<<(std::ostream& out, const Value& value) {
  return absl::visit(
      [&out](const auto& alternative) -> std::ostream& {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return out << "default ctor Value";
        } else {
          return out << alternative;
        }
      },
      value.variant_);
}

common_internal::ValueViewVariant Value::ToViewVariant() const {
  return absl::visit(
      [](const auto& alternative) -> common_internal::ValueViewVariant {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return common_internal::ValueViewVariant{};
        } else {
          return common_internal::ValueViewVariant(
              absl::in_place_type<typename absl::remove_cvref_t<
                  decltype(alternative)>::view_alternative_type>,
              alternative);
        }
      },
      variant_);
}

ValueKind ValueView::kind() const {
  ABSL_DCHECK_NE(variant_.index(), 0)
      << "kind() called on uninitialized cel::ValueView.";
  return kValueToKindArray[variant_.index()];
}

Type ValueView::GetType(TypeManager& type_manager) const {
  AssertIsValid();
  return absl::visit(
      [&type_manager](auto alternative) -> Type {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an invalid type. In debug
          // builds we cannot reach here.
          return Type();
        } else {
          return alternative.GetType(type_manager);
        }
      },
      variant_);
}

absl::string_view ValueView::GetTypeName() const {
  AssertIsValid();
  return absl::visit(
      [](auto alternative) -> absl::string_view {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an empty string. In debug
          // builds we cannot reach here.
          return absl::string_view();
        } else {
          return alternative.GetTypeName();
        }
      },
      variant_);
}

std::string ValueView::DebugString() const {
  AssertIsValid();
  return absl::visit(
      [](auto alternative) -> std::string {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an empty string. In debug
          // builds we cannot reach here.
          return std::string();
        } else {
          return alternative.DebugString();
        }
      },
      variant_);
}

absl::StatusOr<size_t> ValueView::GetSerializedSize(
    AnyToJsonConverter& value_manager) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager](auto alternative) -> absl::StatusOr<size_t> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an error. In debug builds we
          // cannot reach here.
          return absl::InternalError("use of invalid ValueView");
        } else {
          return alternative.GetSerializedSize(value_manager);
        }
      },
      variant_);
}

absl::Status ValueView::SerializeTo(AnyToJsonConverter& value_manager,
                                    absl::Cord& value) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, &value](auto alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an error. In debug builds we
          // cannot reach here.
          return absl::InternalError("use of invalid ValueView");
        } else {
          return alternative.SerializeTo(value_manager, value);
        }
      },
      variant_);
}

absl::StatusOr<absl::Cord> ValueView::Serialize(
    AnyToJsonConverter& value_manager) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager](auto alternative) -> absl::StatusOr<absl::Cord> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an error. In debug builds we
          // cannot reach here.
          return absl::InternalError("use of invalid ValueView");
        } else {
          return alternative.Serialize(value_manager);
        }
      },
      variant_);
}

absl::StatusOr<std::string> ValueView::GetTypeUrl(
    absl::string_view prefix) const {
  AssertIsValid();
  return absl::visit(
      [prefix](auto alternative) -> absl::StatusOr<std::string> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an error. In debug builds we
          // cannot reach here.
          return absl::InternalError("use of invalid ValueView");
        } else {
          return alternative.GetTypeUrl(prefix);
        }
      },
      variant_);
}

absl::StatusOr<Any> ValueView::ConvertToAny(AnyToJsonConverter& value_manager,
                                            absl::string_view prefix) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, prefix](auto alternative) -> absl::StatusOr<Any> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an error. In debug builds we
          // cannot reach here.
          return absl::InternalError("use of invalid ValueView");
        } else {
          return alternative.ConvertToAny(value_manager, prefix);
        }
      },
      variant_);
}

absl::StatusOr<Json> ValueView::ConvertToJson(
    AnyToJsonConverter& value_manager) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager](auto alternative) -> absl::StatusOr<Json> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an error. In debug
          // builds we cannot reach here.
          return absl::InternalError("use of invalid ValueView");
        } else {
          return alternative.ConvertToJson(value_manager);
        }
      },
      variant_);
}

absl::StatusOr<ValueView> ValueView::Equal(ValueManager& value_manager,
                                           ValueView other,
                                           Value& scratch) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, other,
       &scratch](auto alternative) -> absl::StatusOr<ValueView> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an error. In debug
          // builds we cannot reach here.
          return absl::InternalError("use of invalid ValueView");
        } else {
          return alternative.Equal(value_manager, other, scratch);
        }
      },
      variant_);
}

absl::StatusOr<Value> ValueView::Equal(ValueManager& value_manager,
                                       ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

bool ValueView::IsZeroValue() const {
  AssertIsValid();
  return absl::visit(
      [](auto alternative) -> bool {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return false. In debug
          // builds we cannot reach here.
          return false;
        } else {
          return alternative.IsZeroValue();
        }
      },
      variant_);
}

std::ostream& operator<<(std::ostream& out, ValueView value) {
  return absl::visit(
      [&out](auto alternative) -> std::ostream& {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return out << "default ctor ValueView";
        } else {
          return out << alternative;
        }
      },
      value.variant_);
}

common_internal::ValueVariant ValueView::ToVariant() const {
  return absl::visit(
      [](auto alternative) -> common_internal::ValueVariant {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return common_internal::ValueVariant{};
        } else {
          return common_internal::ValueVariant(
              absl::in_place_type<typename absl::remove_cvref_t<
                  decltype(alternative)>::alternative_type>,
              alternative);
        }
      },
      variant_);
}

absl::StatusOr<Value> BytesValue::Equal(ValueManager& value_manager,
                                        ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> BytesValueView::Equal(ValueManager& value_manager,
                                            ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> DurationValue::Equal(ValueManager& value_manager,
                                           ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> DurationValueView::Equal(ValueManager& value_manager,
                                               ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> ErrorValue::Equal(ValueManager& value_manager,
                                        ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> ErrorValueView::Equal(ValueManager& value_manager,
                                            ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> IntValue::Equal(ValueManager& value_manager,
                                      ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> IntValueView::Equal(ValueManager& value_manager,
                                          ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> ListValue::Equal(ValueManager& value_manager,
                                       ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> ListValueView::Equal(ValueManager& value_manager,
                                           ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> MapValue::Equal(ValueManager& value_manager,
                                      ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> MapValueView::Equal(ValueManager& value_manager,
                                          ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> NullValue::Equal(ValueManager& value_manager,
                                       ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> NullValueView::Equal(ValueManager& value_manager,
                                           ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> OpaqueValue::Equal(ValueManager& value_manager,
                                         ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> OpaqueValueView::Equal(ValueManager& value_manager,
                                             ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> StringValue::Equal(ValueManager& value_manager,
                                         ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> StringValueView::Equal(ValueManager& value_manager,
                                             ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> StructValue::Equal(ValueManager& value_manager,
                                         ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> StructValueView::Equal(ValueManager& value_manager,
                                             ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> TimestampValue::Equal(ValueManager& value_manager,
                                            ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> TimestampValueView::Equal(ValueManager& value_manager,
                                                ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> TypeValue::Equal(ValueManager& value_manager,
                                       ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> TypeValueView::Equal(ValueManager& value_manager,
                                           ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> UintValue::Equal(ValueManager& value_manager,
                                       ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> UintValueView::Equal(ValueManager& value_manager,
                                           ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> UnknownValue::Equal(ValueManager& value_manager,
                                          ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<Value> UnknownValueView::Equal(ValueManager& value_manager,
                                              ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Equal(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<ValueView> ListValue::Get(ValueManager& value_manager,
                                         size_t index, Value& scratch) const {
  return absl::visit(
      [&value_manager, index,
       &scratch](const auto& alternative) -> absl::StatusOr<ValueView> {
        return alternative.Get(value_manager, index, scratch);
      },
      variant_);
}

absl::StatusOr<Value> ListValue::Get(ValueManager& value_manager,
                                     size_t index) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Get(value_manager, index, scratch));
  return Value{result};
}

absl::Status ListValue::ForEach(ValueManager& value_manager,
                                ForEachCallback callback) const {
  return absl::visit(
      [&value_manager, callback](const auto& alternative) -> absl::Status {
        return alternative.ForEach(value_manager, callback);
      },
      variant_);
}

absl::Status ListValue::ForEach(ValueManager& value_manager,
                                ForEachWithIndexCallback callback) const {
  return absl::visit(
      [&value_manager, callback](const auto& alternative) -> absl::Status {
        return alternative.ForEach(value_manager, callback);
      },
      variant_);
}

absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> ListValue::NewIterator(
    ValueManager& value_manager) const {
  return absl::visit(
      [&value_manager](const auto& alternative)
          -> absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> {
        return alternative.NewIterator(value_manager);
      },
      variant_);
}

absl::StatusOr<ValueView> ListValue::Equal(ValueManager& value_manager,
                                           ValueView other,
                                           Value& scratch) const {
  return absl::visit(
      [&value_manager, other,
       &scratch](const auto& alternative) -> absl::StatusOr<ValueView> {
        return alternative.Equal(value_manager, other, scratch);
      },
      variant_);
}

absl::StatusOr<ValueView> ListValue::Contains(ValueManager& value_manager,
                                              ValueView other,
                                              Value& scratch) const {
  return absl::visit(
      [&value_manager, other,
       &scratch](const auto& alternative) -> absl::StatusOr<ValueView> {
        return alternative.Contains(value_manager, other, scratch);
      },
      variant_);
}

absl::StatusOr<Value> ListValue::Contains(ValueManager& value_manager,
                                          ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Contains(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<ValueView> ListValueView::Get(ValueManager& value_manager,
                                             size_t index,
                                             Value& scratch) const {
  return absl::visit(
      [&value_manager, index,
       &scratch](auto alternative) -> absl::StatusOr<ValueView> {
        return alternative.Get(value_manager, index, scratch);
      },
      variant_);
}

absl::StatusOr<Value> ListValueView::Get(ValueManager& value_manager,
                                         size_t index) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Get(value_manager, index, scratch));
  return Value{result};
}

absl::Status ListValueView::ForEach(ValueManager& value_manager,
                                    ForEachCallback callback) const {
  return absl::visit(
      [&value_manager, callback](auto alternative) -> absl::Status {
        return alternative.ForEach(value_manager, callback);
      },
      variant_);
}

absl::Status ListValueView::ForEach(ValueManager& value_manager,
                                    ForEachWithIndexCallback callback) const {
  return absl::visit(
      [&value_manager, callback](auto alternative) -> absl::Status {
        return alternative.ForEach(value_manager, callback);
      },
      variant_);
}

absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> ListValueView::NewIterator(
    ValueManager& value_manager) const {
  return absl::visit(
      [&value_manager](
          auto alternative) -> absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> {
        return alternative.NewIterator(value_manager);
      },
      variant_);
}

absl::StatusOr<ValueView> ListValueView::Equal(ValueManager& value_manager,
                                               ValueView other,
                                               Value& scratch) const {
  return absl::visit(
      [&value_manager, other,
       &scratch](auto alternative) -> absl::StatusOr<ValueView> {
        return alternative.Equal(value_manager, other, scratch);
      },
      variant_);
}

absl::StatusOr<ValueView> ListValueView::Contains(ValueManager& value_manager,
                                                  ValueView other,
                                                  Value& scratch) const {
  return absl::visit(
      [&value_manager, other,
       &scratch](auto alternative) -> absl::StatusOr<ValueView> {
        return alternative.Contains(value_manager, other, scratch);
      },
      variant_);
}

absl::StatusOr<Value> ListValueView::Contains(ValueManager& value_manager,
                                              ValueView other) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Contains(value_manager, other, scratch));
  return Value{result};
}

absl::StatusOr<ValueView> MapValue::Get(ValueManager& value_manager,
                                        ValueView key, Value& scratch) const {
  return absl::visit(
      [&value_manager, key,
       &scratch](const auto& alternative) -> absl::StatusOr<ValueView> {
        return alternative.Get(value_manager, key, scratch);
      },
      variant_);
}

absl::StatusOr<Value> MapValue::Get(ValueManager& value_manager,
                                    ValueView key) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Get(value_manager, key, scratch));
  return Value{result};
}

absl::StatusOr<std::pair<ValueView, bool>> MapValue::Find(
    ValueManager& value_manager, ValueView key, Value& scratch) const {
  return absl::visit(
      [&value_manager, key, &scratch](const auto& alternative)
          -> absl::StatusOr<std::pair<ValueView, bool>> {
        return alternative.Find(value_manager, key, scratch);
      },
      variant_);
}

absl::StatusOr<std::pair<Value, bool>> MapValue::Find(
    ValueManager& value_manager, ValueView key) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Find(value_manager, key, scratch));
  return std::pair{Value{result.first}, result.second};
}

absl::StatusOr<ValueView> MapValue::Has(ValueManager& value_manager,
                                        ValueView key, Value& scratch) const {
  return absl::visit(
      [&value_manager, key,
       &scratch](const auto& alternative) -> absl::StatusOr<ValueView> {
        return alternative.Has(value_manager, key, scratch);
      },
      variant_);
}

absl::StatusOr<Value> MapValue::Has(ValueManager& value_manager,
                                    ValueView key) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Has(value_manager, key, scratch));
  return Value{result};
}

absl::StatusOr<ListValueView> MapValue::ListKeys(ValueManager& value_manager,
                                                 ListValue& scratch) const {
  return absl::visit(
      [&value_manager,
       &scratch](const auto& alternative) -> absl::StatusOr<ListValueView> {
        return alternative.ListKeys(value_manager, scratch);
      },
      variant_);
}

absl::StatusOr<ListValue> MapValue::ListKeys(
    ValueManager& value_manager) const {
  ListValue scratch;
  CEL_ASSIGN_OR_RETURN(auto result, ListKeys(value_manager, scratch));
  return ListValue{result};
}

absl::Status MapValue::ForEach(ValueManager& value_manager,
                               ForEachCallback callback) const {
  return absl::visit(
      [&value_manager, callback](const auto& alternative) -> absl::Status {
        return alternative.ForEach(value_manager, callback);
      },
      variant_);
}

absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> MapValue::NewIterator(
    ValueManager& value_manager) const {
  return absl::visit(
      [&value_manager](const auto& alternative)
          -> absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> {
        return alternative.NewIterator(value_manager);
      },
      variant_);
}

absl::StatusOr<ValueView> MapValue::Equal(ValueManager& value_manager,
                                          ValueView other,
                                          Value& scratch) const {
  return absl::visit(
      [&value_manager, other,
       &scratch](const auto& alternative) -> absl::StatusOr<ValueView> {
        return alternative.Equal(value_manager, other, scratch);
      },
      variant_);
}

absl::StatusOr<ValueView> MapValueView::Get(ValueManager& value_manager,
                                            ValueView key,
                                            Value& scratch) const {
  return absl::visit(
      [&value_manager, key,
       &scratch](auto alternative) -> absl::StatusOr<ValueView> {
        return alternative.Get(value_manager, key, scratch);
      },
      variant_);
}

absl::StatusOr<Value> MapValueView::Get(ValueManager& value_manager,
                                        ValueView key) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Get(value_manager, key, scratch));
  return Value{result};
}

absl::StatusOr<std::pair<ValueView, bool>> MapValueView::Find(
    ValueManager& value_manager, ValueView key, Value& scratch) const {
  return absl::visit(
      [&value_manager, key, &scratch](
          auto alternative) -> absl::StatusOr<std::pair<ValueView, bool>> {
        return alternative.Find(value_manager, key, scratch);
      },
      variant_);
}

absl::StatusOr<std::pair<Value, bool>> MapValueView::Find(
    ValueManager& value_manager, ValueView key) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Find(value_manager, key, scratch));
  return std::pair{Value{result.first}, result.second};
}

absl::StatusOr<ValueView> MapValueView::Has(ValueManager& value_manager,
                                            ValueView key,
                                            Value& scratch) const {
  return absl::visit(
      [&value_manager, key,
       &scratch](auto alternative) -> absl::StatusOr<ValueView> {
        return alternative.Has(value_manager, key, scratch);
      },
      variant_);
}

absl::StatusOr<Value> MapValueView::Has(ValueManager& value_manager,
                                        ValueView key) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, Has(value_manager, key, scratch));
  return Value{result};
}

absl::StatusOr<ListValueView> MapValueView::ListKeys(
    ValueManager& value_manager, ListValue& scratch) const {
  return absl::visit(
      [&value_manager,
       &scratch](auto alternative) -> absl::StatusOr<ListValueView> {
        return alternative.ListKeys(value_manager, scratch);
      },
      variant_);
}

absl::StatusOr<ListValue> MapValueView::ListKeys(
    ValueManager& value_manager) const {
  ListValue scratch;
  CEL_ASSIGN_OR_RETURN(auto result, ListKeys(value_manager, scratch));
  return ListValue{result};
}

absl::Status MapValueView::ForEach(ValueManager& value_manager,
                                   ForEachCallback callback) const {
  return absl::visit(
      [&value_manager, callback](auto alternative) -> absl::Status {
        return alternative.ForEach(value_manager, callback);
      },
      variant_);
}

absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> MapValueView::NewIterator(
    ValueManager& value_manager) const {
  return absl::visit(
      [&value_manager](
          auto alternative) -> absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> {
        return alternative.NewIterator(value_manager);
      },
      variant_);
}

absl::StatusOr<ValueView> MapValueView::Equal(ValueManager& value_manager,
                                              ValueView other,
                                              Value& scratch) const {
  return absl::visit(
      [&value_manager, other,
       &scratch](auto alternative) -> absl::StatusOr<ValueView> {
        return alternative.Equal(value_manager, other, scratch);
      },
      variant_);
}

absl::StatusOr<ValueView> StructValue::GetFieldByName(
    ValueManager& value_manager, absl::string_view name, Value& scratch,
    ProtoWrapperTypeOptions unboxing_options) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, name, &scratch,
       unboxing_options](const auto& alternative) -> absl::StatusOr<ValueView> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.GetFieldByName(value_manager, name, scratch,
                                            unboxing_options);
        }
      },
      variant_);
}

absl::StatusOr<Value> StructValue::GetFieldByName(
    ValueManager& value_manager, absl::string_view name,
    ProtoWrapperTypeOptions unboxing_options) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, GetFieldByName(value_manager, name, scratch,
                                                   unboxing_options));
  return Value{result};
}

absl::StatusOr<ValueView> StructValue::GetFieldByNumber(
    ValueManager& value_manager, int64_t number, Value& scratch,
    ProtoWrapperTypeOptions unboxing_options) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, number, &scratch,
       unboxing_options](const auto& alternative) -> absl::StatusOr<ValueView> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.GetFieldByNumber(value_manager, number, scratch,
                                              unboxing_options);
        }
      },
      variant_);
}

absl::StatusOr<Value> StructValue::GetFieldByNumber(
    ValueManager& value_manager, int64_t number,
    ProtoWrapperTypeOptions unboxing_options) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(
      auto result,
      GetFieldByNumber(value_manager, number, scratch, unboxing_options));
  return Value{result};
}

absl::StatusOr<ValueView> StructValue::Equal(ValueManager& value_manager,
                                             ValueView other,
                                             Value& scratch) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, other,
       &scratch](const auto& alternative) -> absl::StatusOr<ValueView> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.Equal(value_manager, other, scratch);
        }
      },
      variant_);
}

absl::Status StructValue::ForEachField(ValueManager& value_manager,
                                       ForEachFieldCallback callback) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, callback](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.ForEachField(value_manager, callback);
        }
      },
      variant_);
}

absl::StatusOr<std::pair<ValueView, int>> StructValue::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test, Value& scratch) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, qualifiers, presence_test,
       &scratch](const auto& alternative)
          -> absl::StatusOr<std::pair<ValueView, int>> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.Qualify(value_manager, qualifiers, presence_test,
                                     scratch);
        }
      },
      variant_);
}

absl::StatusOr<std::pair<Value, int>> StructValue::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(
      auto result, Qualify(value_manager, qualifiers, presence_test, scratch));
  return std::pair{Value{result.first}, result.second};
}

absl::StatusOr<ValueView> StructValueView::GetFieldByName(
    ValueManager& value_manager, absl::string_view name, Value& scratch,
    ProtoWrapperTypeOptions unboxing_options) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, name, &scratch,
       unboxing_options](const auto& alternative) -> absl::StatusOr<ValueView> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValueView");
        } else {
          return alternative.GetFieldByName(value_manager, name, scratch,
                                            unboxing_options);
        }
      },
      variant_);
}

absl::StatusOr<Value> StructValueView::GetFieldByName(
    ValueManager& value_manager, absl::string_view name,
    ProtoWrapperTypeOptions unboxing_options) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(auto result, GetFieldByName(value_manager, name, scratch,
                                                   unboxing_options));
  return Value{result};
}

absl::StatusOr<ValueView> StructValueView::GetFieldByNumber(
    ValueManager& value_manager, int64_t number, Value& scratch,
    ProtoWrapperTypeOptions unboxing_options) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, number, &scratch,
       unboxing_options](const auto& alternative) -> absl::StatusOr<ValueView> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValueView");
        } else {
          return alternative.GetFieldByNumber(value_manager, number, scratch,
                                              unboxing_options);
        }
      },
      variant_);
}

absl::StatusOr<Value> StructValueView::GetFieldByNumber(
    ValueManager& value_manager, int64_t number,
    ProtoWrapperTypeOptions unboxing_options) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(
      auto result,
      GetFieldByNumber(value_manager, number, scratch, unboxing_options));
  return Value{result};
}

absl::StatusOr<ValueView> StructValueView::Equal(ValueManager& value_manager,
                                                 ValueView other,
                                                 Value& scratch) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, other,
       &scratch](auto alternative) -> absl::StatusOr<ValueView> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValueView");
        } else {
          return alternative.Equal(value_manager, other, scratch);
        }
      },
      variant_);
}

absl::Status StructValueView::ForEachField(
    ValueManager& value_manager, ForEachFieldCallback callback) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, callback](auto alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValueView");
        } else {
          return alternative.ForEachField(value_manager, callback);
        }
      },
      variant_);
}

absl::StatusOr<std::pair<ValueView, int>> StructValueView::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test, Value& scratch) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, qualifiers, presence_test, &scratch](
          auto alternative) -> absl::StatusOr<std::pair<ValueView, int>> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValueView");
        } else {
          return alternative.Qualify(value_manager, qualifiers, presence_test,
                                     scratch);
        }
      },
      variant_);
}

absl::StatusOr<std::pair<Value, int>> StructValueView::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test) const {
  Value scratch;
  CEL_ASSIGN_OR_RETURN(
      auto result, Qualify(value_manager, qualifiers, presence_test, scratch));
  return std::pair{Value{result.first}, result.second};
}

}  // namespace cel
