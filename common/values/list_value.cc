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
#include <string>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "common/casting.h"
#include "common/optional_ref.h"
#include "common/value.h"
#include "internal/status_macros.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

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

absl::Status ListValue::SerializeTo(
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Cord& value) const {
  return absl::visit(
      [descriptor_pool, message_factory,
       &value](const auto& alternative) -> absl::Status {
        return alternative.SerializeTo(descriptor_pool, message_factory, value);
      },
      variant_);
}

absl::Status ListValue::ConvertToJson(
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Message*> json) const {
  return absl::visit(
      [descriptor_pool, message_factory,
       json](const auto& alternative) -> absl::Status {
        return alternative.ConvertToJson(descriptor_pool, message_factory,
                                         json);
      },
      variant_);
}

absl::Status ListValue::ConvertToJsonArray(
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Message*> json) const {
  return absl::visit(
      [descriptor_pool, message_factory,
       json](const auto& alternative) -> absl::Status {
        return alternative.ConvertToJsonArray(descriptor_pool, message_factory,
                                              json);
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
                            const CustomListValueInterface& lhs,
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

optional_ref<const CustomListValue> ListValue::AsCustom() const& {
  if (const auto* alt = absl::get_if<CustomListValue>(&variant_);
      alt != nullptr) {
    return *alt;
  }
  return absl::nullopt;
}

absl::optional<CustomListValue> ListValue::AsCustom() && {
  if (auto* alt = absl::get_if<CustomListValue>(&variant_); alt != nullptr) {
    return std::move(*alt);
  }
  return absl::nullopt;
}

const CustomListValue& ListValue::GetCustom() const& {
  ABSL_DCHECK(IsCustom());
  return absl::get<CustomListValue>(variant_);
}

CustomListValue ListValue::GetCustom() && {
  ABSL_DCHECK(IsCustom());
  return absl::get<CustomListValue>(std::move(variant_));
}

common_internal::ValueVariant ListValue::ToValueVariant() const& {
  return absl::visit(
      [](const auto& alternative) -> common_internal::ValueVariant {
        return alternative;
      },
      variant_);
}

common_internal::ValueVariant ListValue::ToValueVariant() && {
  return absl::visit(
      [](auto&& alternative) -> common_internal::ValueVariant {
        return std::move(alternative);
      },
      std::move(variant_));
}

}  // namespace cel
