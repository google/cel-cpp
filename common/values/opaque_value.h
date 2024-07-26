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
// IWYU pragma: friend "common/values/optional_value.h"

// `OpaqueValue` represents values of the `opaque` type. `OpaqueValueView`
// is a non-owning view of `OpaqueValue`. `OpaqueValueInterface` is the abstract
// base class of implementations. `OpaqueValue` and `OpaqueValueView` act as
// smart pointers to `OpaqueValueInterface`.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_OPAQUE_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_OPAQUE_VALUE_H_

#include <cstddef>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/any.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/value_interface.h"
#include "common/value_kind.h"
#include "common/values/values.h"

namespace cel {

class Value;
class OpaqueValueInterface;
class OpaqueValueInterfaceIterator;
class OpaqueValue;
class TypeFactory;
class ValueManager;

// `Is` checks whether `lhs` and `rhs` have the same identity.
bool Is(const OpaqueValue& lhs, const OpaqueValue& rhs);

class OpaqueValueInterface : public ValueInterface {
 public:
  using alternative_type = OpaqueValue;

  static constexpr ValueKind kKind = ValueKind::kOpaque;

  ValueKind kind() const final { return kKind; }

  OpaqueType GetType(TypeManager& type_manager) const {
    return Cast<OpaqueType>(GetTypeImpl(type_manager));
  }

  virtual absl::Status Equal(ValueManager& value_manager, const Value& other,
                             Value& result) const = 0;
};

template <>
struct SubsumptionTraits<OpaqueValueInterface> {
  static bool IsA(const ValueInterface& interface) {
    return interface.kind() == ValueKind::kOpaque;
  }
};

class OpaqueValue {
 public:
  using interface_type = OpaqueValueInterface;

  static constexpr ValueKind kKind = OpaqueValueInterface::kKind;

  template <typename T, typename = std::enable_if_t<std::is_base_of_v<
                            OpaqueValueInterface, std::remove_const_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  OpaqueValue(Shared<T> interface) : interface_(std::move(interface)) {}

  OpaqueValue() = default;
  OpaqueValue(const OpaqueValue&) = default;
  OpaqueValue(OpaqueValue&&) = default;
  OpaqueValue& operator=(const OpaqueValue&) = default;
  OpaqueValue& operator=(OpaqueValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  OpaqueType GetType(TypeManager& type_manager) const {
    return interface_->GetType(type_manager);
  }

  absl::string_view GetTypeName() const { return interface_->GetTypeName(); }

  std::string DebugString() const { return interface_->DebugString(); }

  // See `ValueInterface::SerializeTo`.
  absl::Status SerializeTo(AnyToJsonConverter& converter,
                           absl::Cord& value) const {
    return interface_->SerializeTo(converter, value);
  }

  // See `ValueInterface::Serialize`.
  absl::StatusOr<absl::Cord> Serialize(AnyToJsonConverter& converter) const {
    return interface_->Serialize(converter);
  }

  // See `ValueInterface::GetTypeUrl`.
  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const {
    return interface_->GetTypeUrl(prefix);
  }

  // See `ValueInterface::ConvertToAny`.
  absl::StatusOr<Any> ConvertToAny(
      AnyToJsonConverter& converter,
      absl::string_view prefix = kTypeGoogleApisComPrefix) const {
    return interface_->ConvertToAny(converter, prefix);
  }

  absl::StatusOr<Json> ConvertToJson(AnyToJsonConverter& converter) const {
    return interface_->ConvertToJson(converter);
  }

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     Value& result) const;
  absl::StatusOr<Value> Equal(ValueManager& value_manager,
                              const Value& other) const;

  bool IsZeroValue() const { return false; }

  void swap(OpaqueValue& other) noexcept {
    using std::swap;
    swap(interface_, other.interface_);
  }

  const interface_type& operator*() const { return *interface_; }

  absl::Nonnull<const interface_type*> operator->() const {
    return interface_.operator->();
  }

 private:
  friend struct NativeTypeTraits<OpaqueValue>;
  friend bool Is(const OpaqueValue& lhs, const OpaqueValue& rhs);

  Shared<const OpaqueValueInterface> interface_;
};

inline void swap(OpaqueValue& lhs, OpaqueValue& rhs) noexcept { lhs.swap(rhs); }

inline std::ostream& operator<<(std::ostream& out, const OpaqueValue& type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<OpaqueValue> final {
  static NativeTypeId Id(const OpaqueValue& type) {
    return NativeTypeId::Of(*type.interface_);
  }

  static bool SkipDestructor(const OpaqueValue& type) {
    return NativeType::SkipDestructor(*type.interface_);
  }
};

template <typename T>
struct NativeTypeTraits<T, std::enable_if_t<std::conjunction_v<
                               std::negation<std::is_same<OpaqueValue, T>>,
                               std::is_base_of<OpaqueValue, T>>>>
    final {
  static NativeTypeId Id(const T& type) {
    return NativeTypeTraits<OpaqueValue>::Id(type);
  }

  static bool SkipDestructor(const T& type) {
    return NativeTypeTraits<OpaqueValue>::SkipDestructor(type);
  }
};

// OpaqueValue -> OpaqueValueFor<T>
template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::conjunction_v<
        std::bool_constant<sizeof(To) == sizeof(absl::remove_cvref_t<From>)>,
        std::bool_constant<alignof(To) == alignof(absl::remove_cvref_t<From>)>,
        std::is_same<OpaqueValue, absl::remove_cvref_t<From>>,
        std::negation<std::is_same<OpaqueValue, To>>,
        std::is_base_of<OpaqueValue, To>>>>
    final {
  static bool Compatible(const absl::remove_cvref_t<From>& from) {
    return SubsumptionTraits<To>::IsA(from);
  }

  static decltype(auto) Convert(From from) {
    // `To` is derived from `From`, `From` is `OpaqueValue`, and `To` has the
    // same size and alignment as `OpaqueValue`. We can just reinterpret_cast.
    return SubsumptionTraits<To>::DownCast(std::move(from));
  }
};

inline bool Is(const OpaqueValue& lhs, const OpaqueValue& rhs) {
  return lhs.operator->() == rhs.operator->();
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_OPAQUE_VALUE_H_
