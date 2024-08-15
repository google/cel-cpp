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

#include <cstdint>
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
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "base/attribute.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/legacy_struct_value.h"  // IWYU pragma: export
#include "common/values/parsed_struct_value.h"  // IWYU pragma: export
#include "common/values/struct_value_interface.h"  // IWYU pragma: export
#include "common/values/values.h"
#include "runtime/runtime_options.h"

namespace cel {

class StructValueInterface;
class StructValue;
class Value;
class ValueManager;
class TypeManager;

class StructValue final {
 public:
  using interface_type = StructValueInterface;

  static constexpr ValueKind kKind = StructValueInterface::kKind;

  // Copy constructor for alternative struct values.
  template <
      typename T,
      typename = std::enable_if_t<
          common_internal::IsStructValueAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  StructValue(const T& value)
      : variant_(
            absl::in_place_type<common_internal::BaseStructValueAlternativeForT<
                absl::remove_cvref_t<T>>>,
            value) {}

  // Move constructor for alternative struct values.
  template <
      typename T,
      typename = std::enable_if_t<
          common_internal::IsStructValueAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  StructValue(T&& value)
      : variant_(
            absl::in_place_type<common_internal::BaseStructValueAlternativeForT<
                absl::remove_cvref_t<T>>>,
            std::forward<T>(value)) {}

  StructValue() = default;

  StructValue(const StructValue& other)
      : variant_((other.AssertIsValid(), other.variant_)) {}

  StructValue(StructValue&& other) noexcept
      : variant_((other.AssertIsValid(), std::move(other.variant_))) {}

  StructValue& operator=(const StructValue& other) {
    other.AssertIsValid();
    ABSL_DCHECK(this != std::addressof(other))
        << "StructValue should not be copied to itself";
    variant_ = other.variant_;
    return *this;
  }

  StructValue& operator=(StructValue&& other) noexcept {
    other.AssertIsValid();
    ABSL_DCHECK(this != std::addressof(other))
        << "StructValue should not be moved to itself";
    variant_ = std::move(other.variant_);
    other.variant_.emplace<absl::monostate>();
    return *this;
  }

  constexpr ValueKind kind() const { return kKind; }

  StructType GetRuntimeType() const;

  absl::string_view GetTypeName() const;

  std::string DebugString() const;

  absl::Status SerializeTo(AnyToJsonConverter& converter,
                           absl::Cord& value) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter& converter) const;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const;

  void swap(StructValue& other) noexcept {
    AssertIsValid();
    other.AssertIsValid();
    variant_.swap(other.variant_);
  }

  absl::Status GetFieldByName(ValueManager& value_manager,
                              absl::string_view name, Value& result,
                              ProtoWrapperTypeOptions unboxing_options =
                                  ProtoWrapperTypeOptions::kUnsetNull) const;
  absl::StatusOr<Value> GetFieldByName(
      ValueManager& value_manager, absl::string_view name,
      ProtoWrapperTypeOptions unboxing_options =
          ProtoWrapperTypeOptions::kUnsetNull) const;

  absl::Status GetFieldByNumber(ValueManager& value_manager, int64_t number,
                                Value& result,
                                ProtoWrapperTypeOptions unboxing_options =
                                    ProtoWrapperTypeOptions::kUnsetNull) const;
  absl::StatusOr<Value> GetFieldByNumber(
      ValueManager& value_manager, int64_t number,
      ProtoWrapperTypeOptions unboxing_options =
          ProtoWrapperTypeOptions::kUnsetNull) const;

  absl::StatusOr<bool> HasFieldByName(absl::string_view name) const;

  absl::StatusOr<bool> HasFieldByNumber(int64_t number) const;

  using ForEachFieldCallback = StructValueInterface::ForEachFieldCallback;

  absl::Status ForEachField(ValueManager& value_manager,
                            ForEachFieldCallback callback) const;

  absl::StatusOr<int> Qualify(ValueManager& value_manager,
                              absl::Span<const SelectQualifier> qualifiers,
                              bool presence_test, Value& result) const;
  absl::StatusOr<std::pair<Value, int>> Qualify(
      ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
      bool presence_test) const;

 private:
  friend struct NativeTypeTraits<StructValue>;
  friend struct CompositionTraits<StructValue>;

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
  static std::enable_if_t<std::is_same_v<Value, U>, bool> HasA(
      const StructValue& value) {
    value.AssertIsValid();
    return true;
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

  template <typename U>
  static std::enable_if_t<std::is_same_v<Value, U>, U> Get(
      const StructValue& value) {
    value.AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> U {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            return U{};
          } else {
            return U{alternative};
          }
        },
        value.variant_);
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<Value, U>, U> Get(StructValue& value) {
    value.AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> U {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            return U{};
          } else {
            return U{alternative};
          }
        },
        value.variant_);
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<Value, U>, U> Get(
      const StructValue&& value) {
    value.AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> U {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            return U{};
          } else {
            return U{alternative};
          }
        },
        value.variant_);
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<Value, U>, U> Get(
      StructValue&& value) {
    value.AssertIsValid();
    return absl::visit(
        [](auto&& alternative) -> U {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            return U{};
          } else {
            return U{std::move(alternative)};
          }
        },
        std::move(value.variant_));
  }
};

template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::is_same_v<StructValue, absl::remove_cvref_t<From>>>>
    : CompositionCastTraits<To, From> {};

class StructValueBuilder {
 public:
  virtual ~StructValueBuilder() = default;

  virtual absl::Status SetFieldByName(absl::string_view name, Value value) = 0;

  virtual absl::Status SetFieldByNumber(int64_t number, Value value) = 0;

  virtual absl::StatusOr<StructValue> Build() && = 0;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_STRUCT_VALUE_H_
