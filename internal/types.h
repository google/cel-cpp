// Helpers to work with sets of types.

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_TYPE_UTIL_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_TYPE_UTIL_H_

#include <tuple>
#include <type_traits>

#include "absl/memory/memory.h"
#include "absl/strings/string_view.h"
#include "internal/port.h"
#include "internal/specialize.h"

namespace google {
namespace api {
namespace expr {
namespace internal {

// Short names for AND, OR and NOT.
template <typename... T>
using and_t = conjunction<T...>;
template <typename... T>
using or_t = disjunction<T...>;
template <typename T>
using not_t = negation<T>;

// Holder for a static list of types.
template <typename... Args>
using types = std::tuple<Args...>;

// Helper that defines 'value' to be the number of types in T.
template <typename T>
using types_size = std::tuple_size<T>;

template <typename... T>
using types_cat = decltype(std::tuple_cat(inst_of<decay_t<T>>()...));

// Helper that resolves to the Ith type in T.
template <std::size_t I, typename T>
using type_at = typename std::tuple_element<I, T>::type;

// Helper that defines 'value' to be true if there are no types in T.
template <typename T>
using types_empty = std::integral_constant<bool, types_size<T>::value == 0>;

// Helper that returns the index of the first occurrence of E in T.
template <typename E, typename T, std::size_t I = 0>
struct type_index {
  static constexpr std::size_t value = -1;
};
template <typename E, std::size_t I, class... Types>
struct type_index<E, types<E, Types...>, I> {
  static constexpr std::size_t value = I;
};
template <class E, class U, std::size_t I, class... Types>
struct type_index<E, std::tuple<U, Types...>, I>
    : type_index<E, types<Types...>, I + 1> {};

// Helper that defines 'value' to be true if E occurs in T.
template <typename E, typename T>
using type_in = std::integral_constant<bool, type_index<E, T>::value !=
                                                 static_cast<std::size_t>(-1)>;

// A 'type map' lookup of `Key` in a map from Keys -> Values.
template <typename Key, typename Keys, typename Values>
using get_type = type_at<type_index<Key, Keys>::value, Values>;

// Helper definitions for packed types.
template <typename... Args>
using args_size = types_size<types<Args...>>;
template <typename... Args>
using args_empty = types_empty<types<Args...>>;
template <typename E, typename... Args>
using arg_in = type_in<E, types<Args...>>;

template <typename T, typename S>
using type_is = std::is_same<T, S>;

template <typename T, typename S>
using type_not = negation<type_is<T, S>>;

/**
 * Tests if a type is a raw or smart pointer.
 *
 * Any type that defines overloads for * and -> is considered a smart pointer.
 */
template <typename T, typename = specialize>
struct is_ptr : std::is_pointer<decay_t<T>> {};
template <typename T>
struct is_ptr<T, specialize_for<decltype(&decay_t<T>::operator*),
                                decltype(&decay_t<T>::operator->)>>
    : std::true_type {};

/**
 * Tests if a type is convertible to absl::string_view.
 */
template <typename T>
using is_string = and_t<type_not<remove_cvref_t<T>, std::nullptr_t>,
                        std::is_convertible<T, absl::string_view>>;

/**
 * Tests if a type is a signed integer type.
 */
template <typename T>
using is_int = conjunction<std::is_integral<remove_cvref_t<T>>,
                           std::is_signed<decay_t<T>>>;

/**
 * Tests if a type is an unsigned integer type.
 */
template <typename T>
using is_uint =
    conjunction<type_not<remove_cvref_t<T>, bool>, std::is_integral<decay_t<T>>,
                std::is_unsigned<decay_t<T>>>;

/**
 * Tests if a type is a floating point type.
 */
template <typename T>
using is_float = std::is_floating_point<decay_t<T>>;

template <typename T>
using is_numeric = or_t<is_int<T>, is_uint<T>, is_float<T>>;

// Containers define a "value_type" and "iterator".
// Note: The full spec can be found at
// https://en.cppreference.com/w/cpp/named_req/Container
template <typename T, typename = specialize>
struct is_container : public std::false_type {};
template <typename T>
struct is_container<T, specialize_for<typename remove_cvref_t<T>::value_type,
                                      typename remove_cvref_t<T>::iterator>>
    : public std::true_type {};

// Maps are containers that also define a "mapped_type".
template <typename T, typename = specialize>
struct is_map : public std::false_type {};
template <typename T>
struct is_map<T, specialize_for<typename remove_cvref_t<T>::mapped_type>>
    : public is_container<T> {};

// Lists are containers that are not maps.
template <typename T>
using is_list = bool_constant<is_container<T>::value && !is_map<T>::value>;

// Used to create a compiler error when a specialized function/class is
// instantiated with an unsupported type.
template <typename... Args>
struct UnsupportedType;

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google

#endif
