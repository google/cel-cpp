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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_CASTING_H_
#define THIRD_PARTY_CEL_CPP_COMMON_CASTING_H_

#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/meta/type_traits.h"
#include "common/internal/casting.h"
#include "internal/casts.h"

namespace cel {

// `CastTraits<To, From>` is specialized if `From` can possibility be cast to
// `To`. `From` must be a lvalue or rvalue reference to a non-union class type
// which is optionally const qualified. `To` must be a non-union class type
// which is not qualified. It must adhere to the following interface:
//
// `bool CastTraits<To, From>::Compatible(const From&)`:
// Returns `true` if `From` can be cast to `To` without throwing an exception,
// invoking undefined behavior, or otherwise crashing. Returns `false`
// otherwise.
//
// `To CastTraits<To, From>::Convert(From)`:
// Converts `From` to `To`, assuming that a call to `Compatible` already
// occurred and returned `true`.
template <typename To, typename From, typename>
struct CastTraits;

// `SubsumptionTraits` provides a metaprogramming interface for inspecting
// details of subsumption relationships for a type `T`. All specializations must
// adhere to the following interface:
//
// `bool SubsumptionTraits<Subclass>::IsA(const Superclass&)`:
// Returns `true` if the underlying object referenced as `Superclass` is an
// instance of `Subclass`, `false` otherwise.
template <typename T, typename = void>
struct SubsumptionTraits;

// `CompositionTraits` provides a metaprogramming interface for inspecting
// details of composition relationships for a type `T`. All specializations must
// adhere to the following interface (and must be declared in the `cel`
// namespace):
//
// `bool CompositionTraits<Entity>::HasA<Part>(const Entity&)`:
// Returns `true` if an instance of type `Entity` holds an alternative `Part`,
// `false` otherwise.
//
// `<Part> CompositionTraits<Entity>::Get<Part>(const Entity&)`:
// `<Part> CompositionTraits<Entity>::Get<Part>(Entity&)`:
// `<Part> CompositionTraits<Entity>::Get<Part>(const Entity&&)`:
// `<Part> CompositionTraits<Entity>::Get<Part>(Entity&&)`:
//  Assuming `HasA<Part>(const Entity&)` returned `true`, retrieves `Part` from
//  `Entity`. The return type will either have the same reference and
//  qualifications as `Entity` or will be a plain `Part`.
template <typename T, typename = void>
struct CompositionTraits;

// `SubsumptionCastTraits` is an implementation of `CastTraits` for types with
// subsumption relationships. When upcasting or when `From` and `To` are the
// same type, it just works. When downcasting, it utilizes `SubsumptionTraits`.
// If the types meet these requirements, you can simply specialize `CastTraits`
// similar to the following:
//
// template <typename To, typename From>
// struct CastTraits<To, From, EnableIfSubsumptionCastable<To, From>>
//     : SubsumptionCastTraits<To, From> {};
template <typename To, typename From, typename = void>
struct SubsumptionCastTraits;

// From& => To&
template <typename To, typename From>
struct SubsumptionCastTraits<
    To, From, std::enable_if_t<std::is_lvalue_reference_v<From>>> {
  static_assert(std::is_polymorphic_v<To>);
  static_assert(std::is_polymorphic_v<absl::remove_cvref_t<From>>);
  static_assert(std::is_base_of_v<To, absl::remove_cvref_t<From>> ||
                std::is_base_of_v<absl::remove_cvref_t<From>, To>);

  static bool Compatible(const absl::remove_cvref_t<From>& from) {
    return SubsumptionTraits<To>::IsA(from);
  }

  static common_internal::propagate_cvref_t<To, From> Convert(From from) {
    return internal::down_cast<common_internal::propagate_cvref_t<To, From>>(
        from);
  }
};

// From&& => To&&
template <typename To, typename From>
struct SubsumptionCastTraits<
    To, From, std::enable_if_t<std::is_rvalue_reference_v<From>>> {
  static_assert(std::is_polymorphic_v<To>);
  static_assert(std::is_polymorphic_v<absl::remove_cvref_t<From>>);
  static_assert(std::is_base_of_v<To, absl::remove_cvref_t<From>> ||
                std::is_base_of_v<absl::remove_cvref_t<From>, To>);

  static bool Compatible(const absl::remove_cvref_t<From>& from) {
    return SubsumptionTraits<To>::IsA(from);
  }

  // No Convert because rvalue casting does not make sense with polymorphic
  // types.
  static common_internal::propagate_cvref_t<To, From> Convert(From from);
};

// `EnableIfSubsumptionCastable` is a convenient utility for SFINAE. See
// `SubsumptionCastTraits` for usage.
template <typename To, typename From, typename Base>
using EnableIfSubsumptionCastable = std::enable_if_t<std::conjunction_v<
    std::is_polymorphic<To>, std::is_polymorphic<absl::remove_cvref_t<From>>,
    std::is_base_of<Base, absl::remove_cvref_t<From>>,
    std::is_base_of<absl::remove_cvref_t<From>, To>>>;

// `CompositionCastTraits` is an implementation of `CastTraits` for classes with
// composition relationships. When upcasting (`From` is implicitly convertible
// to `To`) or when `From` and `To` are the same type, it just works. When
// downcasting, it utilizes `CompositionTraits`.
template <typename To, typename From, typename = void>
struct CompositionCastTraits {
  static bool Compatible(const absl::remove_cvref_t<From>& from) {
    return CompositionTraits<absl::remove_cvref_t<From>>::template HasA<To>(
        from);
  }

  static decltype(auto) Convert(From from) {
    return CompositionTraits<absl::remove_cvref_t<From>>::template Get<To>(
        std::forward<From>(from));
  }
};

// `InstanceOf<To>(const From&)` determines whether `From` holds or is `To`.
//
// `To` must be a plain non-union class type that is not qualified.
//
// We expose `InstanceOf` this way to avoid ADL.
//
// Example:
//
// if (InstanceOf<Subclass>(superclass)) {
//   Cast<Subclass>(superclass).SomeMethod();
// }
template <typename To>
ABSL_DEPRECATED("Use Is member functions instead.")
inline constexpr common_internal::InstanceOfImpl<To> InstanceOf{};

// `Cast<To>(From)` is a "checked cast". In debug builds an assertion is emitted
// which verifies `From` is an instance-of `To`. In non-debug builds, invalid
// casts are undefined behavior.
//
// We expose `Cast` this way to avoid ADL.
//
// Example:
//
// if (InstanceOf<Subclass>(superclass)) {
//   Cast<Subclass>(superclass).SomeMethod();
// }
template <typename To>
ABSL_DEPRECATED(
    "Use explicit conversion functions instead through static_cast.")
inline constexpr common_internal::CastImpl<To> Cast{};

// `As<To>(From)` is a "checking cast". The result is explicitly convertible to
// `bool`, such that it can be used with `if` statements. The result can be
// accessed with `operator*` or `operator->`. The return type should be treated
// as an implementation detail, with no assumptions on the concrete type. You
// should use `auto`.
//
// `As` is analogous to the paradigm `if (InstanceOf<B>(a)) Cast<B>(a)`.
//
// We expose `As` this way to avoid ADL.
//
// Example:
//
// if (auto subclass = As<Subclass>(superclass); subclass) {
//   subclass->SomeMethod();
// }
template <typename To>
ABSL_DEPRECATED("Use As member functions instead.")
inline constexpr common_internal::AsImpl<To> As{};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_INSTANCE_OF_H_
