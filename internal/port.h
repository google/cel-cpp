// This files is a forwarding header for other headers containing various
// portability macros and functions.

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_PORT_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_PORT_H_

#include <type_traits>

namespace google {
namespace api {
namespace expr {
namespace internal {

// Back port some helpers.
// Defined in std as of c++14.
template <typename T>
using decay_t = typename std::decay<T>::type;
template <bool B, typename T = void>
using enable_if_t = typename std::enable_if<B, T>::type;
template <bool B, typename T, typename F>
using conditional_t = typename std::conditional<B, T, F>::type;
template <typename T>
using remove_const_t = typename std::remove_const<T>::type;
template <typename T>
using remove_reference_t = typename std::remove_reference<T>::type;
template <typename T>
using remove_cv_t = typename std::remove_cv<T>::type;
template <typename T>
using remove_const_t = typename std::remove_const<T>::type;
template <typename T>
using remove_volatile_t = typename std::remove_volatile<T>::type;

// Defined in std as of c++17
template <typename...>
struct conjunction : std::true_type {};
template <typename T>
struct conjunction<T> : T {};
template <class T, class... Rest>
struct conjunction<T, Rest...>
    : std::conditional<T::value, conjunction<Rest...>, T>::type {};
template <class...>
struct disjunction : std::false_type {};
template <class B1>
struct disjunction<B1> : B1 {};
template <class B1, class... Bn>
struct disjunction<B1, Bn...>
    : conditional_t<bool(B1::value), B1, disjunction<Bn...>> {};
template <bool B>
using bool_constant = std::integral_constant<bool, B>;
template <class B>
struct negation : bool_constant<!static_cast<bool>(B::value)> {};

// Defined in std as of c++20
template <typename T>
struct remove_cvref {
  typedef remove_cv_t<remove_reference_t<T>> type;
};
template <class T>
using remove_cvref_t = typename remove_cvref<T>::type;

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_PORT_H_
