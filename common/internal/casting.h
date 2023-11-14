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

// IWYU pragma: private, include "common/casting.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_CASTING_H_
#define THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_CASTING_H_

#include <memory>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/log/absl_check.h"
#include "absl/meta/type_traits.h"
#include "absl/types/optional.h"
#include "absl/utility/utility.h"
#include "common/native_type.h"
#include "internal/casts.h"
#include "internal/optional_ref.h"

namespace cel {

namespace common_internal {

template <typename To, typename From>
using propagate_const_t =
    std::conditional_t<std::is_const_v<From>, std::add_const_t<To>, To>;

template <typename To, typename From>
using propagate_volatile_t =
    std::conditional_t<std::is_volatile_v<From>, std::add_volatile_t<To>, To>;

template <typename To, typename From>
using propagate_reference_t =
    std::conditional_t<std::is_lvalue_reference_v<From>,
                       std::add_lvalue_reference_t<To>,
                       std::conditional_t<std::is_rvalue_reference_v<From>,
                                          std::add_rvalue_reference_t<To>, To>>;

template <typename To, typename From>
using propagate_cvref_t = propagate_reference_t<
    propagate_volatile_t<propagate_const_t<To, From>, From>, From>;

}  // namespace common_internal

template <typename To, typename From, typename = void>
struct CastTraits;

template <typename To, typename From>
struct CastTraits<
    To, From, std::enable_if_t<std::is_same_v<To, absl::remove_cvref_t<From>>>>
    final {
  static bool Compatible(const absl::remove_cvref_t<From>&) { return true; }

  static From Convert(From from) { return static_cast<From>(from); }
};

// Upcasting.
template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::conjunction_v<
        std::negation<std::is_same<To, absl::remove_cvref_t<From>>>,
        std::is_lvalue_reference<From>,
        std::is_base_of<To, absl::remove_cvref_t<From>>>>>
    final {
  static bool Compatible(const absl::remove_cvref_t<From>&) { return true; }

  static common_internal::propagate_cvref_t<To, From> Convert(From from) {
    return static_cast<common_internal::propagate_cvref_t<To, From>>(from);
  }
};

// Downcasting.
template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::conjunction_v<
        std::negation<std::is_same<To, absl::remove_cvref_t<From>>>,
        std::is_lvalue_reference<From>,
        std::is_base_of<absl::remove_cvref_t<From>, To>>>>
    final {
  static bool Compatible(const absl::remove_cvref_t<From>& from) {
    return To::ClassOf(from);
  }

  static common_internal::propagate_cvref_t<To, From> Convert(From from) {
    return cel::internal::down_cast<
        common_internal::propagate_cvref_t<To, From>>(from);
  }
};

namespace common_internal {

template <typename To>
struct InstanceOfImpl final {
  static_assert(!std::is_pointer_v<To>, "To must not be a pointer");
  static_assert(!std::is_array_v<To>, "To must not be an array");
  static_assert(!std::is_lvalue_reference_v<To>,
                "To must not be a lvalue reference");
  static_assert(!std::is_rvalue_reference_v<To>,
                "To must not be a lvalue reference");
  static_assert(!std::is_const_v<To>, "To must not be const qualified");
  static_assert(!std::is_volatile_v<To>, "To must not be volatile qualified");
  static_assert(std::is_class_v<To>, "To must be a non-union class");

  explicit InstanceOfImpl() = default;

  template <typename From>
  ABSL_MUST_USE_RESULT bool operator()(const From& from) const {
    static_assert(!std::is_volatile_v<From>,
                  "From must not be volatile qualified");
    static_assert(std::is_class_v<From>, "From must be a non-union class");
    return CastTraits<To, const From&>::Compatible(from);
  }

  template <typename From>
  ABSL_MUST_USE_RESULT bool operator()(const From* from) const {
    static_assert(!std::is_volatile_v<From>,
                  "From must not be volatile qualified");
    static_assert(std::is_class_v<From>, "From must be a non-union class");
    return from == nullptr || (*this)(*from);
  }
};

template <typename To>
struct CastImpl final {
  static_assert(!std::is_pointer_v<To>, "To must not be a pointer");
  static_assert(!std::is_array_v<To>, "To must not be an array");
  static_assert(!std::is_lvalue_reference_v<To>,
                "To must not be a lvalue reference");
  static_assert(!std::is_rvalue_reference_v<To>,
                "To must not be a lvalue reference");
  static_assert(!std::is_const_v<To>, "To must not be const qualified");
  static_assert(!std::is_volatile_v<To>, "To must not be volatile qualified");
  static_assert(std::is_class_v<To>, "To must be a non-union class");

  explicit CastImpl() = default;

  template <typename From>
  ABSL_MUST_USE_RESULT decltype(auto) operator()(From&& from) const {
    static_assert(!std::is_volatile_v<From>,
                  "From must not be volatile qualified");
    static_assert(std::is_class_v<absl::remove_cvref_t<From>>,
                  "From must be a non-union class");
    ABSL_DCHECK((CastTraits<To, const From&>::Compatible(from)))
        << NativeTypeId::Of(from) << " => " << NativeTypeId::For<To>();
    return CastTraits<To, From>::Convert(std::forward<From>(from));
  }

  template <typename From>
  ABSL_MUST_USE_RESULT decltype(auto) operator()(From* from) const {
    static_assert(!std::is_volatile_v<From>,
                  "From must not be volatile qualified");
    static_assert(std::is_class_v<From>, "From must be a non-union class");
    using R = decltype((*this)(*from));
    static_assert(std::is_lvalue_reference_v<R>);
    if (from == nullptr) {
      return static_cast<std::add_pointer_t<std::remove_reference_t<R>>>(
          nullptr);
    }
    return static_cast<std::add_pointer_t<std::remove_reference_t<R>>>(
        std::addressof((*this)(*from)));
  }
};

template <typename To>
struct AsImpl final {
  static_assert(!std::is_pointer_v<To>, "To must not be a pointer");
  static_assert(!std::is_array_v<To>, "To must not be an array");
  static_assert(!std::is_lvalue_reference_v<To>,
                "To must not be a lvalue reference");
  static_assert(!std::is_rvalue_reference_v<To>,
                "To must not be a lvalue reference");
  static_assert(!std::is_const_v<To>, "To must not be const qualified");
  static_assert(!std::is_volatile_v<To>, "To must not be volatile qualified");
  static_assert(std::is_class_v<To>, "To must be a non-union class");

  explicit AsImpl() = default;

  template <typename From>
  ABSL_MUST_USE_RESULT decltype(auto) operator()(From&& from) const {
    // Returns either `absl::optional` or `cel::internal::optional_ref`
    // depending on the return type of `CastTraits::Convert`. The use of these
    // two types is an implementation detail.
    static_assert(!std::is_volatile_v<From>,
                  "From must not be volatile qualified");
    static_assert(std::is_class_v<absl::remove_cvref_t<From>>,
                  "From must be a non-union class");
    using R = decltype(CastTraits<To, From>::Convert(std::forward<From>(from)));
    if (!CastTraits<To, const From&>::Compatible(from)) {
      if constexpr (std::is_lvalue_reference_v<R>) {
        return cel::internal::optional_ref<std::remove_reference_t<R>>{
            absl::nullopt};
      } else {
        return absl::optional<absl::remove_cvref_t<R>>{absl::nullopt};
      }
    }
    if constexpr (std::is_lvalue_reference_v<R>) {
      return cel::internal::optional_ref<std::remove_reference_t<R>>{
          CastTraits<To, From>::Convert(std::forward<From>(from))};
    } else {
      return absl::optional<absl::remove_cvref_t<R>>{
          absl::in_place,
          CastTraits<To, From>::Convert(std::forward<From>(from))};
    }
  }

  // Returns a pointer.
  template <typename From>
  ABSL_MUST_USE_RESULT decltype(auto) operator()(From* from) const {
    // Returns either `absl::optional` or `To*` depending on the return type of
    // `CastTraits::Convert`. The use of these two types is an implementation
    // detail.
    static_assert(!std::is_volatile_v<From>,
                  "From must not be volatile qualified");
    static_assert(std::is_class_v<From>, "From must be a non-union class");
    using R = decltype((*this)(*from));
    if (from == nullptr || !CastTraits<To, const From&>::Compatible(from)) {
      if constexpr (std::is_lvalue_reference_v<R>) {
        return static_cast<std::add_pointer_t<std::remove_reference_t<R>>>(
            nullptr);
      } else {
        return absl::optional<absl::remove_cvref_t<R>>{absl::nullopt};
      }
    }
    if constexpr (std::is_lvalue_reference_v<R>) {
      return static_cast<std::add_pointer_t<std::remove_reference_t<R>>>(
          std::addressof((*this)(*from)));
    } else {
      return absl::optional<absl::remove_cvref_t<R>>{absl::in_place,
                                                     (*this)(*from)};
    }
  }
};

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_CASTING_H_
