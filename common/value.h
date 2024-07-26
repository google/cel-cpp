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
#include <cstdint>
#include <memory>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "common/any.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/type_manager.h"
#include "common/value_interface.h"  // IWYU pragma: export
#include "common/value_kind.h"
#include "common/values/bool_value.h"  // IWYU pragma: export
#include "common/values/bytes_value.h"  // IWYU pragma: export
#include "common/values/double_value.h"  // IWYU pragma: export
#include "common/values/duration_value.h"  // IWYU pragma: export
#include "common/values/error_value.h"  // IWYU pragma: export
#include "common/values/int_value.h"  // IWYU pragma: export
#include "common/values/list_value.h"  // IWYU pragma: export
#include "common/values/map_value.h"  // IWYU pragma: export
#include "common/values/null_value.h"  // IWYU pragma: export
#include "common/values/opaque_value.h"  // IWYU pragma: export
#include "common/values/optional_value.h"  // IWYU pragma: export
#include "common/values/string_value.h"  // IWYU pragma: export
#include "common/values/struct_value.h"  // IWYU pragma: export
#include "common/values/timestamp_value.h"  // IWYU pragma: export
#include "common/values/type_value.h"  // IWYU pragma: export
#include "common/values/uint_value.h"  // IWYU pragma: export
#include "common/values/unknown_value.h"  // IWYU pragma: export
#include "common/values/values.h"
#include "internal/status_macros.h"
#include "runtime/runtime_options.h"

namespace cel {

class Value;

// `Value` is a composition type which encompasses all values supported by the
// Common Expression Language. When default constructed or moved, `Value` is in
// a known but invalid state. Any attempt to use it from then on, without
// assigning another type, is undefined behavior. In debug builds, we do our
// best to fail.
class Value final {
 public:
  Value() = default;
  Value(const Value&) = default;
  Value& operator=(const Value&) = default;
  Value(Value&& other) = default;
  Value& operator=(Value&&) = default;

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const ListValue& value)
      : Value(CompositionTraits<ListValue>::Get<Value>(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(ListValue&& value)
      : Value(CompositionTraits<ListValue>::Get<Value>(std::move(value))) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(const ListValue& value) {
    return *this = CompositionTraits<ListValue>::Get<Value>(value);
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(ListValue&& value) {
    return *this = CompositionTraits<ListValue>::Get<Value>(std::move(value));
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const MapValue& value)
      : Value(CompositionTraits<MapValue>::Get<Value>(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(MapValue&& value)
      : Value(CompositionTraits<MapValue>::Get<Value>(std::move(value))) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(const MapValue& value) {
    return *this = CompositionTraits<MapValue>::Get<Value>(value);
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(MapValue&& value) {
    return *this = CompositionTraits<MapValue>::Get<Value>(std::move(value));
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(const StructValue& value)
      : Value(CompositionTraits<StructValue>::Get<Value>(value)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value(StructValue&& value)
      : Value(CompositionTraits<StructValue>::Get<Value>(std::move(value))) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(const StructValue& value) {
    return *this = CompositionTraits<StructValue>::Get<Value>(value);
  }

  // NOLINTNEXTLINE(google-explicit-constructor)
  Value& operator=(StructValue&& value) {
    return *this = CompositionTraits<StructValue>::Get<Value>(std::move(value));
  }

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

  ValueKind kind() const;

  Type GetType(TypeManager& type_manager) const;

  absl::string_view GetTypeName() const;

  std::string DebugString() const;

  // `SerializeTo` serializes this value and appends it to `value`. If this
  // value does not support serialization, `FAILED_PRECONDITION` is returned.
  absl::Status SerializeTo(AnyToJsonConverter& value_manager,
                           absl::Cord& value) const;

  // `Serialize` serializes this value and returns it as `absl::Cord`. If this
  // value does not support serialization, `FAILED_PRECONDITION` is returned.
  absl::StatusOr<absl::Cord> Serialize(AnyToJsonConverter& value_manager) const;

  // 'GetTypeUrl' returns the type URL that can be used as the type URL for
  // `Any`. If this value does not support serialization, `FAILED_PRECONDITION`
  // is returned.
  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  // 'ConvertToAny' converts this value to `Any`. If this value does not support
  // serialization, `FAILED_PRECONDITION` is returned.
  absl::StatusOr<Any> ConvertToAny(
      AnyToJsonConverter& value_manager,
      absl::string_view prefix = kTypeGoogleApisComPrefix) const;

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter& value_manager) const;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const;

  friend void swap(Value& lhs, Value& rhs) noexcept {
    using std::swap;
    swap(lhs.variant_, rhs.variant_);
  }

  friend std::ostream& operator<<(std::ostream& out, const Value& value);

  template <typename T>
  ABSL_DEPRECATED("Use cel::InstanceOf")
  bool Is() const {
    return cel::InstanceOf<T>(*this);
  }

  template <typename T>
  ABSL_DEPRECATED("Use cel::Cast or cel::As")
  auto As() const& {
    return cel::Cast<T>(*this);
  }

  template <typename T>
  ABSL_DEPRECATED("Use cel::Cast or cel::As")
  auto As() & {
    return cel::Cast<T>(*this);
  }

  template <typename T>
  ABSL_DEPRECATED("Use cel::Cast or cel::As")
  auto As() const&& {
    return cel::Cast<T>(std::move(*this));
  }

  template <typename T>
  ABSL_DEPRECATED("Use cel::Cast or cel::As")
  auto As() && {
    return cel::Cast<T>(std::move(*this));
  }

  ABSL_DEPRECATED("Just use operator.()")
  Value* operator->() { return this; }

  ABSL_DEPRECATED("Just use operator.()")
  const Value* operator->() const { return this; }

  // When `Value` is default constructed, it is in a valid but undefined state.
  // Any attempt to use it invokes undefined behavior. This mention can be used
  // to test whether this value is valid.
  explicit operator bool() const noexcept { return IsValid(); }

 private:
  friend struct NativeTypeTraits<Value>;
  friend struct CompositionTraits<Value>;

  constexpr bool IsValid() const noexcept {
    return !absl::holds_alternative<absl::monostate>(variant_);
  }

  void AssertIsValid() const {
    ABSL_DCHECK(IsValid()) << "use of invalid Value";
  }

  common_internal::ValueVariant variant_;
};

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
  static std::enable_if_t<std::is_same_v<ListValue, U>, bool> HasA(
      const Value& value) {
    value.AssertIsValid();
    return absl::holds_alternative<common_internal::LegacyListValue>(
               value.variant_) ||
           absl::holds_alternative<ParsedListValue>(value.variant_);
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<MapValue, U>, bool> HasA(
      const Value& value) {
    value.AssertIsValid();
    return absl::holds_alternative<common_internal::LegacyMapValue>(
               value.variant_) ||
           absl::holds_alternative<ParsedMapValue>(value.variant_);
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<StructValue, U>, bool> HasA(
      const Value& value) {
    value.AssertIsValid();
    return absl::holds_alternative<common_internal::LegacyStructValue>(
               value.variant_) ||
           absl::holds_alternative<ParsedStructValue>(value.variant_);
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

  template <typename U>
  static std::enable_if_t<std::is_same_v<ListValue, U>, U> Get(
      const Value& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyListValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyListValue>(value.variant_)};
    }
    return U{absl::get<ParsedListValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<ListValue, U>, U> Get(Value& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyListValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyListValue>(value.variant_)};
    }
    return U{absl::get<ParsedListValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<ListValue, U>, U> Get(
      const Value&& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyListValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyListValue>(value.variant_)};
    }
    return U{absl::get<ParsedListValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<ListValue, U>, U> Get(Value&& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyListValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyListValue>(
          std::move(value.variant_))};
    }
    return U{absl::get<ParsedListValue>(std::move(value.variant_))};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<MapValue, U>, U> Get(
      const Value& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyMapValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyMapValue>(value.variant_)};
    }
    return U{absl::get<ParsedMapValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<MapValue, U>, U> Get(Value& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyMapValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyMapValue>(value.variant_)};
    }
    return U{absl::get<ParsedMapValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<MapValue, U>, U> Get(
      const Value&& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyMapValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyMapValue>(value.variant_)};
    }
    return U{absl::get<ParsedMapValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<MapValue, U>, U> Get(Value&& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyMapValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyMapValue>(
          std::move(value.variant_))};
    }
    return U{absl::get<ParsedMapValue>(std::move(value.variant_))};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<StructValue, U>, U> Get(
      const Value& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyStructValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyStructValue>(value.variant_)};
    }
    return U{absl::get<ParsedStructValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<StructValue, U>, U> Get(Value& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyStructValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyStructValue>(value.variant_)};
    }
    return U{absl::get<ParsedStructValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<StructValue, U>, U> Get(
      const Value&& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyStructValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyStructValue>(value.variant_)};
    }
    return U{absl::get<ParsedStructValue>(value.variant_)};
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<StructValue, U>, U> Get(
      Value&& value) {
    value.AssertIsValid();
    if (absl::holds_alternative<common_internal::LegacyStructValue>(
            value.variant_)) {
      return U{absl::get<common_internal::LegacyStructValue>(
          std::move(value.variant_))};
    }
    return U{absl::get<ParsedStructValue>(std::move(value.variant_))};
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

using ValueIteratorPtr = std::unique_ptr<ValueIterator>;

class ValueIterator {
 public:
  virtual ~ValueIterator() = default;

  virtual bool HasNext() = 0;

  // Returns a view of the next value. If the underlying implementation cannot
  // directly return a view of a value, the value will be stored in `scratch`,
  // and the returned view will be that of `scratch`.
  virtual absl::Status Next(ValueManager& value_manager, Value& result) = 0;

  absl::StatusOr<Value> Next(ValueManager& value_manager) {
    Value result;
    CEL_RETURN_IF_ERROR(Next(value_manager, result));
    return result;
  }
};

class ValueBuilder {
 public:
  virtual ~ValueBuilder() = default;

  virtual absl::Status SetFieldByName(absl::string_view name, Value value) = 0;

  virtual absl::Status SetFieldByNumber(int64_t number, Value value) = 0;

  virtual Value Build() && = 0;
};

using ListValueBuilderInterface = ListValueBuilder;
using MapValueBuilderInterface = MapValueBuilder;
using StructValueBuilderInterface = StructValueBuilder;

// Now that Value is complete, we can define various parts of list, map, opaque,
// and struct which depend on Value.

inline ErrorValue::ErrorValue()
    : ErrorValue(common_internal::GetDefaultErrorValue()) {}

inline absl::Status ParsedListValue::Get(ValueManager& value_manager,
                                         size_t index, Value& result) const {
  return interface_->Get(value_manager, index, result);
}

inline absl::Status ParsedListValue::ForEach(ValueManager& value_manager,
                                             ForEachCallback callback) const {
  return interface_->ForEach(value_manager, callback);
}

inline absl::Status ParsedListValue::ForEach(
    ValueManager& value_manager, ForEachWithIndexCallback callback) const {
  return interface_->ForEach(value_manager, callback);
}

inline absl::StatusOr<absl::Nonnull<ValueIteratorPtr>>
ParsedListValue::NewIterator(ValueManager& value_manager) const {
  return interface_->NewIterator(value_manager);
}

inline absl::Status ParsedListValue::Equal(ValueManager& value_manager,
                                           const Value& other,
                                           Value& result) const {
  return interface_->Equal(value_manager, other, result);
}

inline absl::Status ParsedListValue::Contains(ValueManager& value_manager,
                                              const Value& other,
                                              Value& result) const {
  return interface_->Contains(value_manager, other, result);
}

inline absl::Status OpaqueValue::Equal(ValueManager& value_manager,
                                       const Value& other,
                                       Value& result) const {
  return interface_->Equal(value_manager, other, result);
}

inline cel::Value OptionalValueInterface::Value() const {
  cel::Value result;
  Value(result);
  return result;
}

inline OptionalValue OptionalValue::None() {
  return OptionalValue(common_internal::GetEmptyDynOptionalValue());
}

inline void OptionalValue::Value(cel::Value& result) const {
  (*this)->Value(result);
}

inline cel::Value OptionalValue::Value() const { return (*this)->Value(); }

inline absl::Status ParsedMapValue::Get(ValueManager& value_manager,
                                        const Value& key, Value& result) const {
  return interface_->Get(value_manager, key, result);
}

inline absl::StatusOr<bool> ParsedMapValue::Find(ValueManager& value_manager,
                                                 const Value& key,
                                                 Value& result) const {
  return interface_->Find(value_manager, key, result);
}

inline absl::Status ParsedMapValue::Has(ValueManager& value_manager,
                                        const Value& key, Value& result) const {
  return interface_->Has(value_manager, key, result);
}

inline absl::Status ParsedMapValue::ListKeys(ValueManager& value_manager,
                                             ListValue& result) const {
  return interface_->ListKeys(value_manager, result);
}

inline absl::Status ParsedMapValue::ForEach(ValueManager& value_manager,
                                            ForEachCallback callback) const {
  return interface_->ForEach(value_manager, callback);
}

inline absl::StatusOr<absl::Nonnull<ValueIteratorPtr>>
ParsedMapValue::NewIterator(ValueManager& value_manager) const {
  return interface_->NewIterator(value_manager);
}

inline absl::Status ParsedMapValue::Equal(ValueManager& value_manager,
                                          const Value& other,
                                          Value& result) const {
  return interface_->Equal(value_manager, other, result);
}

inline absl::Status ParsedStructValue::GetFieldByName(
    ValueManager& value_manager, absl::string_view name, Value& result,
    ProtoWrapperTypeOptions unboxing_options) const {
  return interface_->GetFieldByName(value_manager, name, result,
                                    unboxing_options);
}

inline absl::Status ParsedStructValue::GetFieldByNumber(
    ValueManager& value_manager, int64_t number, Value& result,
    ProtoWrapperTypeOptions unboxing_options) const {
  return interface_->GetFieldByNumber(value_manager, number, result,
                                      unboxing_options);
}

inline absl::Status ParsedStructValue::Equal(ValueManager& value_manager,
                                             const Value& other,
                                             Value& result) const {
  return interface_->Equal(value_manager, other, result);
}

inline absl::Status ParsedStructValue::ForEachField(
    ValueManager& value_manager, ForEachFieldCallback callback) const {
  return interface_->ForEachField(value_manager, callback);
}

inline absl::StatusOr<int> ParsedStructValue::Qualify(
    ValueManager& value_manager, absl::Span<const SelectQualifier> qualifiers,
    bool presence_test, Value& result) const {
  return interface_->Qualify(value_manager, qualifiers, presence_test, result);
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUE_H_
