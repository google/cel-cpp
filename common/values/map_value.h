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
class Value;
class ValueManager;
class TypeManager;

absl::Status CheckMapKey(const Value& key);

bool Is(const MapValue& lhs, const MapValue& rhs);

class MapValue final {
 public:
  using interface_type = MapValueInterface;

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

  MapType GetType(TypeManager& type_manager) const;

  absl::string_view GetTypeName() const;

  std::string DebugString() const;

  absl::Status SerializeTo(AnyToJsonConverter& converter,
                           absl::Cord& value) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter& converter) const;

  absl::StatusOr<JsonObject> ConvertToJsonObject(
      AnyToJsonConverter& converter) const;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const;

  void swap(MapValue& other) noexcept { variant_.swap(other.variant_); }

  absl::StatusOr<bool> IsEmpty() const;

  absl::StatusOr<size_t> Size() const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status Get(ValueManager& value_manager, const Value& key,
                   Value& result) const;
  absl::StatusOr<Value> Get(ValueManager& value_manager,
                            const Value& key) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::StatusOr<bool> Find(ValueManager& value_manager, const Value& key,
                            Value& result) const;
  absl::StatusOr<std::pair<Value, bool>> Find(ValueManager& value_manager,
                                              const Value& key) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status Has(ValueManager& value_manager, const Value& key,
                   Value& result) const;
  absl::StatusOr<Value> Has(ValueManager& value_manager,
                            const Value& key) const;

  // See the corresponding member function of `MapValueInterface` for
  // documentation.
  absl::Status ListKeys(ValueManager& value_manager, ListValue& result) const;
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
  friend struct NativeTypeTraits<MapValue>;
  friend struct CompositionTraits<MapValue>;
  friend bool Is(const MapValue& lhs, const MapValue& rhs);

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

inline bool Is(const MapValue& lhs, const MapValue& rhs) {
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
