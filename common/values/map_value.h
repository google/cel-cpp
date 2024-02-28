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

// `MapValue` represents values of the primitive `map` type. `MapValueView`
// is a non-owning view of `MapValue`. `MapValueInterface` is the abstract
// base class of implementations. `MapValue` and `MapValueView` act as smart
// pointers to `MapValueInterface`.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_MAP_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_MAP_VALUE_H_

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
#include "common/values/legacy_map_value.h"  // IWYU pragma: export
#include "common/values/map_value_interface.h"  // IWYU pragma: export
#include "common/values/parsed_map_value.h"  // IWYU pragma: export
#include "common/values/values.h"

namespace cel {

class MapValueInterface;
class MapValue;
class MapValueView;
class Value;
class ValueView;
class ValueManager;
class TypeManager;

absl::Status CheckMapKey(ValueView key);

bool Is(MapValueView lhs, MapValueView rhs);

class MapValue final {
 public:
  using interface_type = MapValueInterface;
  using view_alternative_type = MapValueView;

  static constexpr ValueKind kKind = MapValueInterface::kKind;

  // Copy constructor for alternative struct values.
  template <typename T,
            typename = std::enable_if_t<common_internal::IsMapValueAlternativeV<
                absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  MapValue(const T& value)
      : variant_(
            absl::in_place_type<common_internal::BaseMapValueAlternativeForT<
                absl::remove_cvref_t<T>>>,
            value) {}

  // Move constructor for alternative struct values.
  template <typename T,
            typename = std::enable_if_t<common_internal::IsMapValueAlternativeV<
                absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  MapValue(T&& value)
      : variant_(
            absl::in_place_type<common_internal::BaseMapValueAlternativeForT<
                absl::remove_cvref_t<T>>>,
            std::forward<T>(value)) {}

  // Constructor for struct value view.
  explicit MapValue(MapValueView value);

  // Constructor for alternative struct value views.
  template <
      typename T,
      typename = std::enable_if_t<
          common_internal::IsMapValueViewAlternativeV<absl::remove_cvref_t<T>>>>
  explicit MapValue(T value)
      : variant_(
            absl::in_place_type<common_internal::BaseMapValueAlternativeForT<
                absl::remove_cvref_t<T>>>,
            value) {}

  MapValue() = default;

  MapValue(const MapValue& other) : variant_(other.variant_) {}

  MapValue(MapValue&& other) noexcept : variant_(std::move(other.variant_)) {}

  MapValue& operator=(const MapValue& other) {
    ABSL_DCHECK(this != std::addressof(other))
        << "MapValue should not be copied to itself";
    variant_ = other.variant_;
    return *this;
  }

  MapValue& operator=(MapValue&& other) noexcept {
    ABSL_DCHECK(this != std::addressof(other))
        << "MapValue should not be moved to itself";
    variant_ = std::move(other.variant_);
    other.variant_.emplace<ParsedMapValue>();
    return *this;
  }

  constexpr ValueKind kind() const { return kKind; }

  MapType GetType(TypeManager& type_manager) const {
    return absl::visit(
        [&type_manager](const auto& alternative) -> MapType {
          return alternative.GetType(type_manager);
        },
        variant_);
  }

  absl::string_view GetTypeName() const {
    return absl::visit(
        [](const auto& alternative) -> absl::string_view {
          return alternative.GetTypeName();
        },
        variant_);
  }

  std::string DebugString() const {
    return absl::visit(
        [](const auto& alternative) -> std::string {
          return alternative.DebugString();
        },
        variant_);
  }

  absl::StatusOr<size_t> GetSerializedSize(ValueManager& value_manager) const {
    return absl::visit(
        [&value_manager](const auto& alternative) -> absl::StatusOr<size_t> {
          return alternative.GetSerializedSize(value_manager);
        },
        variant_);
  }

  absl::Status SerializeTo(ValueManager& value_manager,
                           absl::Cord& value) const {
    return absl::visit(
        [&value_manager, &value](const auto& alternative) -> absl::Status {
          return alternative.SerializeTo(value_manager, value);
        },
        variant_);
  }

  absl::StatusOr<absl::Cord> Serialize(ValueManager& value_manager) const {
    return absl::visit(
        [&value_manager](
            const auto& alternative) -> absl::StatusOr<absl::Cord> {
          return alternative.Serialize(value_manager);
        },
        variant_);
  }

  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const {
    return absl::visit(
        [prefix](const auto& alternative) -> absl::StatusOr<std::string> {
          return alternative.GetTypeUrl(prefix);
        },
        variant_);
  }

  absl::StatusOr<Any> ConvertToAny(
      ValueManager& value_manager,
      absl::string_view prefix = kTypeGoogleApisComPrefix) const {
    return absl::visit(
        [&value_manager,
         prefix](const auto& alternative) -> absl::StatusOr<Any> {
          return alternative.ConvertToAny(value_manager, prefix);
        },
        variant_);
  }

  absl::StatusOr<Json> ConvertToJson(ValueManager& value_manager) const {
    return absl::visit(
        [&value_manager](const auto& alternative) -> absl::StatusOr<Json> {
          return alternative.ConvertToJson(value_manager);
        },
        variant_);
  }

  absl::StatusOr<JsonObject> ConvertToJsonObject(
      ValueManager& value_manager) const {
    return absl::visit(
        [&value_manager](
            const auto& alternative) -> absl::StatusOr<JsonObject> {
          return alternative.ConvertToJsonObject(value_manager);
        },
        variant_);
  }

  absl::StatusOr<ValueView> Equal(ValueManager& value_manager, ValueView other,
                                  Value& scratch
                                      ABSL_ATTRIBUTE_LIFETIME_BOUND) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              ValueView other) const;

  bool IsZeroValue() const {
    return absl::visit(
        [](const auto& alternative) -> bool {
          return alternative.IsZeroValue();
        },
        variant_);
  }

  void swap(MapValue& other) noexcept { variant_.swap(other.variant_); }

  bool IsEmpty() const {
    return absl::visit(
        [](const auto& alternative) -> bool { return alternative.IsEmpty(); },
        variant_);
  }

  size_t Size() const {
    return absl::visit(
        [](const auto& alternative) -> size_t { return alternative.Size(); },
        variant_);
  }

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<ValueView> Get(ValueManager& value_manager, ValueView key,
                                Value& scratch
                                    ABSL_ATTRIBUTE_LIFETIME_BOUND) const;
  absl::StatusOr<Value> Get(ValueManager& value_manager, ValueView key) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<std::pair<ValueView, bool>> Find(
      ValueManager& value_manager, ValueView key,
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;
  absl::StatusOr<std::pair<Value, bool>> Find(ValueManager& value_manager,
                                              ValueView key) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<ValueView> Has(ValueManager& value_manager, ValueView key,
                                Value& scratch
                                    ABSL_ATTRIBUTE_LIFETIME_BOUND) const;
  absl::StatusOr<Value> Has(ValueManager& value_manager, ValueView key) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<ListValueView> ListKeys(
      ValueManager& value_manager,
      ListValue& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;
  absl::StatusOr<ListValue> ListKeys(ValueManager& value_manager) const;

  // See the corresponding type declaration of `MapValueInterface` for
  // documentation.
  using ForEachCallback = typename MapValueInterface::ForEachCallback;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status ForEach(ValueManager& value_manager,
                       ForEachCallback callback) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueManager& value_manager) const;

 private:
  friend class MapValueView;
  friend struct NativeTypeTraits<MapValue>;
  friend struct CompositionTraits<MapValue>;

  common_internal::MapValueViewVariant ToViewVariant() const {
    return absl::visit(
        [](const auto& alternative) -> common_internal::MapValueViewVariant {
          return common_internal::MapValueViewVariant{
              absl::in_place_type<typename absl::remove_cvref_t<
                  decltype(alternative)>::view_alternative_type>,
              alternative};
        },
        variant_);
  }

  // Unlike many of the other derived values, `MapValue` is itself a composed
  // type. This is to avoid making `MapValue` too big and by extension
  // `Value` too big. Instead we store the derived `MapValue` values in
  // `Value` and not `MapValue` itself.
  common_internal::MapValueVariant variant_;
};

inline void swap(MapValue& lhs, MapValue& rhs) noexcept { lhs.swap(rhs); }

inline std::ostream& operator<<(std::ostream& out, const MapValue& value) {
  return out << value.DebugString();
}

template <>
struct NativeTypeTraits<MapValue> final {
  static NativeTypeId Id(const MapValue& value) {
    return absl::visit(
        [](const auto& alternative) -> NativeTypeId {
          return NativeTypeId::Of(alternative);
        },
        value.variant_);
  }

  static bool SkipDestructor(const MapValue& value) {
    return absl::visit(
        [](const auto& alternative) -> bool {
          return NativeType::SkipDestructor(alternative);
        },
        value.variant_);
  }
};

template <>
struct CompositionTraits<MapValue> final {
  template <typename U>
  static std::enable_if_t<common_internal::IsMapValueAlternativeV<U>, bool>
  HasA(const MapValue& value) {
    using Base = common_internal::BaseMapValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::holds_alternative<U>(value.variant_);
    } else {
      return absl::holds_alternative<Base>(value.variant_) &&
             InstanceOf<U>(Get<U>(value));
    }
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<Value, U>, bool> HasA(
      const MapValue& value) {
    return true;
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsMapValueAlternativeV<U>, const U&>
  Get(const MapValue& value) {
    using Base = common_internal::BaseMapValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(value.variant_);
    } else {
      return Cast<U>(absl::get<Base>(value.variant_));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsMapValueAlternativeV<U>, U&> Get(
      MapValue& value) {
    using Base = common_internal::BaseMapValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(value.variant_);
    } else {
      return Cast<U>(absl::get<Base>(value.variant_));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsMapValueAlternativeV<U>, U> Get(
      const MapValue&& value) {
    using Base = common_internal::BaseMapValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(std::move(value.variant_));
    } else {
      return Cast<U>(absl::get<Base>(std::move(value.variant_)));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsMapValueAlternativeV<U>, U> Get(
      MapValue&& value) {
    using Base = common_internal::BaseMapValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(std::move(value.variant_));
    } else {
      return Cast<U>(absl::get<Base>(std::move(value.variant_)));
    }
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<Value, U>, U> Get(
      const MapValue& value) {
    return absl::visit(
        [](const auto& alternative) -> U { return U{alternative}; },
        value.variant_);
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<Value, U>, U> Get(MapValue& value) {
    return absl::visit(
        [](const auto& alternative) -> U { return U{alternative}; },
        value.variant_);
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<Value, U>, U> Get(
      const MapValue&& value) {
    return absl::visit(
        [](const auto& alternative) -> U { return U{alternative}; },
        value.variant_);
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<Value, U>, U> Get(MapValue&& value) {
    return absl::visit(
        [](auto&& alternative) -> U { return U{std::move(alternative)}; },
        std::move(value.variant_));
  }
};

template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::is_same_v<MapValue, absl::remove_cvref_t<From>>>>
    : CompositionCastTraits<To, From> {};

class MapValueView final {
 public:
  using interface_type = MapValueInterface;
  using alternative_type = MapValue;

  static constexpr ValueKind kKind = MapValue::kKind;

  // Constructor for alternative struct value views.
  template <typename T, typename = std::enable_if_t<
                            common_internal::IsMapValueViewAlternativeV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  MapValueView(T value)
      : variant_(absl::in_place_type<
                     common_internal::BaseMapValueViewAlternativeForT<T>>,
                 value) {}

  // Constructor for struct value.
  // NOLINTNEXTLINE(google-explicit-constructor)
  MapValueView(const MapValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : variant_(value.ToViewVariant()) {}

  // Constructor for alternative struct values.
  template <typename T, typename = std::enable_if_t<
                            common_internal::IsMapValueAlternativeV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  MapValueView(const T& value ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : variant_(absl::in_place_type<
                     common_internal::BaseMapValueViewAlternativeForT<T>>,
                 value) {}

  // Prevent binding to temporary struct values.
  MapValueView& operator=(MapValue&&) = delete;

  // Prevent binding to temporary alternative struct values.
  template <typename T,
            typename = std::enable_if_t<common_internal::IsMapValueAlternativeV<
                absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  MapValueView& operator=(T&&) = delete;

  MapValueView() = default;
  MapValueView(const MapValueView&) = default;
  MapValueView& operator=(const MapValueView&) = default;

  constexpr ValueKind kind() const { return kKind; }

  MapType GetType(TypeManager& type_manager) const {
    return absl::visit(
        [&type_manager](auto alternative) -> MapType {
          return alternative.GetType(type_manager);
        },
        variant_);
  }

  absl::string_view GetTypeName() const {
    return absl::visit(
        [](auto alternative) -> absl::string_view {
          return alternative.GetTypeName();
        },
        variant_);
  }

  std::string DebugString() const {
    return absl::visit(
        [](auto alternative) -> std::string {
          return alternative.DebugString();
        },
        variant_);
  }

  absl::StatusOr<size_t> GetSerializedSize(ValueManager& value_manager) const {
    return absl::visit(
        [&value_manager](auto alternative) -> absl::StatusOr<size_t> {
          return alternative.GetSerializedSize(value_manager);
        },
        variant_);
  }

  absl::Status SerializeTo(ValueManager& value_manager,
                           absl::Cord& value) const {
    return absl::visit(
        [&value_manager, &value](auto alternative) -> absl::Status {
          return alternative.SerializeTo(value_manager, value);
        },
        variant_);
  }

  absl::StatusOr<absl::Cord> Serialize(ValueManager& value_manager) const {
    return absl::visit(
        [&value_manager](auto alternative) -> absl::StatusOr<absl::Cord> {
          return alternative.Serialize(value_manager);
        },
        variant_);
  }

  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const {
    return absl::visit(
        [prefix](auto alternative) -> absl::StatusOr<std::string> {
          return alternative.GetTypeUrl(prefix);
        },
        variant_);
  }

  absl::StatusOr<Any> ConvertToAny(
      ValueManager& value_manager,
      absl::string_view prefix = kTypeGoogleApisComPrefix) const {
    return absl::visit(
        [&value_manager, prefix](auto alternative) -> absl::StatusOr<Any> {
          return alternative.ConvertToAny(value_manager, prefix);
        },
        variant_);
  }

  absl::StatusOr<Json> ConvertToJson(ValueManager& value_manager) const {
    return absl::visit(
        [&value_manager](auto alternative) -> absl::StatusOr<Json> {
          return alternative.ConvertToJson(value_manager);
        },
        variant_);
  }

  absl::StatusOr<JsonObject> ConvertToJsonObject(
      ValueManager& value_manager) const {
    return absl::visit(
        [&value_manager](auto alternative) -> absl::StatusOr<JsonObject> {
          return alternative.ConvertToJsonObject(value_manager);
        },
        variant_);
  }

  absl::StatusOr<ValueView> Equal(ValueManager& value_manager, ValueView other,
                                  Value& scratch
                                      ABSL_ATTRIBUTE_LIFETIME_BOUND) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              ValueView other) const;

  bool IsZeroValue() const {
    return absl::visit(
        [](auto alternative) -> bool { return alternative.IsZeroValue(); },
        variant_);
  }

  void swap(MapValueView& other) noexcept { variant_.swap(other.variant_); }

  bool IsEmpty() const {
    return absl::visit(
        [](auto alternative) -> bool { return alternative.IsEmpty(); },
        variant_);
  }

  size_t Size() const {
    return absl::visit(
        [](auto alternative) -> size_t { return alternative.Size(); },
        variant_);
  }

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<ValueView> Get(ValueManager& value_manager, ValueView key,
                                Value& scratch
                                    ABSL_ATTRIBUTE_LIFETIME_BOUND) const;
  absl::StatusOr<Value> Get(ValueManager& value_manager, ValueView key) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<std::pair<ValueView, bool>> Find(
      ValueManager& value_manager, ValueView key,
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;
  absl::StatusOr<std::pair<Value, bool>> Find(ValueManager& value_manager,
                                              ValueView key) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<ValueView> Has(ValueManager& value_manager, ValueView key,
                                Value& scratch
                                    ABSL_ATTRIBUTE_LIFETIME_BOUND) const;
  absl::StatusOr<Value> Has(ValueManager& value_manager, ValueView key) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<ListValueView> ListKeys(
      ValueManager& value_manager,
      ListValue& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;
  absl::StatusOr<ListValue> ListKeys(ValueManager& value_manager) const;

  // See the corresponding type declaration of `MapValueInterface` for
  // documentation.
  using ForEachCallback = typename MapValueInterface::ForEachCallback;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status ForEach(ValueManager& value_manager,
                       ForEachCallback callback) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueManager& value_manager) const;

 private:
  friend class MapValue;
  friend struct NativeTypeTraits<MapValueView>;
  friend struct CompositionTraits<MapValueView>;
  friend bool Is(MapValueView lhs, MapValueView rhs);

  common_internal::MapValueVariant ToVariant() const {
    return absl::visit(
        [](auto alternative) -> common_internal::MapValueVariant {
          return common_internal::MapValueVariant{
              absl::in_place_type<typename absl::remove_cvref_t<
                  decltype(alternative)>::alternative_type>,
              alternative};
        },
        variant_);
  }

  // Unlike many of the other derived values, `MapValue` is itself a composed
  // type. This is to avoid making `MapValue` too big and by extension
  // `Value` too big. Instead we store the derived `MapValue` values in
  // `Value` and not `MapValue` itself.
  common_internal::MapValueViewVariant variant_;
};

inline void swap(MapValueView& lhs, MapValueView& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out, MapValueView value) {
  return out << value.DebugString();
}

template <>
struct NativeTypeTraits<MapValueView> final {
  static NativeTypeId Id(MapValueView value) {
    return absl::visit(
        [](const auto& alternative) -> NativeTypeId {
          return NativeTypeId::Of(alternative);
        },
        value.variant_);
  }
};

template <>
struct CompositionTraits<MapValueView> final {
  template <typename U>
  static std::enable_if_t<common_internal::IsMapValueViewAlternativeV<U>, bool>
  HasA(MapValueView value) {
    using Base = common_internal::BaseMapValueViewAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::holds_alternative<U>(value.variant_);
    } else {
      return InstanceOf<U>(Get<Base>(value));
    }
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<ValueView, U>, bool> HasA(
      MapValueView value) {
    return true;
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsMapValueViewAlternativeV<U>, U>
  Get(MapValueView value) {
    using Base = common_internal::BaseMapValueViewAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(value.variant_);
    } else {
      return Cast<U>(absl::get<Base>(value.variant_));
    }
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<ValueView, U>, U> Get(
      MapValueView value) {
    return absl::visit([](auto alternative) -> U { return U{alternative}; },
                       value.variant_);
  }
};

template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::is_same_v<MapValueView, absl::remove_cvref_t<From>>>>
    : CompositionCastTraits<To, From> {};

inline MapValue::MapValue(MapValueView value) : variant_(value.ToVariant()) {}

inline bool Is(MapValueView lhs, MapValueView rhs) {
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

class MapValueBuilder {
 public:
  virtual ~MapValueBuilder() = default;

  virtual absl::Status Put(Value key, Value value) = 0;

  virtual bool IsEmpty() const { return Size() == 0; }

  virtual size_t Size() const = 0;

  virtual void Reserve(size_t capacity) {}

  virtual MapValue Build() && = 0;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_MAP_VALUE_H_
