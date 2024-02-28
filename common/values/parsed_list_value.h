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

// `ParsedListValue` represents values of the primitive `list` type.
// `ParsedListValueView` is a non-owning view of `ParsedListValue`.
// `ParsedListValueInterface` is the abstract base class of implementations.
// `ParsedListValue` and `ParsedListValueView` act as smart pointers to
// `ParsedListValueInterface`.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_LIST_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_LIST_VALUE_H_

#include <cstddef>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
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
#include "common/values/list_value_interface.h"
#include "common/values/values.h"

namespace cel {

class Value;
class ValueView;
class ParsedListValueInterface;
class ParsedListValueInterfaceIterator;
class ParsedListValue;
class ParsedListValueView;
class ValueManager;

// `Is` checks whether `lhs` and `rhs` have the same identity.
bool Is(ParsedListValueView lhs, ParsedListValueView rhs);

class ParsedListValueInterface : public ListValueInterface {
 public:
  using alternative_type = ParsedListValue;
  using view_alternative_type = ParsedListValueView;

  absl::StatusOr<size_t> GetSerializedSize(
      ValueManager& value_manager) const override;

  absl::Status SerializeTo(ValueManager& value_manager,
                           absl::Cord& value) const override;

  absl::StatusOr<ValueView> Equal(ValueManager& value_manager, ValueView other,
                                  Value& scratch
                                      ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  bool IsZeroValue() const { return IsEmpty(); }

  virtual bool IsEmpty() const { return Size() == 0; }

  virtual size_t Size() const = 0;

  // Returns a view of the element at index `index`. If the underlying
  // implementation cannot directly return a view of a value, the value will be
  // stored in `scratch`, and the returned view will be that of `scratch`.
  absl::StatusOr<ValueView> Get(ValueManager& value_manager, size_t index,
                                Value& scratch
                                    ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  virtual absl::Status ForEach(ValueManager& value_manager,
                               ForEachCallback callback) const;

  virtual absl::Status ForEach(ValueManager& value_manager,
                               ForEachWithIndexCallback callback) const;

  virtual absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueManager& value_manager) const;

  virtual absl::StatusOr<ValueView> Contains(
      ValueManager& value_manager, ValueView other,
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

 private:
  friend class ParsedListValueInterfaceIterator;

  virtual absl::StatusOr<ValueView> GetImpl(ValueManager& value_manager,
                                            size_t index,
                                            Value& scratch) const = 0;
};

class ParsedListValue {
 public:
  using interface_type = ParsedListValueInterface;
  using view_alternative_type = ParsedListValueView;

  static constexpr ValueKind kKind = ParsedListValueInterface::kKind;

  explicit ParsedListValue(ParsedListValueView value);

  // NOLINTNEXTLINE(google-explicit-constructor)
  ParsedListValue(Shared<const ParsedListValueInterface> interface)
      : interface_(std::move(interface)) {}

  // By default, this creates an empty list whose type is `list(dyn)`. Unless
  // you can help it, you should use a more specific typed list value.
  ParsedListValue();
  ParsedListValue(const ParsedListValue&) = default;
  ParsedListValue(ParsedListValue&&) = default;
  ParsedListValue& operator=(const ParsedListValue&) = default;
  ParsedListValue& operator=(ParsedListValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  ListType GetType(TypeManager& type_manager) const {
    return interface_->GetType(type_manager);
  }

  absl::string_view GetTypeName() const { return interface_->GetTypeName(); }

  std::string DebugString() const { return interface_->DebugString(); }

  // See `ValueInterface::GetSerializedSize`.
  absl::StatusOr<size_t> GetSerializedSize(ValueManager& value_manager) const {
    return interface_->GetSerializedSize(value_manager);
  }

  // See `ValueInterface::SerializeTo`.
  absl::Status SerializeTo(ValueManager& value_manager,
                           absl::Cord& value) const {
    return interface_->SerializeTo(value_manager, value);
  }

  // See `ValueInterface::Serialize`.
  absl::StatusOr<absl::Cord> Serialize(ValueManager& value_manager) const {
    return interface_->Serialize(value_manager);
  }

  // See `ValueInterface::GetTypeUrl`.
  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const {
    return interface_->GetTypeUrl(prefix);
  }

  // See `ValueInterface::ConvertToAny`.
  absl::StatusOr<Any> ConvertToAny(
      ValueManager& value_manager,
      absl::string_view prefix = kTypeGoogleApisComPrefix) const {
    return interface_->ConvertToAny(value_manager, prefix);
  }

  absl::StatusOr<Json> ConvertToJson(ValueManager& value_manager) const {
    return interface_->ConvertToJson(value_manager);
  }

  absl::StatusOr<JsonArray> ConvertToJsonArray(
      ValueManager& value_manager) const {
    return interface_->ConvertToJsonArray(value_manager);
  }

  absl::StatusOr<ValueView> Equal(ValueManager& value_manager, ValueView other,
                                  Value& scratch
                                      ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  bool IsZeroValue() const { return interface_->IsZeroValue(); }

  bool IsEmpty() const { return interface_->IsEmpty(); }

  size_t Size() const { return interface_->Size(); }

  // See ListValueInterface::Get for documentation.
  absl::StatusOr<ValueView> Get(ValueManager& value_manager, size_t index,
                                Value& scratch
                                    ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  using ForEachCallback = typename ListValueInterface::ForEachCallback;

  using ForEachWithIndexCallback =
      typename ListValueInterface::ForEachWithIndexCallback;

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachCallback callback) const;

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachWithIndexCallback callback) const;

  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueManager& value_manager) const;

  absl::StatusOr<ValueView> Contains(
      ValueManager& value_manager, ValueView other,
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  void swap(ParsedListValue& other) noexcept {
    using std::swap;
    swap(interface_, other.interface_);
  }

  const interface_type& operator*() const { return *interface_; }

  absl::Nonnull<const interface_type*> operator->() const {
    return interface_.operator->();
  }

 private:
  friend class ParsedListValueView;
  friend struct NativeTypeTraits<ParsedListValue>;

  Shared<const ParsedListValueInterface> interface_;
};

inline void swap(ParsedListValue& lhs, ParsedListValue& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out,
                                const ParsedListValue& type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<ParsedListValue> final {
  static NativeTypeId Id(const ParsedListValue& type) {
    return NativeTypeId::Of(*type.interface_);
  }

  static bool SkipDestructor(const ParsedListValue& type) {
    return NativeType::SkipDestructor(type.interface_);
  }
};

template <typename T>
struct NativeTypeTraits<T, std::enable_if_t<std::conjunction_v<
                               std::negation<std::is_same<ParsedListValue, T>>,
                               std::is_base_of<ParsedListValue, T>>>>
    final {
  static NativeTypeId Id(const T& type) {
    return NativeTypeTraits<ParsedListValue>::Id(type);
  }

  static bool SkipDestructor(const T& type) {
    return NativeTypeTraits<ParsedListValue>::SkipDestructor(type);
  }
};

// ParsedListValue -> ListValueFor<T>
template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::conjunction_v<
        std::bool_constant<sizeof(To) == sizeof(absl::remove_cvref_t<From>)>,
        std::bool_constant<alignof(To) == alignof(absl::remove_cvref_t<From>)>,
        std::is_same<ParsedListValue, absl::remove_cvref_t<From>>,
        std::negation<std::is_same<ParsedListValue, To>>,
        std::is_base_of<ParsedListValue, To>>>>
    final {
  static bool Compatible(const absl::remove_cvref_t<From>& from) {
    return SubsumptionTraits<To>::IsA(from);
  }

  static decltype(auto) Convert(From from) {
    // `To` is derived from `From`, `From` is `ParsedListValue`, and `To` has
    // the same size and alignment as `ParsedListValue`. We can just
    // reinterpret_cast.
    return SubsumptionTraits<To>::DownCast(std::move(from));
  }
};

class ParsedListValueView {
 public:
  using interface_type = ParsedListValueInterface;
  using alternative_type = ParsedListValue;

  static constexpr ValueKind kKind = ParsedListValue::kKind;

  // NOLINTNEXTLINE(google-explicit-constructor)
  ParsedListValueView(SharedView<const ParsedListValueInterface> interface)
      : interface_(interface) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  ParsedListValueView(
      const ParsedListValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : interface_(value.interface_) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  ParsedListValueView& operator=(
      const ParsedListValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    interface_ = value.interface_;
    return *this;
  }

  ParsedListValueView& operator=(ParsedListValue&&) = delete;

  // By default, this creates an empty list whose type is `list(dyn)`. Unless
  // you can help it, you should use a more specific typed list value.
  ParsedListValueView();
  ParsedListValueView(const ParsedListValueView&) = default;
  ParsedListValueView(ParsedListValueView&&) = default;
  ParsedListValueView& operator=(const ParsedListValueView&) = default;
  ParsedListValueView& operator=(ParsedListValueView&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  ListType GetType(TypeManager& type_manager) const {
    return interface_->GetType(type_manager);
  }

  absl::string_view GetTypeName() const { return interface_->GetTypeName(); }

  std::string DebugString() const { return interface_->DebugString(); }

  // See `ValueInterface::GetSerializedSize`.
  absl::StatusOr<size_t> GetSerializedSize(ValueManager& value_manager) const {
    return interface_->GetSerializedSize(value_manager);
  }

  // See `ValueInterface::SerializeTo`.
  absl::Status SerializeTo(ValueManager& value_manager,
                           absl::Cord& value) const {
    return interface_->SerializeTo(value_manager, value);
  }

  // See `ValueInterface::Serialize`.
  absl::StatusOr<absl::Cord> Serialize(ValueManager& value_manager) const {
    return interface_->Serialize(value_manager);
  }

  // See `ValueInterface::GetTypeUrl`.
  absl::StatusOr<std::string> GetTypeUrl(
      absl::string_view prefix = kTypeGoogleApisComPrefix) const {
    return interface_->GetTypeUrl(prefix);
  }

  // See `ValueInterface::ConvertToAny`.
  absl::StatusOr<Any> ConvertToAny(
      ValueManager& value_manager,
      absl::string_view prefix = kTypeGoogleApisComPrefix) const {
    return interface_->ConvertToAny(value_manager, prefix);
  }

  absl::StatusOr<Json> ConvertToJson(ValueManager& value_manager) const {
    return interface_->ConvertToJson(value_manager);
  }

  absl::StatusOr<JsonArray> ConvertToJsonArray(
      ValueManager& value_manager) const {
    return interface_->ConvertToJsonArray(value_manager);
  }

  absl::StatusOr<ValueView> Equal(ValueManager& value_manager, ValueView other,
                                  Value& scratch
                                      ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  bool IsZeroValue() const { return interface_->IsZeroValue(); }

  bool IsEmpty() const { return interface_->IsEmpty(); }

  size_t Size() const { return interface_->Size(); }

  // See ListValueInterface::Get for documentation.
  absl::StatusOr<ValueView> Get(ValueManager& value_manager, size_t index,
                                Value& scratch
                                    ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  using ForEachCallback = typename ListValueInterface::ForEachCallback;

  using ForEachWithIndexCallback =
      typename ListValueInterface::ForEachWithIndexCallback;

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachCallback callback) const;

  absl::Status ForEach(ValueManager& value_manager,
                       ForEachWithIndexCallback callback) const;

  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueManager& value_manager) const;

  absl::StatusOr<ValueView> Contains(
      ValueManager& value_manager, ValueView other,
      Value& scratch ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  void swap(ParsedListValueView& other) noexcept {
    using std::swap;
    swap(interface_, other.interface_);
  }

  const interface_type& operator*() const { return *interface_; }

  absl::Nonnull<const interface_type*> operator->() const {
    return interface_.operator->();
  }

 private:
  friend class ParsedListValue;
  friend struct NativeTypeTraits<ParsedListValueView>;
  friend bool Is(ParsedListValueView lhs, ParsedListValueView rhs);

  SharedView<const ParsedListValueInterface> interface_;
};

inline void swap(ParsedListValueView& lhs, ParsedListValueView& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out, ParsedListValueView type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<ParsedListValueView> final {
  static NativeTypeId Id(ParsedListValueView type) {
    return NativeTypeId::Of(*type.interface_);
  }
};

template <typename T>
struct NativeTypeTraits<T,
                        std::enable_if_t<std::conjunction_v<
                            std::negation<std::is_same<ParsedListValueView, T>>,
                            std::is_base_of<ParsedListValueView, T>>>>
    final {
  static NativeTypeId Id(T type) {
    return NativeTypeTraits<ParsedListValueView>::Id(type);
  }
};

inline ParsedListValue::ParsedListValue()
    : ParsedListValue(common_internal::GetEmptyDynListValue()) {}

inline ParsedListValueView::ParsedListValueView()
    : ParsedListValueView(common_internal::GetEmptyDynListValue()) {}

inline ParsedListValue::ParsedListValue(ParsedListValueView value)
    : interface_(value.interface_) {}

inline bool Is(ParsedListValueView lhs, ParsedListValueView rhs) {
  return lhs.interface_.operator->() == rhs.interface_.operator->();
}

// ParsedListValueView -> ListValueViewFor<T>
template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::conjunction_v<
        std::bool_constant<sizeof(To) == sizeof(absl::remove_cvref_t<From>)>,
        std::bool_constant<alignof(To) == alignof(absl::remove_cvref_t<From>)>,
        std::is_same<ParsedListValueView, absl::remove_cvref_t<From>>,
        std::negation<std::is_same<ParsedListValueView, To>>,
        std::is_base_of<ParsedListValueView, To>>>>
    final {
  static bool Compatible(const absl::remove_cvref_t<From>& from) {
    return SubsumptionTraits<To>::IsA(from);
  }

  static decltype(auto) Convert(From from) {
    // `To` is derived from `From`, `From` is `OpaqueType`, and `To` has the
    // same size and alignment as `OpaqueType`. We can just reinterpret_cast.
    return SubsumptionTraits<To>::DownCast(std::move(from));
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_PARSED_LIST_VALUE_H_
