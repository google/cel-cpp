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
#include <cstdint>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/hash/hash.h"
#include "absl/log/absl_check.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "base/handle.h"
#include "base/internal/data.h"
#include "base/kind.h"
#include "base/type.h"
#include "base/types/bytes_type.h"
#include "base/value.h"
#include "common/any.h"
#include "common/json.h"

namespace cel {

class MemoryManager;
class ValueFactory;
class StringValue;

class BytesValue : public Value,
                   public base_internal::EnableHandleFromThis<BytesValue> {
 public:
  static constexpr ValueKind kKind = ValueKind::kBytes;

  static Handle<BytesValue> Empty(ValueFactory& value_factory);

  // Concat concatenates the contents of two ByteValue, returning a new
  // ByteValue. The resulting ByteValue is not tied to the lifetime of either of
  // the input ByteValue.
  static absl::StatusOr<Handle<BytesValue>> Concat(ValueFactory& value_factory,
                                                   const BytesValue& lhs,
                                                   const BytesValue& rhs);

  static bool Is(const Value& value) { return value.kind() == kKind; }

  using Value::Is;

  static const BytesValue& Cast(const Value& value) {
    ABSL_DCHECK(Is(value)) << "cannot cast " << value.type()->name()
                           << " to bytes";
    return static_cast<const BytesValue&>(value);
  }

  ABSL_ATTRIBUTE_PURE_FUNCTION static std::string DebugString(
      absl::string_view value);

  ABSL_ATTRIBUTE_PURE_FUNCTION static std::string DebugString(
      const absl::Cord& value);

  constexpr ValueKind kind() const { return kKind; }

  Handle<BytesType> type() const { return BytesType::Get(); }

  std::string DebugString() const;

  absl::StatusOr<Any> ConvertToAny(ValueFactory&) const;

  absl::StatusOr<Json> ConvertToJson(ValueFactory&) const;

  absl::StatusOr<Handle<Value>> ConvertToType(ValueFactory& value_factory,
                                              const Handle<Type>& type) const;

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

  absl::StatusOr<Handle<Value>> Equals(ValueFactory& value_factory,
                                       const Value& other) const;

  // Visit the underlying value representation. It must accept `const
  // absl::Cord&` and `absl::string_view`.
  template <typename Visitor>
  auto Visit(Visitor&& visitor) const {
    return absl::visit(std::forward<Visitor>(visitor), rep());
  }

 private:
  friend class StringValue;
  friend class base_internal::ValueHandle;
  friend class base_internal::InlinedCordBytesValue;
  friend class base_internal::InlinedStringViewBytesValue;
  friend class base_internal::StringBytesValue;
  friend base_internal::BytesValueRep interop_internal::GetBytesValueRep(
      const Handle<BytesValue>& value);

  BytesValue() = default;
  BytesValue(const BytesValue&) = default;
  BytesValue(BytesValue&&) = default;
  BytesValue& operator=(const BytesValue&) = default;
  BytesValue& operator=(BytesValue&&) = default;

  // Get the contents of this BytesValue as either absl::string_view or const
  // absl::Cord&.
  base_internal::BytesValueRep rep() const;

  // Assuming the contained bytes are valid UTF-8, reinterpret this bytes value
  // as a string value as efficiently as possible.
  absl::StatusOr<Handle<StringValue>> AsString(
      ValueFactory& value_factory) const;
};

CEL_INTERNAL_VALUE_DECL(BytesValue);

namespace base_internal {

// Implementation of BytesValue that is stored inlined within a handle. Since
// absl::Cord is reference counted itself, this is more efficient than storing
// this on the heap.
class InlinedCordBytesValue final : public BytesValue, public InlineData {
 private:
  friend class BytesValue;
  friend class StringValue;
  template <size_t Size, size_t Align>
  friend struct AnyData;

  static constexpr uintptr_t kMetadata =
      kStoredInline | AsInlineVariant(InlinedBytesValueVariant::kCord) |
      (static_cast<uintptr_t>(kKind) << kKindShift);

  explicit InlinedCordBytesValue(absl::Cord value)
      : InlineData(kMetadata), value_(std::move(value)) {}

  InlinedCordBytesValue(const InlinedCordBytesValue&) = default;
  InlinedCordBytesValue(InlinedCordBytesValue&&) = default;
  InlinedCordBytesValue& operator=(const InlinedCordBytesValue&) = default;
  InlinedCordBytesValue& operator=(InlinedCordBytesValue&&) = default;

  absl::Cord value_;
};

// Implementation of BytesValue that is stored inlined within a handle. This
// class is inherently unsafe and care should be taken when using it.
class InlinedStringViewBytesValue final : public BytesValue, public InlineData {
 private:
  friend class BytesValue;
  friend class StringValue;
  template <size_t Size, size_t Align>
  friend struct AnyData;

  static constexpr uintptr_t kMetadata =
      kStoredInline | (static_cast<uintptr_t>(kKind) << kKindShift);

  explicit InlinedStringViewBytesValue(absl::string_view value)
      : InlinedStringViewBytesValue(value, nullptr) {}

  // Constructs `InlinedStringViewBytesValue` backed by `value` which is owned
  // by `owner`. `owner` may be nullptr, in which case `value` has no owner and
  // must live for the duration of the underlying `MemoryManager`.
  InlinedStringViewBytesValue(absl::string_view value, const cel::Value* owner)
      : InlinedStringViewBytesValue(value, owner, owner == nullptr) {}

  InlinedStringViewBytesValue(absl::string_view value, const cel::Value* owner,
                              bool trivial)
      : InlineData(kMetadata | (trivial ? kTrivial : uintptr_t{0}) |
                   AsInlineVariant(InlinedBytesValueVariant::kStringView)),
        value_(value),
        owner_(trivial ? nullptr : owner) {}

  // Only called when owner_ was, at some point, not nullptr.
  InlinedStringViewBytesValue(const InlinedStringViewBytesValue& other)
      : InlineData(kMetadata |
                   AsInlineVariant(InlinedBytesValueVariant::kStringView)),
        value_(other.value_),
        owner_(other.owner_) {
    if (owner_ != nullptr) {
      Metadata::Ref(*owner_);
    }
  }

  // Only called when owner_ was, at some point, not nullptr.
  InlinedStringViewBytesValue(InlinedStringViewBytesValue&& other)
      : InlineData(kMetadata |
                   AsInlineVariant(InlinedBytesValueVariant::kStringView)),
        value_(other.value_),
        owner_(other.owner_) {
    other.value_ = absl::string_view();
    other.owner_ = nullptr;
  }

  // Only called when owner_ was, at some point, not nullptr.
  ~InlinedStringViewBytesValue();

  // Only called when owner_ was, at some point, not nullptr.
  InlinedStringViewBytesValue& operator=(
      const InlinedStringViewBytesValue& other);

  // Only called when owner_ was, at some point, not nullptr.
  InlinedStringViewBytesValue& operator=(InlinedStringViewBytesValue&& other);

  absl::string_view value_;
  const cel::Value* owner_;
};

// Implementation of BytesValue that uses std::string and is allocated on the
// heap, potentially reference counted.
class StringBytesValue final : public BytesValue, public HeapData {
 private:
  friend class cel::MemoryManager;
  friend class BytesValue;
  friend class StringValue;

  explicit StringBytesValue(std::string value);

  std::string value_;
};

}  // namespace base_internal

namespace base_internal {

template <>
struct ValueTraits<BytesValue> {
  using type = BytesValue;

  using type_type = BytesType;

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

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_BYTES_VALUE_H_
