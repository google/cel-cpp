// Copyright 2021 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_OVERFLOW_H_
#define THIRD_PARTY_CEL_CPP_COMMON_OVERFLOW_H_

#include <cmath>
#include <cstdint>
#include <limits>

#include "absl/base/config.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "internal/time.h"

namespace cel::internal {

// Add two int64_t values together.
// If overflow is detected, return absl::nullopt, e.g.
//   int64_t_max + 1
inline absl::optional<int64_t> CheckedAdd(int64_t x, int64_t y) {
#if ABSL_HAVE_BUILTIN(__builtin_add_overflow)
  int64_t sum;
  if (__builtin_add_overflow(x, y, &sum)) {
    return absl::nullopt;
  }
  return sum;
#else
  if (y > int64_t{0} ? x <= std::numeric_limits<int64_t>::max() - y
                     : x >= std::numeric_limits<int64_t>::min() - y) {
    return x + y;
  }
  return absl::nullopt;
#endif
}

// Subtract two int64_t values from each other.
// If overflow is detected, return absl::nullopt. e.g.
//   int64_t_min - 1
inline absl::optional<int64_t> CheckedSub(int64_t x, int64_t y) {
#if ABSL_HAVE_BUILTIN(__builtin_sub_overflow)
  int64_t diff;
  if (__builtin_sub_overflow(x, y, &diff)) {
    return absl::nullopt;
  }
  return diff;
#else
  if (y < int64_t{0} ? x <= std::numeric_limits<int64_t>::max() + y
                     : x >= std::numeric_limits<int64_t>::min() + y) {
    return x - y;
  }
  return absl::nullopt;
#endif
}

// Negate an int64_t value.
// If overflow is detected, return absl::nullopt, e.g.
//   negate(int64_t_min)
inline absl::optional<int64_t> CheckedNegation(int64_t v) {
#if ABSL_HAVE_BUILTIN(__builtin_mul_overflow)
  int64_t prod;
  if (__builtin_mul_overflow(v, int64_t{-1}, &prod)) {
    return absl::nullopt;
  }
  return prod;
#else
  if (v != std::numeric_limits<int64_t>::min()) {
    return -v;
  }
  return absl::nullopt;
#endif
}

// Multiply two int64_t values together.
// If overflow is detected, return absl::nullopt. e.g.
//   2 * int64_t_max
inline absl::optional<int64_t> CheckedMul(int64_t x, int64_t y) {
#if ABSL_HAVE_BUILTIN(__builtin_mul_overflow)
  int64_t prod;
  if (__builtin_mul_overflow(x, y, &prod)) {
    return absl::nullopt;
  }
  return prod;
#else
  if (!((x == int64_t{-1} && y == std::numeric_limits<int64_t>::min()) ||
        (y == int64_t{-1} && x == std::numeric_limits<int64_t>::min()) ||
        (x > int64_t{0} && y > int64_t{0} &&
         x > std::numeric_limits<int64_t>::max() / y) ||
        (x < int64_t{0} && y < int64_t{0} &&
         x < std::numeric_limits<int64_t>::max() / y) ||
        // Avoid dividing std::numeric_limits<int64_t>::min() by -1, use
        // whichever value of x or y is positive as the divisor.
        (x > int64_t{0} && y < int64_t{0} &&
         y < std::numeric_limits<int64_t>::min() / x) ||
        (x < int64_t{0} && y > int64_t{0} &&
         x < std::numeric_limits<int64_t>::min() / y))) {
    return x * y;
  }
  return absl::nullopt;
#endif
}

// Divide one int64_t value into another.
// If overflow is detected, return absl::nullopt, e.g.
//   int64_t_min / -1
inline absl::optional<int64_t> CheckedDiv(int64_t x, int64_t y) {
  if (!((x == std::numeric_limits<int64_t>::min() && y == int64_t{-1}) ||
        y == int64_t{0})) {
    return x / y;
  }
  return absl::nullopt;
}

// Compute the modulus of x into y.
// If overflow is detected, return absl::nullopt, e.g.
//   int64_t_min % -1
inline absl::optional<int64_t> CheckedMod(int64_t x, int64_t y) {
  if (!((x == std::numeric_limits<int64_t>::min() && y == int64_t{-1}) ||
        y == int64_t{0})) {
    return x % y;
  }
  return absl::nullopt;
}

// Add two uint64_t values together.
// If overflow is detected, return absl::nullopt, e.g.
//   uint64_t_max + 1
inline absl::optional<uint64_t> CheckedAdd(uint64_t x, uint64_t y) {
#if ABSL_HAVE_BUILTIN(__builtin_add_overflow)
  uint64_t sum;
  if (__builtin_add_overflow(x, y, &sum)) {
    return absl::nullopt;
  }
  return sum;
#else
  if (x <= std::numeric_limits<uint64_t>::max() - y) {
    return x + y;
  }
  return absl::nullopt;
#endif
}

// Subtract two uint64_t values from each other.
// If overflow is detected, return absl::nullopt, e.g.
//   1 - uint64_t_max
inline absl::optional<uint64_t> CheckedSub(uint64_t x, uint64_t y) {
#if ABSL_HAVE_BUILTIN(__builtin_sub_overflow)
  uint64_t diff;
  if (__builtin_sub_overflow(x, y, &diff)) {
    return absl::nullopt;
  }
  return diff;
#else
  if (y <= x) {
    return x - y;
  }
  return absl::nullopt;
#endif
}

// Multiply two uint64_t values together.
// If overflow is detected, return absl::nullopt, e.g.
//   2 * uint64_t_max
inline absl::optional<uint64_t> CheckedMul(uint64_t x, uint64_t y) {
#if ABSL_HAVE_BUILTIN(__builtin_mul_overflow)
  uint64_t prod;
  if (__builtin_mul_overflow(x, y, &prod)) {
    return absl::nullopt;
  }
  return prod;
#else
  if (y == uint64_t{0} || x <= std::numeric_limits<uint64_t>::max() / y) {
    return x * y;
  }
  return absl::nullopt;
#endif
}

// Divide one uint64_t value into another.
inline absl::optional<uint64_t> CheckedDiv(uint64_t x, uint64_t y) {
  if (y != uint64_t{0}) {
    return x / y;
  }
  return absl::nullopt;
}

// Compute the modulus of x into y.
// If 'y' is zero, the function will return an
// absl::StatusCode::kInvalidArgumentError, e.g. 1 / 0.
inline absl::optional<uint64_t> CheckedMod(uint64_t x, uint64_t y) {
  if (y != uint64_t{0}) {
    return x % y;
  }
  return absl::nullopt;
}

// Add two durations together.
// If overflow is detected, return absl::nullopt, e.g.
//   duration(int64_t_max, "ns") + duration(int64_t_max, "ns")
//
// Note, absl::Duration is effectively an int64_t under the covers, which means
// the same cases that would result in overflow for int64_t values would hold
// true for absl::Duration values.
inline absl::optional<absl::Duration> CheckedAdd(absl::Duration x,
                                                 absl::Duration y) {
  absl::Duration result = x + y;
  if (result < MinDuration() || result > MaxDuration()) {
    return absl::nullopt;
  }
  return result;
}

// Subtract two durations from each other.
// If overflow is detected, return absl::nullopt, e.g.
//   duration(int64_t_min, "ns") - duration(1, "ns")
//
// Note, absl::Duration is effectively an int64_t under the covers, which means
// the same cases that would result in overflow for int64_t values would hold
// true for absl::Duration values.
inline absl::optional<absl::Duration> CheckedSub(absl::Duration x,
                                                 absl::Duration y) {
  absl::Duration result = x - y;
  if (result < MinDuration() || result > MaxDuration()) {
    return absl::nullopt;
  }
  return result;
}

// Negate a duration.
// If overflow is detected, return absl::nullopt, e.g.
//   negate(duration(int64_t_min, "ns")).
inline absl::optional<absl::Duration> CheckedNegation(absl::Duration v) {
  absl::Duration result = -v;
  if (result < MinDuration() || result > MaxDuration()) {
    return absl::nullopt;
  }
  return result;
}

// Add an absl::Time and absl::Duration value together.
// If overflow is detected, return absl::nullopt, e.g.
//   timestamp(unix_epoch_max) + duration(1, "ns")
//
// Valid time values must be between `0001-01-01T00:00:00Z` (-62135596800s) and
// `9999-12-31T23:59:59.999999999Z` (253402300799s).
inline absl::optional<absl::Time> CheckedAdd(absl::Time t, absl::Duration d) {
  absl::Time result = t + d;
  if (result < MinTimestamp() || result > MaxTimestamp()) {
    return absl::nullopt;
  }
  return result;
}

// Subtract an absl::Time and absl::Duration value together.
// If overflow is detected, return absl::nullopt, e.g.
//   timestamp(unix_epoch_min) - duration(1, "ns")
//
// Valid time values must be between `0001-01-01T00:00:00Z` (-62135596800s) and
// `9999-12-31T23:59:59.999999999Z` (253402300799s).
inline absl::optional<absl::Time> CheckedSub(absl::Time t, absl::Duration d) {
  absl::Time result = t - d;
  if (result < MinTimestamp() || result > MaxTimestamp()) {
    return absl::nullopt;
  }
  return result;
}

// Subtract two absl::Time values from each other to produce an absl::Duration.
// If overflow is detected, return absl::nullopt, e.g.
//   timestamp(unix_epoch_min) - timestamp(unix_epoch_max)
inline absl::optional<absl::Duration> CheckedSub(absl::Time t1, absl::Time t2) {
  absl::Duration result = t1 - t2;
  if (result < MinDuration() || result > MaxDuration()) {
    return absl::nullopt;
  }
  return result;
}

// Convert a double value to an int64_t if possible.
// If the double exceeds the values representable in an int64_t the function
// will return absl::nullopt.
//
// Only finite double values may be converted to an int64_t. CEL may also reject
// some conversions if the value falls into a range where overflow would be
// ambiguous.
//
// The behavior of the static_cast<int64_t_t>(double) assembly instruction on
// x86 (cvttsd2si) can be manipulated by the <cfenv> header:
// https://en.cppreference.com/w/cpp/numeric/fenv/feround. This means that the
// set of values which will result in a valid or invalid conversion are
// environment dependent and the implementation must err on the side of caution
// and reject possibly valid values which might be invalid based on environment
// settings.
inline absl::optional<int64_t> CheckedDoubleToInt64(double v) {
  if (std::isfinite(v) &&
      v > static_cast<double>(std::numeric_limits<int64_t>::min()) &&
      v < static_cast<double>(std::numeric_limits<int64_t>::max())) {
    return static_cast<int64_t>(v);
  }
  return absl::nullopt;
}

// Convert a double value to a uint64_t if possible.
// If the double exceeds the values representable in a uint64_t the function
// will return absl::nullopt.
//
// Only finite double values may be converted to a uint64_t. CEL may also reject
// some conversions if the value falls into a range where overflow would be
// ambiguous.
//
// The behavior of the static_cast<uint64_t_t>(double) assembly instruction on
// x86 (cvttsd2si) can be manipulated by the <cfenv> header:
// https://en.cppreference.com/w/cpp/numeric/fenv/feround. This means that the
// set of values which will result in a valid or invalid conversion are
// environment dependent and the implementation must err on the side of caution
// and reject possibly valid values which might be invalid based on environment
// settings.
inline absl::optional<uint64_t> CheckedDoubleToUint64(double v) {
  if (std::isfinite(v) && v >= 0.0 &&
      v < static_cast<double>(std::numeric_limits<uint64_t>::max())) {
    return static_cast<uint64_t>(v);
  }
  return absl::nullopt;
}

// Convert an int64_t value to a uint64_t value if possible.
// If the int64_t exceeds the values representable in a uint64_t the function
// will return absl::nullopt.
inline absl::optional<uint64_t> CheckedInt64ToUint64(int64_t v) {
  if (v < int64_t{0}) {
    return absl::nullopt;
  }
  return static_cast<uint64_t>(v);
}

// Convert an int64_t value to an int32_t value if possible.
// If the int64_t exceeds the values representable in an int32_t the function
// will return absl::nullopt.
inline absl::optional<int32_t> CheckedInt64ToInt32(int64_t v) {
  if (v < std::numeric_limits<int32_t>::min() ||
      v > std::numeric_limits<int32_t>::max()) {
    return absl::nullopt;
  }
  return static_cast<int32_t>(v);
}

// Convert a uint64_t value to an int64_t value if possible.
// If the uint64_t exceeds the values representable in an int64_t the function
// will return absl::nullopt.
inline absl::optional<int64_t> CheckedUint64ToInt64(uint64_t v) {
  if (v > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
    return absl::nullopt;
  }
  return static_cast<int64_t>(v);
}

// Convert a uint64_t value to a uint32_t value if possible.
// If the uint64_t exceeds the values representable in a uint32_t the function
// will return absl::nullopt.
inline absl::optional<uint32_t> CheckedUint64ToUint32(uint64_t v) {
  if (v > std::numeric_limits<uint32_t>::max()) {
    return absl::nullopt;
  }
  return static_cast<uint32_t>(v);
}

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_OVERFLOW_H_
