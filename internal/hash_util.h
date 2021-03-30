#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_HASH_UTIL_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_HASH_UTIL_H_

#include <atomic>
#include <functional>

#include "google/protobuf/any.pb.h"
#include "google/rpc/status.pb.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "internal/specialize.h"

namespace google {
namespace api {
namespace expr {
namespace internal {

/** Offset to use so that Hash(<zero-value>) is not always 0. */
constexpr const std::size_t kIntegralTypeOffset = 109;

/** Hash value reserved to represent 'not computed'. */
constexpr const std::size_t kNoHash = std::numeric_limits<std::size_t>::max();

// Lightly hash two hash codes together. When used repetitively to mix more
// than two values, the new values should be in the first argument.
ABSL_MUST_USE_RESULT inline size_t MixHash(size_t new_hash, size_t accu) {
  static const size_t kMul = static_cast<size_t>(0xc6a4a7935bd1e995ULL);
  // Multiplicative hashing will mix bits better in the msb end ...
  accu *= kMul;
  // ... and rotating will move the better mixed msb-bits to lsb-bits.
  return ((accu << 21) | (accu >> (std::numeric_limits<size_t>::digits - 21))) +
         new_hash;
}

// Lightly hash two hash codes together, such that the result is order
// independent.
ABSL_MUST_USE_RESULT inline std::size_t MixHashNoOrder(size_t new_hash,
                                                       size_t accu) {
  return new_hash + accu;
}

template <typename T>
ABSL_MUST_USE_RESULT std::size_t Hash(T&& value);

template <typename T, typename... Args>
ABSL_MUST_USE_RESULT std::size_t Hash(T&& head, Args&&... rest);

template <typename T>
void AccumulateHash(T&& new_value, std::size_t* accu) {
  *accu = MixHash(Hash(std::forward<T>(new_value)), *accu);
}

template <typename T>
void AccumulateHashNoOrder(T&& new_value, std::size_t* accu) {
  *accu = MixHashNoOrder(Hash(std::forward<T>(new_value)), *accu);
}

// Helper to call std::hash function.
template <typename T>
std::size_t StdHash(const T& value) {
  return std::hash<T>{}(value);
}

template <typename T>
std::size_t HashImpl(const T& value, general) {
  return StdHash(value);
}

std::size_t HashImpl(const std::string& value, specialize);
std::size_t HashImpl(absl::string_view value, specialize);
std::size_t HashImpl(absl::Duration value, specialize);
std::size_t HashImpl(absl::Time value, specialize);
std::size_t HashImpl(std::nullptr_t, specialize);
std::size_t HashImpl(const google::rpc::Status& value, specialize);
std::size_t HashImpl(const google::protobuf::Any& value, specialize);

// Hack for supporting 'cords'.
template <typename T>
std::size_t HashImpl(const T& cord, specialize_for<decltype(&T::Subcord)>) {
  return StdHash(cord.ToString());
}

// Specialization for any type that defines a `hash_code` function.
template <class T>
std::size_t HashImpl(const T& value, specialize_for<decltype(&T::hash_code)>) {
  return value.hash_code();
}

// Specialization for classes that define a zero arg constructable hasher.
template <typename T>
std::size_t HashImpl(const T& value, specialize_for<typename T::Hasher>) {
  return typename T::Hasher()(value);
}

// Specialization for enums.
template <typename T>
std::size_t HashImpl(T&& v, specialize_ift<std::is_enum<T>>) {
  return Hash(static_cast<typename std::underlying_type<T>::type>(v));
}

template <typename T>
ABSL_MUST_USE_RESULT std::size_t Hash(T&& value) {
  return HashImpl(std::forward<T>(value), specialize());
}

template <typename T, typename... Args>
ABSL_MUST_USE_RESULT std::size_t Hash(T&& head, Args&&... rest) {
  return MixHash(Hash(std::forward<T>(head)),
                 Hash(std::forward<Args>(rest)...));
}

template <typename T>
ABSL_MUST_USE_RESULT std::size_t HashNoOrder(T&& value) {
  return Hash(std::forward<T>(value));
}

template <typename T, typename... Args>
ABSL_MUST_USE_RESULT std::size_t HashNoOrder(T&& head, Args&&... rest) {
  return MixHashNoOrder(Hash(std::forward<T>(head)),
                        Hash(std::forward<Args>(rest)...));
}

template <typename F, typename S>
ABSL_MUST_USE_RESULT std::size_t HashPair(F&& first, S&& second) {
  size_t h1 = Hash(first);
  size_t h2 = Hash(second);
  if (std::is_integral<F>::value) {
    // We want to avoid absl::Hash({x, y}) == 0 for common values of {x, y}.
    // hash<X> is the identity function for integral types X, so without this,
    // absl::Hash({0, 0}) would be 0.
    h1 += kIntegralTypeOffset;
  }
  return MixHash(h1, h2);
}

/** A visitor for use with CelValue's absl::variant. */
struct Hasher {
  template <class T>
  std::size_t operator()(const T& value) const {
    return Hash(value);
  }
};

template <typename T>
std::size_t LazyComputeHash(T&& hash_fn, std::atomic<std::size_t>* hash_code) {
  std::size_t code = hash_code->load(std::memory_order_relaxed);
  if (code == internal::kNoHash) {
    code = hash_fn();
    if (code == internal::kNoHash) {
      --code;
    }
    hash_code->store(code, std::memory_order_relaxed);
  }
  return code;
}

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_HASH_UTIL_H_
