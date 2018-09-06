#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_HANDLE_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_HANDLE_H_

#include "internal/hash_util.h"
#include "internal/specialize.h"

namespace google {
namespace api {
namespace expr {
namespace internal {

class HandleBase {
 protected:
  ~HandleBase() = default;
};

/**
 * Handle provides a strongly typed wrapper around an integral type suitable
 * for use as an ID.
 *
 * Handle disallows mutation with the exception of assignment from
 * another Handle of the same template instantiation type. Handle defines
 * comparison operators (==,!=,<,<=,>=,>). For example:
 *
 *    class DatabaseId : public Handle<int, DatabaseId> {...};
 *    class TableId : public Handle<int, DatabaseId> {...};
 *
 *    DatabaseId(1) == 1;  // compile time error
 *    DatabaseId(1) + 1;  // compile time error
 *    DatabaseId(1) == TableId(1);  // compile time error
 *    DatabaseId(1) == DatabaseId(1);  // true
 *    DatabaseId(1) == DatabaseId(2);  // false
 *    DatabaseId(1) <= DatabaseId(2);  // true
 */
template <typename T, typename DerivedType>
class Handle : public HandleBase {
 public:
  struct Hasher {
    std::size_t operator()(const DerivedType& handle) const {
      return internal::Hash(handle.value());
    }
  };

  constexpr explicit Handle(T value) : value_(value) {}

  constexpr T value() const { return value_; }

 protected:
  static_assert(std::is_enum<T>::value || std::is_integral<T>::value ||
                    std::is_pointer<T>::value,
                "ValueType must be an integer or pointer type");
  T value_;

  // Can't be destroyed directly.
  ~Handle() = default;
};

template <typename T>
constexpr specialize_ift<std::is_convertible<T*, HandleBase*>, bool> operator==(
    const T& lhs, const T& rhs) {
  return lhs.value() == rhs.value();
}

template <typename T>
constexpr specialize_ift<std::is_convertible<T*, HandleBase*>, bool> operator!=(
    const T& lhs, const T& rhs) {
  return lhs.value() != rhs.value();
}

// Comparison operator useful for data structures that require ordering among
// elements like set<>, map<>, etc...
template <typename T>
constexpr specialize_ift<std::is_convertible<T*, HandleBase*>, bool> operator<(
    const T& lhs, const T& rhs) {
  return lhs.value() < rhs.value();
}

template <typename T>
constexpr specialize_ift<std::is_convertible<T*, HandleBase*>, bool> operator<=(
    const T& lhs, const T& rhs) {
  return lhs.value() <= rhs.value();
}

template <typename T>
constexpr specialize_ift<std::is_convertible<T*, HandleBase*>, bool> operator>(
    const T& lhs, const T& rhs) {
  return lhs.value() > rhs.value();
}
template <typename T>
constexpr specialize_ift<std::is_convertible<T*, HandleBase*>, bool> operator>=(
    const T& lhs, const T& rhs) {
  return lhs.value() >= rhs.value();
}

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_HANDLE_H_
