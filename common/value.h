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
#include <cstddef>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/types/variant.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/value_interface.h"  // IWYU pragma: export
#include "common/value_kind.h"
#include "common/values/bool_value.h"  // IWYU pragma: export
#include "common/values/bytes_value.h"  // IWYU pragma: export
#include "common/values/double_value.h"  // IWYU pragma: export
#include "common/values/duration_value.h"  // IWYU pragma: export
#include "common/values/error_value.h"  // IWYU pragma: export
#include "common/values/int_value.h"  // IWYU pragma: export
#include "common/values/list_value.h"  // IWYU pragma: export
#include "common/values/null_value.h"  // IWYU pragma: export
#include "common/values/string_value.h"  // IWYU pragma: export
#include "common/values/timestamp_value.h"  // IWYU pragma: export
#include "common/values/type_value.h"  // IWYU pragma: export
#include "common/values/uint_value.h"  // IWYU pragma: export
#include "common/values/unknown_value.h"  // IWYU pragma: export
#include "common/values/values.h"

namespace cel {

class Value;
class ValueView;

// `Value` is a composition type which encompasses all values supported by the
// Common Expression Language. When default constructed or moved, `Value` is in
// a known but invalid state. Any attempt to use it from then on, without
// assigning another type, is undefined behavior. In debug builds, we do our
// best to fail.
class Value final {
 public:
  Value() = default;

  Value(const Value& other)
      : variant_((other.AssertIsValid(), other.variant_)) {}

  Value& operator=(const Value& other) {
    other.AssertIsValid();
    ABSL_DCHECK(this != std::addressof(other))
        << "Value should not be copied to itself";
    variant_ = other.variant_;
    return *this;
  }

  Value(Value&& other) noexcept
      : variant_((other.AssertIsValid(), std::move(other.variant_))) {
    other.variant_.emplace<absl::monostate>();
  }

  Value& operator=(Value&& other) noexcept {
    other.AssertIsValid();
    ABSL_DCHECK(this != std::addressof(other))
        << "Value should not be moved to itself";
    variant_ = std::move(other.variant_);
    other.variant_.emplace<absl::monostate>();
    return *this;
  }

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
    AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> ValueKind {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return ValueKind::kError. In debug
            // builds we cannot reach here.
            return ValueKind::kError;
          } else {
            return alternative.kind();
          }
        },
        variant_);
  }

  TypeView type() const {
    AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> TypeView {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return an invalid type. In debug
            // builds we cannot reach here.
            return TypeView();
          } else {
            return alternative.type();
          }
        },
        variant_);
  }

  std::string DebugString() const {
    AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> std::string {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return an empty string. In debug
            // builds we cannot reach here.
            return std::string();
          } else {
            return alternative.DebugString();
          }
        },
        variant_);
  }

  void swap(Value& other) noexcept {
    AssertIsValid();
    other.AssertIsValid();
    variant_.swap(other.variant_);
  }

  friend std::ostream& operator<<(std::ostream& out, const Value& value) {
    value.AssertIsValid();
    return absl::visit(
        [&out](const auto& alternative) -> std::ostream& {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we do nothing. In debug builds we cannot
            // reach here.
            return out;
          } else {
            return out << alternative;
          }
        },
        value.variant_);
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

  constexpr bool IsValid() const {
    return !absl::holds_alternative<absl::monostate>(variant_);
  }

  void AssertIsValid() const {
    ABSL_DCHECK(IsValid()) << "use of invalid Value";
  }

  common_internal::ValueVariant variant_;
};

inline void swap(Value& lhs, Value& rhs) noexcept { lhs.swap(rhs); }

template <>
struct NativeTypeTraits<Value> final {
  static NativeTypeId Id(const Value& value) {
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

  static bool SkipDestructor(const Value& value) {
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
struct CompositionTraits<Value> final {
  template <typename U>
  static std::enable_if_t<common_internal::IsValueAlternativeV<U>, bool> HasA(
      const Value& value) {
    value.AssertIsValid();
    using Base = common_internal::BaseValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::holds_alternative<U>(value.variant_);
    } else {
      return absl::holds_alternative<Base>(value.variant_) &&
             InstanceOf<U>(Get<U>(value));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsValueAlternativeV<U>, const U&>
  Get(const Value& value) {
    value.AssertIsValid();
    using Base = common_internal::BaseValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(value.variant_);
    } else {
      return Cast<U>(absl::get<Base>(value.variant_));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsValueAlternativeV<U>, U&> Get(
      Value& value) {
    value.AssertIsValid();
    using Base = common_internal::BaseValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(value.variant_);
    } else {
      return Cast<U>(absl::get<Base>(value.variant_));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsValueAlternativeV<U>, U> Get(
      const Value&& value) {
    value.AssertIsValid();
    using Base = common_internal::BaseValueAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(std::move(value.variant_));
    } else {
      return Cast<U>(absl::get<Base>(std::move(value.variant_)));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsValueAlternativeV<U>, U> Get(
      Value&& value) {
    value.AssertIsValid();
    using Base = common_internal::BaseValueAlternativeForT<U>;
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
    std::enable_if_t<std::is_same_v<Value, absl::remove_cvref_t<From>>>>
    : CompositionCastTraits<To, From> {};

// Statically assert some expectations.
static_assert(std::is_default_constructible_v<Value>);
static_assert(std::is_copy_constructible_v<Value>);
static_assert(std::is_copy_assignable_v<Value>);
static_assert(std::is_nothrow_move_constructible_v<Value>);
static_assert(std::is_nothrow_move_assignable_v<Value>);
static_assert(std::is_nothrow_swappable_v<Value>);

// `ValueView` is a composition type which acts as a view of `Value` and its
// composed types. Like `Value`, it is also invalid when default constructed and
// must be assigned another type.
class ValueView final {
 public:
  ValueView() = default;
  ValueView(const ValueView&) = default;
  ValueView(ValueView&&) = default;
  ValueView& operator=(const ValueView&) = default;
  ValueView& operator=(ValueView&&) = default;

  // NOLINTNEXTLINE(google-explicit-constructor)
  ValueView(const Value& value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : variant_((value.AssertIsValid(), value.ToViewVariant())) {}

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
    AssertIsValid();
    return absl::visit(
        [](auto alternative) -> ValueKind {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return ValueKind::kError. In debug
            // builds we cannot reach here.
            return ValueKind::kError;
          } else {
            return alternative.kind();
          }
        },
        variant_);
  }

  TypeView type() const {
    AssertIsValid();
    return absl::visit(
        [](auto alternative) -> TypeView {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return an invalid type. In debug
            // builds we cannot reach here.
            return TypeView();
          } else {
            return alternative.type();
          }
        },
        variant_);
  }

  std::string DebugString() const {
    AssertIsValid();
    return absl::visit(
        [](auto alternative) -> std::string {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return an empty string. In debug
            // builds we cannot reach here.
            return std::string();
          } else {
            return alternative.DebugString();
          }
        },
        variant_);
  }

  void swap(ValueView& other) noexcept {
    AssertIsValid();
    other.AssertIsValid();
    variant_.swap(other.variant_);
  }

  friend std::ostream& operator<<(std::ostream& out, ValueView value) {
    value.AssertIsValid();
    return absl::visit(
        [&out](auto alternative) -> std::ostream& {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we do nothing. In debug builds we cannot
            // reach here.
            return out;
          } else {
            return out << alternative;
          }
        },
        value.variant_);
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

  constexpr bool IsValid() const {
    return !absl::holds_alternative<absl::monostate>(variant_);
  }

  void AssertIsValid() const {
    ABSL_DCHECK(IsValid()) << "use of invalid ValueView";
  }

  common_internal::ValueViewVariant variant_;
};

inline void swap(ValueView& lhs, ValueView& rhs) noexcept { lhs.swap(rhs); }

template <>
struct NativeTypeTraits<ValueView> final {
  static NativeTypeId Id(ValueView value) {
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
struct CompositionTraits<ValueView> final {
  template <typename U>
  static std::enable_if_t<common_internal::IsValueViewAlternativeV<U>, bool>
  HasA(ValueView value) {
    value.AssertIsValid();
    using Base = common_internal::BaseValueViewAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::holds_alternative<U>(value.variant_);
    } else {
      return InstanceOf<U>(Get<Base>(value));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsValueViewAlternativeV<U>, U> Get(
      ValueView value) {
    value.AssertIsValid();
    using Base = common_internal::BaseValueViewAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(value.variant_);
    } else {
      return Cast<U>(absl::get<Base>(value.variant_));
    }
  }
};

template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::is_same_v<ValueView, absl::remove_cvref_t<From>>>>
    : CompositionCastTraits<To, From> {};

// Statically assert some expectations.
static_assert(std::is_default_constructible_v<ValueView>);
static_assert(std::is_nothrow_copy_constructible_v<ValueView>);
static_assert(std::is_nothrow_copy_assignable_v<ValueView>);
static_assert(std::is_nothrow_move_constructible_v<ValueView>);
static_assert(std::is_nothrow_move_assignable_v<ValueView>);
static_assert(std::is_nothrow_swappable_v<ValueView>);
static_assert(std::is_trivially_copyable_v<ValueView>);
static_assert(std::is_trivially_destructible_v<ValueView>);

inline Value::Value(ValueView other)
    : variant_((other.AssertIsValid(), other.ToVariant())) {}

inline Value& Value::operator=(ValueView other) {
  other.AssertIsValid();
  variant_ = other.ToVariant();
  return *this;
}

using ValueIteratorPtr = std::unique_ptr<ValueIterator>;

class ValueIterator {
 public:
  virtual ~ValueIterator() = default;

  virtual bool HasNext() = 0;

  // Returns a view of the next value. If the underlying implementation cannot
  // directly return a view of a value, the value will be stored in `scratch`,
  // and the returned view will be that of `scratch`.
  virtual absl::StatusOr<ValueView> Next(
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) = 0;
};

// Now that Value and ValueView are complete, we can define various parts of
// list, map, opaque, and struct which depend on Value and ValueView.

namespace common_internal {

template <typename T>
class TypedListValue final : public ListValueInterface {
 public:
  TypedListValue(ListType type, std::vector<T>&& elements)
      : type_(std::move(type)), elements_(std::move(elements)) {}

  std::string DebugString() const override {
    return absl::StrCat(
        "[", absl::StrJoin(elements_, ", ", absl::StreamFormatter()), "]");
  }

  bool IsEmpty() const override { return elements_.empty(); }

  size_t Size() const override { return elements_.size(); }

 protected:
  TypeView get_type() const override { return type_; }

 private:
  absl::StatusOr<ValueView> GetImpl(size_t index, Value&) const override {
    return elements_[index];
  }

  NativeTypeId GetNativeTypeId() const noexcept override {
    return NativeTypeId::For<TypedListValue<T>>();
  }

  const ListType type_;
  const std::vector<T> elements_;
};

}  // namespace common_internal

inline absl::StatusOr<ValueView> ListValue::Get(size_t index,
                                                Value& scratch) const {
  return interface_->Get(index, scratch);
}

inline absl::Status ListValue::ForEach(ForEachCallback callback) const {
  return interface_->ForEach(callback);
}

inline absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> ListValue::NewIterator()
    const {
  return interface_->NewIterator();
}

inline absl::StatusOr<ValueView> ListValueView::Get(size_t index,
                                                    Value& scratch) const {
  return interface_->Get(index, scratch);
}

inline absl::Status ListValueView::ForEach(ForEachCallback callback) const {
  return interface_->ForEach(callback);
}

inline absl::StatusOr<absl::Nonnull<ValueIteratorPtr>>
ListValueView::NewIterator() const {
  return interface_->NewIterator();
}

template <>
class ListValueBuilder<Value> final : public ListValueBuilderInterface {
 public:
  ListValueBuilder(TypeFactory& type_factory ABSL_ATTRIBUTE_LIFETIME_BOUND,
                   TypeView element)
      : ListValueBuilder(type_factory.memory_manager(),
                         type_factory.CreateListType(element)) {}

  ListValueBuilder(MemoryManagerRef memory_manager, ListType type)
      : memory_manager_(memory_manager), type_(std::move(type)) {}

  ListValueBuilder(const ListValueBuilder&) = delete;
  ListValueBuilder(ListValueBuilder&&) = delete;
  ListValueBuilder& operator=(const ListValueBuilder&) = delete;
  ListValueBuilder& operator=(ListValueBuilder&&) = delete;

  void Add(Value value) override { elements_.push_back(std::move(value)); }

  bool IsEmpty() const override { return elements_.empty(); }

  size_t Size() const override { return elements_.size(); }

  void Reserve(size_t capacity) override { elements_.reserve(capacity); }

  ListValue Build() && override {
    return ListValue(
        memory_manager_.MakeShared<common_internal::TypedListValue<Value>>(
            std::move(type_), std::move(elements_)));
  }

 private:
  MemoryManagerRef memory_manager_;
  ListType type_;
  std::vector<Value> elements_;
};

template <typename T>
void ListValueBuilder<T>::Add(Value value) {
  Add(Cast<T>(std::move(value)));
}

template <typename T>
void ListValueBuilder<T>::Add(T value) {
  elements_.push_back(std::move(value));
}

template <typename T>
ListValue ListValueBuilder<T>::Build() && {
  return ListValue(
      memory_manager_.template MakeShared<common_internal::TypedListValue<T>>(
          std::move(type_), std::move(elements_)));
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUE_H_
