// Copyright 2024 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_ARENA_BYTES_H_
#define THIRD_PARTY_CEL_CPP_COMMON_ARENA_BYTES_H_

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <limits>
#include <string>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/casts.h"
#include "absl/base/macros.h"
#include "absl/base/nullability.h"
#include "absl/strings/string_view.h"

namespace cel {

class ArenaBytesPool;

// Bug in current Abseil LTS. Fixed in
// https://github.com/abseil/abseil-cpp/commit/fd7713cb9a97c49096211ff40de280b6cebbb21c
// which is not yet in an LTS.
#if defined(__clang__) && (!defined(__clang_major__) || __clang_major__ >= 13)
#define CEL_ATTRIBUTE_ARENA_BYTES_VIEW ABSL_ATTRIBUTE_VIEW
#else
#define CEL_ATTRIBUTE_ARENA_BYTES_VIEW
#endif

class CEL_ATTRIBUTE_ARENA_BYTES_VIEW ArenaBytes final {
 private:
  template <size_t N>
  static constexpr bool IsStringLiteral(const char (&string)[N]) {
    static_assert(N > 0);
    for (size_t i = 0; i < N - 1; ++i) {
      if (string[i] == '\0') {
        return false;
      }
    }
    return string[N - 1] == '\0';
  }

 public:
  using traits_type = std::char_traits<char>;
  using value_type = char;
  using pointer = char*;
  using const_pointer = const char*;
  using reference = char&;
  using const_reference = const char&;
  using const_iterator = const_pointer;
  using iterator = const_iterator;
  using const_reverse_iterator = std::reverse_iterator<const_iterator>;
  using reverse_iterator = const_reverse_iterator;
  using size_type = uint32_t;
  using difference_type = int32_t;

  using absl_internal_is_view = std::true_type;

  template <size_t N>
  static constexpr ArenaBytes Static(const char (&bytes)[N])
#if ABSL_HAVE_ATTRIBUTE(enable_if)
      __attribute__((enable_if(IsStringLiteral(bytes),
                               "chosen when 'bytes' is a string literal")))
#endif
  {
    static_assert(N > 0);
    static_assert(N - 1 <= std::numeric_limits<int32_t>::max());
    return ArenaBytes(bytes);
  }

  ArenaBytes() = default;
  ArenaBytes(const ArenaBytes&) = default;
  ArenaBytes& operator=(const ArenaBytes&) = default;

  constexpr size_type size() const { return size_; }

  constexpr bool empty() const { return size() == 0; }

  constexpr size_type max_size() const {
    return std::numeric_limits<int32_t>::max();
  }

  constexpr absl::Nonnull<const_pointer> data() const { return data_; }

  constexpr const_reference front() const {
    ABSL_ASSERT(!empty());
    return data()[0];
  }

  constexpr const_reference back() const {
    ABSL_ASSERT(!empty());
    return data()[size() - 1];
  }

  constexpr const_reference operator[](size_type index) const {
    ABSL_ASSERT(index < size());
    return data()[index];
  }

  constexpr void remove_prefix(size_type n) {
    ABSL_ASSERT(n <= size());
    data_ += n;
    size_ -= n;
  }

  constexpr void remove_suffix(size_type n) {
    ABSL_ASSERT(n <= size());
    size_ -= n;
  }

  constexpr const_iterator begin() const { return data(); }

  constexpr const_iterator cbegin() const { return begin(); }

  constexpr const_iterator end() const { return data() + size(); }

  constexpr const_iterator cend() const { return end(); }

  constexpr const_reverse_iterator rbegin() const {
    return std::make_reverse_iterator(end());
  }

  constexpr const_reverse_iterator crbegin() const { return rbegin(); }

  constexpr const_reverse_iterator rend() const {
    return std::make_reverse_iterator(begin());
  }

  constexpr const_reverse_iterator crend() const { return rend(); }

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr operator absl::string_view() const {
    return absl::string_view(data(), size());
  }

 private:
  friend class ArenaBytesPool;

  constexpr explicit ArenaBytes(absl::string_view value)
      : data_(value.data()), size_(static_cast<size_type>(value.size())) {
    ABSL_ASSERT(value.data() != nullptr);
    ABSL_ASSERT(value.size() <= max_size());
  }

  absl::Nonnull<const char*> data_ = "";
  size_type size_ = 0;
};

constexpr bool operator==(ArenaBytes lhs, ArenaBytes rhs) {
  return absl::implicit_cast<absl::string_view>(lhs) ==
         absl::implicit_cast<absl::string_view>(rhs);
}

constexpr bool operator==(ArenaBytes lhs, absl::string_view rhs) {
  return absl::implicit_cast<absl::string_view>(lhs) == rhs;
}

constexpr bool operator==(absl::string_view lhs, ArenaBytes rhs) {
  return lhs == absl::implicit_cast<absl::string_view>(rhs);
}

constexpr bool operator!=(ArenaBytes lhs, ArenaBytes rhs) {
  return absl::implicit_cast<absl::string_view>(lhs) !=
         absl::implicit_cast<absl::string_view>(rhs);
}

constexpr bool operator!=(ArenaBytes lhs, absl::string_view rhs) {
  return absl::implicit_cast<absl::string_view>(lhs) != rhs;
}

constexpr bool operator!=(absl::string_view lhs, ArenaBytes rhs) {
  return lhs != absl::implicit_cast<absl::string_view>(rhs);
}

constexpr bool operator<(ArenaBytes lhs, ArenaBytes rhs) {
  return absl::implicit_cast<absl::string_view>(lhs) <
         absl::implicit_cast<absl::string_view>(rhs);
}

constexpr bool operator<(ArenaBytes lhs, absl::string_view rhs) {
  return absl::implicit_cast<absl::string_view>(lhs) < rhs;
}

constexpr bool operator<(absl::string_view lhs, ArenaBytes rhs) {
  return lhs < absl::implicit_cast<absl::string_view>(rhs);
}

constexpr bool operator<=(ArenaBytes lhs, ArenaBytes rhs) {
  return absl::implicit_cast<absl::string_view>(lhs) <=
         absl::implicit_cast<absl::string_view>(rhs);
}

constexpr bool operator<=(ArenaBytes lhs, absl::string_view rhs) {
  return absl::implicit_cast<absl::string_view>(lhs) <= rhs;
}

constexpr bool operator<=(absl::string_view lhs, ArenaBytes rhs) {
  return lhs <= absl::implicit_cast<absl::string_view>(rhs);
}

constexpr bool operator>(ArenaBytes lhs, ArenaBytes rhs) {
  return absl::implicit_cast<absl::string_view>(lhs) >
         absl::implicit_cast<absl::string_view>(rhs);
}

constexpr bool operator>(ArenaBytes lhs, absl::string_view rhs) {
  return absl::implicit_cast<absl::string_view>(lhs) > rhs;
}

constexpr bool operator>(absl::string_view lhs, ArenaBytes rhs) {
  return lhs > absl::implicit_cast<absl::string_view>(rhs);
}

constexpr bool operator>=(ArenaBytes lhs, ArenaBytes rhs) {
  return absl::implicit_cast<absl::string_view>(lhs) >=
         absl::implicit_cast<absl::string_view>(rhs);
}

constexpr bool operator>=(ArenaBytes lhs, absl::string_view rhs) {
  return absl::implicit_cast<absl::string_view>(lhs) >= rhs;
}

constexpr bool operator>=(absl::string_view lhs, ArenaBytes rhs) {
  return lhs >= absl::implicit_cast<absl::string_view>(rhs);
}

template <typename H>
H AbslHashValue(H state, ArenaBytes arena_string) {
  return H::combine(std::move(state),
                    absl::implicit_cast<absl::string_view>(arena_string));
}

#undef CEL_ATTRIBUTE_ARENA_BYTES_VIEW

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_ARENA_BYTES_H_
