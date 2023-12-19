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

// `ListValue` represents values of the primitive `list` type. `ListValueView`
// is a non-owning view of `ListValue`. `ListValueInterface` is the abstract
// base class of implementations. `ListValue` and `ListValueView` act as smart
// pointers to `ListValueInterface`.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_LIST_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_LIST_VALUE_H_

#include <cstddef>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/functional/function_ref.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
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
class ValueView;
class ListValueInterface;
class ListValueInterfaceIterator;
class ListValue;
class ListValueView;
class ListValueBuilder;
class TypeFactory;
class ValueFactory;

// `Is` checks whether `lhs` and `rhs` have the same identity.
bool Is(ListValueView lhs, ListValueView rhs);

class ListValueInterface : public ValueInterface {
 public:
  using alternative_type = ListValue;
  using view_alternative_type = ListValueView;

  static constexpr ValueKind kKind = ValueKind::kList;

  ValueKind kind() const final { return kKind; }

  ListTypeView type() const { return Cast<ListTypeView>(get_type()); }

  absl::StatusOr<Json> ConvertToJson() const final {
    return ConvertToJsonArray();
  }

  virtual absl::StatusOr<JsonArray> ConvertToJsonArray() const = 0;

  virtual bool IsEmpty() const { return Size() == 0; }

  virtual size_t Size() const = 0;

  // Returns a view of the element at index `index`. If the underlying
  // implementation cannot directly return a view of a value, the value will be
  // stored in `scratch`, and the returned view will be that of `scratch`.
  absl::StatusOr<ValueView> Get(ValueFactory& value_factory, size_t index,
                                Value& scratch
                                    ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  using ForEachCallback = absl::FunctionRef<absl::StatusOr<bool>(ValueView)>;

  virtual absl::Status ForEach(ValueFactory& value_factory,
                               ForEachCallback callback) const;

  virtual absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueFactory& value_factory) const;

 private:
  friend class ListValueInterfaceIterator;

  virtual absl::StatusOr<ValueView> GetImpl(ValueFactory& value_factory,
                                            size_t index,
                                            Value& scratch) const = 0;
};

class ListValue {
 public:
  using interface_type = ListValueInterface;
  using view_alternative_type = ListValueView;

  static constexpr ValueKind kKind = ListValueInterface::kKind;

  explicit ListValue(ListValueView value);

  // NOLINTNEXTLINE(google-explicit-constructor)
  ListValue(Shared<const ListValueInterface> interface)
      : interface_(std::move(interface)) {}

  // By default, this creates an empty list whose type is `list(dyn)`. Unless
  // you can help it, you should use a more specific typed list value.
  ListValue();
  ListValue(const ListValue&) = default;
  ListValue(ListValue&&) = default;
  ListValue& operator=(const ListValue&) = default;
  ListValue& operator=(ListValue&&) = default;

  ValueKind kind() const { return interface_->kind(); }

  ListTypeView type() const { return interface_->type(); }

  std::string DebugString() const { return interface_->DebugString(); }

  absl::StatusOr<Json> ConvertToJson() const {
    return interface_->ConvertToJson();
  }

  absl::StatusOr<JsonArray> ConvertToJsonArray() const {
    return interface_->ConvertToJsonArray();
  }

  bool IsEmpty() const { return interface_->IsEmpty(); }

  size_t Size() const { return interface_->Size(); }

  // See ListValueInterface::Get for documentation.
  absl::StatusOr<ValueView> Get(ValueFactory& value_factory, size_t index,
                                Value& scratch
                                    ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  using ForEachCallback = typename ListValueInterface::ForEachCallback;

  absl::Status ForEach(ValueFactory& value_factory,
                       ForEachCallback callback) const;

  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueFactory& value_factory) const;

  void swap(ListValue& other) noexcept {
    using std::swap;
    swap(interface_, other.interface_);
  }

  const interface_type& operator*() const { return *interface_; }

  absl::Nonnull<const interface_type*> operator->() const {
    return interface_.operator->();
  }

 private:
  friend class ListValueView;
  friend struct NativeTypeTraits<ListValue>;

  Shared<const ListValueInterface> interface_;
};

inline void swap(ListValue& lhs, ListValue& rhs) noexcept { lhs.swap(rhs); }

inline std::ostream& operator<<(std::ostream& out, const ListValue& type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<ListValue> final {
  static NativeTypeId Id(const ListValue& type) {
    return NativeTypeId::Of(*type.interface_);
  }

  static bool SkipDestructor(const ListValue& type) {
    return NativeType::SkipDestructor(type.interface_);
  }
};

template <typename T>
struct NativeTypeTraits<T, std::enable_if_t<std::conjunction_v<
                               std::negation<std::is_same<ListValue, T>>,
                               std::is_base_of<ListValue, T>>>>
    final {
  static NativeTypeId Id(const T& type) {
    return NativeTypeTraits<ListValue>::Id(type);
  }

  static bool SkipDestructor(const T& type) {
    return NativeTypeTraits<ListValue>::SkipDestructor(type);
  }
};

// ListValue -> ListValueFor<T>
template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::conjunction_v<
        std::bool_constant<sizeof(To) == sizeof(absl::remove_cvref_t<From>)>,
        std::bool_constant<alignof(To) == alignof(absl::remove_cvref_t<From>)>,
        std::is_same<ListValue, absl::remove_cvref_t<From>>,
        std::negation<std::is_same<ListValue, To>>,
        std::is_base_of<ListValue, To>>>>
    final {
  static bool Compatible(const absl::remove_cvref_t<From>& from) {
    return SubsumptionTraits<To>::IsA(from);
  }

  static decltype(auto) Convert(From from) {
    // `To` is derived from `From`, `From` is `ListValue`, and `To` has the
    // same size and alignment as `ListValue`. We can just reinterpret_cast.
    return SubsumptionTraits<To>::DownCast(std::move(from));
  }
};

class ListValueView {
 public:
  using interface_type = ListValueInterface;
  using alternative_type = ListValue;

  static constexpr ValueKind kKind = ListValue::kKind;

  // NOLINTNEXTLINE(google-explicit-constructor)
  ListValueView(SharedView<const ListValueInterface> interface)
      : interface_(interface) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  ListValueView(const ListValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : interface_(value.interface_) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  ListValueView& operator=(
      const ListValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    interface_ = value.interface_;
    return *this;
  }

  ListValueView& operator=(ListValue&&) = delete;

  // By default, this creates an empty list whose type is `list(dyn)`. Unless
  // you can help it, you should use a more specific typed list value.
  ListValueView();
  ListValueView(const ListValueView&) = default;
  ListValueView(ListValueView&&) = default;
  ListValueView& operator=(const ListValueView&) = default;
  ListValueView& operator=(ListValueView&&) = default;

  ValueKind kind() const { return interface_->kind(); }

  ListTypeView type() const { return interface_->type(); }

  std::string DebugString() const { return interface_->DebugString(); }

  absl::StatusOr<Json> ConvertToJson() const {
    return interface_->ConvertToJson();
  }

  absl::StatusOr<JsonArray> ConvertToJsonArray() const {
    return interface_->ConvertToJsonArray();
  }

  bool IsEmpty() const { return interface_->IsEmpty(); }

  size_t Size() const { return interface_->Size(); }

  // See ListValueInterface::Get for documentation.
  absl::StatusOr<ValueView> Get(ValueFactory& value_factory, size_t index,
                                Value& scratch
                                    ABSL_ATTRIBUTE_LIFETIME_BOUND) const;

  using ForEachCallback = typename ListValueInterface::ForEachCallback;

  absl::Status ForEach(ValueFactory& value_factory,
                       ForEachCallback callback) const;

  absl::StatusOr<absl::Nonnull<ValueIteratorPtr>> NewIterator(
      ValueFactory& value_factory) const;

  void swap(ListValueView& other) noexcept {
    using std::swap;
    swap(interface_, other.interface_);
  }

  const interface_type& operator*() const { return *interface_; }

  absl::Nonnull<const interface_type*> operator->() const {
    return interface_.operator->();
  }

 private:
  friend class ListValue;
  friend struct NativeTypeTraits<ListValueView>;
  friend bool Is(ListValueView lhs, ListValueView rhs);

  SharedView<const ListValueInterface> interface_;
};

inline void swap(ListValueView& lhs, ListValueView& rhs) noexcept {
  lhs.swap(rhs);
}

inline std::ostream& operator<<(std::ostream& out, ListValueView type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<ListValueView> final {
  static NativeTypeId Id(ListValueView type) {
    return NativeTypeId::Of(*type.interface_);
  }
};

template <typename T>
struct NativeTypeTraits<T, std::enable_if_t<std::conjunction_v<
                               std::negation<std::is_same<ListValueView, T>>,
                               std::is_base_of<ListValueView, T>>>>
    final {
  static NativeTypeId Id(T type) {
    return NativeTypeTraits<ListValueView>::Id(type);
  }
};

inline ListValue::ListValue(ListValueView value)
    : interface_(value.interface_) {}

inline bool Is(ListValueView lhs, ListValueView rhs) {
  return lhs.interface_.operator->() == rhs.interface_.operator->();
}

// ListValueView -> ListValueViewFor<T>
template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::conjunction_v<
        std::bool_constant<sizeof(To) == sizeof(absl::remove_cvref_t<From>)>,
        std::bool_constant<alignof(To) == alignof(absl::remove_cvref_t<From>)>,
        std::is_same<ListValueView, absl::remove_cvref_t<From>>,
        std::negation<std::is_same<ListValueView, To>>,
        std::is_base_of<ListValueView, To>>>>
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

class ListValueBuilder {
 public:
  virtual ~ListValueBuilder() = default;

  virtual absl::Status Add(Value value) = 0;

  virtual bool IsEmpty() const { return Size() == 0; }

  virtual size_t Size() const = 0;

  virtual void Reserve(size_t capacity) {}

  virtual ListValue Build() && = 0;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_LIST_VALUE_H_
