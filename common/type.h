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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_TYPE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_TYPE_H_

#include <algorithm>
#include <array>
#include <cstdint>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/algorithm/container.h"
#include "absl/base/attributes.h"
#include "absl/container/fixed_array.h"
#include "absl/log/absl_check.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "absl/types/variant.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type_interface.h"  // IWYU pragma: export
#include "common/type_kind.h"
#include "common/types/any_type.h"   // IWYU pragma: export
#include "common/types/bool_type.h"  // IWYU pragma: export
#include "common/types/bool_wrapper_type.h"  // IWYU pragma: export
#include "common/types/bytes_type.h"  // IWYU pragma: export
#include "common/types/bytes_wrapper_type.h"  // IWYU pragma: export
#include "common/types/double_type.h"  // IWYU pragma: export
#include "common/types/double_wrapper_type.h"  // IWYU pragma: export
#include "common/types/duration_type.h"  // IWYU pragma: export
#include "common/types/dyn_type.h"    // IWYU pragma: export
#include "common/types/error_type.h"  // IWYU pragma: export
#include "common/types/function_type.h"  // IWYU pragma: export
#include "common/types/int_type.h"  // IWYU pragma: export
#include "common/types/int_wrapper_type.h"  // IWYU pragma: export
#include "common/types/list_type.h"  // IWYU pragma: export
#include "common/types/map_type.h"   // IWYU pragma: export
#include "common/types/null_type.h"  // IWYU pragma: export
#include "common/types/opaque_type.h"  // IWYU pragma: export
#include "common/types/optional_type.h"  // IWYU pragma: export
#include "common/types/string_type.h"  // IWYU pragma: export
#include "common/types/string_wrapper_type.h"  // IWYU pragma: export
#include "common/types/struct_type.h"  // IWYU pragma: export
#include "common/types/timestamp_type.h"  // IWYU pragma: export
#include "common/types/type_param_type.h"  // IWYU pragma: export
#include "common/types/type_type.h"  // IWYU pragma: export
#include "common/types/types.h"
#include "common/types/uint_type.h"  // IWYU pragma: export
#include "common/types/uint_wrapper_type.h"  // IWYU pragma: export
#include "common/types/unknown_type.h"  // IWYU pragma: export

namespace cel {

class Type;

// `Type` is a composition type which encompasses all types supported by the
// Common Expression Language. When default constructed or moved, `Type` is in a
// known but invalid state. Any attempt to use it from then on, without
// assigning another type, is undefined behavior. In debug builds, we do our
// best to fail.
class Type final {
 public:
  Type() = default;
  Type(const Type&) = default;
  Type(Type&&) = default;
  Type& operator=(const Type&) = default;
  Type& operator=(Type&&) = default;

  template <typename T,
            typename = std::enable_if_t<common_internal::IsTypeInterfaceV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Type(const Shared<const T>& interface) noexcept
      : variant_(
            absl::in_place_type<common_internal::BaseTypeAlternativeForT<T>>,
            interface) {}

  template <typename T,
            typename = std::enable_if_t<common_internal::IsTypeInterfaceV<T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Type(Shared<const T>&& interface) noexcept
      : variant_(
            absl::in_place_type<common_internal::BaseTypeAlternativeForT<T>>,
            std::move(interface)) {}

  template <typename T,
            typename = std::enable_if_t<
                common_internal::IsTypeAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Type(T&& alternative) noexcept
      : variant_(absl::in_place_type<common_internal::BaseTypeAlternativeForT<
                     absl::remove_cvref_t<T>>>,
                 std::forward<T>(alternative)) {}

  template <typename T,
            typename = std::enable_if_t<
                common_internal::IsTypeAlternativeV<absl::remove_cvref_t<T>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Type& operator=(T&& type) noexcept {
    variant_.emplace<
        common_internal::BaseTypeAlternativeForT<absl::remove_cvref_t<T>>>(
        std::forward<T>(type));
    return *this;
  }

  TypeKind kind() const {
    AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> TypeKind {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return TypeKind::kError. In debug
            // builds we cannot reach here.
            return TypeKind::kError;
          } else {
            return alternative.kind();
          }
        },
        variant_);
  }

  absl::string_view name() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> absl::string_view {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return an empty string. In debug
            // builds we cannot reach here.
            return absl::string_view();
          } else {
            return alternative.name();
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

  absl::Span<const Type> parameters() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    AssertIsValid();
    return absl::visit(
        [](const auto& alternative) -> absl::Span<const Type> {
          if constexpr (std::is_same_v<
                            absl::remove_cvref_t<decltype(alternative)>,
                            absl::monostate>) {
            // In optimized builds, we just return an empty string. In debug
            // builds we cannot reach here.
            return {};
          } else {
            return alternative.parameters();
          }
        },
        variant_);
  }

  void swap(Type& other) noexcept { variant_.swap(other.variant_); }

  template <typename H>
  friend H AbslHashValue(H state, const Type& type) {
    type.AssertIsValid();
    return H::combine(
        absl::visit(
            [state = std::move(state)](const auto& alternative) mutable -> H {
              if constexpr (std::is_same_v<
                                absl::remove_cvref_t<decltype(alternative)>,
                                absl::monostate>) {
                // In optimized builds, we just hash `absl::monostate`. In debug
                // builds we cannot reach here.
                return H::combine(std::move(state), alternative);
              } else {
                return H::combine(std::move(state), alternative.kind(),
                                  alternative);
              }
            },
            type.variant_),
        type.variant_.index());
  }

  friend bool operator==(const Type& lhs, const Type& rhs) {
    lhs.AssertIsValid();
    rhs.AssertIsValid();
    return lhs.variant_ == rhs.variant_;
  }

  friend std::ostream& operator<<(std::ostream& out, const Type& type) {
    type.AssertIsValid();
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
        type.variant_);
  }

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

  Type* operator->() { return this; }

  const Type* operator->() const { return this; }

 private:
  friend struct NativeTypeTraits<Type>;
  friend struct CompositionTraits<Type>;

  constexpr bool IsValid() const {
    return !absl::holds_alternative<absl::monostate>(variant_);
  }

  void AssertIsValid() const {
    ABSL_DCHECK(IsValid()) << "use of invalid Type";
  }

  common_internal::TypeVariant variant_;
};

inline void swap(Type& lhs, Type& rhs) noexcept { lhs.swap(rhs); }

inline bool operator!=(const Type& lhs, const Type& rhs) {
  return !operator==(lhs, rhs);
}

template <>
struct NativeTypeTraits<Type> final {
  static NativeTypeId Id(const Type& type) {
    type.AssertIsValid();
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
        type.variant_);
  }

  static bool SkipDestructor(const Type& type) {
    type.AssertIsValid();
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
        type.variant_);
  }
};

template <>
struct CompositionTraits<Type> final {
  template <typename U>
  static std::enable_if_t<common_internal::IsTypeAlternativeV<U>, bool> HasA(
      const Type& type) {
    type.AssertIsValid();
    using Base = common_internal::BaseTypeAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::holds_alternative<U>(type.variant_);
    } else {
      return absl::holds_alternative<Base>(type.variant_) &&
             InstanceOf<U>(Get<U>(type));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsTypeAlternativeV<U>, const U&> Get(
      const Type& type) {
    type.AssertIsValid();
    using Base = common_internal::BaseTypeAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(type.variant_);
    } else {
      return Cast<U>(absl::get<Base>(type.variant_));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsTypeAlternativeV<U>, U&> Get(
      Type& type) {
    type.AssertIsValid();
    using Base = common_internal::BaseTypeAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(type.variant_);
    } else {
      return Cast<U>(absl::get<Base>(type.variant_));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsTypeAlternativeV<U>, U> Get(
      const Type&& type) {
    type.AssertIsValid();
    using Base = common_internal::BaseTypeAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(std::move(type.variant_));
    } else {
      return Cast<U>(absl::get<Base>(std::move(type.variant_)));
    }
  }

  template <typename U>
  static std::enable_if_t<common_internal::IsTypeAlternativeV<U>, U> Get(
      Type&& type) {
    type.AssertIsValid();
    using Base = common_internal::BaseTypeAlternativeForT<U>;
    if constexpr (std::is_same_v<Base, U>) {
      return absl::get<U>(std::move(type.variant_));
    } else {
      return Cast<U>(absl::get<Base>(std::move(type.variant_)));
    }
  }
};

template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::is_same_v<Type, absl::remove_cvref_t<From>>>>
    : CompositionCastTraits<To, From> {};

// Statically assert some expectations.
static_assert(std::is_default_constructible_v<Type>);
static_assert(std::is_copy_constructible_v<Type>);
static_assert(std::is_copy_assignable_v<Type>);
static_assert(std::is_nothrow_move_constructible_v<Type>);
static_assert(std::is_nothrow_move_assignable_v<Type>);
static_assert(std::is_nothrow_swappable_v<Type>);

struct StructTypeField {
  std::string name;
  Type type;
  // The field number, if less than or equal to 0 it is not available.
  int64_t number = 0;
};

// Now that Type and TypeView are complete, we can define various parts of list,
// map, opaque, and struct which depend on Type and TypeView.

namespace common_internal {

struct ListTypeData final {
  explicit ListTypeData(Type element) noexcept : element(std::move(element)) {}

  const Type element;
};

struct MapTypeData final {
  explicit MapTypeData(Type key, Type value) noexcept
      : key_and_value{std::move(key), std::move(value)} {}

  std::array<Type, 2> key_and_value;
};

struct OpaqueTypeData final {
  explicit OpaqueTypeData(std::string name,
                          absl::FixedArray<Type, 1> parameters)
      : name(std::move(name)), parameters(std::move(parameters)) {}

  const std::string name;
  const absl::FixedArray<Type, 1> parameters;
};

struct StructTypeData final {
  explicit StructTypeData(std::string name) : name(std::move(name)) {}

  const std::string name;
};

struct TypeParamTypeData final {
  explicit TypeParamTypeData(std::string name) : name(std::move(name)) {}

  const std::string name;
};

struct FunctionTypeData final {
  explicit FunctionTypeData(absl::FixedArray<Type, 3> result_and_args)
      : result_and_args(std::move(result_and_args)) {}

  const absl::FixedArray<Type, 3> result_and_args;
};

struct TypeTypeData final {
  explicit TypeTypeData(Type type) : type(std::move(type)) {}

  const Type type;
};

}  // namespace common_internal

template <>
struct NativeTypeTraits<common_internal::ListTypeData> final {
  static bool SkipDestructor(const common_internal::ListTypeData& data) {
    return NativeType::SkipDestructor(data.element);
  }
};

template <>
struct NativeTypeTraits<common_internal::MapTypeData> final {
  static bool SkipDestructor(const common_internal::MapTypeData& data) {
    return NativeType::SkipDestructor(data.key_and_value[0]) &&
           NativeType::SkipDestructor(data.key_and_value[1]);
  }
};

template <>
struct NativeTypeTraits<common_internal::FunctionTypeData> final {
  static bool SkipDestructor(const common_internal::FunctionTypeData& data) {
    return absl::c_all_of(data.result_and_args, [](const Type& type) -> bool {
      return NativeType::SkipDestructor(type);
    });
  }
};

inline ListType::ListType() : ListType(common_internal::GetDynListType()) {}

inline ListType::ListType(MemoryManagerRef memory_manager, Type element)
    : data_(memory_manager.MakeShared<common_internal::ListTypeData>(
          std::move(element))) {}

inline absl::Span<const Type> ListType::parameters() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return absl::MakeConstSpan(&data_->element, 1);
}

inline const Type& ListType::element() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return data_->element;
}

inline bool operator==(const ListType& lhs, const ListType& rhs) {
  return &lhs == &rhs || lhs.element() == rhs.element();
}

template <typename H>
inline H AbslHashValue(H state, const ListType& type) {
  return H::combine(std::move(state), type.element());
}

inline MapType::MapType() : MapType(common_internal::GetDynDynMapType()) {}

inline MapType::MapType(MemoryManagerRef memory_manager, Type key, Type value)
    : data_(memory_manager.MakeShared<common_internal::MapTypeData>(
          std::move(key), std::move(value))) {}

inline absl::Span<const Type> MapType::parameters() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return absl::MakeConstSpan(data_->key_and_value);
}

inline const Type& MapType::key() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return data_->key_and_value[0];
}

inline const Type& MapType::value() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return data_->key_and_value[1];
}

inline bool operator==(const MapType& lhs, const MapType& rhs) {
  return &lhs == &rhs || (lhs.key() == rhs.key() && lhs.value() == rhs.value());
}

template <typename H>
inline H AbslHashValue(H state, const MapType& type) {
  return H::combine(std::move(state), type.key(), type.value());
}

inline StructType::StructType(MemoryManagerRef memory_manager,
                              absl::string_view name)
    : data_(memory_manager.MakeShared<common_internal::StructTypeData>(
          std::string(name))) {}

inline absl::string_view StructType::name() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return data_->name;
}

inline TypeParamType::TypeParamType(MemoryManagerRef memory_manager,
                                    absl::string_view name)
    : data_(memory_manager.MakeShared<common_internal::TypeParamTypeData>(
          std::string(name))) {}

inline absl::string_view TypeParamType::name() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return data_->name;
}

inline absl::string_view OpaqueType::name() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return data_->name;
}

inline absl::Span<const Type> OpaqueType::parameters() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return data_->parameters;
}

inline bool operator==(const OpaqueType& lhs, const OpaqueType& rhs) {
  return lhs.name() == rhs.name() &&
         absl::c_equal(lhs.parameters(), rhs.parameters());
}

template <typename H>
inline H AbslHashValue(H state, const OpaqueType& type) {
  state = H::combine(std::move(state), type.name());
  auto parameters = type.parameters();
  for (const auto& parameter : parameters) {
    state = H::combine(std::move(state), parameter);
  }
  return std::move(state);
}

inline OptionalType::OptionalType()
    : OptionalType(common_internal::GetDynOptionalType()) {}

inline OptionalType::OptionalType(MemoryManagerRef memory_manager,
                                  const Type& parameter)
    : OpaqueType(memory_manager, kName, {parameter}) {}

inline const Type& OptionalType::parameter() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return parameters().front();
}

inline bool operator==(const OptionalType& lhs, const OptionalType& rhs) {
  return lhs.parameter() == rhs.parameter();
}

template <typename H>
inline H AbslHashValue(H state, const OptionalType& type) {
  return H::combine(std::move(state), type.parameter());
}

inline absl::Span<const Type> FunctionType::parameters() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return absl::MakeConstSpan(data_->result_and_args);
}

inline const Type& FunctionType::result() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return data_->result_and_args[0];
}

inline absl::Span<const Type> FunctionType::args() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  return absl::MakeConstSpan(data_->result_and_args).subspan(1);
}

inline bool operator==(const FunctionType& lhs, const FunctionType& rhs) {
  return lhs.result() == rhs.result() && absl::c_equal(lhs.args(), rhs.args());
}

template <typename H>
inline H AbslHashValue(H state, const FunctionType& type) {
  state = H::combine(std::move(state), type.result());
  auto args = type.args();
  for (const auto& arg : args) {
    state = H::combine(std::move(state), arg);
  }
  return std::move(state);
}

inline TypeType::TypeType(MemoryManagerRef memory_manager, Type parameter)
    : data_(memory_manager.MakeShared<common_internal::TypeTypeData>(
          std::move(parameter))) {}

inline absl::Span<const Type> TypeType::parameters() const
    ABSL_ATTRIBUTE_LIFETIME_BOUND {
  if (data_) {
    return absl::MakeConstSpan(&data_->type, 1);
  }
  return {};
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_TYPE_H_
