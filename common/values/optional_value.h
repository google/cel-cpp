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
// `OptionalValueView` is a non-owning view of `OptionalValue`.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_OPTIONAL_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_OPTIONAL_VALUE_H_

#include <memory>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/value_interface.h"
#include "common/value_kind.h"
#include "common/values/opaque_value.h"

namespace cel {

class ValueView;
class Value;
class ValueManager;
class OptionalValueInterface;
class OptionalValue;
class OptionalValueView;

class OptionalValueInterface : public OpaqueValueInterface {
 public:
  using alternative_type = OptionalValue;
  using view_alternative_type = OptionalValueView;

  OptionalType GetType(TypeManager& type_manager) const {
    return Cast<OptionalType>(GetTypeImpl(type_manager));
  }

  absl::string_view GetTypeName() const final { return "optional_type"; }

  std::string DebugString() const final;

  virtual bool HasValue() const = 0;

  absl::StatusOr<ValueView> Equal(
      ValueManager& value_manager, ValueView other,
      cel::Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const override;

  virtual ValueView Value(
      cel::Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const = 0;

 private:
  Type GetTypeImpl(TypeManager&) const override { return OptionalType(); }

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
  using view_alternative_type = OptionalValueView;

  ABSL_ATTRIBUTE_PURE_FUNCTION static OptionalValue None();

  static OptionalValue Of(MemoryManagerRef memory_manager, cel::Value value);

  explicit OptionalValue(OptionalValueView value);

  OptionalValue() : OptionalValue(None()) {}

  OptionalValue(const OptionalValue&) = default;
  OptionalValue(OptionalValue&&) = default;
  OptionalValue& operator=(const OptionalValue&) = default;
  OptionalValue& operator=(OptionalValue&&) = default;

  // NOLINTNEXTLINE(google-explicit-constructor)
  OptionalValue(Shared<const OptionalValueInterface> interface)
      : OpaqueValue(std::move(interface)) {}

  OptionalType GetType(TypeManager& type_manager) const {
    return (*this)->GetType(type_manager);
  }

  bool HasValue() const { return (*this)->HasValue(); }

  ValueView Value(cel::Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

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
    return NativeTypeId::Of(value) == NativeTypeId::For<OpaqueValueInterface>();
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

class OptionalValueView final : public OpaqueValueView {
 public:
  using interface_type = OptionalValueInterface;
  using alternative_type = OptionalValue;

  ABSL_ATTRIBUTE_PURE_FUNCTION static OptionalValueView None();

  OptionalValueView() : OptionalValueView(None()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  OptionalValueView(SharedView<const OptionalValueInterface> interface)
      : OpaqueValueView(interface) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  OptionalValueView(
      const OptionalValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : OpaqueValueView(value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  OptionalValueView& operator=(
      const OptionalValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    OpaqueValueView::operator=(value);
    return *this;
  }

  OptionalValueView& operator=(OptionalValue&&) = delete;

  OptionalValueView(const OptionalValueView&) = default;
  OptionalValueView& operator=(const OptionalValueView&) = default;

  OptionalType GetType(TypeManager& type_manager) const {
    return (*this)->GetType(type_manager);
  }

  bool HasValue() const { return (*this)->HasValue(); }

  ValueView Value(cel::Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  const interface_type& operator*() const {
    return Cast<OptionalValueInterface>(OpaqueValueView::operator*());
  }

  absl::Nonnull<const interface_type*> operator->() const {
    return Cast<OptionalValueInterface>(OpaqueValueView::operator->());
  }

 private:
  friend struct SubsumptionTraits<OptionalValueView>;

  // Used by SubsumptionTraits to downcast OpaqueTypeView.
  explicit OptionalValueView(OpaqueValueView value) noexcept
      : OpaqueValueView(value) {}
};

inline OptionalValue::OptionalValue(OptionalValueView value)
    : OpaqueValue(value) {}

template <>
struct SubsumptionTraits<OptionalValueView> final {
  static bool IsA(OpaqueValueView value) {
    return NativeTypeId::Of(value) == NativeTypeId::For<OpaqueValueInterface>();
  }

  static OptionalValueView DownCast(OpaqueValueView value) {
    ABSL_DCHECK(IsA(value));
    return OptionalValueView(value);
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_OPTIONAL_VALUE_H_
