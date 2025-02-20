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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_BYTES_VALUE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_BYTES_VALUE_H_

#include <cstddef>
#include <ostream>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/meta/type_traits.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/allocator.h"
#include "common/internal/arena_string.h"
#include "common/internal/shared_byte_string.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value_kind.h"
#include "common/values/values.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {

class Value;
class BytesValue;
class TypeManager;

namespace common_internal {
class TrivialValue;
}  // namespace common_internal

// `BytesValue` represents values of the primitive `bytes` type.
class BytesValue final : private common_internal::ValueMixin<BytesValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kBytes;

  explicit BytesValue(absl::Cord value) noexcept : value_(std::move(value)) {}

  explicit BytesValue(absl::string_view value) noexcept
      : value_(absl::Cord(value)) {}

  explicit BytesValue(common_internal::ArenaString value) noexcept
      : value_(value) {}

  explicit BytesValue(common_internal::SharedByteString value) noexcept
      : value_(std::move(value)) {}

  template <typename T, typename = std::enable_if_t<std::is_same_v<
                            absl::remove_cvref_t<T>, std::string>>>
  explicit BytesValue(T&& data) : value_(absl::Cord(std::forward<T>(data))) {}

  // Clang exposes `__attribute__((enable_if))` which can be used to detect
  // compile time string constants. When available, we use this to avoid
  // unnecessary copying as `BytesValue(absl::string_view)` makes a copy.
#if ABSL_HAVE_ATTRIBUTE(enable_if)
  template <size_t N>
  explicit BytesValue(const char (&data)[N])
      __attribute__((enable_if(::cel::common_internal::IsStringLiteral(data),
                               "chosen when 'data' is a string literal")))
      : value_(absl::string_view(data)) {}
#endif

  BytesValue(Allocator<> allocator, absl::string_view value)
      : value_(allocator, value) {}

  BytesValue(Allocator<> allocator, const absl::Cord& value)
      : value_(allocator, value) {}

  BytesValue(Borrower borrower, absl::string_view value)
      : value_(borrower, value) {}

  BytesValue(Borrower borrower, const absl::Cord& value)
      : value_(borrower, value) {}

  BytesValue() = default;
  BytesValue(const BytesValue&) = default;
  BytesValue(BytesValue&&) = default;
  BytesValue& operator=(const BytesValue&) = default;
  BytesValue& operator=(BytesValue&&) = default;

  constexpr ValueKind kind() const { return kKind; }

  absl::string_view GetTypeName() const { return BytesType::kName; }

  std::string DebugString() const;

  // See Value::SerializeTo().
  absl::Status SerializeTo(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Cord& value) const;

  // See Value::ConvertToJson().
  absl::Status ConvertToJson(
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Message*> json) const;

  absl::Status Equal(
      const Value& other,
      absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool,
      absl::Nonnull<google::protobuf::MessageFactory*> message_factory,
      absl::Nonnull<google::protobuf::Arena*> arena, absl::Nonnull<Value*> result) const;
  using ValueMixin::Equal;

  bool IsZeroValue() const {
    return NativeValue([](const auto& value) -> bool { return value.empty(); });
  }

  BytesValue Clone(Allocator<> allocator) const;

  ABSL_DEPRECATED("Use ToString()")
  std::string NativeString() const { return value_.ToString(); }

  ABSL_DEPRECATED("Use TryFlat()")
  absl::string_view NativeString(
      std::string& scratch
          ABSL_ATTRIBUTE_LIFETIME_BOUND) const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return value_.ToString(scratch);
  }

  ABSL_DEPRECATED("Use ToCord()")
  absl::Cord NativeCord() const { return value_.ToCord(); }

  template <typename Visitor>
  ABSL_DEPRECATED("Use TryFlat()")
  std::common_type_t<
      std::invoke_result_t<Visitor, absl::string_view>,
      std::invoke_result_t<Visitor, const absl::Cord&>> NativeValue(Visitor&&
                                                                        visitor)
      const {
    return value_.Visit(std::forward<Visitor>(visitor));
  }

  void swap(BytesValue& other) noexcept {
    using std::swap;
    swap(value_, other.value_);
  }

  size_t Size() const;

  bool IsEmpty() const;

  bool Equals(absl::string_view bytes) const;
  bool Equals(const absl::Cord& bytes) const;
  bool Equals(const BytesValue& bytes) const;

  int Compare(absl::string_view bytes) const;
  int Compare(const absl::Cord& bytes) const;
  int Compare(const BytesValue& bytes) const;

  absl::optional<absl::string_view> TryFlat() const
      ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return value_.TryFlat();
  }

  std::string ToString() const { return value_.ToString(); }

  void CopyToString(absl::Nonnull<std::string*> out) const {
    value_.CopyToString(out);
  }

  void AppendToString(absl::Nonnull<std::string*> out) const {
    value_.AppendToString(out);
  }

  absl::Cord ToCord() const { return value_.ToCord(); }

  void CopyToCord(absl::Nonnull<absl::Cord*> out) const {
    value_.CopyToCord(out);
  }

  void AppendToCord(absl::Nonnull<absl::Cord*> out) const {
    value_.AppendToCord(out);
  }

  friend bool operator<(const BytesValue& lhs, const BytesValue& rhs) {
    return lhs.value_ < rhs.value_;
  }

 private:
  friend class common_internal::TrivialValue;
  friend const common_internal::SharedByteString&
  common_internal::AsSharedByteString(const BytesValue& value);
  friend class common_internal::ValueMixin<BytesValue>;

  common_internal::SharedByteString value_;
};

inline void swap(BytesValue& lhs, BytesValue& rhs) noexcept { lhs.swap(rhs); }

inline std::ostream& operator<<(std::ostream& out, const BytesValue& value) {
  return out << value.DebugString();
}

inline bool operator==(const BytesValue& lhs, absl::string_view rhs) {
  return lhs.Equals(rhs);
}

inline bool operator==(absl::string_view lhs, const BytesValue& rhs) {
  return rhs == lhs;
}

inline bool operator!=(const BytesValue& lhs, absl::string_view rhs) {
  return !lhs.Equals(rhs);
}

inline bool operator!=(absl::string_view lhs, const BytesValue& rhs) {
  return rhs != lhs;
}

namespace common_internal {

inline const SharedByteString& AsSharedByteString(const BytesValue& value) {
  return value.value_;
}

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_BYTES_VALUE_H_
