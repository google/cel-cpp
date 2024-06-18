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

// `ListValue` represents values of the primitive `list` type. `ListValueView`
// is a non-owning view of `ListValue`. `ListValueInterface` is the abstract
// base class of implementations. `ListValue` and `ListValueView` act as smart
// pointers to `ListValueInterface`.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_LIST_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_LIST_VALUE_H_

#include <cstddef>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/log/absl_check.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "common/any.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/legacy_list_value.h"  // IWYU pragma: export
#include "common/values/list_value_interface.h"  // IWYU pragma: export
#include "common/values/parsed_list_value.h"  // IWYU pragma: export
#include "common/values/values.h"

namespace cel {

class ListValueInterface;
class ListValue;
class ListValueView;
class Value;
class ValueView;
class ValueManager;
class TypeManager;

bool Is(ListValueView lhs, ListValueView rhs);

class ListValue final {
 public:
  using interface_type = ListValueInterface;
  using view_alternative_type = ListValueView;

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

  // Constructor for struct value view.
  explicit ListValue(ListValueView value);

  // Constructor for alternative struct value views.
  template <typename T, typename = std::enable_if_t<
                            common_internal::IsListValueViewAlternativeV<
                                absl::remove_cvref_t<T>>>>
  explicit ListValue(T value)
      : variant_(
            absl::in_place_type<common_internal::BaseListValueAlternativeForT<
                absl::remove_cvref_t<T>>>,
            value) {}

  ListValue() = default;

  ListValue(const ListValue& other) : variant_(other.variant_) {}

  ListValue(ListValue&& other) noexcept : variant_(std::move(other.variant_)) {}

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

  ListType GetType(TypeManager& type_manager) const;

  absl::string_view GetTypeName() const;

  std::string DebugString() const;

  absl::StatusOr<size_t> GetSerializedSize(AnyToJsonConverter& converter) const;

  absl::Status SerializeTo(AnyToJsonConverter& converter,
                           absl::Cord& value) const;

  absl::StatusOr<absl::Cord> Serialize(AnyToJsonConverter& converter) const;

  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Any> ConvertToAny(
      AnyToJsonConverter& converter,
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter& converter) const;

  absl::StatusOr<JsonArray> ConvertToJsonArray(
      AnyToJsonConverter& converter) const;

  absl::Status Equal(ValueManager& value_manager, ValueView other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              ValueView other) const;

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

  absl::Status Contains(ValueManager& value_manager, ValueView other,
                        Value& result) const;
  absl::StatusOr<Value> Contains(ValueManager& value_manager,
                                 ValueView other) const;

 private:
  friend class ListValueView;
  friend struct NativeTypeTraits<ListValue>;
  friend struct CompositionTraits<ListValue>;

  common_internal::ListValueViewVariant ToViewVariant() const;

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

template <>
struct CompositionTraits<ListValue> final {
  template <typename U>
  static std::enable_if_t<common_internal::IsListValueAlternativeV<U>, bool>
  HasA(const ListValue& value) {
    using Base = common_internal::BaseListValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::holds_alternative<U>(value.variant_);
    } else {
      return absl::holds_alternative<Base>(value.variant_) &&
             InstanceOf<U>(Get<U>(value));
    }
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<Value, U>, bool> HasA(
      const ListValue& value) {
    return true;
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsListValueAlternativeV<U>, const U&>
  Get(const ListValue& value) {
    using Base = common_internal::BaseListValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(value.variant_);
    } else {
      return Cast<U>(absl::get<Base>(value.variant_));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsListValueAlternativeV<U>, U&> Get(
      ListValue& value) {
    using Base = common_internal::BaseListValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(value.variant_);
    } else {
      return Cast<U>(absl::get<Base>(value.variant_));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsListValueAlternativeV<U>, U> Get(
      const ListValue&& value) {
    using Base = common_internal::BaseListValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(std::move(value.variant_));
    } else {
      return Cast<U>(absl::get<Base>(std::move(value.variant_)));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsListValueAlternativeV<U>, U> Get(
      ListValue&& value) {
    using Base = common_internal::BaseListValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(std::move(value.variant_));
    } else {
      return Cast<U>(absl::get<Base>(std::move(value.variant_)));
    }
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<Value, U>, U> Get(
      const ListValue& value) {
    return absl::visit(
        [](const auto& alternative) -> U { return U{alternative}; },
        value.variant_);
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<Value, U>, U> Get(ListValue& value) {
    return absl::visit(
        [](const auto& alternative) -> U { return U{alternative}; },
        value.variant_);
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<Value, U>, U> Get(
      const ListValue&& value) {
    return absl::visit(
        [](const auto& alternative) -> U { return U{alternative}; },
        value.variant_);
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<Value, U>, U> Get(ListValue&& value) {
    return absl::visit(
        [](auto&& alternative) -> U { return U{std::move(alternative)}; },
        std::move(value.variant_));
  }
};

template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::is_same_v<ListValue, absl::remove_cvref_t<From>>>>
    : CompositionCastTraits<To, From> {};

class ListValueView final {
 public:
  using interface_type = ListValueInterface;
  using alternative_type = ListValue;

  static constexpr ValueKind kKind = ListValue::kKind;

  // Constructor for alternative struct value views.
  template <typename T, typename = std::enable_if_t<
                            common_internal::IsListValueViewAlternativeV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  ListValueView(T value)
      : variant_(absl::in_place_type<
                     common_internal::BaseListValueViewAlternativeForT<T>>,
                 value) {}

  // Constructor for struct value.
  // NOLINTNEXTLINE(google-explicit-constructor)
  ListValueView(const ListValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : variant_(value.ToViewVariant()) {}

  // Constructor for alternative struct values.
  template <typename T, typename = std::enable_if_t<
                            common_internal::IsListValueAlternativeV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  ListValueView(const T& value ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : variant_(absl::in_place_type<
                     common_internal::BaseListValueViewAlternativeForT<T>>,
                 value) {}

  // Prevent binding to temporary struct values.
  ListValueView& operator=(ListValue&&) = delete;

  // Prevent binding to temporary alternative struct values.
  template <
      typename T,
      typename = std::enable_if_t<
          common_internal::IsListValueAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  ListValueView& operator=(T&&) = delete;

  ListValueView() = default;
  ListValueView(const ListValueView&) = default;
  ListValueView& operator=(const ListValueView&) = default;

  constexpr ValueKind kind() const { return kKind; }

  ListType GetType(TypeManager& type_manager) const;

  absl::string_view GetTypeName() const;

  std::string DebugString() const;

  absl::StatusOr<size_t> GetSerializedSize(AnyToJsonConverter& converter) const;

  absl::Status SerializeTo(AnyToJsonConverter& converter,
                           absl::Cord& value) const;

  absl::StatusOr<absl::Cord> Serialize(AnyToJsonConverter& converter) const;

  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Any> ConvertToAny(
      AnyToJsonConverter& converter,
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter& converter) const;

  absl::StatusOr<JsonArray> ConvertToJsonArray(
      AnyToJsonConverter& converter) const;

  absl::Status Equal(ValueManager& value_manager, ValueView other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              ValueView other) const;

  bool IsZeroValue() const;

  void swap(ListValueView& other) noexcept { variant_.swap(other.variant_); }

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

  absl::Status Contains(ValueManager& value_manager, ValueView other,
                        Value& result) const;
  absl::StatusOr<Value> Contains(ValueManager& value_manager,
                                 ValueView other) const;

 private:
  friend class ListValue;
  friend struct NativeTypeTraits<ListValueView>;
  friend struct CompositionTraits<ListValueView>;
  friend bool Is(ListValueView lhs, ListValueView rhs);

  common_internal::ListValueVariant ToVariant() const;

  // Unlike many of the other derived values, `ListValue` is itself a composed
  // type. This is to avoid making `ListValue` too big and by extension
  // `Value` too big. Instead we store the derived `ListValue` values in
  // `Value` and not `ListValue` itself.
  common_internal::ListValueViewVariant variant_;
};

inline void swap(ListValueView& lhs, ListValueView& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out, ListValueView value) {
  return out << value.DebugString();
}

template <>
struct NativeTypeTraits<ListValueView> final {
  static NativeTypeId Id(ListValueView value) {
    return absl::visit(
        [](const auto& alternative) -> NativeTypeId {
          return NativeTypeId::Of(alternative);
        },
        value.variant_);
  }
};

template <>
struct CompositionTraits<ListValueView> final {
  template <typename U>
  static std::enable_if_t<common_internal::IsListValueViewAlternativeV<U>, bool>
  HasA(ListValueView value) {
    using Base = common_internal::BaseListValueViewAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::holds_alternative<U>(value.variant_);
    } else {
      return InstanceOf<U>(Get<Base>(value));
    }
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<ValueView, U>, bool> HasA(
      ListValueView value) {
    return true;
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsListValueViewAlternativeV<U>, U>
  Get(ListValueView value) {
    using Base = common_internal::BaseListValueViewAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(value.variant_);
    } else {
      return Cast<U>(absl::get<Base>(value.variant_));
    }
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<ValueView, U>, U> Get(
      ListValueView value) {
    return absl::visit([](auto alternative) -> U { return U{alternative}; },
                       value.variant_);
  }
};

template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::is_same_v<ListValueView, absl::remove_cvref_t<From>>>>
    : CompositionCastTraits<To, From> {};

inline ListValue::ListValue(ListValueView value)
    : variant_(value.ToVariant()) {}

inline bool Is(ListValueView lhs, ListValueView rhs) {
  return absl::visit(
      [](auto alternative_lhs, auto alternative_rhs) -> bool {
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

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_LIST_VALUE_H_
