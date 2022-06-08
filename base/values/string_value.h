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

#include <cstddef>
#include <string>
#include <utility>

#include "absl/hash/hash.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "base/kind.h"
#include "base/type.h"
#include "base/value.h"

namespace cel {

class ValueFactory;

class StringValue : public Value {
 protected:
  using Rep = base_internal::StringValueRep;

 public:
  static Persistent<const StringValue> Empty(ValueFactory& value_factory);

  static absl::StatusOr<Persistent<const StringValue>> Concat(
      ValueFactory& value_factory, const Persistent<const StringValue>& lhs,
      const Persistent<const StringValue>& rhs);

  Persistent<const Type> type() const final;

  Kind kind() const final { return Kind::kString; }

  std::string DebugString() const final;

  size_t size() const;

  bool empty() const;

  bool Equals(absl::string_view string) const;
  bool Equals(const absl::Cord& string) const;
  bool Equals(const Persistent<const StringValue>& string) const;

  int Compare(absl::string_view string) const;
  int Compare(const absl::Cord& string) const;
  int Compare(const Persistent<const StringValue>& string) const;

  std::string ToString() const;

  absl::Cord ToCord() const {
    // Without the handle we cannot know if this is reference counted.
    return ToCord(/*reference_counted=*/false);
  }

 private:
  template <base_internal::HandleType H>
  friend class base_internal::ValueHandle;
  friend class base_internal::ValueHandleBase;
  friend class base_internal::InlinedCordStringValue;
  friend class base_internal::InlinedStringViewStringValue;
  friend class base_internal::StringStringValue;
  friend class base_internal::ExternalDataStringValue;
  friend base_internal::StringValueRep interop_internal::GetStringValueRep(
      const Persistent<const StringValue>& value);

  // Called by base_internal::ValueHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Value& value) { return value.kind() == Kind::kString; }

  explicit StringValue(size_t size) : size_(size) {}

  StringValue() = default;

  StringValue(const StringValue& other)
      : StringValue(other.size_.load(std::memory_order_relaxed)) {}

  StringValue(StringValue&& other)
      : StringValue(other.size_.exchange(0, std::memory_order_relaxed)) {}

  // Get the contents of this BytesValue as absl::Cord. When reference_counted
  // is true, the implementation can potentially return an absl::Cord that wraps
  // the contents instead of copying.
  virtual absl::Cord ToCord(bool reference_counted) const = 0;

  // Get the contents of this StringValue as either absl::string_view or const
  // absl::Cord&.
  virtual Rep rep() const = 0;

  // See comments for respective member functions on `Value`.
  bool Equals(const Value& other) const final;
  void HashValue(absl::HashState state) const final;

  // Lazily cached code point count.
  mutable std::atomic<size_t> size_ = 0;
};

namespace base_internal {

// Implementation of StringValue that is stored inlined within a handle. Since
// absl::Cord is reference counted itself, this is more efficient then storing
// this on the heap.
class InlinedCordStringValue final : public StringValue,
                                     public ResourceInlined {
 private:
  template <HandleType H>
  friend class ValueHandle;

  explicit InlinedCordStringValue(absl::Cord value)
      : InlinedCordStringValue(0, std::move(value)) {}

  InlinedCordStringValue(size_t size, absl::Cord value)
      : StringValue(size), value_(std::move(value)) {}

  InlinedCordStringValue() = delete;

  InlinedCordStringValue(const InlinedCordStringValue&) = default;
  InlinedCordStringValue(InlinedCordStringValue&&) = default;

  // See comments for respective member functions on `StringValue` and `Value`.
  void CopyTo(Value& address) const override;
  void MoveTo(Value& address) override;
  absl::Cord ToCord(bool reference_counted) const override;
  Rep rep() const override;

  absl::Cord value_;
};

// Implementation of StringValue that is stored inlined within a handle. This
// class is inheritently unsafe and care should be taken when using it.
// Typically this should only be used for empty strings or data that is static
// and lives for the duration of a program.
class InlinedStringViewStringValue final : public StringValue,
                                           public ResourceInlined {
 private:
  template <HandleType H>
  friend class ValueHandle;

  explicit InlinedStringViewStringValue(absl::string_view value)
      : InlinedStringViewStringValue(0, value) {}

  InlinedStringViewStringValue(size_t size, absl::string_view value)
      : StringValue(size), value_(value) {}

  InlinedStringViewStringValue() = delete;

  InlinedStringViewStringValue(const InlinedStringViewStringValue&) = default;
  InlinedStringViewStringValue(InlinedStringViewStringValue&&) = default;

  // See comments for respective member functions on `StringValue` and `Value`.
  void CopyTo(Value& address) const override;
  void MoveTo(Value& address) override;
  absl::Cord ToCord(bool reference_counted) const override;
  Rep rep() const override;

  absl::string_view value_;
};

// Implementation of StringValue that uses std::string and is allocated on the
// heap, potentially reference counted.
class StringStringValue final : public StringValue {
 private:
  friend class cel::MemoryManager;

  explicit StringStringValue(std::string value)
      : StringStringValue(0, std::move(value)) {}

  StringStringValue(size_t size, std::string value)
      : StringValue(size), value_(std::move(value)) {}

  StringStringValue() = delete;
  StringStringValue(const StringStringValue&) = delete;
  StringStringValue(StringStringValue&&) = delete;

  // See comments for respective member functions on `StringValue` and `Value`.
  std::pair<size_t, size_t> SizeAndAlignment() const override;
  absl::Cord ToCord(bool reference_counted) const override;
  Rep rep() const override;

  std::string value_;
};

// Implementation of StringValue that wraps a contiguous array of bytes and
// calls the releaser when it is no longer needed. It is stored on the heap and
// potentially reference counted.
class ExternalDataStringValue final : public StringValue {
 private:
  friend class cel::MemoryManager;

  explicit ExternalDataStringValue(ExternalData value)
      : ExternalDataStringValue(0, std::move(value)) {}

  ExternalDataStringValue(size_t size, ExternalData value)
      : StringValue(size), value_(std::move(value)) {}

  ExternalDataStringValue() = delete;
  ExternalDataStringValue(const ExternalDataStringValue&) = delete;
  ExternalDataStringValue(ExternalDataStringValue&&) = delete;

  // See comments for respective member functions on `StringValue` and `Value`.
  std::pair<size_t, size_t> SizeAndAlignment() const override;
  absl::Cord ToCord(bool reference_counted) const override;
  Rep rep() const override;

  ExternalData value_;
};

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_STRING_VALUE_H_
