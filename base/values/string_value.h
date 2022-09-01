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

#include "absl/hash/hash.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "base/internal/data.h"
#include "base/kind.h"
#include "base/type.h"
#include "base/types/string_type.h"
#include "base/value.h"

namespace cel {

class MemoryManager;
class ValueFactory;

class StringValue : public Value {
 public:
  static constexpr Kind kKind = StringType::kKind;

  static Persistent<const StringValue> Empty(ValueFactory& value_factory);

  // Concat concatenates the contents of two ByteValue, returning a new
  // ByteValue. The resulting ByteValue is not tied to the lifetime of either of
  // the input ByteValue.
  static absl::StatusOr<Persistent<const StringValue>> Concat(
      ValueFactory& value_factory, const StringValue& lhs,
      const StringValue& rhs);

  static bool Is(const Value& value) { return value.kind() == kKind; }

  constexpr Kind kind() const { return kKind; }

  Persistent<const StringType> type() const { return StringType::Get(); }

  std::string DebugString() const;

  size_t size() const;

  bool empty() const;

  bool Equals(absl::string_view string) const;
  bool Equals(const absl::Cord& string) const;
  bool Equals(const StringValue& string) const;

  int Compare(absl::string_view string) const;
  int Compare(const absl::Cord& string) const;
  int Compare(const StringValue& string) const;

  std::string ToString() const;

  absl::Cord ToCord() const;

  void HashValue(absl::HashState state) const;

  bool Equals(const Value& other) const;

 private:
  friend class base_internal::PersistentValueHandle;
  friend class base_internal::InlinedCordStringValue;
  friend class base_internal::InlinedStringViewStringValue;
  friend class base_internal::StringStringValue;
  friend base_internal::StringValueRep interop_internal::GetStringValueRep(
      const Persistent<const StringValue>& value);

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

namespace base_internal {

// Implementation of StringValue that is stored inlined within a handle. Since
// absl::Cord is reference counted itself, this is more efficient than storing
// this on the heap.
class InlinedCordStringValue final : public StringValue,
                                     public base_internal::InlineData {
 private:
  friend class StringValue;
  friend class ValueFactory;
  template <size_t Size, size_t Align>
  friend class AnyData;

  static constexpr uintptr_t kMetadata =
      base_internal::kStoredInline |
      (static_cast<uintptr_t>(kKind) << base_internal::kKindShift);

  explicit InlinedCordStringValue(absl::Cord value)
      : base_internal::InlineData(kMetadata), value_(std::move(value)) {}

  InlinedCordStringValue(const InlinedCordStringValue&) = default;
  InlinedCordStringValue(InlinedCordStringValue&&) = default;
  InlinedCordStringValue& operator=(const InlinedCordStringValue&) = default;
  InlinedCordStringValue& operator=(InlinedCordStringValue&&) = default;

  absl::Cord value_;
};

// Implementation of StringValue that is stored inlined within a handle. This
// class is inheritently unsafe and care should be taken when using it.
// Typically this should only be used for empty strings or data that is static
// and lives for the duration of a program.
class InlinedStringViewStringValue final : public StringValue,
                                           public base_internal::InlineData {
 private:
  friend class StringValue;
  friend class ValueFactory;
  template <size_t Size, size_t Align>
  friend class AnyData;

  static constexpr uintptr_t kMetadata =
      base_internal::kStoredInline | base_internal::kTriviallyCopyable |
      base_internal::kTriviallyDestructible |
      (static_cast<uintptr_t>(kKind) << base_internal::kKindShift);

  explicit InlinedStringViewStringValue(absl::string_view value)
      : base_internal::InlineData(kMetadata), value_(value) {}

  InlinedStringViewStringValue(const InlinedStringViewStringValue&) = default;
  InlinedStringViewStringValue(InlinedStringViewStringValue&&) = default;
  InlinedStringViewStringValue& operator=(const InlinedStringViewStringValue&) =
      default;
  InlinedStringViewStringValue& operator=(InlinedStringViewStringValue&&) =
      default;

  absl::string_view value_;
};

// Implementation of StringValue that uses std::string and is allocated on the
// heap, potentially reference counted.
class StringStringValue final : public StringValue,
                                public base_internal::HeapData {
 private:
  friend class cel::MemoryManager;
  friend class StringValue;
  friend class ValueFactory;

  explicit StringStringValue(std::string value);

  std::string value_;
};

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_STRING_VALUE_H_
