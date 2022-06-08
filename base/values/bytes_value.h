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

class BytesValue : public Value {
 protected:
  using Rep = base_internal::BytesValueRep;

 public:
  static Persistent<const BytesValue> Empty(ValueFactory& value_factory);

  // Concat concatenates the contents of two ByteValue, returning a new
  // ByteValue. The resulting ByteValue is not tied to the lifetime of either of
  // the input ByteValue.
  static absl::StatusOr<Persistent<const BytesValue>> Concat(
      ValueFactory& value_factory, const Persistent<const BytesValue>& lhs,
      const Persistent<const BytesValue>& rhs);

  Persistent<const Type> type() const final;

  Kind kind() const final { return Kind::kBytes; }

  std::string DebugString() const final;

  size_t size() const;

  bool empty() const;

  bool Equals(absl::string_view bytes) const;
  bool Equals(const absl::Cord& bytes) const;
  bool Equals(const Persistent<const BytesValue>& bytes) const;

  int Compare(absl::string_view bytes) const;
  int Compare(const absl::Cord& bytes) const;
  int Compare(const Persistent<const BytesValue>& bytes) const;

  std::string ToString() const;

  absl::Cord ToCord() const {
    // Without the handle we cannot know if this is reference counted.
    return ToCord(/*reference_counted=*/false);
  }

 private:
  template <base_internal::HandleType H>
  friend class base_internal::ValueHandle;
  friend class base_internal::ValueHandleBase;
  friend class base_internal::InlinedCordBytesValue;
  friend class base_internal::InlinedStringViewBytesValue;
  friend class base_internal::StringBytesValue;
  friend class base_internal::ExternalDataBytesValue;
  friend base_internal::BytesValueRep interop_internal::GetBytesValueRep(
      const Persistent<const BytesValue>& value);

  // Called by base_internal::ValueHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Value& value) { return value.kind() == Kind::kBytes; }

  BytesValue() = default;
  BytesValue(const BytesValue&) = default;
  BytesValue(BytesValue&&) = default;

  // Get the contents of this BytesValue as absl::Cord. When reference_counted
  // is true, the implementation can potentially return an absl::Cord that wraps
  // the contents instead of copying.
  virtual absl::Cord ToCord(bool reference_counted) const = 0;

  // Get the contents of this BytesValue as either absl::string_view or const
  // absl::Cord&.
  virtual Rep rep() const = 0;

  // See comments for respective member functions on `Value`.
  bool Equals(const Value& other) const final;
  void HashValue(absl::HashState state) const final;
};

namespace base_internal {

// Implementation of BytesValue that is stored inlined within a handle. Since
// absl::Cord is reference counted itself, this is more efficient than storing
// this on the heap.
class InlinedCordBytesValue final : public BytesValue, public ResourceInlined {
 private:
  template <HandleType H>
  friend class ValueHandle;

  explicit InlinedCordBytesValue(absl::Cord value) : value_(std::move(value)) {}

  InlinedCordBytesValue() = delete;

  InlinedCordBytesValue(const InlinedCordBytesValue&) = default;
  InlinedCordBytesValue(InlinedCordBytesValue&&) = default;

  // See comments for respective member functions on `ByteValue` and `Value`.
  void CopyTo(Value& address) const override;
  void MoveTo(Value& address) override;
  absl::Cord ToCord(bool reference_counted) const override;
  Rep rep() const override;

  absl::Cord value_;
};

// Implementation of BytesValue that is stored inlined within a handle. This
// class is inheritently unsafe and care should be taken when using it.
// Typically this should only be used for empty strings or data that is static
// and lives for the duration of a program.
class InlinedStringViewBytesValue final : public BytesValue,
                                          public ResourceInlined {
 private:
  template <HandleType H>
  friend class ValueHandle;

  explicit InlinedStringViewBytesValue(absl::string_view value)
      : value_(value) {}

  InlinedStringViewBytesValue() = delete;

  InlinedStringViewBytesValue(const InlinedStringViewBytesValue&) = default;
  InlinedStringViewBytesValue(InlinedStringViewBytesValue&&) = default;

  // See comments for respective member functions on `ByteValue` and `Value`.
  void CopyTo(Value& address) const override;
  void MoveTo(Value& address) override;
  absl::Cord ToCord(bool reference_counted) const override;
  Rep rep() const override;

  absl::string_view value_;
};

// Implementation of BytesValue that uses std::string and is allocated on the
// heap, potentially reference counted.
class StringBytesValue final : public BytesValue {
 private:
  friend class cel::MemoryManager;

  explicit StringBytesValue(std::string value) : value_(std::move(value)) {}

  StringBytesValue() = delete;
  StringBytesValue(const StringBytesValue&) = delete;
  StringBytesValue(StringBytesValue&&) = delete;

  // See comments for respective member functions on `ByteValue` and `Value`.
  std::pair<size_t, size_t> SizeAndAlignment() const override;
  absl::Cord ToCord(bool reference_counted) const override;
  Rep rep() const override;

  std::string value_;
};

// Implementation of BytesValue that wraps a contiguous array of bytes and calls
// the releaser when it is no longer needed. It is stored on the heap and
// potentially reference counted.
class ExternalDataBytesValue final : public BytesValue {
 private:
  friend class cel::MemoryManager;

  explicit ExternalDataBytesValue(ExternalData value)
      : value_(std::move(value)) {}

  ExternalDataBytesValue() = delete;
  ExternalDataBytesValue(const ExternalDataBytesValue&) = delete;
  ExternalDataBytesValue(ExternalDataBytesValue&&) = delete;

  // See comments for respective member functions on `ByteValue` and `Value`.
  std::pair<size_t, size_t> SizeAndAlignment() const override;
  absl::Cord ToCord(bool reference_counted) const override;
  Rep rep() const override;

  ExternalData value_;
};

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_BYTES_VALUE_H_
