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

// `StructValue` is the value representation of `StructType`. `StructValue`
// itself is a composed type of more specific runtime representations.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_STRUCT_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_STRUCT_VALUE_H_

#include <cstddef>
#include <cstdint>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/optimization.h"
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
#include "common/values/legacy_struct_value.h"  // IWYU pragma: export
#include "common/values/parsed_struct_value.h"  // IWYU pragma: export
#include "common/values/struct_value_interface.h"  // IWYU pragma: export
#include "common/values/values.h"

namespace cel {

class StructValueInterface;
class StructValue;
class StructValueView;
class Value;
class ValueView;
class ValueManager;
class TypeManager;

class StructValue final {
 public:
  using interface_type = StructValueInterface;
  using view_alternative_type = StructValueView;

  static constexpr ValueKind kKind = StructValueInterface::kKind;

  // Copy constructor for alternative struct values.
  template <typename T, typename = std::enable_if_t<
                            common_internal::IsStructValueAlternativeV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  StructValue(const T& value)
      : variant_(absl::in_place_type<
                     common_internal::BaseStructValueAlternativeFor<T>>,
                 value) {}

  // Move constructor for alternative struct values.
  template <
      typename T,
      typename = std::enable_if_t<
          common_internal::IsStructValueAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  StructValue(T&& value)
      : variant_(absl::in_place_type<
                     common_internal::BaseStructValueAlternativeFor<T>>,
                 std::forward<T>(value)) {}

  // Constructor for struct value view.
  explicit StructValue(StructValueView value);

  // Constructor for alternative struct value views.
  template <typename T, typename = std::enable_if_t<
                            common_internal::IsStructValueViewAlternativeV<T>>>
  explicit StructValue(T value)
      : variant_(absl::in_place_type<
                     common_internal::BaseStructValueAlternativeForT<T>>,
                 value) {}

  StructValue() = default;
  StructValue(const StructValue& other);
  StructValue(StructValue&& other);
  StructValue& operator=(const StructValue& other);
  StructValue& operator=(StructValue&& other);

  ValueKind kind() const {
    AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> ValueKind {
          if constexpr (std::is_same_v<
                            absl::monostate,
                            absl::remove_cvref_t<decltype(alternative)>>) {
            return ValueKind::kStruct;
          } else {
            return alternative.kind();
          }
        },
        variant_);
  }

  StructType GetType(TypeManager& type_manager) const {
    AssertIsValid();
    return absl::visit(
        [&type_manager](const auto& alternative) -> StructType {
          if constexpr (std::is_same_v<
                            absl::monostate,
                            absl::remove_cvref_t<decltype(alternative)>>) {
            ABSL_UNREACHABLE();
          } else {
            return alternative.GetType(type_manager);
          }
        },
        variant_);
  }

  absl::string_view GetTypeName() const {
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

  std::string DebugString() const {
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

  absl::StatusOr<size_t> GetSerializedSize() const {
    AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> absl::StatusOr<size_t> {
          if constexpr (std::is_same_v<
                            absl::monostate,
                            absl::remove_cvref_t<decltype(alternative)>>) {
            return absl::InternalError("use of invalid StructValue");
          } else {
            return alternative.GetSerializedSize();
          }
        },
        variant_);
  }

  absl::Status SerializeTo(absl::Cord& value) const {
    AssertIsValid();
    return absl::visit(
        [&value](const auto& alternative) -> absl::Status {
          if constexpr (std::is_same_v<
                            absl::monostate,
                            absl::remove_cvref_t<decltype(alternative)>>) {
            return absl::InternalError("use of invalid StructValue");
          } else {
            return alternative.SerializeTo(value);
          }
        },
        variant_);
  }

  absl::StatusOr<absl::Cord> Serialize() const {
    AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> absl::StatusOr<absl::Cord> {
          if constexpr (std::is_same_v<
                            absl::monostate,
                            absl::remove_cvref_t<decltype(alternative)>>) {
            return absl::InternalError("use of invalid StructValue");
          } else {
            return alternative.Serialize();
          }
        },
        variant_);
  }

  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const {
    AssertIsValid();
    return absl::visit(
        [prefix](const auto& alternative) -> absl::StatusOr<std::string> {
          if constexpr (std::is_same_v<
                            absl::monostate,
                            absl::remove_cvref_t<decltype(alternative)>>) {
            return absl::InternalError("use of invalid StructValue");
          } else {
            return alternative.GetTypeUrl(prefix);
          }
        },
        variant_);
  }

  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const {
    AssertIsValid();
    return absl::visit(
        [prefix](const auto& alternative) -> absl::StatusOr<Any> {
          if constexpr (std::is_same_v<
                            absl::monostate,
                            absl::remove_cvref_t<decltype(alternative)>>) {
            return absl::InternalError("use of invalid StructValue");
          } else {
            return alternative.ConvertToAny(prefix);
          }
        },
        variant_);
  }

  absl::StatusOr<Json> ConvertToJson() const {
    AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> absl::StatusOr<Json> {
          if constexpr (std::is_same_v<
                            absl::monostate,
                            absl::remove_cvref_t<decltype(alternative)>>) {
            return absl::InternalError("use of invalid StructValue");
          } else {
            return alternative.ConvertToJson();
          }
        },
        variant_);
  }

  absl::StatusOr<JsonObject> ConvertToJsonObject() const {
    AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> absl::StatusOr<JsonObject> {
          if constexpr (std::is_same_v<
                            absl::monostate,
                            absl::remove_cvref_t<decltype(alternative)>>) {
            return absl::InternalError("use of invalid StructValue");
          } else {
            return alternative.ConvertToJsonObject();
          }
        },
        variant_);
  }

  void swap(StructValue& other) noexcept {
    AssertIsValid();
    other.AssertIsValid();
    variant_.swap(other.variant_);
  }

  absl::StatusOr<ValueView> GetFieldByName(
      ValueManager& value_manager, absl::string_view name,
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  absl::StatusOr<ValueView> GetFieldByNumber(
      ValueManager& value_manager, int64_t number,
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  absl::StatusOr<bool> HasFieldByName(absl::string_view name) const {
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

  absl::StatusOr<bool> HasFieldByNumber(int64_t number) const {
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

 private:
  friend class StructValueView;
  friend struct NativeTypeTraits<StructValue>;
  friend struct CompositionTraits<StructValue>;

  common_internal::StructValueViewVariant ToViewVariant() const;

  constexpr bool IsValid() const {
    return !absl::holds_alternative<absl::monostate>(variant_);
  }

  void AssertIsValid() const {
    ABSL_DCHECK(IsValid()) << "use of invalid StructValue";
  }

  // Unlike many of the other derived values, `StructValue` is itself a composed
  // type. This is to avoid making `StructValue` too big and by extension
  // `Value` too big. Instead we store the derived `StructValue` values in
  // `Value` and not `StructValue` itself.
  common_internal::StructValueVariant variant_;
};

inline void swap(StructValue& lhs, StructValue& rhs) noexcept { lhs.swap(rhs); }

inline std::ostream& operator<<(std::ostream& out, const StructValue& value) {
  return out << value.DebugString();
}

template <>
struct NativeTypeTraits<StructValue> final {
  static NativeTypeId Id(const StructValue& value) {
    value.AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> NativeTypeId {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return
            // `NativeTypeId::For<absl::monostate>()`. In debug builds we cannot
            // reach here.
            return NativeTypeId::For<absl::monostate>();
          } else {
            return NativeTypeId::Of(alternative);
          }
        },
        value.variant_);
  }

  static bool SkipDestructor(const StructValue& value) {
    value.AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> bool {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just say we should skip the destructor.
            // In debug builds we cannot reach here.
            return true;
          } else {
            return NativeType::SkipDestructor(alternative);
          }
        },
        value.variant_);
  }
};

template <>
struct CompositionTraits<StructValue> final {
  template <typename U>
  static std::enable_if_t<common_internal::IsStructValueAlternativeV<U>, bool>
  HasA(const StructValue& value) {
    value.AssertIsValid();
    using Base = common_internal::BaseStructValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::holds_alternative<U>(value.variant_);
    } else {
      return absl::holds_alternative<Base>(value.variant_) &&
             InstanceOf<U>(Get<U>(value));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsStructValueAlternativeV<U>,
                          const U&>
  Get(const StructValue& value) {
    value.AssertIsValid();
    using Base = common_internal::BaseStructValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(value.variant_);
    } else {
      return Cast<U>(absl::get<Base>(value.variant_));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsStructValueAlternativeV<U>, U&>
  Get(StructValue& value) {
    value.AssertIsValid();
    using Base = common_internal::BaseStructValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(value.variant_);
    } else {
      return Cast<U>(absl::get<Base>(value.variant_));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsStructValueAlternativeV<U>, U> Get(
      const StructValue&& value) {
    value.AssertIsValid();
    using Base = common_internal::BaseStructValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(std::move(value.variant_));
    } else {
      return Cast<U>(absl::get<Base>(std::move(value.variant_)));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsStructValueAlternativeV<U>, U> Get(
      StructValue&& value) {
    value.AssertIsValid();
    using Base = common_internal::BaseStructValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(std::move(value.variant_));
    } else {
      return Cast<U>(absl::get<Base>(std::move(value.variant_)));
    }
  }
};

template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::is_same_v<StructValue, absl::remove_cvref_t<From>>>>
    : CompositionCastTraits<To, From> {};

class StructValueView final {
 public:
  using interface_type = StructValueInterface;
  using alternative_type = StructValue;

  static constexpr ValueKind kKind = StructValue::kKind;

  // Constructor for alternative struct value views.
  template <typename T, typename = std::enable_if_t<
                            common_internal::IsStructValueViewAlternativeV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  StructValueView(T value)
      : variant_(absl::in_place_type<
                     common_internal::BaseStructValueViewAlternativeForT<T>>,
                 value) {}

  // Constructor for struct value.
  // NOLINTNEXTLINE(google-explicit-constructor)
  StructValueView(const StructValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : variant_(value.ToViewVariant()) {}

  // Constructor for alternative struct values.
  template <typename T, typename = std::enable_if_t<
                            common_internal::IsStructValueAlternativeV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  StructValueView(const T& value ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : variant_(absl::in_place_type<
                     common_internal::BaseStructValueViewAlternativeForT<T>>,
                 value) {}

  // Prevent binding to temporary struct values.
  StructValueView(StructValue&&) = delete;

  // Prevent binding to temporary alternative struct values.
  template <
      typename T,
      typename = std::enable_if_t<
          common_internal::IsStructValueAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  StructValueView(T&&) = delete;

  StructValueView() = default;
  StructValueView(const StructValueView&) = default;
  StructValueView& operator=(const StructValueView&) = default;

  ValueKind kind() const {
    AssertIsValid();
    return absl::visit(
        [](auto alternative) -> ValueKind {
          if constexpr (std::is_same_v<
                            absl::monostate,
                            absl::remove_cvref_t<decltype(alternative)>>) {
            return ValueKind::kStruct;
          } else {
            return alternative.kind();
          }
        },
        variant_);
  }

  StructType GetType(TypeManager& type_manager) const {
    AssertIsValid();
    return absl::visit(
        [&type_manager](auto alternative) -> StructType {
          if constexpr (std::is_same_v<
                            absl::monostate,
                            absl::remove_cvref_t<decltype(alternative)>>) {
            ABSL_UNREACHABLE();
          } else {
            return alternative.GetType(type_manager);
          }
        },
        variant_);
  }

  absl::string_view GetTypeName() const {
    AssertIsValid();
    return absl::visit(
        [](auto alternative) -> absl::string_view {
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

  std::string DebugString() const {
    AssertIsValid();
    return absl::visit(
        [](auto alternative) -> std::string {
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

  absl::StatusOr<size_t> GetSerializedSize() const {
    AssertIsValid();
    return absl::visit(
        [](auto alternative) -> absl::StatusOr<size_t> {
          if constexpr (std::is_same_v<
                            absl::monostate,
                            absl::remove_cvref_t<decltype(alternative)>>) {
            return absl::InternalError("use of invalid StructValueView");
          } else {
            return alternative.GetSerializedSize();
          }
        },
        variant_);
  }

  absl::Status SerializeTo(absl::Cord& value) const {
    AssertIsValid();
    return absl::visit(
        [&value](auto alternative) -> absl::Status {
          if constexpr (std::is_same_v<
                            absl::monostate,
                            absl::remove_cvref_t<decltype(alternative)>>) {
            return absl::InternalError("use of invalid StructValueView");
          } else {
            return alternative.SerializeTo(value);
          }
        },
        variant_);
  }

  absl::StatusOr<absl::Cord> Serialize() const {
    AssertIsValid();
    return absl::visit(
        [](auto alternative) -> absl::StatusOr<absl::Cord> {
          if constexpr (std::is_same_v<
                            absl::monostate,
                            absl::remove_cvref_t<decltype(alternative)>>) {
            return absl::InternalError("use of invalid StructValueView");
          } else {
            return alternative.Serialize();
          }
        },
        variant_);
  }

  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const {
    AssertIsValid();
    return absl::visit(
        [prefix](auto alternative) -> absl::StatusOr<std::string> {
          if constexpr (std::is_same_v<
                            absl::monostate,
                            absl::remove_cvref_t<decltype(alternative)>>) {
            return absl::InternalError("use of invalid StructValueView");
          } else {
            return alternative.GetTypeUrl(prefix);
          }
        },
        variant_);
  }

  absl::StatusOr<Any> ConvertToAny(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const {
    AssertIsValid();
    return absl::visit(
        [prefix](auto alternative) -> absl::StatusOr<Any> {
          if constexpr (std::is_same_v<
                            absl::monostate,
                            absl::remove_cvref_t<decltype(alternative)>>) {
            return absl::InternalError("use of invalid StructValueView");
          } else {
            return alternative.ConvertToAny(prefix);
          }
        },
        variant_);
  }

  absl::StatusOr<Json> ConvertToJson() const {
    AssertIsValid();
    return absl::visit(
        [](auto alternative) -> absl::StatusOr<Json> {
          if constexpr (std::is_same_v<
                            absl::monostate,
                            absl::remove_cvref_t<decltype(alternative)>>) {
            return absl::InternalError("use of invalid StructValueView");
          } else {
            return alternative.ConvertToJson();
          }
        },
        variant_);
  }

  absl::StatusOr<JsonObject> ConvertToJsonObject() const {
    AssertIsValid();
    return absl::visit(
        [](auto alternative) -> absl::StatusOr<JsonObject> {
          if constexpr (std::is_same_v<
                            absl::monostate,
                            absl::remove_cvref_t<decltype(alternative)>>) {
            return absl::InternalError("use of invalid StructValueView");
          } else {
            return alternative.ConvertToJsonObject();
          }
        },
        variant_);
  }

  void swap(StructValueView& other) noexcept {
    AssertIsValid();
    other.AssertIsValid();
    variant_.swap(other.variant_);
  }

  absl::StatusOr<ValueView> GetFieldByName(
      ValueManager& value_manager, absl::string_view name,
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  absl::StatusOr<ValueView> GetFieldByNumber(
      ValueManager& value_manager, int64_t number,
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  absl::StatusOr<bool> HasFieldByName(absl::string_view name) const {
    AssertIsValid();
    return absl::visit(
        [name](auto alternative) -> absl::StatusOr<bool> {
          if constexpr (std::is_same_v<
                            absl::monostate,
                            absl::remove_cvref_t<decltype(alternative)>>) {
            return absl::InternalError("use of invalid StructValueView");
          } else {
            return alternative.HasFieldByName(name);
          }
        },
        variant_);
  }

  absl::StatusOr<bool> HasFieldByNumber(int64_t number) const {
    AssertIsValid();
    return absl::visit(
        [number](auto alternative) -> absl::StatusOr<bool> {
          if constexpr (std::is_same_v<
                            absl::monostate,
                            absl::remove_cvref_t<decltype(alternative)>>) {
            return absl::InternalError("use of invalid StructValueView");
          } else {
            return alternative.HasFieldByNumber(number);
          }
        },
        variant_);
  }

 private:
  friend class StructValue;
  friend struct NativeTypeTraits<StructValueView>;
  friend struct CompositionTraits<StructValueView>;

  common_internal::StructValueVariant ToVariant() const;

  constexpr bool IsValid() const {
    return !absl::holds_alternative<absl::monostate>(variant_);
  }

  void AssertIsValid() const {
    ABSL_DCHECK(IsValid()) << "use of invalid StructValueView";
  }

  // Unlike many of the other derived values, `StructValue` is itself a composed
  // type. This is to avoid making `StructValue` too big and by extension
  // `Value` too big. Instead we store the derived `StructValue` values in
  // `Value` and not `StructValue` itself.
  common_internal::StructValueViewVariant variant_;
};

inline void swap(StructValueView& lhs, StructValueView& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out, StructValueView value) {
  return out << value.DebugString();
}

template <>
struct NativeTypeTraits<StructValueView> final {
  static NativeTypeId Id(StructValueView value) {
    value.AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> NativeTypeId {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return
            // `NativeTypeId::For<absl::monostate>()`. In debug builds we cannot
            // reach here.
            return NativeTypeId::For<absl::monostate>();
          } else {
            return NativeTypeId::Of(alternative);
          }
        },
        value.variant_);
  }
};

template <>
struct CompositionTraits<StructValueView> final {
  template <typename U>
  static std::enable_if_t<common_internal::IsStructValueViewAlternativeV<U>,
                          bool>
  HasA(StructValueView value) {
    value.AssertIsValid();
    using Base = common_internal::BaseStructValueViewAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::holds_alternative<U>(value.variant_);
    } else {
      return InstanceOf<U>(Get<Base>(value));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsStructValueViewAlternativeV<U>, U>
  Get(StructValueView value) {
    value.AssertIsValid();
    using Base = common_internal::BaseStructValueViewAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(value.variant_);
    } else {
      return Cast<U>(absl::get<Base>(value.variant_));
    }
  }
};

template <typename To, typename From>
struct CastTraits<To, From,
                  std::enable_if_t<std::is_same_v<StructValueView,
                                                  absl::remove_cvref_t<From>>>>
    : CompositionCastTraits<To, From> {};

inline StructValue::StructValue(StructValueView value)
    : variant_(value.ToVariant()) {}

class StructValueBuilder {
 public:
  virtual ~StructValueBuilder() = default;

  virtual absl::Status SetFieldByName(absl::string_view name, Value value) = 0;

  virtual absl::Status SetFieldByNumber(int64_t number, Value value) = 0;

  virtual StructValue Build() && = 0;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_STRUCT_VALUE_H_
