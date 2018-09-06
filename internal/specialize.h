/**
 * Classes and helpers to allow preferring a specialized function, even if it
 * is tied for "most specialized" with a generic implementation.
 * Two flavors are supported. The first works with function overloads:
 *
 *      template <typename T> void foo(T&& v, generic);
 *      template <typename T> void foo(T&& v, specialize_ift<is_ptr<T>>);
 *      template <typename T> void foo(T&& v) {
 *        return foo(std::forward<T>(v), specialize());
 *      }
 *
 * In this case, if T is a pointer type, the second `foo` overload is chosen.
 * Without general/specialize, a call to `foo` with a pointer type would be
 * ambiguous.
 *
 * The second flavor works with class secializations:
 *
 *     template <typename T, typename = specialize>
 *     struct Foo {
 *       ... default impl ...
 *     };
 *
 *     template <typename T>
 *     struct Foo<T, specialize_ift<is_string<T>>> {
 *       ... specialized impl ...
 *     };
 *
 * In this case, if T is a std::string type, the second `Foo` implemenation is
 * chosen. Without specialize, each std::string type would have to be instantiated
 * explicitly.
 *
 * Specialize helper functions come in three different flavors. A traling 't'
 * indicates a type containing a value is expected. A trailing 'd' indicates
 * that the arguments just need to be defined.
 */

#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_SPECIALIZE_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_SPECIALIZE_H_

#include <type_traits>

namespace google {
namespace api {
namespace expr {
namespace internal {

struct general {};
struct specialize : general {};

/** Returns an instance of T for use in decltype expressions. */
template <typename T, typename...>
T inst_of();

/**
 * Resolves to std::true_type, when all args are valid, otherwise is
 * not defined.
 */
template <typename... Args>
struct is_defined : std::true_type {};

/**
 * A type that is only defined if the template argument is `true`.
 *
 * Not that at least one dependent type argument is required when used to
 * select between overloads with identical signatures. A compile time error
 * with a message similar to "'enable_if' cannot be used to disable this
 * declaration" is produced when this is not the case. Any local template
 * argument can be added to `Args` to resolve this issue.
 *
 * @tparam B The value to be tested.
 * @tparam T The type returned when B is true.
 * @tparam Args Any additional types that must resolve or that are needed to
 * distinguish between two overload.
 */
template <int B, typename T = specialize, typename... Args>
using specialize_if =
    typename std::enable_if<bool(B) && is_defined<Args...>::value, T>::type;

/**
 * A type that is only defined if the template argument defines a constexpr
 * 'value' that resolves to 'true'.
 *
 * For use with many std::* helper types.
 */
template <typename B, typename T = specialize, typename... Args>
using specialize_ift = specialize_if<B::value, T, Args...>;

/**
 * A type that is only defined if the template arguments are defined.
 */
template <typename T, typename... Args>
using specialize_ifd = specialize_if<true, T, Args...>;

/**
 * A type that is only defined if all `Arg` types can be resolved.
 *
 * Used to take advantage of SFINAE so that a specialization considered
 * if all `Arg` types can be resolved. See `is_ptr` for an example.
 */
template <typename... Args>
using specialize_for = specialize_ifd<specialize, Args...>;

/**
 * A type that is only defined the return type of C is R.
 */
template <typename R, typename C, typename T = R>
using specialize_if_returns =
    specialize_ift<std::is_same<R, typename std::result_of<C>::type>, T>;

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_SPECIALIZE_H_
