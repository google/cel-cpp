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

#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "base/attribute.h"
#include "common/optional_ref.h"
#include "common/type.h"
#include "common/value.h"
#include "common/values/value_variant.h"
#include "internal/status_macros.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

StructType StructValue::GetRuntimeType() const {
  AssertIsValid();
  return absl::visit(
      [](const auto& alternative) -> StructType {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          ABSL_UNREACHABLE();
        } else {
          return alternative.GetRuntimeType();
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

absl::Status StructValue::SerializeTo(
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<absl::Cord*> value) const {
  AssertIsValid();
  return absl::visit(
      [descriptor_pool, message_factory,
       value](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          // In optimized builds, we just return an error. In debug builds we
          // cannot reach here.
          return absl::InternalError("use of invalid Value");
        } else {
          return alternative.SerializeTo(descriptor_pool, message_factory,
                                         value);
        }
      },
      variant_);
}

absl::Status StructValue::ConvertToJson(
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Message*> json) const {
  AssertIsValid();
  return absl::visit(
      [descriptor_pool, message_factory,
       json](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          // In optimized builds, we just return an error. In debug
          // builds we cannot reach here.
          return absl::InternalError("use of invalid Value");
        } else {
          return alternative.ConvertToJson(descriptor_pool, message_factory,
                                           json);
        }
      },
      variant_);
}

absl::Status StructValue::ConvertToJsonObject(
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Message*> json) const {
  AssertIsValid();
  return absl::visit(
      [descriptor_pool, message_factory,
       json](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::monostate,
                          absl::remove_cvref_t<decltype(alternative)>>) {
          // In optimized builds, we just return an error. In debug
          // builds we cannot reach here.
          return absl::InternalError("use of invalid Value");
        } else {
          return alternative.ConvertToJsonObject(descriptor_pool,
                                                 message_factory, json);
        }
      },
      variant_);
}

namespace {

template <typename T>
struct IsMonostate : std::is_same<absl::remove_cvref_t<T>, absl::monostate> {};

}  // namespace

absl::Status StructValue::Equal(
    const Value& other,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);
  AssertIsValid();

  return absl::visit(
      [&other, descriptor_pool, message_factory, arena,
       result](const auto& alternative) -> absl::Status {
        if constexpr (IsMonostate<decltype(alternative)>::value) {
          // In optimized builds, we just return an error. In debug
          // builds we cannot reach here.
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.Equal(other, descriptor_pool, message_factory,
                                   arena, result);
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

absl::Status StructValue::GetFieldByName(
    absl::string_view name, ProtoWrapperTypeOptions unboxing_options,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const {
  AssertIsValid();
  return absl::visit(
      [&](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.GetFieldByName(name, unboxing_options,
                                            descriptor_pool, message_factory,
                                            arena, result);
        }
      },
      variant_);
}

absl::Status StructValue::GetFieldByNumber(
    int64_t number, ProtoWrapperTypeOptions unboxing_options,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const {
  AssertIsValid();
  return absl::visit(
      [&](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.GetFieldByNumber(number, unboxing_options,
                                              descriptor_pool, message_factory,
                                              arena, result);
        }
      },
      variant_);
}

absl::Status StructValue::ForEachField(
    ForEachFieldCallback callback,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena) const {
  AssertIsValid();
  return absl::visit(
      [&](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.ForEachField(callback, descriptor_pool,
                                          message_factory, arena);
        }
      },
      variant_);
}

absl::Status StructValue::Qualify(
    absl::Span<const SelectQualifier> qualifiers, bool presence_test,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result,
    absl::Nonnull<int*> count) const {
  AssertIsValid();
  return absl::visit(
      [&](const auto& alternative) -> absl::Status {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return absl::InternalError("use of invalid StructValue");
        } else {
          return alternative.Qualify(qualifiers, presence_test, descriptor_pool,
                                     message_factory, arena, result, count);
        }
      },
      variant_);
}

namespace common_internal {

absl::Status StructValueEqual(
    const StructValue& lhs, const StructValue& rhs,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  if (lhs.GetTypeName() != rhs.GetTypeName()) {
    *result = FalseValue();
    return absl::OkStatus();
  }
  absl::flat_hash_map<std::string, Value> lhs_fields;
  CEL_RETURN_IF_ERROR(lhs.ForEachField(
      [&lhs_fields](absl::string_view name,
                    const Value& lhs_value) -> absl::StatusOr<bool> {
        lhs_fields.insert_or_assign(std::string(name), Value(lhs_value));
        return true;
      },
      descriptor_pool, message_factory, arena));
  bool equal = true;
  size_t rhs_fields_count = 0;
  CEL_RETURN_IF_ERROR(rhs.ForEachField(
      [&](absl::string_view name,
          const Value& rhs_value) -> absl::StatusOr<bool> {
        auto lhs_field = lhs_fields.find(name);
        if (lhs_field == lhs_fields.end()) {
          equal = false;
          return false;
        }
        CEL_RETURN_IF_ERROR(lhs_field->second.Equal(
            rhs_value, descriptor_pool, message_factory, arena, result));
        if (result->IsFalse()) {
          equal = false;
          return false;
        }
        ++rhs_fields_count;
        return true;
      },
      descriptor_pool, message_factory, arena));
  if (!equal || rhs_fields_count != lhs_fields.size()) {
    *result = FalseValue();
    return absl::OkStatus();
  }
  *result = TrueValue();
  return absl::OkStatus();
}

absl::Status StructValueEqual(
    const CustomStructValueInterface& lhs, const StructValue& rhs,
    absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
    absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
    absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) {
  ABSL_DCHECK(descriptor_pool != nullptr);
  ABSL_DCHECK(message_factory != nullptr);
  ABSL_DCHECK(arena != nullptr);
  ABSL_DCHECK(result != nullptr);

  if (lhs.GetTypeName() != rhs.GetTypeName()) {
    *result = FalseValue();
    return absl::OkStatus();
  }
  absl::flat_hash_map<std::string, Value> lhs_fields;
  CEL_RETURN_IF_ERROR(lhs.ForEachField(
      [&lhs_fields](absl::string_view name,
                    const Value& lhs_value) -> absl::StatusOr<bool> {
        lhs_fields.insert_or_assign(std::string(name), Value(lhs_value));
        return true;
      },
      descriptor_pool, message_factory, arena));
  bool equal = true;
  size_t rhs_fields_count = 0;
  CEL_RETURN_IF_ERROR(rhs.ForEachField(
      [&](absl::string_view name,
          const Value& rhs_value) -> absl::StatusOr<bool> {
        auto lhs_field = lhs_fields.find(name);
        if (lhs_field == lhs_fields.end()) {
          equal = false;
          return false;
        }
        CEL_RETURN_IF_ERROR(lhs_field->second.Equal(
            rhs_value, descriptor_pool, message_factory, arena, result));
        if (result->IsFalse()) {
          equal = false;
          return false;
        }
        ++rhs_fields_count;
        return true;
      },
      descriptor_pool, message_factory, arena));
  if (!equal || rhs_fields_count != lhs_fields.size()) {
    *result = FalseValue();
    return absl::OkStatus();
  }
  *result = TrueValue();
  return absl::OkStatus();
}

}  // namespace common_internal

absl::optional<MessageValue> StructValue::AsMessage() const& {
  if (const auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<MessageValue> StructValue::AsMessage() && {
  if (auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

optional_ref<const ParsedMessageValue> StructValue::AsParsedMessage() const& {
  if (const auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return *alternative;
  }
  return absl::nullopt;
}

absl::optional<ParsedMessageValue> StructValue::AsParsedMessage() && {
  if (auto* alternative = absl::get_if<ParsedMessageValue>(&variant_);
      alternative != nullptr) {
    return std::move(*alternative);
  }
  return absl::nullopt;
}

MessageValue StructValue::GetMessage() const& {
  ABSL_DCHECK(IsMessage()) << *this;
  return absl::get<ParsedMessageValue>(variant_);
}

MessageValue StructValue::GetMessage() && {
  ABSL_DCHECK(IsMessage()) << *this;
  return absl::get<ParsedMessageValue>(std::move(variant_));
}

const ParsedMessageValue& StructValue::GetParsedMessage() const& {
  ABSL_DCHECK(IsParsedMessage()) << *this;
  return absl::get<ParsedMessageValue>(variant_);
}

ParsedMessageValue StructValue::GetParsedMessage() && {
  ABSL_DCHECK(IsParsedMessage()) << *this;
  return absl::get<ParsedMessageValue>(std::move(variant_));
}

common_internal::ValueVariant StructValue::ToValueVariant() const& {
  ABSL_DCHECK(IsValid());

  return absl::visit(
      [](const auto& alternative) -> common_internal::ValueVariant {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return common_internal::ValueVariant();
        } else {
          return common_internal::ValueVariant(alternative);
        }
      },
      variant_);
}

common_internal::ValueVariant StructValue::ToValueVariant() && {
  ABSL_DCHECK(IsValid());

  return absl::visit(
      [](auto&& alternative) -> common_internal::ValueVariant {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative)>,
                          absl::monostate>) {
          return common_internal::ValueVariant();
        } else {
          // NOLINTNEXTLINE(bugprone-move-forwarding-reference)
          return common_internal::ValueVariant(std::move(alternative));
        }
      },
      std::move(variant_));
}

}  // namespace cel
