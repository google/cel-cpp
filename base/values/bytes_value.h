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

#ifndef THIRD_PARTY_CEL_CPP_BASE_VALUES_BYTES_VALUE_H_
#define THIRD_PARTY_CEL_CPP_BASE_VALUES_BYTES_VALUE_H_

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
#include "base/types/bytes_type.h"
#include "base/value.h"

namespace cel {

class MemoryManager;
class ValueFactory;

class BytesValue : public Value {
 public:
  static constexpr Kind kKind = BytesType::kKind;

  static Persistent<const BytesValue> Empty(ValueFactory& value_factory);

  // Concat concatenates the contents of two ByteValue, returning a new
  // ByteValue. The resulting ByteValue is not tied to the lifetime of either of
  // the input ByteValue.
  static absl::StatusOr<Persistent<const BytesValue>> Concat(
      ValueFactory& value_factory, const BytesValue& lhs,
      const BytesValue& rhs);

  static bool Is(const Value& value) { return value.kind() == kKind; }

  constexpr Kind kind() const { return kKind; }

  Persistent<const BytesType> type() const { return BytesType::Get(); }

  std::string DebugString() const;

  size_t size() const;

  bool empty() const;

  bool Equals(absl::string_view bytes) const;
  bool Equals(const absl::Cord& bytes) const;
  bool Equals(const BytesValue& bytes) const;

  int Compare(absl::string_view bytes) const;
  int Compare(const absl::Cord& bytes) const;
  int Compare(const BytesValue& bytes) const;

  std::string ToString() const;

  absl::Cord ToCord() const;

  void HashValue(absl::HashState state) const;

  bool Equals(const Value& other) const;

 private:
  friend class base_internal::PersistentValueHandle;
  friend class base_internal::InlinedCordBytesValue;
  friend class base_internal::InlinedStringViewBytesValue;
  friend class base_internal::StringBytesValue;
  friend base_internal::BytesValueRep interop_internal::GetBytesValueRep(
      const Persistent<const BytesValue>& value);

  BytesValue() = default;
  BytesValue(const BytesValue&) = default;
  BytesValue(BytesValue&&) = default;
  BytesValue& operator=(const BytesValue&) = default;
  BytesValue& operator=(BytesValue&&) = default;

  // Get the contents of this BytesValue as either absl::string_view or const
  // absl::Cord&.
  base_internal::BytesValueRep rep() const;
};

CEL_INTERNAL_VALUE_DECL(BytesValue);

namespace base_internal {

// Implementation of BytesValue that is stored inlined within a handle. Since
// absl::Cord is reference counted itself, this is more efficient than storing
// this on the heap.
class InlinedCordBytesValue final : public BytesValue,
                                    public base_internal::InlineData {
 private:
  friend class BytesValue;
  template <size_t Size, size_t Align>
  friend class AnyData;

  static constexpr uintptr_t kMetadata =
      base_internal::kStoredInline |
      (static_cast<uintptr_t>(kKind) << base_internal::kKindShift);

  explicit InlinedCordBytesValue(absl::Cord value)
      : base_internal::InlineData(kMetadata), value_(std::move(value)) {}

  InlinedCordBytesValue(const InlinedCordBytesValue&) = default;
  InlinedCordBytesValue(InlinedCordBytesValue&&) = default;
  InlinedCordBytesValue& operator=(const InlinedCordBytesValue&) = default;
  InlinedCordBytesValue& operator=(InlinedCordBytesValue&&) = default;

  absl::Cord value_;
};

// Implementation of BytesValue that is stored inlined within a handle. This
// class is inheritently unsafe and care should be taken when using it.
// Typically this should only be used for empty strings or data that is static
// and lives for the duration of a program.
class InlinedStringViewBytesValue final : public BytesValue,
                                          public base_internal::InlineData {
 private:
  friend class BytesValue;
  template <size_t Size, size_t Align>
  friend class AnyData;

  static constexpr uintptr_t kMetadata =
      base_internal::kStoredInline | base_internal::kTriviallyCopyable |
      base_internal::kTriviallyDestructible |
      (static_cast<uintptr_t>(kKind) << base_internal::kKindShift);

  explicit InlinedStringViewBytesValue(absl::string_view value)
      : base_internal::InlineData(kMetadata), value_(value) {}

  InlinedStringViewBytesValue(const InlinedStringViewBytesValue&) = default;
  InlinedStringViewBytesValue(InlinedStringViewBytesValue&&) = default;
  InlinedStringViewBytesValue& operator=(const InlinedStringViewBytesValue&) =
      default;
  InlinedStringViewBytesValue& operator=(InlinedStringViewBytesValue&&) =
      default;

  absl::string_view value_;
};

// Implementation of BytesValue that uses std::string and is allocated on the
// heap, potentially reference counted.
class StringBytesValue final : public BytesValue,
                               public base_internal::HeapData {
 private:
  friend class cel::MemoryManager;
  friend class BytesValue;

  explicit StringBytesValue(std::string value);

  std::string value_;
};

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_BYTES_VALUE_H_
