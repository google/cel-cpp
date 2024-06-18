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

absl::Status Value::Equal(ValueManager& value_manager, ValueView other,
                          Value& result) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, other,
       &result](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an error. In debug
          // builds we cannot reach here.
          return absl::InternalError("use of invalid Value");
        } else {
          return alternative.Equal(value_manager, other, result);
        }
      },
      variant_);
}

absl::StatusOr<Value> Value::Equal(ValueManager& value_manager,
                                   ValueView other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
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

absl::Status ValueView::Equal(ValueManager& value_manager, ValueView other,
                              Value& result) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, other, &result](auto alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          // In optimized builds, we just return an error. In debug
          // builds we cannot reach here.
          return absl::InternalError("use of invalid ValueView");
        } else {
          return alternative.Equal(value_manager, other, result);
        }
      },
      variant_);
}

absl::StatusOr<Value> ValueView::Equal(ValueManager& value_manager,
                                       ValueView other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
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
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> BytesValueView::Equal(ValueManager& value_manager,
                                            ValueView other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> ErrorValue::Equal(ValueManager& value_manager,
                                        ValueView other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> ErrorValueView::Equal(ValueManager& value_manager,
                                            ValueView other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> ListValue::Equal(ValueManager& value_manager,
                                       ValueView other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> ListValueView::Equal(ValueManager& value_manager,
                                           ValueView other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> MapValue::Equal(ValueManager& value_manager,
                                      ValueView other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> MapValueView::Equal(ValueManager& value_manager,
                                          ValueView other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> OpaqueValue::Equal(ValueManager& value_manager,
                                         ValueView other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> OpaqueValueView::Equal(ValueManager& value_manager,
                                             ValueView other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> StringValue::Equal(ValueManager& value_manager,
                                         ValueView other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> StringValueView::Equal(ValueManager& value_manager,
                                             ValueView other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> StructValue::Equal(ValueManager& value_manager,
                                         ValueView other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> StructValueView::Equal(ValueManager& value_manager,
                                             ValueView other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> TypeValue::Equal(ValueManager& value_manager,
                                       ValueView other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> TypeValueView::Equal(ValueManager& value_manager,
                                           ValueView other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> UnknownValue::Equal(ValueManager& value_manager,
                                          ValueView other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::StatusOr<Value> UnknownValueView::Equal(ValueManager& value_manager,
                                              ValueView other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Equal(value_manager, other, result));
  return result;
}

absl::Status ListValue::Get(ValueManager& value_manager, size_t index,
                            Value& result) const {
  return absl::visit(
      [&value_manager, index,
       &result](const auto& alternative) -> absl::Status {
        return alternative.Get(value_manager, index, result);
      },
      variant_);
}

absl::StatusOr<Value> ListValue::Get(ValueManager& value_manager,
                                     size_t index) const {
  Value result;
  CEL_RETURN_IF_ERROR(Get(value_manager, index, result));
  return result;
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

absl::Status ListValue::Equal(ValueManager& value_manager, ValueView other,
                              Value& result) const {
  return absl::visit(
      [&value_manager, other,
       &result](const auto& alternative) -> absl::Status {
        return alternative.Equal(value_manager, other, result);
      },
      variant_);
}

absl::Status ListValue::Contains(ValueManager& value_manager, ValueView other,
                                 Value& result) const {
  return absl::visit(
      [&value_manager, other,
       &result](const auto& alternative) -> absl::Status {
        return alternative.Contains(value_manager, other, result);
      },
      variant_);
}

absl::StatusOr<Value> ListValue::Contains(ValueManager& value_manager,
                                          ValueView other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Contains(value_manager, other, result));
  return result;
}

absl::Status ListValueView::Get(ValueManager& value_manager, size_t index,
                                Value& result) const {
  return absl::visit(
      [&value_manager, index, &result](auto alternative) -> absl::Status {
        return alternative.Get(value_manager, index, result);
      },
      variant_);
}

absl::StatusOr<Value> ListValueView::Get(ValueManager& value_manager,
                                         size_t index) const {
  Value result;
  CEL_RETURN_IF_ERROR(Get(value_manager, index, result));
  return result;
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

absl::Status ListValueView::Equal(ValueManager& value_manager, ValueView other,
                                  Value& result) const {
  return absl::visit(
      [&value_manager, other, &result](auto alternative) -> absl::Status {
        return alternative.Equal(value_manager, other, result);
      },
      variant_);
}

absl::Status ListValueView::Contains(ValueManager& value_manager,
                                     ValueView other, Value& result) const {
  return absl::visit(
      [&value_manager, other, &result](auto alternative) -> absl::Status {
        return alternative.Contains(value_manager, other, result);
      },
      variant_);
}

absl::StatusOr<Value> ListValueView::Contains(ValueManager& value_manager,
                                              ValueView other) const {
  Value result;
  CEL_RETURN_IF_ERROR(Contains(value_manager, other, result));
  return result;
}

absl::Status MapValue::Get(ValueManager& value_manager, ValueView key,
                           Value& result) const {
  return absl::visit(
      [&value_manager, key, &result](const auto& alternative) -> absl::Status {
        return alternative.Get(value_manager, key, result);
      },
      variant_);
}

absl::StatusOr<Value> MapValue::Get(ValueManager& value_manager,
                                    ValueView key) const {
  Value result;
  CEL_RETURN_IF_ERROR(Get(value_manager, key, result));
  return result;
}

absl::StatusOr<bool> MapValue::Find(ValueManager& value_manager, ValueView key,
                                    Value& result) const {
  return absl::visit(
      [&value_manager, key,
       &result](const auto& alternative) -> absl::StatusOr<bool> {
        return alternative.Find(value_manager, key, result);
      },
      variant_);
}

absl::StatusOr<std::pair<Value, bool>> MapValue::Find(
    ValueManager& value_manager, ValueView key) const {
  Value result;
  CEL_ASSIGN_OR_RETURN(auto ok, Find(value_manager, key, result));
  return std::pair{std::move(result), ok};
}

absl::Status MapValue::Has(ValueManager& value_manager, ValueView key,
                           Value& result) const {
  return absl::visit(
      [&value_manager, key, &result](const auto& alternative) -> absl::Status {
        return alternative.Has(value_manager, key, result);
      },
      variant_);
}

absl::StatusOr<Value> MapValue::Has(ValueManager& value_manager,
                                    ValueView key) const {
  Value result;
  CEL_RETURN_IF_ERROR(Has(value_manager, key, result));
  return result;
}

absl::Status MapValue::ListKeys(ValueManager& value_manager,
                                ListValue& result) const {
  return absl::visit(
      [&value_manager, &result](const auto& alternative) -> absl::Status {
        return alternative.ListKeys(value_manager, result);
      },
      variant_);
}

absl::StatusOr<ListValue> MapValue::ListKeys(
    ValueManager& value_manager) const {
  ListValue result;
  CEL_RETURN_IF_ERROR(ListKeys(value_manager, result));
  return result;
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

absl::Status MapValue::Equal(ValueManager& value_manager, ValueView other,
                             Value& result) const {
  return absl::visit(
      [&value_manager, other,
       &result](const auto& alternative) -> absl::Status {
        return alternative.Equal(value_manager, other, result);
      },
      variant_);
}

absl::Status MapValueView::Get(ValueManager& value_manager, ValueView key,
                               Value& result) const {
  return absl::visit(
      [&value_manager, key, &result](auto alternative) -> absl::Status {
        return alternative.Get(value_manager, key, result);
      },
      variant_);
}

absl::StatusOr<Value> MapValueView::Get(ValueManager& value_manager,
                                        ValueView key) const {
  Value result;
  CEL_RETURN_IF_ERROR(Get(value_manager, key, result));
  return result;
}

absl::StatusOr<bool> MapValueView::Find(ValueManager& value_manager,
                                        ValueView key, Value& result) const {
  return absl::visit(
      [&value_manager, key, &result](auto alternative) -> absl::StatusOr<bool> {
        return alternative.Find(value_manager, key, result);
      },
      variant_);
}

absl::StatusOr<std::pair<Value, bool>> MapValueView::Find(
    ValueManager& value_manager, ValueView key) const {
  Value result;
  CEL_ASSIGN_OR_RETURN(auto ok, Find(value_manager, key, result));
  return std::pair{std::move(result), ok};
}

absl::Status MapValueView::Has(ValueManager& value_manager, ValueView key,
                               Value& result) const {
  return absl::visit(
      [&value_manager, key, &result](auto alternative) -> absl::Status {
        return alternative.Has(value_manager, key, result);
      },
      variant_);
}

absl::StatusOr<Value> MapValueView::Has(ValueManager& value_manager,
                                        ValueView key) const {
  Value result;
  CEL_RETURN_IF_ERROR(Has(value_manager, key, result));
  return result;
}

absl::Status MapValueView::ListKeys(ValueManager& value_manager,
                                    ListValue& result) const {
  return absl::visit(
      [&value_manager, &result](auto alternative) -> absl::Status {
        return alternative.ListKeys(value_manager, result);
      },
      variant_);
}

absl::StatusOr<ListValue> MapValueView::ListKeys(
    ValueManager& value_manager) const {
  ListValue result;
  CEL_RETURN_IF_ERROR(ListKeys(value_manager, result));
  return result;
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

absl::Status MapValueView::Equal(ValueManager& value_manager, ValueView other,
                                 Value& result) const {
  return absl::visit(
      [&value_manager, other, &result](auto alternative) -> absl::Status {
        return alternative.Equal(value_manager, other, result);
      },
      variant_);
}

absl::Status StructValue::GetFieldByName(
    ValueManager& value_manager, absl::string_view name, Value& result,
    ProtoWrapperTypeOptions unboxing_options) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, name, &result,
       unboxing_options](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.GetFieldByName(value_manager, name, result,
                                            unboxing_options);
        }
      },
      variant_);
}

absl::StatusOr<Value> StructValue::GetFieldByName(
    ValueManager& value_manager, absl::string_view name,
    ProtoWrapperTypeOptions unboxing_options) const {
  Value result;
  CEL_RETURN_IF_ERROR(
      GetFieldByName(value_manager, name, result, unboxing_options));
  return result;
}

absl::Status StructValue::GetFieldByNumber(
    ValueManager& value_manager, int64_t number, Value& result,
    ProtoWrapperTypeOptions unboxing_options) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, number, &result,
       unboxing_options](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.GetFieldByNumber(value_manager, number, result,
                                              unboxing_options);
        }
      },
      variant_);
}

absl::StatusOr<Value> StructValue::GetFieldByNumber(
    ValueManager& value_manager, int64_t number,
    ProtoWrapperTypeOptions unboxing_options) const {
  Value result;
  CEL_RETURN_IF_ERROR(
      GetFieldByNumber(value_manager, number, result, unboxing_options));
  return result;
}

absl::Status StructValue::Equal(ValueManager& value_manager, ValueView other,
                                Value& result) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, other,
       &result](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.Equal(value_manager, other, result);
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

absl::StatusOr<int> StructValue::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test, Value& result) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, qualifiers, presence_test,
       &result](const auto& alternative) -> absl::StatusOr<int> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.Qualify(value_manager, qualifiers, presence_test,
                                     result);
        }
      },
      variant_);
}

absl::StatusOr<std::pair<Value, int>> StructValue::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test) const {
  Value result;
  CEL_ASSIGN_OR_RETURN(
      auto count, Qualify(value_manager, qualifiers, presence_test, result));
  return std::pair{std::move(result), count};
}

absl::Status StructValueView::GetFieldByName(
    ValueManager& value_manager, absl::string_view name, Value& result,
    ProtoWrapperTypeOptions unboxing_options) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, name, &result,
       unboxing_options](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValueView");
        } else {
          return alternative.GetFieldByName(value_manager, name, result,
                                            unboxing_options);
        }
      },
      variant_);
}

absl::StatusOr<Value> StructValueView::GetFieldByName(
    ValueManager& value_manager, absl::string_view name,
    ProtoWrapperTypeOptions unboxing_options) const {
  Value result;
  CEL_RETURN_IF_ERROR(
      GetFieldByName(value_manager, name, result, unboxing_options));
  return result;
}

absl::Status StructValueView::GetFieldByNumber(
    ValueManager& value_manager, int64_t number, Value& result,
    ProtoWrapperTypeOptions unboxing_options) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, number, &result,
       unboxing_options](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValueView");
        } else {
          return alternative.GetFieldByNumber(value_manager, number, result,
                                              unboxing_options);
        }
      },
      variant_);
}

absl::StatusOr<Value> StructValueView::GetFieldByNumber(
    ValueManager& value_manager, int64_t number,
    ProtoWrapperTypeOptions unboxing_options) const {
  Value result;
  CEL_RETURN_IF_ERROR(
      GetFieldByNumber(value_manager, number, result, unboxing_options));
  return result;
}

absl::Status StructValueView::Equal(ValueManager& value_manager,
                                    ValueView other, Value& result) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, other, &result](auto alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValueView");
        } else {
          return alternative.Equal(value_manager, other, result);
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

absl::StatusOr<int> StructValueView::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test, Value& result) const {
  AssertIsValid();
  return absl::visit(
      [&value_manager, qualifiers, presence_test,
       &result](auto alternative) -> absl::StatusOr<int> {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValueView");
        } else {
          return alternative.Qualify(value_manager, qualifiers, presence_test,
                                     result);
        }
      },
      variant_);
}

absl::StatusOr<std::pair<Value, int>> StructValueView::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test) const {
  Value result;
  CEL_ASSIGN_OR_RETURN(
      auto count, Qualify(value_manager, qualifiers, presence_test, result));
  return std::pair{std::move(result), count};
}

}  // namespace cel
