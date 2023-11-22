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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUE_H_

#include <algorithm>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_log.h"
#include "absl/meta/type_traits.h"
#include "absl/types/variant.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/value_interface.h"  // IWYU pragma: export
#include "common/value_kind.h"
#include "common/values/bool_value.h"  // IWYU pragma: export
#include "common/values/double_value.h"  // IWYU pragma: export
#include "common/values/duration_value.h"  // IWYU pragma: export
#include "common/values/enum_value.h"  // IWYU pragma: export
#include "common/values/int_value.h"  // IWYU pragma: export
#include "common/values/null_value.h"  // IWYU pragma: export
#include "common/values/timestamp_value.h"  // IWYU pragma: export
#include "common/values/values.h"

namespace cel {

class Value;
class ValueView;

class Value final {
 public:
  Value() = delete;

  Value(const Value&) = default;
  Value& operator=(const Value&) = default;

#ifndef NDEBUG
  Value(Value&& other) noexcept : variant_(std::move(other.variant_)) {
    other.variant_.emplace<absl::monostate>();
  }

  Value& operator=(Value&& other) noexcept {
    variant_ = std::move(other.variant_);
    other.variant_.emplace<absl::monostate>();
    return *this;
  }
#else
  Value(Value&&) = default;

  Value& operator=(Value&&) = default;
#endif

  explicit Value(ValueView other);

  Value& operator=(ValueView other);

  template <typename T,
            typename = std::enable_if_t<common_internal::IsValueInterfaceV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const Shared<const T>& interface) noexcept
      : variant_(
            absl::in_place_type<common_internal::BaseValueAlternativeForT<T>>,
            interface) {}

  template <typename T,
            typename = std::enable_if_t<common_internal::IsValueInterfaceV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(Shared<const T>&& interface) noexcept
      : variant_(
            absl::in_place_type<common_internal::BaseValueAlternativeForT<T>>,
            std::move(interface)) {}

  template <typename T,
            typename = std::enable_if_t<
                common_internal::IsValueAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(T&& alternative) noexcept
      : variant_(absl::in_place_type<common_internal::BaseValueAlternativeForT<
                     absl::remove_cvref_t<T>>>,
                 std::forward<T>(alternative)) {}

  template <typename T,
            typename = std::enable_if_t<
                common_internal::IsValueAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(T&& type) noexcept {
    variant_.emplace<
        common_internal::BaseValueAlternativeForT<absl::remove_cvref_t<T>>>(
        std::forward<T>(type));
    return *this;
  }

  ValueKind kind() const {
    return absl::visit(
        [](const auto& alternative) -> ValueKind {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            ABSL_LOG(FATAL) << "use of moved-from Value";  // Crash OK
            ABSL_UNREACHABLE();
          } else {
            return alternative.kind();
          }
        },
        variant_);
  }

  TypeView type() const {
    return absl::visit(
        [](const auto& alternative) -> TypeView {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            ABSL_LOG(FATAL) << "use of moved-from Value";  // Crash OK
            ABSL_UNREACHABLE();
          } else {
            return alternative.type();
          }
        },
        variant_);
  }

  std::string DebugString() const {
    return absl::visit(
        [](const auto& alternative) -> std::string {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            ABSL_LOG(FATAL) << "use of moved-from Value";  // Crash OK
            ABSL_UNREACHABLE();
          } else {
            return alternative.DebugString();
          }
        },
        variant_);
  }

  void swap(Value& other) noexcept { variant_.swap(other.variant_); }

  friend std::ostream& operator<<(std::ostream& out, const Value& type) {
    return absl::visit(
        [&out](const auto& alternative) -> std::ostream& {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            ABSL_LOG(FATAL) << "use of moved-from Value";  // Crash OK
            ABSL_UNREACHABLE();
          } else {
            return out << alternative;
          }
        },
        type.variant_);
  }

 private:
  friend class ValueView;
  friend struct NativeTypeTraits<Value>;
  friend struct CompositionTraits<Value>;

  common_internal::ValueViewVariant ToViewVariant() const {
    return absl::visit(
        [](const auto& alternative) -> common_internal::ValueViewVariant {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            static_assert(
                std::is_same_v<absl::variant_alternative_t<
                                   0, common_internal::ValueViewVariant>,
                               absl::monostate>);
            static_assert(std::is_same_v<absl::variant_alternative_t<
                                             0, common_internal::ValueVariant>,
                                         absl::monostate>);
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

  common_internal::ValueVariant variant_;
};

inline void swap(Value& lhs, Value& rhs) noexcept { lhs.swap(rhs); }

template <>
struct NativeTypeTraits<Value> final {
  static NativeTypeId Id(const Value& type) {
    return absl::visit(
        [](const auto& alternative) -> NativeTypeId {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            ABSL_LOG(FATAL) << "use of moved-from Value";  // Crash OK
            ABSL_UNREACHABLE();
          } else {
            return NativeTypeId::Of(alternative);
          }
        },
        type.variant_);
  }

  static bool SkipDestructor(const Value& type) {
    return absl::visit(
        [](const auto& alternative) -> bool {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            ABSL_LOG(FATAL) << "use of moved-from Value";  // Crash OK
            ABSL_UNREACHABLE();
          } else {
            return NativeType::SkipDestructor(alternative);
          }
        },
        type.variant_);
  }
};

template <>
struct CompositionTraits<Value> final {
  template <typename U>
  static std::enable_if_t<common_internal::IsValueAlternativeV<U>, bool> HasA(
      const Value& type) {
    using Base = common_internal::BaseValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::holds_alternative<U>(type.variant_);
    } else {
      return absl::holds_alternative<Base>(type.variant_) &&
             InstanceOf<U>(Get<U>(type));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsValueAlternativeV<U>, const U&>
  Get(const Value& type) {
    using Base = common_internal::BaseValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(type.variant_);
    } else {
      return Cast<U>(absl::get<Base>(type.variant_));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsValueAlternativeV<U>, U&> Get(
      Value& type) {
    using Base = common_internal::BaseValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(type.variant_);
    } else {
      return Cast<U>(absl::get<Base>(type.variant_));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsValueAlternativeV<U>, U> Get(
      const Value&& type) {
    using Base = common_internal::BaseValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(std::move(type.variant_));
    } else {
      return Cast<U>(absl::get<Base>(std::move(type.variant_)));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsValueAlternativeV<U>, U> Get(
      Value&& type) {
    using Base = common_internal::BaseValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(std::move(type.variant_));
    } else {
      return Cast<U>(absl::get<Base>(std::move(type.variant_)));
    }
  }
};

template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::is_same_v<Value, absl::remove_cvref_t<From>>>>
    : CompositionCastTraits<To, From> {};

// Statically assert some expectations.
static_assert(!std::is_default_constructible_v<Value>);
static_assert(std::is_copy_constructible_v<Value>);
static_assert(std::is_copy_assignable_v<Value>);
static_assert(std::is_nothrow_move_constructible_v<Value>);
static_assert(std::is_nothrow_move_assignable_v<Value>);
static_assert(std::is_nothrow_swappable_v<Value>);

class ValueView final {
 public:
  ValueView() = delete;
  ValueView(const ValueView&) = default;
  ValueView(ValueView&&) = default;
  ValueView& operator=(const ValueView&) = default;
  ValueView& operator=(ValueView&&) = default;

  // NOLINTNEXTLINE(google-explicit-constructor)
  ValueView(const Value& type ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : variant_(type.ToViewVariant()) {}

  template <typename T, typename = std::enable_if_t<
                            common_internal::IsValueAlternativeV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  ValueView(const T& alternative ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : variant_(absl::in_place_type<
                     common_internal::BaseValueViewAlternativeForT<T>>,
                 alternative) {}

  template <
      typename T,
      typename = std::enable_if_t<
          common_internal::IsValueViewAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  ValueView(T&& alternative) noexcept
      : variant_(
            absl::in_place_type<common_internal::BaseValueViewAlternativeForT<
                absl::remove_cvref_t<T>>>,
            std::forward<T>(alternative)) {}

  template <typename T,
            typename = std::enable_if_t<common_internal::IsValueInterfaceV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  ValueView(SharedView<const T> interface) noexcept
      : variant_(absl::in_place_type<
                     common_internal::BaseValueViewAlternativeForT<T>>,
                 interface) {}

  ValueKind kind() const {
    return absl::visit(
        [](auto alternative) -> ValueKind {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            ABSL_LOG(FATAL) << "use of moved-from Value";  // Crash OK
            ABSL_UNREACHABLE();
          } else {
            return alternative.kind();
          }
        },
        variant_);
  }

  TypeView type() const {
    return absl::visit(
        [](auto alternative) -> TypeView {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            ABSL_LOG(FATAL) << "use of moved-from Value";  // Crash OK
            ABSL_UNREACHABLE();
          } else {
            return alternative.type();
          }
        },
        variant_);
  }

  std::string DebugString() const {
    return absl::visit(
        [](auto alternative) -> std::string {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            ABSL_LOG(FATAL) << "use of moved-from Value";  // Crash OK
            ABSL_UNREACHABLE();
          } else {
            return alternative.DebugString();
          }
        },
        variant_);
  }

  void swap(ValueView& other) noexcept { variant_.swap(other.variant_); }

  friend std::ostream& operator<<(std::ostream& out, ValueView type) {
    return absl::visit(
        [&out](auto alternative) -> std::ostream& {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            ABSL_LOG(FATAL) << "use of moved-from Value";  // Crash OK
            ABSL_UNREACHABLE();
          } else {
            return out << alternative;
          }
        },
        type.variant_);
  }

 private:
  friend class Value;
  friend struct NativeTypeTraits<ValueView>;
  friend struct CompositionTraits<ValueView>;

  common_internal::ValueVariant ToVariant() const {
    return absl::visit(
        [](auto alternative) -> common_internal::ValueVariant {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            static_assert(
                std::is_same_v<absl::variant_alternative_t<
                                   0, common_internal::ValueViewVariant>,
                               absl::monostate>);
            static_assert(std::is_same_v<absl::variant_alternative_t<
                                             0, common_internal::ValueVariant>,
                                         absl::monostate>);
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

  common_internal::ValueViewVariant variant_;
};

inline void swap(ValueView& lhs, ValueView& rhs) noexcept { lhs.swap(rhs); }

template <>
struct NativeTypeTraits<ValueView> final {
  static NativeTypeId Id(ValueView type) {
    return absl::visit(
        [](const auto& alternative) -> NativeTypeId {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // Only present in debug builds.
            ABSL_LOG(FATAL) << "use of moved-from Value";  // Crash OK
            ABSL_UNREACHABLE();
          } else {
            return NativeTypeId::Of(alternative);
          }
        },
        type.variant_);
  }
};

template <>
struct CompositionTraits<ValueView> final {
  template <typename U>
  static std::enable_if_t<common_internal::IsValueViewAlternativeV<U>, bool>
  HasA(ValueView type) {
    using Base = common_internal::BaseValueViewAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::holds_alternative<U>(type.variant_);
    } else {
      return InstanceOf<U>(Get<Base>(type));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsValueViewAlternativeV<U>, U> Get(
      ValueView type) {
    using Base = common_internal::BaseValueViewAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(type.variant_);
    } else {
      return Cast<U>(absl::get<Base>(type.variant_));
    }
  }
};

template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::is_same_v<ValueView, absl::remove_cvref_t<From>>>>
    : CompositionCastTraits<To, From> {};

// Statically assert some expectations.
static_assert(!std::is_default_constructible_v<ValueView>);
static_assert(std::is_nothrow_copy_constructible_v<ValueView>);
static_assert(std::is_nothrow_copy_assignable_v<ValueView>);
static_assert(std::is_nothrow_move_constructible_v<ValueView>);
static_assert(std::is_nothrow_move_assignable_v<ValueView>);
static_assert(std::is_nothrow_swappable_v<ValueView>);
static_assert(std::is_trivially_copyable_v<ValueView>);
static_assert(std::is_trivially_destructible_v<ValueView>);

inline Value::Value(ValueView other) : variant_(other.ToVariant()) {}

inline Value& Value::operator=(ValueView other) {
  variant_ = other.ToVariant();
  return *this;
}

// Now that Value and ValueView are complete, we can define various parts of
// list, map, opaque, and struct which depend on Value and ValueView.

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUE_H_
