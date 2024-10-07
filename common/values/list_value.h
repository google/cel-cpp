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

// IWYU pragma: private, include "common/value.h"
// IWYU pragma: friend "common/value.h"

// `ListValue` represents values of the primitive `list` type.
// `ListValueInterface` is the abstract base class of implementations.
// `ListValue` acts as a smart pointer to `ListValueInterface`.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_LIST_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_LIST_VALUE_H_

#include <cstddef>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/log/absl_check.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "absl/utility/utility.h"
#include "common/json.h"
#include "common/native_type.h"
#include "common/value_kind.h"
#include "common/values/legacy_list_value.h"  // IWYU pragma: export
#include "common/values/list_value_interface.h"  // IWYU pragma: export
#include "common/values/parsed_json_list_value.h"
#include "common/values/parsed_list_value.h"  // IWYU pragma: export
#include "common/values/parsed_repeated_field_value.h"
#include "common/values/values.h"

namespace cel {

class ListValueInterface;
class ListValue;
class Value;
class ValueManager;
class TypeManager;

bool Is(const ListValue& lhs, const ListValue& rhs);

class ListValue final {
 public:
  using interface_type = ListValueInterface;

  static constexpr ValueKind kKind = ListValueInterface::kKind;

  // Copy constructor for alternative struct values.
  template <
      typename T,
      typename = std::enable_if_t<
          common_internal::IsListValueAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  ListValue(const T& value)
      : variant_(
            absl::in_place_type<common_internal::BaseListValueAlternativeForT<
                absl::remove_cvref_t<T>>>,
            value) {}

  // Move constructor for alternative struct values.
  template <
      typename T,
      typename = std::enable_if_t<
          common_internal::IsListValueAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  ListValue(T&& value)
      : variant_(
            absl::in_place_type<common_internal::BaseListValueAlternativeForT<
                absl::remove_cvref_t<T>>>,
            std::forward<T>(value)) {}

  ListValue() = default;
  ListValue(const ListValue&) = default;
  ListValue(ListValue&&) = default;

  // NOLINTNEXTLINE(google-explicit-constructor)
  ListValue(const ParsedRepeatedFieldValue& other)
      : variant_(absl::in_place_type<ParsedRepeatedFieldValue>, other) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  ListValue(ParsedRepeatedFieldValue&& other)
      : variant_(absl::in_place_type<ParsedRepeatedFieldValue>,
                 std::move(other)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  ListValue(const ParsedJsonListValue& other)
      : variant_(absl::in_place_type<ParsedJsonListValue>, other) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  ListValue(ParsedJsonListValue&& other)
      : variant_(absl::in_place_type<ParsedJsonListValue>, std::move(other)) {}

  ListValue& operator=(const ListValue& other) {
    ABSL_DCHECK(this != std::addressof(other))
        << "ListValue should not be copied to itself";
    variant_ = other.variant_;
    return *this;
  }

  ListValue& operator=(ListValue&& other) noexcept {
    ABSL_DCHECK(this != std::addressof(other))
        << "ListValue should not be moved to itself";
    variant_ = std::move(other.variant_);
    other.variant_.emplace<ParsedListValue>();
    return *this;
  }

  constexpr ValueKind kind() const { return kKind; }

  absl::string_view GetTypeName() const;

  std::string DebugString() const;

  absl::Status SerializeTo(AnyToJsonConverter& converter,
                           absl::Cord& value) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter& converter) const;

  absl::StatusOr<JsonArray> ConvertToJsonArray(
      AnyToJsonConverter& converter) const;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const;

  void swap(ListValue& other) noexcept { variant_.swap(other.variant_); }

  absl::StatusOr<bool> IsEmpty() const;

  absl::StatusOr<size_t> Size() const;

  // See ListValueInterface::Get for documentation.
  absl::Status Get(ValueManager& value_manager, size_t index,
                   Value& result) const;
  absl::StatusOr<Value> Get(ValueManager& value_manager, size_t index) const;

  using ForEachCallback = typename ListValueInterface::ForEachCallback;

  using ForEachWithIndexCallback =
      typename ListValueInterface::ForEachWithIndexCallback;

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachCallback callback) const;

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachWithIndexCallback callback) const;

  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueManager& value_manager) const;

  absl::Status Contains(ValueManager& value_manager, const Value& other,
                        Value& result) const;
  absl::StatusOr<Value> Contains(ValueManager& value_manager,
                                 const Value& other) const;

 private:
  friend class Value;
  friend struct NativeTypeTraits<ListValue>;
  friend bool Is(const ListValue& lhs, const ListValue& rhs);

  common_internal::ValueVariant ToValueVariant() const&;
  common_internal::ValueVariant ToValueVariant() &&;

  // Unlike many of the other derived values, `ListValue` is itself a composed
  // type. This is to avoid making `ListValue` too big and by extension
  // `Value` too big. Instead we store the derived `ListValue` values in
  // `Value` and not `ListValue` itself.
  common_internal::ListValueVariant variant_;
};

inline void swap(ListValue& lhs, ListValue& rhs) noexcept { lhs.swap(rhs); }

inline std::ostream& operator<<(std::ostream& out, const ListValue& value) {
  return out << value.DebugString();
}

template <>
struct NativeTypeTraits<ListValue> final {
  static NativeTypeId Id(const ListValue& value) {
    return absl::visit(
        [](const auto& alternative) -> NativeTypeId {
          return NativeTypeId::Of(alternative);
        },
        value.variant_);
  }

  static bool SkipDestructor(const ListValue& value) {
    return absl::visit(
        [](const auto& alternative) -> bool {
          return NativeType::SkipDestructor(alternative);
        },
        value.variant_);
  }
};

inline bool Is(const ListValue& lhs, const ListValue& rhs) {
  return absl::visit(
      [](const auto& alternative_lhs, const auto& alternative_rhs) -> bool {
        if constexpr (std::is_same_v<
                          absl::remove_cvref_t<decltype(alternative_lhs)>,
                          absl::remove_cvref_t<decltype(alternative_rhs)>>) {
          return cel::Is(alternative_lhs, alternative_rhs);
        } else {
          return false;
        }
      },
      lhs.variant_, rhs.variant_);
}

class ListValueBuilder {
 public:
  virtual ~ListValueBuilder() = default;

  virtual absl::Status Add(Value value) = 0;

  virtual bool IsEmpty() const { return Size() == 0; }

  virtual size_t Size() const = 0;

  virtual void Reserve(size_t capacity) {}

  virtual ListValue Build() && = 0;
};

using ListValueBuilderPtr = std::unique_ptr<ListValueBuilder>;

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_LIST_VALUE_H_
