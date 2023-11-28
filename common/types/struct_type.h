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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_STRUCT_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_STRUCT_TYPE_H_

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

class Type;
class StructTypeFieldId;
struct StructTypeField;
struct StructTypeFieldView;
class StructTypeFieldIterator;
using StructTypeFieldIteratorPtr = std::unique_ptr<StructTypeFieldIterator>;
class StructTypeInterface;
template <typename InterfaceType>
struct ExtendStructTypeInterface;
class StructType;
template <typename InterfaceType>
class StructTypeFor;
class StructTypeView;
template <typename InterfaceType>
class StructTypeViewFor;

class StructTypeFieldId final {
 public:
  StructTypeFieldId() = delete;

  StructTypeFieldId(const StructTypeFieldId&) = default;
  StructTypeFieldId(StructTypeFieldId&&) = default;
  StructTypeFieldId& operator=(const StructTypeFieldId&) = default;
  StructTypeFieldId& operator=(StructTypeFieldId&&) = default;

  template <typename T, typename... Args>
  explicit StructTypeFieldId(absl::in_place_type_t<T>, Args&&... args)
      : type_(NativeTypeId::For<T>()) {
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
    ABSL_DCHECK_EQ(NativeTypeId::For<T>(), type_);
    return *reinterpret_cast<const T*>(&value_[0]);
  }

 private:
  static constexpr size_t kNativeValueAlign = 8;
  static constexpr size_t kNativeValueSize = sizeof(void*) * 2;

  alignas(kNativeValueAlign) char value_[kNativeValueSize];
  NativeTypeId type_;
};

class StructTypeInterface : public TypeInterface {
 public:
  using alternative_type = StructType;
  using view_alternative_type = StructTypeView;

  static constexpr TypeKind kKind = TypeKind::kStruct;

  TypeKind kind() const final { return kKind; }

  std::string DebugString() const override { return std::string(name()); }

  virtual size_t field_count() const = 0;

  virtual absl::StatusOr<absl::optional<StructTypeFieldId>> FindFieldByName(
      absl::string_view name) const = 0;

  virtual absl::StatusOr<absl::optional<StructTypeFieldId>> FindFieldByNumber(
      int64_t number) const = 0;

  virtual absl::string_view GetFieldName(StructTypeFieldId id) const = 0;

  virtual int64_t GetFieldNumber(StructTypeFieldId id) const = 0;

  virtual absl::StatusOr<Type> GetFieldType(StructTypeFieldId id) const = 0;

  virtual absl::StatusOr<absl::Nonnull<StructTypeFieldIteratorPtr>>
  NewFieldIterator() const ABSL_ATTRIBUTE_LIFETIME_BOUND = 0;

 private:
  friend class StructType;
  friend class StructTypeView;
  friend bool operator==(const StructType& lhs, const StructType& rhs);
  template <typename H>
  friend H AbslHashValue(H state, const StructType& type);
  friend bool operator==(StructTypeView lhs, StructTypeView rhs);
  template <typename H>
  friend H AbslHashValue(H state, StructTypeView type);
};

template <typename InterfaceType>
struct ExtendStructTypeInterface : public StructTypeInterface {
 public:
  using alternative_type = StructTypeFor<InterfaceType>;
  using view_alternative_type = StructTypeViewFor<InterfaceType>;

 private:
  NativeTypeId GetNativeTypeId() const noexcept final {
    return NativeTypeId::For<InterfaceType>();
  }
};

class StructType {
 public:
  using interface_type = StructTypeInterface;
  using view_alternative_type = StructTypeView;

  static constexpr TypeKind kKind = StructTypeInterface::kKind;

  static absl::StatusOr<StructType> Create(
      MemoryManagerRef memory_manager, absl::string_view name,
      SizedInputView<StructTypeFieldView> fields);

  explicit StructType(StructTypeView type);

  // NOLINTNEXTLINE(google-explicit-constructor)
  StructType(Shared<const StructTypeInterface> interface)
      : interface_(std::move(interface)) {}

  StructType(const StructType&) = default;
  StructType(StructType&&) = default;
  StructType& operator=(const StructType&) = default;
  StructType& operator=(StructType&&) = default;

  TypeKind kind() const { return interface_->kind(); }

  absl::string_view name() const { return interface_->name(); }

  std::string DebugString() const { return interface_->DebugString(); }

  size_t field_count() const;

  absl::StatusOr<absl::optional<StructTypeFieldId>> FindFieldByName(
      absl::string_view name) const;

  absl::StatusOr<absl::optional<StructTypeFieldId>> FindFieldByNumber(
      int64_t number) const;

  absl::string_view GetFieldName(StructTypeFieldId id) const;

  int64_t GetFieldNumber(StructTypeFieldId id) const;

  absl::StatusOr<Type> GetFieldType(StructTypeFieldId id) const;

  absl::StatusOr<absl::Nonnull<StructTypeFieldIteratorPtr>> NewFieldIterator()
      const;

  const interface_type& operator*() const { return *interface_; }

  absl::Nonnull<const interface_type*> operator->() const {
    return interface_.operator->();
  }

  void swap(StructType& other) noexcept {
    using std::swap;
    swap(interface_, other.interface_);
  }

  friend bool operator==(const StructType& lhs, const StructType& rhs) {
    return lhs.name() == rhs.name();
  }

  template <typename H>
  friend H AbslHashValue(H state, const StructType& type) {
    return H::combine(std::move(state), type.kind(), type.name());
  }

 private:
  friend class StructTypeView;
  friend struct NativeTypeTraits<StructType>;

  Shared<const StructTypeInterface> interface_;
};

inline void swap(StructType& lhs, StructType& rhs) noexcept { lhs.swap(rhs); }

inline bool operator!=(const StructType& lhs, const StructType& rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, const StructType& type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<StructType> final {
  static NativeTypeId Id(const StructType& type) {
    return NativeTypeId::Of(*type.interface_);
  }

  static bool SkipDestructor(const StructType& type) {
    return NativeType::SkipDestructor(type.interface_);
  }
};

template <typename T>
struct NativeTypeTraits<T, std::enable_if_t<std::conjunction_v<
                               std::negation<std::is_same<StructType, T>>,
                               std::is_base_of<StructType, T>>>>
    final {
  static NativeTypeId Id(const T& type) {
    return NativeTypeTraits<StructType>::Id(type);
  }

  static bool SkipDestructor(const T& type) {
    return NativeTypeTraits<StructType>::SkipDestructor(type);
  }
};

// OpaqueType -> OpaqueTypeFor<T>
template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::conjunction_v<
        std::bool_constant<sizeof(To) == sizeof(absl::remove_cvref_t<From>)>,
        std::bool_constant<alignof(To) == alignof(absl::remove_cvref_t<From>)>,
        std::is_same<StructType, absl::remove_cvref_t<From>>,
        std::negation<std::is_same<StructType, To>>,
        std::is_base_of<StructType, To>>>>
    final {
  static bool Compatible(const absl::remove_cvref_t<From>& from) {
    return SubsumptionTraits<To>::IsA(from);
  }

  static decltype(auto) Convert(From from) {
    // `To` is derived from `From`, `From` is `StructType`, and `To` has the
    // same size and alignment as `StructType`. We can just reinterpret_cast.
    return SubsumptionTraits<To>::DownCast(std::move(from));
  }
};

template <typename InterfaceType>
class StructTypeFor final : public StructType {
 public:
  using interface_type = InterfaceType;
  using view_alternative_type = StructTypeViewFor<interface_type>;

  explicit StructTypeFor(view_alternative_type type) : StructTypeFor(type) {}

  template <typename T, typename = std::enable_if_t<std::is_base_of_v<
                            interface_type, std::remove_const_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  StructTypeFor(Shared<T> interface) : StructType(std::move(interface)) {}

  StructTypeFor(const StructTypeFor&) = default;
  StructTypeFor(StructTypeFor&&) = default;
  StructTypeFor& operator=(const StructTypeFor&) = default;
  StructTypeFor& operator=(StructTypeFor&&) = default;

  const interface_type& operator*() const {
    return cel::internal::down_cast<const interface_type&>(
        StructTypeFor::operator*());
  }

  absl::Nonnull<const interface_type*> operator->() const {
    return cel::internal::down_cast<const interface_type*>(
        StructTypeFor::operator->());
  }
};

template <typename InterfaceType>
struct SubsumptionTraits<StructTypeFor<InterfaceType>> final {
  static bool IsA(const StructType& type) {
    return NativeTypeId::Of(type) == NativeTypeId::For<InterfaceType>();
  }

  static const StructTypeFor<InterfaceType>& DownCast(const StructType& type) {
    static_assert(sizeof(StructType) == sizeof(StructTypeFor<InterfaceType>));
    static_assert(alignof(StructType) == alignof(StructTypeFor<InterfaceType>));
    CheckDownCast(type);
    return static_cast<const StructTypeFor<InterfaceType>&>(type);
  }

  static StructTypeFor<InterfaceType>& DownCast(StructType& type) {
    static_assert(sizeof(StructType) == sizeof(StructTypeFor<InterfaceType>));
    static_assert(alignof(StructType) == alignof(StructTypeFor<InterfaceType>));
    CheckDownCast(type);
    return static_cast<StructTypeFor<InterfaceType>&>(type);
  }

  static StructTypeFor<InterfaceType> DownCast(const StructType&& type) {
    static_assert(sizeof(StructType) == sizeof(StructTypeFor<InterfaceType>));
    static_assert(alignof(StructType) == alignof(StructTypeFor<InterfaceType>));
    CheckDownCast(type);
    return StructTypeFor<InterfaceType>(
        std::move(static_cast<const StructTypeFor<InterfaceType>&>(type)));
  }

  static StructTypeFor<InterfaceType> DownCast(StructType&& type) {
    static_assert(sizeof(StructType) == sizeof(StructTypeFor<InterfaceType>));
    static_assert(alignof(StructType) == alignof(StructTypeFor<InterfaceType>));
    CheckDownCast(type);
    return StructTypeFor<InterfaceType>(
        std::move(static_cast<StructTypeFor<InterfaceType>&>(type)));
  }

 private:
  static void CheckDownCast(const StructType& type) {
#ifndef NDEBUG
#ifdef CEL_INTERNAL_HAVE_RTTI
    ABSL_DCHECK(cel::internal::down_cast<const InterfaceType*>(
                    type.operator->()) != nullptr);
#endif
#endif
  }
};

class StructTypeView {
 public:
  using interface_type = StructTypeInterface;
  using alternative_type = StructType;

  static constexpr TypeKind kKind = StructType::kKind;

  // NOLINTNEXTLINE(google-explicit-constructor)
  StructTypeView(const StructType& type ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : interface_(type.interface_) {}

  StructTypeView() = delete;
  StructTypeView(const StructTypeView&) = default;
  StructTypeView(StructTypeView&&) = default;
  StructTypeView& operator=(const StructTypeView&) = default;
  StructTypeView& operator=(StructTypeView&&) = default;

  TypeKind kind() const { return interface_->kind(); }

  absl::string_view name() const { return interface_->name(); }

  std::string DebugString() const { return interface_->DebugString(); }

  size_t field_count() const;

  absl::StatusOr<absl::optional<StructTypeFieldId>> FindFieldByName(
      absl::string_view name) const;

  absl::StatusOr<absl::optional<StructTypeFieldId>> FindFieldByNumber(
      int64_t number) const;

  absl::string_view GetFieldName(StructTypeFieldId id) const;

  int64_t GetFieldNumber(StructTypeFieldId id) const;

  absl::StatusOr<Type> GetFieldType(StructTypeFieldId id) const;

  absl::StatusOr<absl::Nonnull<StructTypeFieldIteratorPtr>> NewFieldIterator()
      const;

  const interface_type& operator*() const { return *interface_; }

  absl::Nonnull<const interface_type*> operator->() const {
    return interface_.operator->();
  }

  void swap(StructTypeView& other) noexcept {
    using std::swap;
    swap(interface_, other.interface_);
  }

  friend bool operator==(StructTypeView lhs, StructTypeView rhs) {
    return lhs.name() == rhs.name();
  }

  template <typename H>
  friend H AbslHashValue(H state, StructTypeView type) {
    return H::combine(std::move(state), type.kind(), type.name());
  }

 private:
  friend class StructType;
  friend struct NativeTypeTraits<StructTypeView>;

  SharedView<const StructTypeInterface> interface_;
};

inline void swap(StructTypeView& lhs, StructTypeView& rhs) noexcept {
  lhs.swap(rhs);
}

inline bool operator!=(StructTypeView lhs, StructTypeView rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, StructTypeView type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<StructTypeView> final {
  static NativeTypeId Id(StructTypeView type) {
    return NativeTypeId::Of(*type.interface_);
  }
};

template <typename T>
struct NativeTypeTraits<T, std::enable_if_t<std::conjunction_v<
                               std::negation<std::is_same<StructTypeView, T>>,
                               std::is_base_of<StructTypeView, T>>>>
    final {
  static NativeTypeId Id(T type) {
    return NativeTypeTraits<StructTypeView>::Id(type);
  }
};

inline StructType::StructType(StructTypeView type)
    : interface_(type.interface_) {}

// OpaqueTypeView -> OpaqueTypeViewFor<T>
template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::conjunction_v<
        std::bool_constant<sizeof(To) == sizeof(absl::remove_cvref_t<From>)>,
        std::bool_constant<alignof(To) == alignof(absl::remove_cvref_t<From>)>,
        std::is_same<StructTypeView, absl::remove_cvref_t<From>>,
        std::negation<std::is_same<StructTypeView, To>>,
        std::is_base_of<StructTypeView, To>>>>
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
class StructTypeViewFor final : public StructTypeView {
 public:
  using interface_type = InterfaceType;
  using alternative_type = StructTypeFor<interface_type>;

  // NOLINTNEXTLINE(google-explicit-constructor)
  StructTypeViewFor(
      const alternative_type& type ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : StructTypeView(type) {}

  template <typename T, typename = std::enable_if_t<std::is_base_of_v<
                            interface_type, std::remove_const_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  StructTypeViewFor(SharedView<T> interface) : StructTypeView(interface) {}

  StructTypeViewFor() = delete;
  StructTypeViewFor(const StructTypeViewFor&) = default;
  StructTypeViewFor(StructTypeViewFor&&) = default;
  StructTypeViewFor& operator=(const StructTypeViewFor&) = default;
  StructTypeViewFor& operator=(StructTypeViewFor&&) = default;

  const interface_type& operator*() const {
    return cel::internal::down_cast<const interface_type&>(
        StructTypeView::operator*());
  }

  absl::Nonnull<const interface_type*> operator->() const {
    return cel::internal::down_cast<const interface_type*>(
        StructTypeView::operator->());
  }
};

template <typename InterfaceType>
struct SubsumptionTraits<StructTypeViewFor<InterfaceType>> final {
  static bool IsA(StructTypeView type) {
    return NativeTypeId::Of(type) == NativeTypeId::For<InterfaceType>();
  }

  static StructTypeViewFor<InterfaceType> DownCast(StructTypeView type) {
    static_assert(sizeof(StructTypeView) ==
                  sizeof(StructTypeViewFor<InterfaceType>));
    static_assert(alignof(StructTypeView) ==
                  alignof(StructTypeViewFor<InterfaceType>));
    CheckDownCast(type);
    return StructTypeViewFor<InterfaceType>(
        static_cast<const StructTypeViewFor<InterfaceType>&>(type));
  }

 private:
  static void CheckDownCast(StructTypeView type) {
#ifndef NDEBUG
#ifdef CEL_INTERNAL_HAVE_RTTI
    ABSL_DCHECK(cel::internal::down_cast<const InterfaceType*>(
                    type.operator->()) != nullptr);
#endif
#endif
  }
};

class StructTypeFieldIterator {
 public:
  virtual ~StructTypeFieldIterator() = default;

  virtual bool HasNext() = 0;

  virtual absl::StatusOr<StructTypeFieldId> Next() = 0;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_STRUCT_TYPE_H_
