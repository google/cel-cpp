#ifndef THIRD_PARTY_CEL_CPP_INTERNAL_DOWN_CAST_H_H_
#define THIRD_PARTY_CEL_CPP_INTERNAL_DOWN_CAST_H_H_

#include <cmath>
#include <memory>

#include "absl/memory/memory.h"
#include "absl/meta/type_traits.h"
#include "absl/types/optional.h"
#include "internal/specialize.h"
#include "internal/types.h"

namespace google {
namespace api {
namespace expr {
namespace internal {

template <typename T>
struct StaticDownCastHelper;

// A down casting function that only checks RTT if NDEBUG is not defined and
// works for std::unique_ptr.
template <typename T, typename F>
inline T static_down_cast(F&& value) {
  return std::forward<T>(StaticDownCastHelper<T>::cast(std::forward<F>(value)));
}

template <typename T>
struct StaticDownCastHelper<T*> {
  template <typename F>
  inline static T* cast(F* value) {
    static_assert(std::is_base_of<F, T>::value, "bad down cast.");
    assert(dynamic_cast<T*>(value) != nullptr);
    return static_cast<T*>(value);
  }
};

template <typename T>
struct StaticDownCastHelper<T&> {
  template <typename F>
  inline static T& cast(F&& value) {
    return *StaticDownCastHelper<T*>::cast(&value);
  }
};

template <typename T>
struct StaticDownCastHelper<T&&> {
  template <typename F>
  inline static T&& cast(F&& value) {
    return std::forward<T>(*StaticDownCastHelper<T*>::cast(&value));
  }
};

template <typename T>
struct StaticDownCastHelper<std::unique_ptr<T>> {
  template <typename F>
  inline static std::unique_ptr<T> cast(std::unique_ptr<F>&& value) {
    return absl::WrapUnique(StaticDownCastHelper<T*>::cast(value.release()));
  }
};

// Default impl returns true if the types are the same.
template <typename T, typename U, typename = specialize>
struct RepresentableAsHelper {
  static constexpr bool check(const U&) {
    return std::is_same<absl::remove_cvref_t<T>, U>::value;
  }
};

// Convertible pointers always return true.
template <typename T, typename U>
struct RepresentableAsHelper<T*, U*,
                             specialize_ift<std::is_convertible<U*, T*>>> {
  static constexpr bool check(const U*) { return true; }
};

// Convertible references always return true.
template <typename T, typename U>
struct RepresentableAsHelper<T&, U,
                             specialize_ift<std::is_convertible<U&, T&>>> {
  static constexpr bool check(const U&) { return true; }
};

// Numeric types check boundaries.
template <typename T, typename U>
struct RepresentableAsHelper<T, U, specialize_ift<is_numeric<U>>> {
  static bool check(const U& value) {
    // Handle infinity and nan.
    if (std::numeric_limits<T>::has_infinity && !std::isfinite(value)) {
      return true;
    }
    // Explicitly handle signed to avoid implicit conversion issues.
    if (!std::numeric_limits<T>::is_signed && value < 0) {
      return false;
    }
    return value >= std::numeric_limits<T>::min() &&
           value <= std::numeric_limits<T>::max();
  }
};

/** Returns if the value is representable as the given type T. */
template <typename T, typename U>
bool representable_as(const U& value) {
  return RepresentableAsHelper<T, U>::check(value);
}

/** Converts a smart pointer to an absl::optional value. */
template <typename T, typename F>
absl::optional<T> copy_if(const F& value) {
  if (value) {
    return absl::optional<T>(absl::in_place, *value);
  }
  return absl::nullopt;
}

/** Converts a pointer to an absl::optional value. */
template <typename T>
absl::optional<T> copy_if(const T* value) {
  return copy_if<T, const T*>(value);
}

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google

#endif  // THIRD_PARTY_CEL_CPP_INTERNAL_DOWN_CAST_H_H_
