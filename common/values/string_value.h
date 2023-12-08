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

// IWYU pragma: private, include "common/value.h"
// IWYU pragma: friend "common/value.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_STRING_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_STRING_VALUE_H_

#include <cstddef>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/meta/type_traits.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "common/internal/shared_byte_string.h"
#include "common/type.h"
#include "common/value_kind.h"

namespace cel {

class StringValue;
class StringValueView;

// `StringValue` represents values of the primitive `string` type.
class StringValue final {
 public:
  using view_alternative_type = StringValueView;

  static constexpr ValueKind kKind = ValueKind::kString;

  explicit StringValue(absl::Cord value) noexcept : value_(std::move(value)) {}

  explicit StringValue(absl::string_view value) noexcept
      : value_(absl::Cord(value)) {}

  template <typename T, typename = std::enable_if_t<std::is_same_v<
                            absl::remove_cvref_t<T>, std::string>>>
  explicit StringValue(T&& data) : value_(absl::Cord(std::forward<T>(data))) {}

  // Clang exposes `__attribute__((enable_if))` which can be used to detect
  // compile time string constants. When available, we use this to avoid
  // unnecessary copying as `StringValue(absl::string_view)` makes a copy.
#if ABSL_HAVE_ATTRIBUTE(enable_if)
  template <size_t N>
  explicit StringValue(const char (&data)[N])
      __attribute__((enable_if(::cel::common_internal::IsStringLiteral(data),
                               "chosen when 'data' is a string literal")))
      : value_(absl::string_view(data)) {}
#endif

  explicit StringValue(StringValueView value) noexcept;

  StringValue() = default;
  StringValue(const StringValue&) = default;
  StringValue(StringValue&&) = default;
  StringValue& operator=(const StringValue&) = default;
  StringValue& operator=(StringValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  StringTypeView type() const { return StringTypeView(); }

  std::string DebugString() const;

  std::string NativeString() const { return value_.ToString(); }

  absl::string_view NativeString(
      std::string& scratch
          ABSL_ATTRIBUTE_LIFETIME_BOUND) const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return value_.ToString(scratch);
  }

  absl::Cord NativeCord() const { return value_.ToCord(); }

  template <typename Visitor>
  std::common_type_t<std::invoke_result_t<Visitor, absl::string_view>,
                     std::invoke_result_t<Visitor, const absl::Cord&>>
  NativeValue(Visitor&& visitor) const {
    return value_.Visit(std::forward<Visitor>(visitor));
  }

  void swap(StringValue& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

  template <typename H>
  friend H AbslHashValue(H state, const StringValue& string) {
    return H::combine(std::move(state), string.value_);
  }

  friend bool operator==(const StringValue& lhs, const StringValue& rhs) {
    return lhs.value_ == rhs.value_;
  }

  friend bool operator<(const StringValue& lhs, const StringValue& rhs) {
    return lhs.value_ < rhs.value_;
  }

 private:
  friend class StringValueView;

  common_internal::SharedByteString value_;
};

inline void swap(StringValue& lhs, StringValue& rhs) noexcept { lhs.swap(rhs); }

inline bool operator!=(const StringValue& lhs, const StringValue& rhs) {
  return !operator==(lhs, rhs);
}

inline std::ostream& operator<<(std::ostream& out, const StringValue& value) {
  return out << value.DebugString();
}

class StringValueView final {
 public:
  using alternative_type = StringValue;

  static constexpr ValueKind kKind = StringValue::kKind;

  explicit StringValueView(
      const absl::Cord& value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : value_(value) {}

  explicit StringValueView(absl::string_view value) noexcept : value_(value) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  StringValueView(
      const StringValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : value_(value.value_) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  StringValueView& operator=(
      const StringValue& value ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    value_ = value.value_;
    return *this;
  }

  StringValueView& operator=(StringValue&&) = delete;

  StringValueView() = default;
  StringValueView(const StringValueView&) = default;
  StringValueView(StringValueView&&) = default;
  StringValueView& operator=(const StringValueView&) = default;
  StringValueView& operator=(StringValueView&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  StringTypeView type() const { return StringTypeView(); }

  std::string DebugString() const;

  std::string NativeString() const { return value_.ToString(); }

  absl::string_view NativeString(
      std::string& scratch
          ABSL_ATTRIBUTE_LIFETIME_BOUND) const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return value_.ToString(scratch);
  }

  absl::Cord NativeCord() const { return value_.ToCord(); }

  template <typename Visitor>
  std::common_type_t<std::invoke_result_t<Visitor, absl::string_view>,
                     std::invoke_result_t<Visitor, const absl::Cord&>>
  NativeValue(Visitor&& visitor) const {
    return value_.Visit(std::forward<Visitor>(visitor));
  }

  void swap(StringValueView& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

  template <typename H>
  friend H AbslHashValue(H state, StringValueView string) {
    return H::combine(std::move(state), string.value_);
  }

  friend bool operator==(StringValueView lhs, StringValueView rhs) {
    return lhs.value_ == rhs.value_;
  }

  friend bool operator<(StringValueView lhs, StringValueView rhs) {
    return lhs.value_ < rhs.value_;
  }

 private:
  friend class StringValue;

  common_internal::SharedByteStringView value_;
};

inline void swap(StringValueView& lhs, StringValueView& rhs) noexcept {
  lhs.swap(rhs);
}

inline bool operator==(StringValueView lhs, absl::string_view rhs) {
  return lhs == StringValueView(rhs);
}

inline bool operator==(absl::string_view lhs, StringValueView rhs) {
  return StringValueView(lhs) == rhs;
}

inline bool operator==(StringValueView lhs, const absl::Cord& rhs) {
  return lhs == StringValueView(rhs);
}

inline bool operator==(const absl::Cord& lhs, StringValueView rhs) {
  return StringValueView(lhs) == rhs;
}

inline bool operator!=(StringValueView lhs, StringValueView rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator!=(StringValueView lhs, absl::string_view rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator!=(absl::string_view lhs, StringValueView rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator!=(StringValueView lhs, const absl::Cord& rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator!=(const absl::Cord& lhs, StringValueView rhs) {
  return !operator==(lhs, rhs);
}

inline bool operator<(StringValueView lhs, absl::string_view rhs) {
  return lhs < StringValueView(rhs);
}

inline bool operator<(absl::string_view lhs, StringValueView rhs) {
  return StringValueView(lhs) < rhs;
}

inline bool operator<(StringValueView lhs, const absl::Cord& rhs) {
  return lhs < StringValueView(rhs);
}

inline bool operator<(const absl::Cord& lhs, StringValueView rhs) {
  return StringValueView(lhs) < rhs;
}

inline std::ostream& operator<<(std::ostream& out, StringValueView value) {
  return out << value.DebugString();
}

inline StringValue::StringValue(StringValueView value) noexcept
    : value_(value.value_) {}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_STRING_VALUE_H_
