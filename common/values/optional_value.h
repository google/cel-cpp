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

// `OptionalValue` represents values of the `optional_type` type.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_OPTIONAL_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_OPTIONAL_VALUE_H_

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/value_interface.h"
#include "common/value_kind.h"
#include "common/values/opaque_value.h"

namespace cel {

class Value;
class ValueManager;
class OptionalValueInterface;
class OptionalValue;

class OptionalValueInterface : public OpaqueValueInterface {
 public:
  using alternative_type = OptionalValue;

  OpaqueType GetRuntimeType() const final { return OptionalType(); }

  absl::string_view GetTypeName() const final { return "optional_type"; }

  std::string DebugString() const final;

  virtual bool HasValue() const = 0;

  absl::Status Equal(ValueManager& value_manager, const Value& other,
                     cel::Value& result) const override;

  virtual void Value(cel::Value& scratch) const = 0;

  cel::Value Value() const;

 private:
  NativeTypeId GetNativeTypeId() const noexcept final {
    return NativeTypeId::For<OptionalValueInterface>();
  }
};

template <>
struct SubsumptionTraits<OptionalValueInterface> {
  static bool IsA(const ValueInterface& interface) {
    return interface.kind() == ValueKind::kOpaque &&
           NativeTypeId::Of(interface) ==
               NativeTypeId::For<OptionalValueInterface>();
  }
};

class OptionalValue final : public OpaqueValue {
 public:
  using interface_type = OptionalValueInterface;

  static OptionalValue None();

  static OptionalValue Of(MemoryManagerRef memory_manager, cel::Value value);

  OptionalValue() : OptionalValue(None()) {}

  OptionalValue(const OptionalValue&) = default;
  OptionalValue(OptionalValue&&) = default;
  OptionalValue& operator=(const OptionalValue&) = default;
  OptionalValue& operator=(OptionalValue&&) = default;

  template <typename T, typename = std::enable_if_t<std::is_base_of_v<
                            OptionalValueInterface, std::remove_const_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  OptionalValue(Shared<T> interface) : OpaqueValue(std::move(interface)) {}

  OptionalType GetRuntimeType() const {
    return static_cast<OptionalType>((*this)->GetRuntimeType());
  }

  bool HasValue() const { return (*this)->HasValue(); }

  void Value(cel::Value& result) const;

  cel::Value Value() const;

  const interface_type& operator*() const {
    return Cast<OptionalValueInterface>(OpaqueValue::operator*());
  }

  absl::Nonnull<const interface_type*> operator->() const {
    return Cast<OptionalValueInterface>(OpaqueValue::operator->());
  }

 private:
  friend struct SubsumptionTraits<OptionalValue>;

  // Used by SubsumptionTraits to downcast OpaqueType rvalue references.
  explicit OptionalValue(OpaqueValue&& value) noexcept
      : OpaqueValue(std::move(value)) {}
};

template <>
struct SubsumptionTraits<OptionalValue> final {
  static bool IsA(const OpaqueValue& value) {
    return NativeTypeId::Of(value) ==
           NativeTypeId::For<OptionalValueInterface>();
  }

  static const OptionalValue& DownCast(const OpaqueValue& value) {
    ABSL_DCHECK(IsA(value));
    return *reinterpret_cast<const OptionalValue*>(std::addressof(value));
  }

  static OptionalValue& DownCast(OpaqueValue& value) {
    ABSL_DCHECK(IsA(value));
    return *reinterpret_cast<OptionalValue*>(std::addressof(value));
  }

  static OptionalValue DownCast(OpaqueValue&& value) {
    ABSL_DCHECK(IsA(value));
    return OptionalValue(std::move(value));
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_OPTIONAL_VALUE_H_
