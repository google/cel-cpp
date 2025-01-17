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
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "common/casting.h"
#include "common/optional_ref.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "internal/status_macros.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

namespace {

absl::Status InvalidMapKeyTypeError(ValueKind kind) {
  return absl::InvalidArgumentError(
      absl::StrCat("Invalid map key type: '", ValueKindToString(kind), "'"));
}

}  // namespace

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

absl::Status MapValue::SerializeTo(
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

absl::Status MapValue::ConvertToJson(
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

absl::Status MapValue::ConvertToJsonObject(
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Message*> json) const {
  return absl::visit(
      [descriptor_pool, message_factory,
       json](const auto& alternative) -> absl::Status {
        return alternative.ConvertToJsonObject(descriptor_pool, message_factory,
                                               json);
      },
      variant_);
}

bool MapValue::IsZeroValue() const {
  return absl::visit(
      [](const auto& alternative) -> bool { return alternative.IsZeroValue(); },
      variant_);
}

absl::StatusOr<bool> MapValue::IsEmpty() const {
  return absl::visit(
      [](const auto& alternative) -> bool { return alternative.IsEmpty(); },
      variant_);
}

absl::StatusOr<size_t> MapValue::Size() const {
  return absl::visit(
      [](const auto& alternative) -> size_t { return alternative.Size(); },
      variant_);
}

namespace common_internal {

absl::Status MapValueEqual(ValueManager& value_manager, const MapValue& lhs,
                           const MapValue& rhs, Value& result) {
  CEL_ASSIGN_OR_RETURN(auto lhs_size, lhs.Size());
  CEL_ASSIGN_OR_RETURN(auto rhs_size, rhs.Size());
  if (lhs_size != rhs_size) {
    result = BoolValue{false};
    return absl::OkStatus();
  }
  CEL_ASSIGN_OR_RETURN(auto lhs_iterator, lhs.NewIterator(value_manager));
  Value lhs_key;
  Value lhs_value;
  Value rhs_value;
  for (size_t index = 0; index < lhs_size; ++index) {
    ABSL_CHECK(lhs_iterator->HasNext());  // Crash OK
    CEL_RETURN_IF_ERROR(lhs_iterator->Next(value_manager, lhs_key));
    bool rhs_value_found;
    CEL_ASSIGN_OR_RETURN(rhs_value_found,
                         rhs.Find(value_manager, lhs_key, rhs_value));
    if (!rhs_value_found) {
      result = BoolValue{false};
      return absl::OkStatus();
    }
    CEL_RETURN_IF_ERROR(lhs.Get(value_manager, lhs_key, lhs_value));
    CEL_RETURN_IF_ERROR(lhs_value.Equal(value_manager, rhs_value, result));
    if (auto bool_value = As<BoolValue>(result);
        bool_value.has_value() && !bool_value->NativeValue()) {
      return absl::OkStatus();
    }
  }
  ABSL_DCHECK(!lhs_iterator->HasNext());
  result = BoolValue{true};
  return absl::OkStatus();
}

absl::Status MapValueEqual(ValueManager& value_manager,
                           const CustomMapValueInterface& lhs,
                           const MapValue& rhs, Value& result) {
  auto lhs_size = lhs.Size();
  CEL_ASSIGN_OR_RETURN(auto rhs_size, rhs.Size());
  if (lhs_size != rhs_size) {
    result = BoolValue{false};
    return absl::OkStatus();
  }
  CEL_ASSIGN_OR_RETURN(auto lhs_iterator, lhs.NewIterator(value_manager));
  Value lhs_key;
  Value lhs_value;
  Value rhs_value;
  for (size_t index = 0; index < lhs_size; ++index) {
    ABSL_CHECK(lhs_iterator->HasNext());  // Crash OK
    CEL_RETURN_IF_ERROR(lhs_iterator->Next(value_manager, lhs_key));
    bool rhs_value_found;
    CEL_ASSIGN_OR_RETURN(rhs_value_found,
                         rhs.Find(value_manager, lhs_key, rhs_value));
    if (!rhs_value_found) {
      result = BoolValue{false};
      return absl::OkStatus();
    }
    CEL_RETURN_IF_ERROR(lhs.Get(value_manager, lhs_key, lhs_value));
    CEL_RETURN_IF_ERROR(lhs_value.Equal(value_manager, rhs_value, result));
    if (auto bool_value = As<BoolValue>(result);
        bool_value.has_value() && !bool_value->NativeValue()) {
      return absl::OkStatus();
    }
  }
  ABSL_DCHECK(!lhs_iterator->HasNext());
  result = BoolValue{true};
  return absl::OkStatus();
}

}  // namespace common_internal

absl::Status CheckMapKey(const Value& key) {
  switch (key.kind()) {
    case ValueKind::kBool:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kInt:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUint:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kString:
      return absl::OkStatus();
    case ValueKind::kError:
      return key.GetError().NativeValue();
    default:
      return InvalidMapKeyTypeError(key.kind());
  }
}

optional_ref<const CustomMapValue> MapValue::AsCustom() const& {
  if (const auto* alt = absl::get_if<CustomMapValue>(&variant_);
      alt != nullptr) {
    return *alt;
  }
  return absl::nullopt;
}

absl::optional<CustomMapValue> MapValue::AsCustom() && {
  if (auto* alt = absl::get_if<CustomMapValue>(&variant_); alt != nullptr) {
    return std::move(*alt);
  }
  return absl::nullopt;
}

const CustomMapValue& MapValue::GetCustom() const& {
  ABSL_DCHECK(IsCustom());
  return absl::get<CustomMapValue>(variant_);
}

CustomMapValue MapValue::GetCustom() && {
  ABSL_DCHECK(IsCustom());
  return absl::get<CustomMapValue>(std::move(variant_));
}

common_internal::ValueVariant MapValue::ToValueVariant() const& {
  return absl::visit(
      [](const auto& alternative) -> common_internal::ValueVariant {
        return alternative;
      },
      variant_);
}

common_internal::ValueVariant MapValue::ToValueVariant() && {
  return absl::visit(
      [](auto&& alternative) -> common_internal::ValueVariant {
        return std::move(alternative);
      },
      std::move(variant_));
}

}  // namespace cel
