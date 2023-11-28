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

// IWYU pragma: private, include "common/type.h"
// IWYU pragma: friend "common/type.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_ENUM_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_ENUM_TYPE_H_

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
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/utility/utility.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/sized_input_view.h"
#include "common/type_interface.h"
#include "common/type_kind.h"
#include "internal/casts.h"

namespace cel {

struct EnumTypeValue;
struct EnumTypeValueView;
class EnumTypeValueId;
class EnumTypeInterface;
template <typename InterfaceType>
struct ExtendEnumTypeInterface;
class EnumType;
template <typename InterfaceType>
class EnumTypeFor;
class EnumTypeView;
template <typename InterfaceType>
class EnumTypeViewFor;
class EnumValue;
class EnumValueView;
class EnumTypeValueIterator;
using EnumTypeValueIteratorPtr = std::unique_ptr<EnumTypeValueIterator>;

struct EnumTypeValue final {
  EnumTypeValue() = default;

  explicit EnumTypeValue(EnumTypeValueView value);

  EnumTypeValue(std::string name, int64_t number)
      : name(std::move(name)), number(number) {}

  std::string name;
  int64_t number = -1;
};

struct EnumTypeValueView final {
  EnumTypeValueView() = default;

  // NOLINTNEXTLINE(google-explicit-constructor)
  EnumTypeValueView(const EnumTypeValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : EnumTypeValueView(value.name, value.number) {}

  EnumTypeValueView(absl::string_view name, int64_t number)
      : name(name), number(number) {}

  absl::string_view name;
  int64_t number = -1;
};

inline EnumTypeValue::EnumTypeValue(EnumTypeValueView value)
    : EnumTypeValue(std::string(value.name), value.number) {}

// `EnumTypeValueId` is an opaque type used by `EnumTypeInterface`
// implementations to enable efficient lookup of enum values. The pairing of
// `EnumTypeInterface` and `EnumTypeValueId` is used to implement `EnumValue`.
//
// `EnumTypeValueId` from one `EnumTypeInterface` cannot be used with another.
class EnumTypeValueId final {
 public:
  EnumTypeValueId() = delete;

  EnumTypeValueId(const EnumTypeValueId&) = default;
  EnumTypeValueId(EnumTypeValueId&&) = default;
  EnumTypeValueId& operator=(const EnumTypeValueId&) = default;
  EnumTypeValueId& operator=(EnumTypeValueId&&) = default;

  template <typename T, typename... Args>
  explicit EnumTypeValueId(absl::in_place_type_t<T>, Args&&... args) {
    static_assert(std::is_trivially_destructible_v<T>);
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(alignof(T) <= kNativeValueAlign);
    static_assert(sizeof(T) <= kNativeValueSize);
    ::new (static_cast<void*>(&value_[0])) T(std::forward<Args>(args)...);
  }

  template <typename T>
  T Get() const {
    static_assert(std::is_trivially_destructible_v<T>);
    static_assert(std::is_trivially_copyable_v<T>);
    static_assert(alignof(T) <= kNativeValueAlign);
    static_assert(sizeof(T) <= kNativeValueSize);
    return *reinterpret_cast<const T*>(&value_[0]);
  }

 private:
  static constexpr size_t kNativeValueAlign =
      std::max(alignof(uint64_t), alignof(uintptr_t));
  static constexpr size_t kNativeValueSize =
      std::max(sizeof(uint64_t), sizeof(uintptr_t));

  alignas(kNativeValueAlign) char value_[kNativeValueSize];
};

// `EnumTypeInterface` describes the interface for implementing enum types.
class EnumTypeInterface : public TypeInterface,
                          public EnableSharedFromThis<EnumTypeInterface> {
 public:
  using alternative_type = EnumType;
  using view_alternative_type = EnumTypeView;

  static constexpr TypeKind kKind = TypeKind::kEnum;

  TypeKind kind() const final { return kKind; }

  std::string DebugString() const override { return std::string(name()); }

  virtual size_t value_count() const = 0;

  absl::StatusOr<absl::optional<EnumValue>> FindValueByName(
      absl::string_view name) const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  absl::StatusOr<absl::optional<EnumValue>> FindValueByNumber(
      int64_t number) const ABSL_ATTRIBUTE_LIFETIME_BOUND;

  virtual absl::StatusOr<absl::Nonnull<EnumTypeValueIteratorPtr>>
  NewValueIterator() const ABSL_ATTRIBUTE_LIFETIME_BOUND = 0;

 private:
  friend class EnumType;
  friend class EnumTypeView;

  virtual absl::StatusOr<absl::optional<EnumTypeValueId>> FindIdByName(
      absl::string_view name) const ABSL_ATTRIBUTE_LIFETIME_BOUND = 0;

  virtual absl::StatusOr<absl::optional<EnumTypeValueId>> FindIdByNumber(
      int64_t number) const ABSL_ATTRIBUTE_LIFETIME_BOUND = 0;

  ABSL_ATTRIBUTE_PURE_FUNCTION
  virtual absl::string_view GetNameForId(EnumTypeValueId id) const = 0;

  ABSL_ATTRIBUTE_PURE_FUNCTION
  virtual int64_t GetNumberForId(EnumTypeValueId id) const = 0;
};

template <typename InterfaceType>
struct ExtendEnumTypeInterface : public EnumTypeInterface {
 public:
  using alternative_type = EnumTypeFor<InterfaceType>;
  using view_alternative_type = EnumTypeViewFor<InterfaceType>;

 private:
  NativeTypeId GetNativeTypeId() const noexcept final {
    return NativeTypeId::For<InterfaceType>();
  }
};

// `EnumType` represents an enumeration type.
class EnumType {
 public:
  using interface_type = EnumTypeInterface;
  using view_alternative_type = EnumTypeView;

  static constexpr TypeKind kKind = EnumTypeInterface::kKind;

  static absl::StatusOr<EnumType> Create(
      MemoryManagerRef memory_manager, absl::string_view name,
      SizedInputView<EnumTypeValueView> values);

  explicit EnumType(EnumTypeView type);

  // NOLINTNEXTLINE(google-explicit-constructor)
  EnumType(Shared<const EnumTypeInterface> interface)
      : interface_(std::move(interface)) {}

  EnumType(const EnumType&) = default;
  EnumType(EnumType&&) = default;
  EnumType& operator=(const EnumType&) = default;
  EnumType& operator=(EnumType&&) = default;

  TypeKind kind() const { return interface_->kind(); }

  absl::string_view name() const { return interface_->name(); }

  std::string DebugString() const { return interface_->DebugString(); }

  size_t value_count() const;

  absl::StatusOr<absl::optional<EnumValue>> FindValueByName(
      absl::string_view name) const;

  absl::StatusOr<absl::optional<EnumValue>> FindValueByNumber(
      int64_t number) const;

  absl::StatusOr<absl::Nonnull<EnumTypeValueIteratorPtr>> NewValueIterator()
      const;

  const interface_type& operator*() const { return *interface_; }

  absl::Nonnull<const interface_type*> operator->() const {
    return interface_.operator->();
  }

  void swap(EnumType& other) noexcept {
    using std::swap;
    swap(interface_, other.interface_);
  }

  friend bool operator==(const EnumType& lhs, const EnumType& rhs) {
    return lhs.name() == rhs.name();
  }

  template <typename H>
  friend H AbslHashValue(H state, const EnumType& type) {
    return H::combine(std::move(state), type.kind(), type.name());
  }

 private:
  friend class EnumTypeView;
  friend struct NativeTypeTraits<EnumType>;
  friend class EnumValue;

  absl::string_view GetNameForId(EnumTypeValueId id) const {
    return interface_->GetNameForId(id);
  }

  int64_t GetNumberForId(EnumTypeValueId id) const {
    return interface_->GetNumberForId(id);
  }

  Shared<const EnumTypeInterface> interface_;
};

inline void swap(EnumType& lhs, EnumType& rhs) noexcept { lhs.swap(rhs); }

inline bool operator!=(const EnumType& lhs, const EnumType& rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, const EnumType& type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<EnumType> final {
  static NativeTypeId Id(const EnumType& type) {
    return NativeTypeId::Of(*type.interface_);
  }

  static bool SkipDestructor(const EnumType& type) {
    return NativeType::SkipDestructor(type.interface_);
  }
};

template <typename T>
struct NativeTypeTraits<T, std::enable_if_t<std::conjunction_v<
                               std::negation<std::is_same<EnumType, T>>,
                               std::is_base_of<EnumType, T>>>>
    final {
  static NativeTypeId Id(const T& type) {
    return NativeTypeTraits<EnumType>::Id(type);
  }

  static bool SkipDestructor(const T& type) {
    return NativeTypeTraits<EnumType>::SkipDestructor(type);
  }
};

// OpaqueType -> OpaqueTypeFor<T>
template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::conjunction_v<
        std::bool_constant<sizeof(To) == sizeof(absl::remove_cvref_t<From>)>,
        std::bool_constant<alignof(To) == alignof(absl::remove_cvref_t<From>)>,
        std::is_same<EnumType, absl::remove_cvref_t<From>>,
        std::negation<std::is_same<EnumType, To>>,
        std::is_base_of<EnumType, To>>>>
    final {
  static bool Compatible(const absl::remove_cvref_t<From>& from) {
    return SubsumptionTraits<To>::IsA(from);
  }

  static decltype(auto) Convert(From from) {
    // `To` is derived from `From`, `From` is `EnumType`, and `To` has the
    // same size and alignment as `EnumType`. We can just reinterpret_cast.
    return SubsumptionTraits<To>::DownCast(std::move(from));
  }
};

template <typename InterfaceType>
class EnumTypeFor final : public EnumType {
 public:
  using interface_type = InterfaceType;
  using view_alternative_type = EnumTypeViewFor<interface_type>;

  explicit EnumTypeFor(view_alternative_type type) : EnumTypeFor(type) {}

  template <typename T, typename = std::enable_if_t<std::is_base_of_v<
                            interface_type, std::remove_const_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  EnumTypeFor(Shared<T> interface) : EnumType(std::move(interface)) {}

  EnumTypeFor(const EnumTypeFor&) = default;
  EnumTypeFor(EnumTypeFor&&) = default;
  EnumTypeFor& operator=(const EnumTypeFor&) = default;
  EnumTypeFor& operator=(EnumTypeFor&&) = default;

  const interface_type& operator*() const {
    return cel::internal::down_cast<const interface_type&>(
        EnumTypeFor::operator*());
  }

  absl::Nonnull<const interface_type*> operator->() const {
    return cel::internal::down_cast<const interface_type*>(
        EnumTypeFor::operator->());
  }
};

template <typename InterfaceType>
struct SubsumptionTraits<EnumTypeFor<InterfaceType>> final {
  static bool IsA(const EnumType& type) {
    return NativeTypeId::Of(type) == NativeTypeId::For<InterfaceType>();
  }

  static const EnumTypeFor<InterfaceType>& DownCast(const EnumType& type) {
    static_assert(sizeof(EnumType) == sizeof(EnumTypeFor<InterfaceType>));
    static_assert(alignof(EnumType) == alignof(EnumTypeFor<InterfaceType>));
    CheckDownCast(type);
    return static_cast<const EnumTypeFor<InterfaceType>&>(type);
  }

  static EnumTypeFor<InterfaceType>& DownCast(EnumType& type) {
    static_assert(sizeof(EnumType) == sizeof(EnumTypeFor<InterfaceType>));
    static_assert(alignof(EnumType) == alignof(EnumTypeFor<InterfaceType>));
    CheckDownCast(type);
    return static_cast<EnumTypeFor<InterfaceType>&>(type);
  }

  static EnumTypeFor<InterfaceType> DownCast(const EnumType&& type) {
    static_assert(sizeof(EnumType) == sizeof(EnumTypeFor<InterfaceType>));
    static_assert(alignof(EnumType) == alignof(EnumTypeFor<InterfaceType>));
    CheckDownCast(type);
    return EnumTypeFor<InterfaceType>(
        std::move(static_cast<const EnumTypeFor<InterfaceType>&>(type)));
  }

  static EnumTypeFor<InterfaceType> DownCast(EnumType&& type) {
    static_assert(sizeof(EnumType) == sizeof(EnumTypeFor<InterfaceType>));
    static_assert(alignof(EnumType) == alignof(EnumTypeFor<InterfaceType>));
    CheckDownCast(type);
    return EnumTypeFor<InterfaceType>(
        std::move(static_cast<EnumTypeFor<InterfaceType>&>(type)));
  }

 private:
  static void CheckDownCast(const EnumType& type) {
#ifndef NDEBUG
#ifdef CEL_INTERNAL_HAVE_RTTI
    ABSL_DCHECK(cel::internal::down_cast<const InterfaceType*>(
                    type.operator->()) != nullptr);
#endif
#endif
  }
};

class EnumTypeView {
 public:
  using interface_type = EnumTypeInterface;
  using alternative_type = EnumType;

  static constexpr TypeKind kKind = EnumType::kKind;

  // NOLINTNEXTLINE(google-explicit-constructor)
  EnumTypeView(const EnumType& type ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : interface_(type.interface_) {}

  EnumTypeView() = delete;
  EnumTypeView(const EnumTypeView&) = default;
  EnumTypeView(EnumTypeView&&) = default;
  EnumTypeView& operator=(const EnumTypeView&) = default;
  EnumTypeView& operator=(EnumTypeView&&) = default;

  TypeKind kind() const { return interface_->kind(); }

  absl::string_view name() const { return interface_->name(); }

  std::string DebugString() const { return interface_->DebugString(); }

  size_t value_count() const;

  absl::StatusOr<absl::optional<EnumValue>> FindValueByName(
      absl::string_view name) const;

  absl::StatusOr<absl::optional<EnumValue>> FindValueByNumber(
      int64_t number) const;

  absl::StatusOr<absl::Nonnull<EnumTypeValueIteratorPtr>> NewValueIterator()
      const;

  const interface_type& operator*() const { return *interface_; }

  absl::Nonnull<const interface_type*> operator->() const {
    return interface_.operator->();
  }

  void swap(EnumTypeView& other) noexcept {
    using std::swap;
    swap(interface_, other.interface_);
  }

  friend bool operator==(EnumTypeView lhs, EnumTypeView rhs) {
    return lhs.name() == rhs.name();
  }

  template <typename H>
  friend H AbslHashValue(H state, EnumTypeView type) {
    return H::combine(std::move(state), type.kind(), type.name());
  }

 private:
  friend class EnumType;
  friend struct NativeTypeTraits<EnumTypeView>;
  friend class EnumValueView;

  absl::string_view GetNameForId(EnumTypeValueId id) const {
    return interface_->GetNameForId(id);
  }

  int64_t GetNumberForId(EnumTypeValueId id) const {
    return interface_->GetNumberForId(id);
  }

  SharedView<const EnumTypeInterface> interface_;
};

inline void swap(EnumTypeView& lhs, EnumTypeView& rhs) noexcept {
  lhs.swap(rhs);
}

inline bool operator!=(EnumTypeView lhs, EnumTypeView rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, EnumTypeView type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<EnumTypeView> final {
  static NativeTypeId Id(EnumTypeView type) {
    return NativeTypeId::Of(*type.interface_);
  }
};

template <typename T>
struct NativeTypeTraits<T, std::enable_if_t<std::conjunction_v<
                               std::negation<std::is_same<EnumTypeView, T>>,
                               std::is_base_of<EnumTypeView, T>>>>
    final {
  static NativeTypeId Id(T type) {
    return NativeTypeTraits<EnumTypeView>::Id(type);
  }
};

inline EnumType::EnumType(EnumTypeView type) : interface_(type.interface_) {}

// OpaqueTypeView -> OpaqueTypeViewFor<T>
template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::conjunction_v<
        std::bool_constant<sizeof(To) == sizeof(absl::remove_cvref_t<From>)>,
        std::bool_constant<alignof(To) == alignof(absl::remove_cvref_t<From>)>,
        std::is_same<EnumTypeView, absl::remove_cvref_t<From>>,
        std::negation<std::is_same<EnumTypeView, To>>,
        std::is_base_of<EnumTypeView, To>>>>
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

template <typename InterfaceType>
class EnumTypeViewFor final : public EnumTypeView {
 public:
  using interface_type = InterfaceType;
  using alternative_type = EnumTypeFor<interface_type>;

  // NOLINTNEXTLINE(google-explicit-constructor)
  EnumTypeViewFor(
      const alternative_type& type ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : EnumTypeView(type) {}

  template <typename T, typename = std::enable_if_t<std::is_base_of_v<
                            interface_type, std::remove_const_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  EnumTypeViewFor(SharedView<T> interface) : EnumTypeView(interface) {}

  EnumTypeViewFor() = delete;
  EnumTypeViewFor(const EnumTypeViewFor&) = default;
  EnumTypeViewFor(EnumTypeViewFor&&) = default;
  EnumTypeViewFor& operator=(const EnumTypeViewFor&) = default;
  EnumTypeViewFor& operator=(EnumTypeViewFor&&) = default;

  const interface_type& operator*() const {
    return cel::internal::down_cast<const interface_type&>(
        EnumTypeView::operator*());
  }

  absl::Nonnull<const interface_type*> operator->() const {
    return cel::internal::down_cast<const interface_type*>(
        EnumTypeView::operator->());
  }
};

template <typename InterfaceType>
struct SubsumptionTraits<EnumTypeViewFor<InterfaceType>> final {
  static bool IsA(EnumTypeView type) {
    return NativeTypeId::Of(type) == NativeTypeId::For<InterfaceType>();
  }

  static EnumTypeViewFor<InterfaceType> DownCast(EnumTypeView type) {
    static_assert(sizeof(EnumTypeView) ==
                  sizeof(EnumTypeViewFor<InterfaceType>));
    static_assert(alignof(EnumTypeView) ==
                  alignof(EnumTypeViewFor<InterfaceType>));
    CheckDownCast(type);
    return EnumTypeViewFor<InterfaceType>(
        static_cast<const EnumTypeViewFor<InterfaceType>&>(type));
  }

 private:
  static void CheckDownCast(EnumTypeView type) {
#ifndef NDEBUG
#ifdef CEL_INTERNAL_HAVE_RTTI
    ABSL_DCHECK(cel::internal::down_cast<const InterfaceType*>(
                    type.operator->()) != nullptr);
#endif
#endif
  }
};

class EnumTypeValueIterator {
 public:
  virtual ~EnumTypeValueIterator() = default;

  virtual bool HasNext() = 0;

  absl::StatusOr<EnumValue> Next();

 private:
  virtual absl::StatusOr<EnumTypeValueId> NextId() = 0;

  virtual EnumType GetType() const = 0;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_ENUM_TYPE_H_
