// Copyright 2022 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_BASE_VALUES_STRING_VALUE_H_
#define THIRD_PARTY_CEL_CPP_BASE_VALUES_STRING_VALUE_H_

#include <atomic>
#include <cstddef>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/hash/hash.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "base/internal/data.h"
#include "base/kind.h"
#include "base/type.h"
#include "base/types/string_type.h"
#include "base/value.h"

class RE2;

namespace cel {

class MemoryManager;
class ValueFactory;

class StringValue : public Value {
 public:
  static constexpr Kind kKind = StringType::kKind;

  static Handle<StringValue> Empty(ValueFactory& value_factory);

  // Concat concatenates the contents of two ByteValue, returning a new
  // ByteValue. The resulting ByteValue is not tied to the lifetime of either of
  // the input ByteValue.
  static absl::StatusOr<Handle<StringValue>> Concat(ValueFactory& value_factory,
                                                    const StringValue& lhs,
                                                    const StringValue& rhs);

  static bool Is(const Value& value) { return value.kind() == kKind; }

  ABSL_ATTRIBUTE_PURE_FUNCTION static std::string DebugString(
      absl::string_view value);

  ABSL_ATTRIBUTE_PURE_FUNCTION static std::string DebugString(
      const absl::Cord& value);

  constexpr Kind kind() const { return kKind; }

  Handle<StringType> type() const { return StringType::Get(); }

  std::string DebugString() const;

  size_t size() const;

  bool empty() const;

  bool Equals(absl::string_view string) const;
  bool Equals(const absl::Cord& string) const;
  bool Equals(const StringValue& string) const;

  int Compare(absl::string_view string) const;
  int Compare(const absl::Cord& string) const;
  int Compare(const StringValue& string) const;

  bool Matches(const RE2& re) const;

  std::string ToString() const;

  absl::Cord ToCord() const;

  void HashValue(absl::HashState state) const;

  bool Equals(const Value& other) const;

 private:
  friend class base_internal::ValueHandle;
  friend class base_internal::InlinedCordStringValue;
  friend class base_internal::InlinedStringViewStringValue;
  friend class base_internal::StringStringValue;
  friend base_internal::StringValueRep interop_internal::GetStringValueRep(
      const Handle<StringValue>& value);

  StringValue() = default;
  StringValue(const StringValue&) = default;
  StringValue(StringValue&&) = default;
  StringValue& operator=(const StringValue&) = default;
  StringValue& operator=(StringValue&&) = default;

  // Get the contents of this StringValue as either absl::string_view or const
  // absl::Cord&.
  base_internal::StringValueRep rep() const;
};

CEL_INTERNAL_VALUE_DECL(StringValue);

template <typename H>
H AbslHashValue(H state, const StringValue& value) {
  value.HashValue(absl::HashState::Create(&state));
  return state;
}

inline bool operator==(const StringValue& lhs, const StringValue& rhs) {
  return lhs.Equals(rhs);
}

namespace base_internal {

// Implementation of StringValue that is stored inlined within a handle. Since
// absl::Cord is reference counted itself, this is more efficient than storing
// this on the heap.
class InlinedCordStringValue final : public StringValue, public InlineData {
 private:
  friend class StringValue;
  friend class ValueFactory;
  template <size_t Size, size_t Align>
  friend class AnyData;

  static constexpr uintptr_t kMetadata =
      kStoredInline | AsInlineVariant(InlinedStringValueVariant::kCord) |
      (static_cast<uintptr_t>(kKind) << kKindShift);

  explicit InlinedCordStringValue(absl::Cord value)
      : InlineData(kMetadata), value_(std::move(value)) {}

  InlinedCordStringValue(const InlinedCordStringValue&) = default;
  InlinedCordStringValue(InlinedCordStringValue&&) = default;
  InlinedCordStringValue& operator=(const InlinedCordStringValue&) = default;
  InlinedCordStringValue& operator=(InlinedCordStringValue&&) = default;

  absl::Cord value_;
};

// Implementation of StringValue that is stored inlined within a handle. This
// class is inheritently unsafe and care should be taken when using it.
class InlinedStringViewStringValue final : public StringValue,
                                           public InlineData {
 private:
  friend class StringValue;
  template <size_t Size, size_t Align>
  friend class AnyData;

  static constexpr uintptr_t kMetadata =
      kStoredInline | (static_cast<uintptr_t>(kKind) << kKindShift);

  explicit InlinedStringViewStringValue(absl::string_view value)
      : InlinedStringViewStringValue(value, nullptr) {}

  // Constructs `InlinedStringViewStringValue` backed by `value` which is owned
  // by `owner`. `owner` may be nullptr, in which case `value` has no owner and
  // must live for the duration of the underlying `MemoryManager`.
  InlinedStringViewStringValue(absl::string_view value, const Value* owner)
      : InlinedStringViewStringValue(
            value, owner,
            owner == nullptr || !Metadata::IsArenaAllocated(*owner)) {}

  InlinedStringViewStringValue(absl::string_view value, const Value* owner,
                               bool trivial)
      : InlineData(kMetadata | (trivial ? kTrivial : uintptr_t{0}) |
                   AsInlineVariant(InlinedStringValueVariant::kStringView)),
        value_(value),
        owner_(trivial ? nullptr : owner) {
    if (owner_ != nullptr) {
      Metadata::Ref(*owner_);
    }
  }

  // Only called when owner_ was, at some point, not nullptr.
  InlinedStringViewStringValue(const InlinedStringViewStringValue& other)
      : InlineData(kMetadata |
                   AsInlineVariant(InlinedStringValueVariant::kStringView)),
        value_(other.value_),
        owner_(other.owner_) {
    if (owner_ != nullptr) {
      Metadata::Ref(*owner_);
    }
  }

  // Only called when owner_ was, at some point, not nullptr.
  InlinedStringViewStringValue(InlinedStringViewStringValue&& other)
      : InlineData(kMetadata |
                   AsInlineVariant(InlinedStringValueVariant::kStringView)),
        value_(other.value_),
        owner_(other.owner_) {
    other.value_ = absl::string_view();
    other.owner_ = nullptr;
  }

  // Only called when owner_ was, at some point, not nullptr.
  ~InlinedStringViewStringValue();

  // Only called when owner_ was, at some point, not nullptr.
  InlinedStringViewStringValue& operator=(
      const InlinedStringViewStringValue& other);

  // Only called when owner_ was, at some point, not nullptr.
  InlinedStringViewStringValue& operator=(InlinedStringViewStringValue&& other);

  absl::string_view value_;
  const Value* owner_;
};

// Implementation of StringValue that uses std::string and is allocated on the
// heap, potentially reference counted.
class StringStringValue final : public StringValue, public HeapData {
 private:
  friend class cel::MemoryManager;
  friend class StringValue;
  friend class ValueFactory;

  explicit StringStringValue(std::string value);

  std::string value_;
};

}  // namespace base_internal

namespace base_internal {

template <>
struct ValueTraits<StringValue> {
  using type = StringValue;

  using type_type = StringType;

  using underlying_type = void;

  static std::string DebugString(const type& value) {
    return value.DebugString();
  }

  static Handle<type> Wrap(ValueFactory& value_factory, Handle<type> value) {
    static_cast<void>(value_factory);
    return value;
  }

  static Handle<type> Unwrap(Handle<type> value) { return value; }
};

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_STRING_VALUE_H_
