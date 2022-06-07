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

#ifndef THIRD_PARTY_CEL_CPP_BASE_VALUE_H_
#define THIRD_PARTY_CEL_CPP_BASE_VALUE_H_

#include <atomic>
#include <cstdint>
#include <functional>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "base/handle.h"
#include "base/internal/value.pre.h"  // IWYU pragma: export
#include "base/kind.h"
#include "base/memory_manager.h"
#include "base/type.h"
#include "internal/casts.h"
#include "internal/rtti.h"

namespace cel {

class Value;
class NullValue;
class ErrorValue;
class BoolValue;
class IntValue;
class UintValue;
class DoubleValue;
class BytesValue;
class StringValue;
class DurationValue;
class TimestampValue;
class EnumValue;
class StructValue;
class ListValue;
class MapValue;
class TypeValue;
class ValueFactory;

namespace internal {
template <typename T>
class NoDestructor;
}

namespace interop_internal {
base_internal::StringValueRep GetStringValueRep(
    const Persistent<const StringValue>& value);
base_internal::BytesValueRep GetBytesValueRep(
    const Persistent<const BytesValue>& value);
}  // namespace interop_internal

// A representation of a CEL value that enables reflection and introspection of
// values.
class Value : public base_internal::Resource {
 public:
  // Returns the type of the value. If you only need the kind, prefer `kind()`.
  virtual Persistent<const Type> type() const = 0;

  // Returns the kind of the value. This is equivalent to `type().kind()` but
  // faster in many scenarios. As such it should be preffered when only the kind
  // is required.
  virtual Kind kind() const { return type()->kind(); }

  virtual std::string DebugString() const = 0;

  // Called by base_internal::ValueHandleBase.
  // Note GCC does not consider a friend member as a member of a friend.
  virtual bool Equals(const Value& other) const = 0;

  // Called by base_internal::ValueHandleBase.
  // Note GCC does not consider a friend member as a member of a friend.
  virtual void HashValue(absl::HashState state) const = 0;

 private:
  friend class NullValue;
  friend class ErrorValue;
  friend class BoolValue;
  friend class IntValue;
  friend class UintValue;
  friend class DoubleValue;
  friend class BytesValue;
  friend class StringValue;
  friend class DurationValue;
  friend class TimestampValue;
  friend class EnumValue;
  friend class StructValue;
  friend class ListValue;
  friend class MapValue;
  friend class TypeValue;
  friend class base_internal::ValueHandleBase;
  friend class base_internal::StringBytesValue;
  friend class base_internal::ExternalDataBytesValue;
  friend class base_internal::StringStringValue;
  friend class base_internal::ExternalDataStringValue;

  Value() = default;
  Value(const Value&) = default;
  Value(Value&&) = default;

  // Called by base_internal::ValueHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Value& value) { return true; }

  // For non-inlined values that are reference counted, this is the result of
  // `sizeof` and `alignof` for the most derived class.
  std::pair<size_t, size_t> SizeAndAlignment() const override;

  // Expose to some value implementations using friendship.
  using base_internal::Resource::Ref;
  using base_internal::Resource::Unref;

  // Called by base_internal::ValueHandleBase for inlined values.
  virtual void CopyTo(Value& address) const;

  // Called by base_internal::ValueHandleBase for inlined values.
  virtual void MoveTo(Value& address);
};

class NullValue final : public Value, public base_internal::ResourceInlined {
 public:
  static Persistent<const NullValue> Get(ValueFactory& value_factory);

  Persistent<const Type> type() const override;

  Kind kind() const override { return Kind::kNullType; }

  std::string DebugString() const override;

  // Note GCC does not consider a friend member as a member of a friend.
  ABSL_ATTRIBUTE_PURE_FUNCTION static const NullValue& Get();

  bool Equals(const Value& other) const override;

  void HashValue(absl::HashState state) const override;

 private:
  friend class ValueFactory;
  template <typename T>
  friend class internal::NoDestructor;
  template <base_internal::HandleType H>
  friend class base_internal::ValueHandle;
  friend class base_internal::ValueHandleBase;

  // Called by base_internal::ValueHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Value& value) { return value.kind() == Kind::kNullType; }

  NullValue() = default;
  NullValue(const NullValue&) = default;
  NullValue(NullValue&&) = default;

  // See comments for respective member functions on `Value`.
  void CopyTo(Value& address) const override;
  void MoveTo(Value& address) override;
};

class ErrorValue final : public Value, public base_internal::ResourceInlined {
 public:
  Persistent<const Type> type() const override;

  Kind kind() const override { return Kind::kError; }

  std::string DebugString() const override;

  const absl::Status& value() const { return value_; }

 private:
  template <base_internal::HandleType H>
  friend class base_internal::ValueHandle;
  friend class base_internal::ValueHandleBase;

  // Called by base_internal::ValueHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Value& value) { return value.kind() == Kind::kError; }

  // Called by `base_internal::ValueHandle` to construct value inline.
  explicit ErrorValue(absl::Status value) : value_(std::move(value)) {}

  ErrorValue() = delete;

  ErrorValue(const ErrorValue&) = default;
  ErrorValue(ErrorValue&&) = default;

  // See comments for respective member functions on `Value`.
  void CopyTo(Value& address) const override;
  void MoveTo(Value& address) override;
  bool Equals(const Value& other) const override;
  void HashValue(absl::HashState state) const override;

  absl::Status value_;
};

class BoolValue final : public Value, public base_internal::ResourceInlined {
 public:
  static Persistent<const BoolValue> False(ValueFactory& value_factory);

  static Persistent<const BoolValue> True(ValueFactory& value_factory);

  Persistent<const Type> type() const override;

  Kind kind() const override { return Kind::kBool; }

  std::string DebugString() const override;

  constexpr bool value() const { return value_; }

 private:
  template <base_internal::HandleType H>
  friend class base_internal::ValueHandle;
  friend class base_internal::ValueHandleBase;

  // Called by base_internal::ValueHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Value& value) { return value.kind() == Kind::kBool; }

  // Called by `base_internal::ValueHandle` to construct value inline.
  explicit BoolValue(bool value) : value_(value) {}

  BoolValue() = delete;

  BoolValue(const BoolValue&) = default;
  BoolValue(BoolValue&&) = default;

  // See comments for respective member functions on `Value`.
  void CopyTo(Value& address) const override;
  void MoveTo(Value& address) override;
  bool Equals(const Value& other) const override;
  void HashValue(absl::HashState state) const override;

  bool value_;
};

class IntValue final : public Value, public base_internal::ResourceInlined {
 public:
  Persistent<const Type> type() const override;

  Kind kind() const override { return Kind::kInt; }

  std::string DebugString() const override;

  constexpr int64_t value() const { return value_; }

 private:
  template <base_internal::HandleType H>
  friend class base_internal::ValueHandle;
  friend class base_internal::ValueHandleBase;

  // Called by base_internal::ValueHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Value& value) { return value.kind() == Kind::kInt; }

  // Called by `base_internal::ValueHandle` to construct value inline.
  explicit IntValue(int64_t value) : value_(value) {}

  IntValue() = delete;

  IntValue(const IntValue&) = default;
  IntValue(IntValue&&) = default;

  // See comments for respective member functions on `Value`.
  void CopyTo(Value& address) const override;
  void MoveTo(Value& address) override;
  bool Equals(const Value& other) const override;
  void HashValue(absl::HashState state) const override;

  int64_t value_;
};

class UintValue final : public Value, public base_internal::ResourceInlined {
 public:
  Persistent<const Type> type() const override;

  Kind kind() const override { return Kind::kUint; }

  std::string DebugString() const override;

  constexpr uint64_t value() const { return value_; }

 private:
  template <base_internal::HandleType H>
  friend class base_internal::ValueHandle;
  friend class base_internal::ValueHandleBase;

  // Called by base_internal::ValueHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Value& value) { return value.kind() == Kind::kUint; }

  // Called by `base_internal::ValueHandle` to construct value inline.
  explicit UintValue(uint64_t value) : value_(value) {}

  UintValue() = delete;

  UintValue(const UintValue&) = default;
  UintValue(UintValue&&) = default;

  // See comments for respective member functions on `Value`.
  void CopyTo(Value& address) const override;
  void MoveTo(Value& address) override;
  bool Equals(const Value& other) const override;
  void HashValue(absl::HashState state) const override;

  uint64_t value_;
};

class DoubleValue final : public Value, public base_internal::ResourceInlined {
 public:
  static Persistent<const DoubleValue> NaN(ValueFactory& value_factory);

  static Persistent<const DoubleValue> PositiveInfinity(
      ValueFactory& value_factory);

  static Persistent<const DoubleValue> NegativeInfinity(
      ValueFactory& value_factory);

  Persistent<const Type> type() const override;

  Kind kind() const override { return Kind::kDouble; }

  std::string DebugString() const override;

  constexpr double value() const { return value_; }

 private:
  template <base_internal::HandleType H>
  friend class base_internal::ValueHandle;
  friend class base_internal::ValueHandleBase;

  // Called by base_internal::ValueHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Value& value) { return value.kind() == Kind::kDouble; }

  // Called by `base_internal::ValueHandle` to construct value inline.
  explicit DoubleValue(double value) : value_(value) {}

  DoubleValue() = delete;

  DoubleValue(const DoubleValue&) = default;
  DoubleValue(DoubleValue&&) = default;

  // See comments for respective member functions on `Value`.
  void CopyTo(Value& address) const override;
  void MoveTo(Value& address) override;
  bool Equals(const Value& other) const override;
  void HashValue(absl::HashState state) const override;

  double value_;
};

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

class DurationValue final : public Value,
                            public base_internal::ResourceInlined {
 public:
  static Persistent<const DurationValue> Zero(ValueFactory& value_factory);

  Persistent<const Type> type() const override;

  Kind kind() const override { return Kind::kDuration; }

  std::string DebugString() const override;

  constexpr absl::Duration value() const { return value_; }

 private:
  template <base_internal::HandleType H>
  friend class base_internal::ValueHandle;
  friend class base_internal::ValueHandleBase;

  // Called by base_internal::ValueHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Value& value) { return value.kind() == Kind::kDuration; }

  // Called by `base_internal::ValueHandle` to construct value inline.
  explicit DurationValue(absl::Duration value) : value_(value) {}

  DurationValue() = delete;

  DurationValue(const DurationValue&) = default;
  DurationValue(DurationValue&&) = default;

  // See comments for respective member functions on `Value`.
  void CopyTo(Value& address) const override;
  void MoveTo(Value& address) override;
  bool Equals(const Value& other) const override;
  void HashValue(absl::HashState state) const override;

  absl::Duration value_;
};

class TimestampValue final : public Value,
                             public base_internal::ResourceInlined {
 public:
  static Persistent<const TimestampValue> UnixEpoch(
      ValueFactory& value_factory);

  Persistent<const Type> type() const override;

  Kind kind() const override { return Kind::kTimestamp; }

  std::string DebugString() const override;

  constexpr absl::Time value() const { return value_; }

 private:
  template <base_internal::HandleType H>
  friend class base_internal::ValueHandle;
  friend class base_internal::ValueHandleBase;

  // Called by base_internal::ValueHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Value& value) {
    return value.kind() == Kind::kTimestamp;
  }

  // Called by `base_internal::ValueHandle` to construct value inline.
  explicit TimestampValue(absl::Time value) : value_(value) {}

  TimestampValue() = delete;

  TimestampValue(const TimestampValue&) = default;
  TimestampValue(TimestampValue&&) = default;

  // See comments for respective member functions on `Value`.
  void CopyTo(Value& address) const override;
  void MoveTo(Value& address) override;
  bool Equals(const Value& other) const override;
  void HashValue(absl::HashState state) const override;

  absl::Time value_;
};

// EnumValue represents a single constant belonging to cel::EnumType.
class EnumValue : public Value {
 public:
  static absl::StatusOr<Persistent<const EnumValue>> New(
      const Persistent<const EnumType>& enum_type, ValueFactory& value_factory,
      EnumType::ConstantId id);

  Persistent<const Type> type() const final { return type_; }

  Kind kind() const final { return Kind::kEnum; }

  virtual int64_t number() const = 0;

  virtual absl::string_view name() const = 0;

 protected:
  explicit EnumValue(const Persistent<const EnumType>& type) : type_(type) {
    ABSL_ASSERT(type_);
  }

 private:
  friend internal::TypeInfo base_internal::GetEnumValueTypeId(
      const EnumValue& enum_value);
  template <base_internal::HandleType H>
  friend class base_internal::ValueHandle;
  friend class base_internal::ValueHandleBase;

  // Called by base_internal::ValueHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Value& value) { return value.kind() == Kind::kEnum; }

  EnumValue(const EnumValue&) = delete;
  EnumValue(EnumValue&&) = delete;

  bool Equals(const Value& other) const final;
  void HashValue(absl::HashState state) const final;

  std::pair<size_t, size_t> SizeAndAlignment() const override = 0;

  // Called by CEL_IMPLEMENT_ENUM_VALUE() and Is() to perform type checking.
  virtual internal::TypeInfo TypeId() const = 0;

  Persistent<const EnumType> type_;
};

// CEL_DECLARE_ENUM_VALUE declares `enum_value` as an enumeration value. It must
// be part of the class definition of `enum_value`.
//
// class MyEnumValue : public cel::EnumValue {
//  ...
// private:
//   CEL_DECLARE_ENUM_VALUE(MyEnumValue);
// };
#define CEL_DECLARE_ENUM_VALUE(enum_value) \
  CEL_INTERNAL_DECLARE_VALUE(Enum, enum_value)

// CEL_IMPLEMENT_ENUM_VALUE implements `enum_value` as an enumeration value. It
// must be called after the class definition of `enum_value`.
//
// class MyEnumValue : public cel::EnumValue {
//  ...
// private:
//   CEL_DECLARE_ENUM_VALUE(MyEnumValue);
// };
//
// CEL_IMPLEMENT_ENUM_VALUE(MyEnumValue);
#define CEL_IMPLEMENT_ENUM_VALUE(enum_value) \
  CEL_INTERNAL_IMPLEMENT_VALUE(Enum, enum_value)

// StructValue represents an instance of cel::StructType.
class StructValue : public Value {
 public:
  using FieldId = StructType::FieldId;

  static absl::StatusOr<Persistent<StructValue>> New(
      const Persistent<const StructType>& struct_type,
      ValueFactory& value_factory);

  Persistent<const Type> type() const final { return type_; }

  Kind kind() const final { return Kind::kStruct; }

  absl::Status SetField(FieldId field, const Persistent<const Value>& value);

  absl::StatusOr<Persistent<const Value>> GetField(ValueFactory& value_factory,
                                                   FieldId field) const;

  absl::StatusOr<bool> HasField(FieldId field) const;

 protected:
  explicit StructValue(const Persistent<const StructType>& type) : type_(type) {
    ABSL_ASSERT(type_);
  }

  virtual absl::Status SetFieldByName(absl::string_view name,
                                      const Persistent<const Value>& value) = 0;

  virtual absl::Status SetFieldByNumber(
      int64_t number, const Persistent<const Value>& value) = 0;

  virtual absl::StatusOr<Persistent<const Value>> GetFieldByName(
      ValueFactory& value_factory, absl::string_view name) const = 0;

  virtual absl::StatusOr<Persistent<const Value>> GetFieldByNumber(
      ValueFactory& value_factory, int64_t number) const = 0;

  virtual absl::StatusOr<bool> HasFieldByName(absl::string_view name) const = 0;

  virtual absl::StatusOr<bool> HasFieldByNumber(int64_t number) const = 0;

 private:
  struct SetFieldVisitor;
  struct GetFieldVisitor;
  struct HasFieldVisitor;

  friend struct SetFieldVisitor;
  friend struct GetFieldVisitor;
  friend struct HasFieldVisitor;
  friend internal::TypeInfo base_internal::GetStructValueTypeId(
      const StructValue& struct_value);
  template <base_internal::HandleType H>
  friend class base_internal::ValueHandle;
  friend class base_internal::ValueHandleBase;

  // Called by base_internal::ValueHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Value& value) { return value.kind() == Kind::kStruct; }

  StructValue(const StructValue&) = delete;
  StructValue(StructValue&&) = delete;

  bool Equals(const Value& other) const override = 0;
  void HashValue(absl::HashState state) const override = 0;

  std::pair<size_t, size_t> SizeAndAlignment() const override = 0;

  // Called by CEL_IMPLEMENT_STRUCT_VALUE() and Is() to perform type checking.
  virtual internal::TypeInfo TypeId() const = 0;

  Persistent<const StructType> type_;
};

// CEL_DECLARE_STRUCT_VALUE declares `struct_value` as an struct value. It must
// be part of the class definition of `struct_value`.
//
// class MyStructValue : public cel::StructValue {
//  ...
// private:
//   CEL_DECLARE_STRUCT_VALUE(MyStructValue);
// };
#define CEL_DECLARE_STRUCT_VALUE(struct_value) \
  CEL_INTERNAL_DECLARE_VALUE(Struct, struct_value)

// CEL_IMPLEMENT_STRUCT_VALUE implements `struct_value` as an struct
// value. It must be called after the class definition of `struct_value`.
//
// class MyStructValue : public cel::StructValue {
//  ...
// private:
//   CEL_DECLARE_STRUCT_VALUE(MyStructValue);
// };
//
// CEL_IMPLEMENT_STRUCT_VALUE(MyStructValue);
#define CEL_IMPLEMENT_STRUCT_VALUE(struct_value) \
  CEL_INTERNAL_IMPLEMENT_VALUE(Struct, struct_value)

// ListValue represents an instance of cel::ListType.
class ListValue : public Value {
 public:
  // TODO(issues/5): implement iterators so we can have cheap concated lists

  Persistent<const Type> type() const final { return type_; }

  Kind kind() const final { return Kind::kList; }

  virtual size_t size() const = 0;

  virtual bool empty() const { return size() == 0; }

  virtual absl::StatusOr<Persistent<const Value>> Get(
      ValueFactory& value_factory, size_t index) const = 0;

 protected:
  explicit ListValue(const Persistent<const ListType>& type) : type_(type) {}

 private:
  friend internal::TypeInfo base_internal::GetListValueTypeId(
      const ListValue& list_value);
  template <base_internal::HandleType H>
  friend class base_internal::ValueHandle;
  friend class base_internal::ValueHandleBase;

  // Called by base_internal::ValueHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Value& value) { return value.kind() == Kind::kList; }

  ListValue(const ListValue&) = delete;
  ListValue(ListValue&&) = delete;

  // TODO(issues/5): I do not like this, we should have these two take a
  // ValueFactory and return absl::StatusOr<bool> and absl::Status. We support
  // lazily created values, so errors can occur during equality testing.
  // Especially if there are different value implementations for the same type.
  bool Equals(const Value& other) const override = 0;
  void HashValue(absl::HashState state) const override = 0;

  std::pair<size_t, size_t> SizeAndAlignment() const override = 0;

  // Called by CEL_IMPLEMENT_LIST_VALUE() and Is() to perform type checking.
  virtual internal::TypeInfo TypeId() const = 0;

  const Persistent<const ListType> type_;
};

// CEL_DECLARE_LIST_VALUE declares `list_value` as an list value. It must
// be part of the class definition of `list_value`.
//
// class MyListValue : public cel::ListValue {
//  ...
// private:
//   CEL_DECLARE_LIST_VALUE(MyListValue);
// };
#define CEL_DECLARE_LIST_VALUE(list_value) \
  CEL_INTERNAL_DECLARE_VALUE(List, list_value)

// CEL_IMPLEMENT_LIST_VALUE implements `list_value` as an list
// value. It must be called after the class definition of `list_value`.
//
// class MyListValue : public cel::ListValue {
//  ...
// private:
//   CEL_DECLARE_LIST_VALUE(MyListValue);
// };
//
// CEL_IMPLEMENT_LIST_VALUE(MyListValue);
#define CEL_IMPLEMENT_LIST_VALUE(list_value) \
  CEL_INTERNAL_IMPLEMENT_VALUE(List, list_value)

// MapValue represents an instance of cel::MapType.
class MapValue : public Value {
 public:
  Persistent<const Type> type() const final { return type_; }

  Kind kind() const final { return Kind::kMap; }

  virtual size_t size() const = 0;

  virtual bool empty() const { return size() == 0; }

  virtual absl::StatusOr<Persistent<const Value>> Get(
      ValueFactory& value_factory,
      const Persistent<const Value>& key) const = 0;

  virtual absl::StatusOr<bool> Has(
      const Persistent<const Value>& key) const = 0;

 protected:
  explicit MapValue(const Persistent<const MapType>& type) : type_(type) {}

 private:
  friend internal::TypeInfo base_internal::GetMapValueTypeId(
      const MapValue& map_value);
  template <base_internal::HandleType H>
  friend class base_internal::ValueHandle;
  friend class base_internal::ValueHandleBase;

  // Called by base_internal::ValueHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Value& value) { return value.kind() == Kind::kMap; }

  MapValue(const MapValue&) = delete;
  MapValue(MapValue&&) = delete;

  bool Equals(const Value& other) const override = 0;
  void HashValue(absl::HashState state) const override = 0;

  std::pair<size_t, size_t> SizeAndAlignment() const override = 0;

  // Called by CEL_IMPLEMENT_MAP_VALUE() and Is() to perform type checking.
  virtual internal::TypeInfo TypeId() const = 0;

  // Set lazily, by EnumValue::New.
  Persistent<const MapType> type_;
};

// CEL_DECLARE_MAP_VALUE declares `map_value` as an map value. It must
// be part of the class definition of `map_value`.
//
// class MyMapValue : public cel::MapValue {
//  ...
// private:
//   CEL_DECLARE_MAP_VALUE(MyMapValue);
// };
#define CEL_DECLARE_MAP_VALUE(map_value) \
  CEL_INTERNAL_DECLARE_VALUE(Map, map_value)

// CEL_IMPLEMENT_MAP_VALUE implements `map_value` as an map
// value. It must be called after the class definition of `map_value`.
//
// class MyMapValue : public cel::MapValue {
//  ...
// private:
//   CEL_DECLARE_MAP_VALUE(MyMapValue);
// };
//
// CEL_IMPLEMENT_MAP_VALUE(MyMapValue);
#define CEL_IMPLEMENT_MAP_VALUE(map_value) \
  CEL_INTERNAL_IMPLEMENT_VALUE(Map, map_value)

// TypeValue represents an instance of cel::Type.
class TypeValue final : public Value, base_internal::ResourceInlined {
 public:
  Persistent<const Type> type() const override;

  Kind kind() const override { return Kind::kType; }

  std::string DebugString() const override;

  Persistent<const Type> value() const { return value_; }

 private:
  template <base_internal::HandleType H>
  friend class base_internal::ValueHandle;
  friend class base_internal::ValueHandleBase;

  // Called by base_internal::ValueHandleBase to implement Is for Transient and
  // Persistent.
  static bool Is(const Value& value) { return value.kind() == Kind::kType; }

  // Called by `base_internal::ValueHandle` to construct value inline.
  explicit TypeValue(Persistent<const Type> type) : value_(std::move(type)) {}

  TypeValue() = delete;

  TypeValue(const TypeValue&) = default;
  TypeValue(TypeValue&&) = default;

  // See comments for respective member functions on `Value`.
  void CopyTo(Value& address) const override;
  void MoveTo(Value& address) override;
  bool Equals(const Value& other) const override;
  void HashValue(absl::HashState state) const override;

  Persistent<const Type> value_;
};

}  // namespace cel

// value.pre.h forward declares types so they can be friended above. The types
// themselves need to be defined after everything else as they need to access or
// derive from the above types. We do this in value.post.h to avoid mudying this
// header and making it difficult to read.
#include "base/internal/value.post.h"  // IWYU pragma: export

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUE_H_
