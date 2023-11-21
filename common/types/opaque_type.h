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

// TODO(uncreated-issue/60): better document extending OpaqueType/OpaqueValue

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPES_OPAQUE_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPES_OPAQUE_TYPE_H_

#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/hash/hash.h"
#include "absl/log/absl_check.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/string_view.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/sized_input_view.h"
#include "common/type_interface.h"
#include "common/type_kind.h"
#include "internal/casts.h"

namespace cel {

class Type;
class TypeView;
class OpaqueTypeInterface;
template <typename InterfaceType>
struct ExtendOpaqueTypeInterface;
class OpaqueType;
template <typename InterfaceType>
class OpaqueTypeFor;
class OpaqueTypeView;
template <typename InterfaceType>
class OpaqueTypeViewFor;

class OpaqueTypeInterface : public TypeInterface {
 public:
  using alternative_type = OpaqueType;
  using view_alternative_type = OpaqueTypeView;

  static constexpr TypeKind kKind = TypeKind::kOpaque;

  TypeKind kind() const final { return kKind; }

  std::string DebugString() const override;

  virtual SizedInputView<TypeView> parameters() const;

 private:
  friend class OpaqueType;
  friend class OpaqueTypeView;
  friend bool operator==(const OpaqueType& lhs, const OpaqueType& rhs);
  template <typename H>
  friend H AbslHashValue(H state, const OpaqueType& type);
  friend bool operator==(OpaqueTypeView lhs, OpaqueTypeView rhs);
  template <typename H>
  friend H AbslHashValue(H state, OpaqueTypeView type);

  virtual bool Equals(const OpaqueTypeInterface& other) const = 0;

  virtual void HashValue(absl::HashState state) const = 0;
};

template <typename InterfaceType>
struct ExtendOpaqueTypeInterface : public OpaqueTypeInterface {
 public:
  using alternative_type = OpaqueTypeFor<InterfaceType>;
  using view_alternative_type = OpaqueTypeViewFor<InterfaceType>;

 private:
  NativeTypeId GetNativeTypeId() const noexcept final {
    return NativeTypeId::For<InterfaceType>();
  }
};

// `OpaqueType` represents an opaque type. Opaque types have fields.
// `DurationType` and `TimestampType` are similar to `OpaqueType`, but are
// represented directly for efficiency.
class OpaqueType {
 public:
  using interface_type = OpaqueTypeInterface;
  using view_alternative_type = OpaqueTypeView;

  static constexpr TypeKind kKind = OpaqueTypeInterface::kKind;

  explicit OpaqueType(OpaqueTypeView type);

  // NOLINTNEXTLINE(google-explicit-constructor)
  OpaqueType(Shared<const OpaqueTypeInterface> interface)
      : interface_(std::move(interface)) {}

  OpaqueType(const OpaqueType&) = default;
  OpaqueType(OpaqueType&&) = default;
  OpaqueType& operator=(const OpaqueType&) = default;
  OpaqueType& operator=(OpaqueType&&) = default;

  TypeKind kind() const { return interface_->kind(); }

  absl::string_view name() const { return interface_->name(); }

  std::string DebugString() const { return interface_->DebugString(); }

  SizedInputView<TypeView> parameters() const;

  void swap(OpaqueType& other) noexcept {
    using std::swap;
    swap(interface_, other.interface_);
  }

  const interface_type& operator*() const { return *interface_; }

  absl::Nonnull<const interface_type*> operator->() const {
    return interface_.operator->();
  }

  friend bool operator==(const OpaqueType& lhs, const OpaqueType& rhs);

  template <typename H>
  friend H AbslHashValue(H state, const OpaqueType& type);

 private:
  friend class OpaqueTypeView;
  friend struct NativeTypeTraits<OpaqueType>;

  Shared<const OpaqueTypeInterface> interface_;
};

inline void swap(OpaqueType& lhs, OpaqueType& rhs) noexcept { lhs.swap(rhs); }

inline bool operator!=(const OpaqueType& lhs, const OpaqueType& rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, const OpaqueType& type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<OpaqueType> final {
  static NativeTypeId Id(const OpaqueType& type) {
    return NativeTypeId::Of(*type.interface_);
  }

  static bool SkipDestructor(const OpaqueType& type) {
    return NativeType::SkipDestructor(type.interface_);
  }
};

template <typename T>
struct NativeTypeTraits<T, std::enable_if_t<std::conjunction_v<
                               std::negation<std::is_same<OpaqueType, T>>,
                               std::is_base_of<OpaqueType, T>>>>
    final {
  static NativeTypeId Id(const T& type) {
    return NativeTypeTraits<OpaqueType>::Id(type);
  }

  static bool SkipDestructor(const T& type) {
    return NativeTypeTraits<OpaqueType>::SkipDestructor(type);
  }
};

// OpaqueType -> OpaqueTypeFor<T>
template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::conjunction_v<
        std::bool_constant<sizeof(To) == sizeof(absl::remove_cvref_t<From>)>,
        std::bool_constant<alignof(To) == alignof(absl::remove_cvref_t<From>)>,
        std::is_same<OpaqueType, absl::remove_cvref_t<From>>,
        std::negation<std::is_same<OpaqueType, To>>,
        std::is_base_of<OpaqueType, To>>>>
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
class OpaqueTypeFor final : public OpaqueType {
 public:
  using interface_type = InterfaceType;
  using view_alternative_type = OpaqueTypeViewFor<interface_type>;

  explicit OpaqueTypeFor(view_alternative_type type) : OpaqueType(type) {}

  template <typename T, typename = std::enable_if_t<std::is_base_of_v<
                            interface_type, std::remove_const_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  OpaqueTypeFor(Shared<T> interface) : OpaqueType(std::move(interface)) {}

  OpaqueTypeFor(const OpaqueTypeFor&) = default;
  OpaqueTypeFor(OpaqueTypeFor&&) = default;
  OpaqueTypeFor& operator=(const OpaqueTypeFor&) = default;
  OpaqueTypeFor& operator=(OpaqueTypeFor&&) = default;

  const interface_type& operator*() const {
    return cel::internal::down_cast<const interface_type&>(
        OpaqueType::operator*());
  }

  absl::Nonnull<const interface_type*> operator->() const {
    return cel::internal::down_cast<const interface_type*>(
        OpaqueType::operator->());
  }
};

template <typename InterfaceType>
struct SubsumptionTraits<OpaqueTypeFor<InterfaceType>> final {
  static bool IsA(const OpaqueType& type) {
    return NativeTypeId::Of(type) == NativeTypeId::For<InterfaceType>();
  }

  static const OpaqueTypeFor<InterfaceType>& DownCast(const OpaqueType& type) {
    static_assert(sizeof(OpaqueType) == sizeof(OpaqueTypeFor<InterfaceType>));
    static_assert(alignof(OpaqueType) == alignof(OpaqueTypeFor<InterfaceType>));
    CheckDownCast(type);
    return static_cast<const OpaqueTypeFor<InterfaceType>&>(type);
  }

  static OpaqueTypeFor<InterfaceType>& DownCast(OpaqueType& type) {
    static_assert(sizeof(OpaqueType) == sizeof(OpaqueTypeFor<InterfaceType>));
    static_assert(alignof(OpaqueType) == alignof(OpaqueTypeFor<InterfaceType>));
    CheckDownCast(type);
    return static_cast<OpaqueTypeFor<InterfaceType>&>(type);
  }

  static OpaqueTypeFor<InterfaceType> DownCast(const OpaqueType&& type) {
    static_assert(sizeof(OpaqueType) == sizeof(OpaqueTypeFor<InterfaceType>));
    static_assert(alignof(OpaqueType) == alignof(OpaqueTypeFor<InterfaceType>));
    CheckDownCast(type);
    return OpaqueTypeFor<InterfaceType>(
        std::move(static_cast<const OpaqueTypeFor<InterfaceType>&>(type)));
  }

  static OpaqueTypeFor<InterfaceType> DownCast(OpaqueType&& type) {
    static_assert(sizeof(OpaqueType) == sizeof(OpaqueTypeFor<InterfaceType>));
    static_assert(alignof(OpaqueType) == alignof(OpaqueTypeFor<InterfaceType>));
    CheckDownCast(type);
    return OpaqueTypeFor<InterfaceType>(
        std::move(static_cast<OpaqueTypeFor<InterfaceType>&>(type)));
  }

 private:
  static void CheckDownCast(const OpaqueType& type) {
#ifndef NDEBUG
#ifdef CEL_INTERNAL_HAVE_RTTI
    ABSL_DCHECK(cel::internal::down_cast<const InterfaceType*>(
                    type.operator->()) != nullptr);
#endif
#endif
  }
};

class OpaqueTypeView {
 public:
  using interface_type = OpaqueTypeInterface;
  using alternative_type = OpaqueType;

  static constexpr TypeKind kKind = OpaqueType::kKind;

  // NOLINTNEXTLINE(google-explicit-constructor)
  OpaqueTypeView(const OpaqueType& type ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : interface_(type.interface_) {}

  OpaqueTypeView() = delete;
  OpaqueTypeView(const OpaqueTypeView&) = default;
  OpaqueTypeView(OpaqueTypeView&&) = default;
  OpaqueTypeView& operator=(const OpaqueTypeView&) = default;
  OpaqueTypeView& operator=(OpaqueTypeView&&) = default;

  TypeKind kind() const { return interface_->kind(); }

  absl::string_view name() const { return interface_->name(); }

  std::string DebugString() const { return interface_->DebugString(); }

  SizedInputView<TypeView> parameters() const;

  const interface_type& operator*() const { return *interface_; }

  absl::Nonnull<const interface_type*> operator->() const {
    return interface_.operator->();
  }

  void swap(OpaqueTypeView& other) noexcept {
    using std::swap;
    swap(interface_, other.interface_);
  }

  friend bool operator==(OpaqueTypeView lhs, OpaqueTypeView rhs);

  template <typename H>
  friend H AbslHashValue(H state, OpaqueTypeView type);

 private:
  friend class OpaqueType;
  friend struct NativeTypeTraits<OpaqueTypeView>;

  SharedView<const OpaqueTypeInterface> interface_;
};

inline void swap(OpaqueTypeView& lhs, OpaqueTypeView& rhs) noexcept {
  lhs.swap(rhs);
}

inline bool operator!=(OpaqueTypeView lhs, OpaqueTypeView rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, OpaqueTypeView type) {
  return out << type.DebugString();
}

template <>
struct NativeTypeTraits<OpaqueTypeView> final {
  static NativeTypeId Id(OpaqueTypeView type) {
    return NativeTypeId::Of(*type.interface_);
  }
};

template <typename T>
struct NativeTypeTraits<T, std::enable_if_t<std::conjunction_v<
                               std::negation<std::is_same<OpaqueTypeView, T>>,
                               std::is_base_of<OpaqueTypeView, T>>>>
    final {
  static NativeTypeId Id(T type) {
    return NativeTypeTraits<OpaqueTypeView>::Id(type);
  }
};

inline OpaqueType::OpaqueType(OpaqueTypeView type)
    : interface_(type.interface_) {}

// OpaqueTypeView -> OpaqueTypeViewFor<T>
template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::conjunction_v<
        std::bool_constant<sizeof(To) == sizeof(absl::remove_cvref_t<From>)>,
        std::bool_constant<alignof(To) == alignof(absl::remove_cvref_t<From>)>,
        std::is_same<OpaqueTypeView, absl::remove_cvref_t<From>>,
        std::negation<std::is_same<OpaqueTypeView, To>>,
        std::is_base_of<OpaqueTypeView, To>>>>
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
class OpaqueTypeViewFor final : public OpaqueTypeView {
 public:
  using interface_type = InterfaceType;
  using alternative_type = OpaqueTypeFor<interface_type>;

  // NOLINTNEXTLINE(google-explicit-constructor)
  OpaqueTypeViewFor(
      const alternative_type& type ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : OpaqueTypeView(type) {}

  template <typename T, typename = std::enable_if_t<std::is_base_of_v<
                            interface_type, std::remove_const_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  OpaqueTypeViewFor(SharedView<T> interface) : OpaqueTypeView(interface) {}

  OpaqueTypeViewFor() = delete;
  OpaqueTypeViewFor(const OpaqueTypeViewFor&) = default;
  OpaqueTypeViewFor(OpaqueTypeViewFor&&) = default;
  OpaqueTypeViewFor& operator=(const OpaqueTypeViewFor&) = default;
  OpaqueTypeViewFor& operator=(OpaqueTypeViewFor&&) = default;

  const interface_type& operator*() const {
    return cel::internal::down_cast<const interface_type&>(
        OpaqueTypeView::operator*());
  }

  absl::Nonnull<const interface_type*> operator->() const {
    return cel::internal::down_cast<const interface_type*>(
        OpaqueTypeView::operator->());
  }
};

template <typename InterfaceType>
struct SubsumptionTraits<OpaqueTypeViewFor<InterfaceType>> final {
  static bool IsA(OpaqueTypeView type) {
    return NativeTypeId::Of(type) == NativeTypeId::For<InterfaceType>();
  }

  static OpaqueTypeViewFor<InterfaceType> DownCast(OpaqueTypeView type) {
    static_assert(sizeof(OpaqueTypeView) ==
                  sizeof(OpaqueTypeViewFor<InterfaceType>));
    static_assert(alignof(OpaqueTypeView) ==
                  alignof(OpaqueTypeViewFor<InterfaceType>));
    CheckDownCast(type);
    return OpaqueTypeViewFor<InterfaceType>(
        static_cast<const OpaqueTypeViewFor<InterfaceType>&>(type));
  }

 private:
  static void CheckDownCast(OpaqueTypeView type) {
#ifndef NDEBUG
#ifdef CEL_INTERNAL_HAVE_RTTI
    ABSL_DCHECK(cel::internal::down_cast<const InterfaceType*>(
                    type.operator->()) != nullptr);
#endif
#endif
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPES_OPAQUE_TYPE_H_
