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

#include "base/value.h"

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/call_once.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/container/btree_set.h"
#include "absl/container/inlined_vector.h"
#include "absl/hash/hash.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "base/value_factory.h"
#include "internal/casts.h"
#include "internal/no_destructor.h"
#include "internal/status_macros.h"
#include "internal/strings.h"
#include "internal/time.h"
#include "internal/utf8.h"

namespace cel {

#define CEL_INTERNAL_VALUE_IMPL(name)   \
  template class Persistent<name>;      \
  template class Persistent<const name>
CEL_INTERNAL_VALUE_IMPL(Value);
CEL_INTERNAL_VALUE_IMPL(NullValue);
CEL_INTERNAL_VALUE_IMPL(ErrorValue);
CEL_INTERNAL_VALUE_IMPL(BoolValue);
CEL_INTERNAL_VALUE_IMPL(IntValue);
CEL_INTERNAL_VALUE_IMPL(UintValue);
CEL_INTERNAL_VALUE_IMPL(DoubleValue);
CEL_INTERNAL_VALUE_IMPL(BytesValue);
CEL_INTERNAL_VALUE_IMPL(StringValue);
CEL_INTERNAL_VALUE_IMPL(DurationValue);
CEL_INTERNAL_VALUE_IMPL(TimestampValue);
CEL_INTERNAL_VALUE_IMPL(EnumValue);
CEL_INTERNAL_VALUE_IMPL(StructValue);
CEL_INTERNAL_VALUE_IMPL(ListValue);
CEL_INTERNAL_VALUE_IMPL(MapValue);
CEL_INTERNAL_VALUE_IMPL(TypeValue);
#undef CEL_INTERNAL_VALUE_IMPL

namespace {

using base_internal::PersistentHandleFactory;

// Both are equivalent to std::construct_at implementation from C++20.
#define CEL_COPY_TO_IMPL(type, src, dest) \
  ::new (const_cast<void*>(               \
      static_cast<const volatile void*>(std::addressof(dest)))) type(src)
#define CEL_MOVE_TO_IMPL(type, src, dest)                     \
  ::new (const_cast<void*>(static_cast<const volatile void*>( \
      std::addressof(dest)))) type(std::move(src))

}  // namespace

std::pair<size_t, size_t> Value::SizeAndAlignment() const {
  // Currently most implementations of Value are not reference counted, so those
  // that are override this and those that do not inherit this. Using 0 here
  // will trigger runtime asserts in case of undefined behavior.
  return std::pair<size_t, size_t>(0, 0);
}

void Value::CopyTo(Value& address) const {}

void Value::MoveTo(Value& address) {}

Persistent<const NullValue> NullValue::Get(ValueFactory& value_factory) {
  return value_factory.GetNullValue();
}

Persistent<const Type> NullValue::type() const {
  return PersistentHandleFactory<const Type>::MakeUnmanaged<const NullType>(
      NullType::Get());
}

std::string NullValue::DebugString() const { return "null"; }

const NullValue& NullValue::Get() {
  static const internal::NoDestructor<NullValue> instance;
  return *instance;
}

void NullValue::CopyTo(Value& address) const {
  CEL_COPY_TO_IMPL(NullValue, *this, address);
}

void NullValue::MoveTo(Value& address) {
  CEL_MOVE_TO_IMPL(NullValue, *this, address);
}

bool NullValue::Equals(const Value& other) const {
  return kind() == other.kind();
}

void NullValue::HashValue(absl::HashState state) const {
  absl::HashState::combine(std::move(state), type(), 0);
}

namespace {

struct StatusPayload final {
  std::string key;
  absl::Cord value;
};

void StatusHashValue(absl::HashState state, const absl::Status& status) {
  // absl::Status::operator== compares `raw_code()`, `message()` and the
  // payloads.
  state = absl::HashState::combine(std::move(state), status.raw_code(),
                                   status.message());
  // In order to determistically hash, we need to put the payloads in sorted
  // order. There is no guarantee from `absl::Status` on the order of the
  // payloads returned from `absl::Status::ForEachPayload`.
  //
  // This should be the same inline size as
  // `absl::status_internal::StatusPayloads`.
  absl::InlinedVector<StatusPayload, 1> payloads;
  status.ForEachPayload([&](absl::string_view key, const absl::Cord& value) {
    payloads.push_back(StatusPayload{std::string(key), value});
  });
  std::stable_sort(
      payloads.begin(), payloads.end(),
      [](const StatusPayload& lhs, const StatusPayload& rhs) -> bool {
        return lhs.key < rhs.key;
      });
  for (const auto& payload : payloads) {
    state =
        absl::HashState::combine(std::move(state), payload.key, payload.value);
  }
}

}  // namespace

Persistent<const Type> ErrorValue::type() const {
  return PersistentHandleFactory<const Type>::MakeUnmanaged<const ErrorType>(
      ErrorType::Get());
}

std::string ErrorValue::DebugString() const { return value().ToString(); }

void ErrorValue::CopyTo(Value& address) const {
  CEL_COPY_TO_IMPL(ErrorValue, *this, address);
}

void ErrorValue::MoveTo(Value& address) {
  CEL_MOVE_TO_IMPL(ErrorValue, *this, address);
}

bool ErrorValue::Equals(const Value& other) const {
  return kind() == other.kind() &&
         value() == internal::down_cast<const ErrorValue&>(other).value();
}

void ErrorValue::HashValue(absl::HashState state) const {
  StatusHashValue(absl::HashState::combine(std::move(state), type()), value());
}

Persistent<const BoolValue> BoolValue::False(ValueFactory& value_factory) {
  return value_factory.CreateBoolValue(false);
}

Persistent<const BoolValue> BoolValue::True(ValueFactory& value_factory) {
  return value_factory.CreateBoolValue(true);
}

Persistent<const Type> BoolValue::type() const {
  return PersistentHandleFactory<const Type>::MakeUnmanaged<const BoolType>(
      BoolType::Get());
}

std::string BoolValue::DebugString() const {
  return value() ? "true" : "false";
}

void BoolValue::CopyTo(Value& address) const {
  CEL_COPY_TO_IMPL(BoolValue, *this, address);
}

void BoolValue::MoveTo(Value& address) {
  CEL_MOVE_TO_IMPL(BoolValue, *this, address);
}

bool BoolValue::Equals(const Value& other) const {
  return kind() == other.kind() &&
         value() == internal::down_cast<const BoolValue&>(other).value();
}

void BoolValue::HashValue(absl::HashState state) const {
  absl::HashState::combine(std::move(state), type(), value());
}

Persistent<const Type> IntValue::type() const {
  return PersistentHandleFactory<const Type>::MakeUnmanaged<const IntType>(
      IntType::Get());
}

std::string IntValue::DebugString() const { return absl::StrCat(value()); }

void IntValue::CopyTo(Value& address) const {
  CEL_COPY_TO_IMPL(IntValue, *this, address);
}

void IntValue::MoveTo(Value& address) {
  CEL_MOVE_TO_IMPL(IntValue, *this, address);
}

bool IntValue::Equals(const Value& other) const {
  return kind() == other.kind() &&
         value() == internal::down_cast<const IntValue&>(other).value();
}

void IntValue::HashValue(absl::HashState state) const {
  absl::HashState::combine(std::move(state), type(), value());
}

Persistent<const Type> UintValue::type() const {
  return PersistentHandleFactory<const Type>::MakeUnmanaged<const UintType>(
      UintType::Get());
}

std::string UintValue::DebugString() const {
  return absl::StrCat(value(), "u");
}

void UintValue::CopyTo(Value& address) const {
  CEL_COPY_TO_IMPL(UintValue, *this, address);
}

void UintValue::MoveTo(Value& address) {
  CEL_MOVE_TO_IMPL(UintValue, *this, address);
}

bool UintValue::Equals(const Value& other) const {
  return kind() == other.kind() &&
         value() == internal::down_cast<const UintValue&>(other).value();
}

void UintValue::HashValue(absl::HashState state) const {
  absl::HashState::combine(std::move(state), type(), value());
}

Persistent<const DoubleValue> DoubleValue::NaN(ValueFactory& value_factory) {
  return value_factory.CreateDoubleValue(
      std::numeric_limits<double>::quiet_NaN());
}

Persistent<const DoubleValue> DoubleValue::PositiveInfinity(
    ValueFactory& value_factory) {
  return value_factory.CreateDoubleValue(
      std::numeric_limits<double>::infinity());
}

Persistent<const DoubleValue> DoubleValue::NegativeInfinity(
    ValueFactory& value_factory) {
  return value_factory.CreateDoubleValue(
      -std::numeric_limits<double>::infinity());
}

Persistent<const Type> DoubleValue::type() const {
  return PersistentHandleFactory<const Type>::MakeUnmanaged<const DoubleType>(
      DoubleType::Get());
}

std::string DoubleValue::DebugString() const {
  if (std::isfinite(value())) {
    if (std::floor(value()) != value()) {
      // The double is not representable as a whole number, so use
      // absl::StrCat which will add decimal places.
      return absl::StrCat(value());
    }
    // absl::StrCat historically would represent 0.0 as 0, and we want the
    // decimal places so ZetaSQL correctly assumes the type as double
    // instead of int64_t.
    std::string stringified = absl::StrCat(value());
    if (!absl::StrContains(stringified, '.')) {
      absl::StrAppend(&stringified, ".0");
    } else {
      // absl::StrCat has a decimal now? Use it directly.
    }
    return stringified;
  }
  if (std::isnan(value())) {
    return "nan";
  }
  if (std::signbit(value())) {
    return "-infinity";
  }
  return "+infinity";
}

void DoubleValue::CopyTo(Value& address) const {
  CEL_COPY_TO_IMPL(DoubleValue, *this, address);
}

void DoubleValue::MoveTo(Value& address) {
  CEL_MOVE_TO_IMPL(DoubleValue, *this, address);
}

bool DoubleValue::Equals(const Value& other) const {
  return kind() == other.kind() &&
         value() == internal::down_cast<const DoubleValue&>(other).value();
}

void DoubleValue::HashValue(absl::HashState state) const {
  absl::HashState::combine(std::move(state), type(), value());
}

Persistent<const DurationValue> DurationValue::Zero(
    ValueFactory& value_factory) {
  // Should never fail, tests assert this.
  return value_factory.CreateDurationValue(absl::ZeroDuration()).value();
}

Persistent<const Type> DurationValue::type() const {
  return PersistentHandleFactory<const Type>::MakeUnmanaged<const DurationType>(
      DurationType::Get());
}

std::string DurationValue::DebugString() const {
  return internal::FormatDuration(value()).value();
}

void DurationValue::CopyTo(Value& address) const {
  CEL_COPY_TO_IMPL(DurationValue, *this, address);
}

void DurationValue::MoveTo(Value& address) {
  CEL_MOVE_TO_IMPL(DurationValue, *this, address);
}

bool DurationValue::Equals(const Value& other) const {
  return kind() == other.kind() &&
         value() == internal::down_cast<const DurationValue&>(other).value();
}

void DurationValue::HashValue(absl::HashState state) const {
  absl::HashState::combine(std::move(state), type(), value());
}

Persistent<const TimestampValue> TimestampValue::UnixEpoch(
    ValueFactory& value_factory) {
  // Should never fail, tests assert this.
  return value_factory.CreateTimestampValue(absl::UnixEpoch()).value();
}

Persistent<const Type> TimestampValue::type() const {
  return PersistentHandleFactory<const Type>::MakeUnmanaged<
      const TimestampType>(TimestampType::Get());
}

std::string TimestampValue::DebugString() const {
  return internal::FormatTimestamp(value()).value();
}

void TimestampValue::CopyTo(Value& address) const {
  CEL_COPY_TO_IMPL(TimestampValue, *this, address);
}

void TimestampValue::MoveTo(Value& address) {
  CEL_MOVE_TO_IMPL(TimestampValue, *this, address);
}

bool TimestampValue::Equals(const Value& other) const {
  return kind() == other.kind() &&
         value() == internal::down_cast<const TimestampValue&>(other).value();
}

void TimestampValue::HashValue(absl::HashState state) const {
  absl::HashState::combine(std::move(state), type(), value());
}

namespace {

struct BytesValueDebugStringVisitor final {
  std::string operator()(absl::string_view value) const {
    return internal::FormatBytesLiteral(value);
  }

  std::string operator()(const absl::Cord& value) const {
    return internal::FormatBytesLiteral(static_cast<std::string>(value));
  }
};

struct StringValueDebugStringVisitor final {
  std::string operator()(absl::string_view value) const {
    return internal::FormatStringLiteral(value);
  }

  std::string operator()(const absl::Cord& value) const {
    return internal::FormatStringLiteral(static_cast<std::string>(value));
  }
};

struct ToStringVisitor final {
  std::string operator()(absl::string_view value) const {
    return std::string(value);
  }

  std::string operator()(const absl::Cord& value) const {
    return static_cast<std::string>(value);
  }
};

struct BytesValueSizeVisitor final {
  size_t operator()(absl::string_view value) const { return value.size(); }

  size_t operator()(const absl::Cord& value) const { return value.size(); }
};

struct StringValueSizeVisitor final {
  size_t operator()(absl::string_view value) const {
    return internal::Utf8CodePointCount(value);
  }

  size_t operator()(const absl::Cord& value) const {
    return internal::Utf8CodePointCount(value);
  }
};

struct EmptyVisitor final {
  bool operator()(absl::string_view value) const { return value.empty(); }

  bool operator()(const absl::Cord& value) const { return value.empty(); }
};

bool EqualsImpl(absl::string_view lhs, absl::string_view rhs) {
  return lhs == rhs;
}

bool EqualsImpl(absl::string_view lhs, const absl::Cord& rhs) {
  return lhs == rhs;
}

bool EqualsImpl(const absl::Cord& lhs, absl::string_view rhs) {
  return lhs == rhs;
}

bool EqualsImpl(const absl::Cord& lhs, const absl::Cord& rhs) {
  return lhs == rhs;
}

int CompareImpl(absl::string_view lhs, absl::string_view rhs) {
  return lhs.compare(rhs);
}

int CompareImpl(absl::string_view lhs, const absl::Cord& rhs) {
  return -rhs.Compare(lhs);
}

int CompareImpl(const absl::Cord& lhs, absl::string_view rhs) {
  return lhs.Compare(rhs);
}

int CompareImpl(const absl::Cord& lhs, const absl::Cord& rhs) {
  return lhs.Compare(rhs);
}

template <typename T>
class EqualsVisitor final {
 public:
  explicit EqualsVisitor(const T& ref) : ref_(ref) {}

  bool operator()(absl::string_view value) const {
    return EqualsImpl(value, ref_);
  }

  bool operator()(const absl::Cord& value) const {
    return EqualsImpl(value, ref_);
  }

 private:
  const T& ref_;
};

template <>
class EqualsVisitor<BytesValue> final {
 public:
  explicit EqualsVisitor(const BytesValue& ref) : ref_(ref) {}

  bool operator()(absl::string_view value) const { return ref_.Equals(value); }

  bool operator()(const absl::Cord& value) const { return ref_.Equals(value); }

 private:
  const BytesValue& ref_;
};

template <>
class EqualsVisitor<StringValue> final {
 public:
  explicit EqualsVisitor(const StringValue& ref) : ref_(ref) {}

  bool operator()(absl::string_view value) const { return ref_.Equals(value); }

  bool operator()(const absl::Cord& value) const { return ref_.Equals(value); }

 private:
  const StringValue& ref_;
};

template <typename T>
class CompareVisitor final {
 public:
  explicit CompareVisitor(const T& ref) : ref_(ref) {}

  int operator()(absl::string_view value) const {
    return CompareImpl(value, ref_);
  }

  int operator()(const absl::Cord& value) const {
    return CompareImpl(value, ref_);
  }

 private:
  const T& ref_;
};

template <>
class CompareVisitor<BytesValue> final {
 public:
  explicit CompareVisitor(const BytesValue& ref) : ref_(ref) {}

  int operator()(const absl::Cord& value) const { return ref_.Compare(value); }

  int operator()(absl::string_view value) const { return ref_.Compare(value); }

 private:
  const BytesValue& ref_;
};

template <>
class CompareVisitor<StringValue> final {
 public:
  explicit CompareVisitor(const StringValue& ref) : ref_(ref) {}

  int operator()(const absl::Cord& value) const { return ref_.Compare(value); }

  int operator()(absl::string_view value) const { return ref_.Compare(value); }

 private:
  const StringValue& ref_;
};

class HashValueVisitor final {
 public:
  explicit HashValueVisitor(absl::HashState state) : state_(std::move(state)) {}

  void operator()(absl::string_view value) {
    absl::HashState::combine(std::move(state_), value);
  }

  void operator()(const absl::Cord& value) {
    absl::HashState::combine(std::move(state_), value);
  }

 private:
  absl::HashState state_;
};

template <typename T>
bool CanPerformZeroCopy(MemoryManager& memory_manager,
                        const Persistent<T>& handle) {
  return base_internal::IsManagedHandle(handle) &&
         std::addressof(memory_manager) ==
             std::addressof(base_internal::GetMemoryManager(handle));
}

}  // namespace

Persistent<const BytesValue> BytesValue::Empty(ValueFactory& value_factory) {
  return value_factory.GetBytesValue();
}

absl::StatusOr<Persistent<const BytesValue>> BytesValue::Concat(
    ValueFactory& value_factory, const Persistent<const BytesValue>& lhs,
    const Persistent<const BytesValue>& rhs) {
  absl::Cord cord;
  // We can only use the potential zero-copy path if the memory managers are
  // the same. Otherwise we need to escape the original memory manager scope.
  cord.Append(
      lhs->ToCord(CanPerformZeroCopy(value_factory.memory_manager(), lhs)));
  // We can only use the potential zero-copy path if the memory managers are
  // the same. Otherwise we need to escape the original memory manager scope.
  cord.Append(
      rhs->ToCord(CanPerformZeroCopy(value_factory.memory_manager(), rhs)));
  return value_factory.CreateBytesValue(std::move(cord));
}

Persistent<const Type> BytesValue::type() const {
  return PersistentHandleFactory<const Type>::MakeUnmanaged<const BytesType>(
      BytesType::Get());
}

size_t BytesValue::size() const {
  return absl::visit(BytesValueSizeVisitor{}, rep());
}

bool BytesValue::empty() const { return absl::visit(EmptyVisitor{}, rep()); }

bool BytesValue::Equals(absl::string_view bytes) const {
  return absl::visit(EqualsVisitor<absl::string_view>(bytes), rep());
}

bool BytesValue::Equals(const absl::Cord& bytes) const {
  return absl::visit(EqualsVisitor<absl::Cord>(bytes), rep());
}

bool BytesValue::Equals(const Persistent<const BytesValue>& bytes) const {
  return absl::visit(EqualsVisitor<BytesValue>(*this), bytes->rep());
}

int BytesValue::Compare(absl::string_view bytes) const {
  return absl::visit(CompareVisitor<absl::string_view>(bytes), rep());
}

int BytesValue::Compare(const absl::Cord& bytes) const {
  return absl::visit(CompareVisitor<absl::Cord>(bytes), rep());
}

int BytesValue::Compare(const Persistent<const BytesValue>& bytes) const {
  return absl::visit(CompareVisitor<BytesValue>(*this), bytes->rep());
}

std::string BytesValue::ToString() const {
  return absl::visit(ToStringVisitor{}, rep());
}

std::string BytesValue::DebugString() const {
  return absl::visit(BytesValueDebugStringVisitor{}, rep());
}

bool BytesValue::Equals(const Value& other) const {
  return kind() == other.kind() &&
         absl::visit(EqualsVisitor<BytesValue>(*this),
                     internal::down_cast<const BytesValue&>(other).rep());
}

void BytesValue::HashValue(absl::HashState state) const {
  absl::visit(
      HashValueVisitor(absl::HashState::combine(std::move(state), type())),
      rep());
}

Persistent<const StringValue> StringValue::Empty(ValueFactory& value_factory) {
  return value_factory.GetStringValue();
}

absl::StatusOr<Persistent<const StringValue>> StringValue::Concat(
    ValueFactory& value_factory, const Persistent<const StringValue>& lhs,
    const Persistent<const StringValue>& rhs) {
  absl::Cord cord;
  // We can only use the potential zero-copy path if the memory managers are
  // the same. Otherwise we need to escape the original memory manager scope.
  cord.Append(
      lhs->ToCord(CanPerformZeroCopy(value_factory.memory_manager(), lhs)));
  // We can only use the potential zero-copy path if the memory managers are
  // the same. Otherwise we need to escape the original memory manager scope.
  cord.Append(
      rhs->ToCord(CanPerformZeroCopy(value_factory.memory_manager(), rhs)));
  size_t size = 0;
  size_t lhs_size = lhs->size_.load(std::memory_order_relaxed);
  if (lhs_size != 0 && !lhs->empty()) {
    size_t rhs_size = rhs->size_.load(std::memory_order_relaxed);
    if (rhs_size != 0 && !rhs->empty()) {
      size = lhs_size + rhs_size;
    }
  }
  return value_factory.CreateStringValue(std::move(cord), size);
}

Persistent<const Type> StringValue::type() const {
  return PersistentHandleFactory<const Type>::MakeUnmanaged<const StringType>(
      StringType::Get());
}

size_t StringValue::size() const {
  // We lazily calculate the code point count in some circumstances. If the code
  // point count is 0 and the underlying rep is not empty we need to actually
  // calculate the size. It is okay if this is done by multiple threads
  // simultaneously, it is a benign race.
  size_t size = size_.load(std::memory_order_relaxed);
  if (size == 0 && !empty()) {
    size = absl::visit(StringValueSizeVisitor{}, rep());
    size_.store(size, std::memory_order_relaxed);
  }
  return size;
}

bool StringValue::empty() const { return absl::visit(EmptyVisitor{}, rep()); }

bool StringValue::Equals(absl::string_view string) const {
  return absl::visit(EqualsVisitor<absl::string_view>(string), rep());
}

bool StringValue::Equals(const absl::Cord& string) const {
  return absl::visit(EqualsVisitor<absl::Cord>(string), rep());
}

bool StringValue::Equals(const Persistent<const StringValue>& string) const {
  return absl::visit(EqualsVisitor<StringValue>(*this), string->rep());
}

int StringValue::Compare(absl::string_view string) const {
  return absl::visit(CompareVisitor<absl::string_view>(string), rep());
}

int StringValue::Compare(const absl::Cord& string) const {
  return absl::visit(CompareVisitor<absl::Cord>(string), rep());
}

int StringValue::Compare(const Persistent<const StringValue>& string) const {
  return absl::visit(CompareVisitor<StringValue>(*this), string->rep());
}

std::string StringValue::ToString() const {
  return absl::visit(ToStringVisitor{}, rep());
}

std::string StringValue::DebugString() const {
  return absl::visit(StringValueDebugStringVisitor{}, rep());
}

bool StringValue::Equals(const Value& other) const {
  return kind() == other.kind() &&
         absl::visit(EqualsVisitor<StringValue>(*this),
                     internal::down_cast<const StringValue&>(other).rep());
}

void StringValue::HashValue(absl::HashState state) const {
  absl::visit(
      HashValueVisitor(absl::HashState::combine(std::move(state), type())),
      rep());
}

struct EnumType::NewInstanceVisitor final {
  const Persistent<const EnumType>& enum_type;
  ValueFactory& value_factory;

  absl::StatusOr<Persistent<const EnumValue>> operator()(
      absl::string_view name) const {
    TypedEnumValueFactory factory(value_factory, enum_type);
    return enum_type->NewInstanceByName(factory, name);
  }

  absl::StatusOr<Persistent<const EnumValue>> operator()(int64_t number) const {
    TypedEnumValueFactory factory(value_factory, enum_type);
    return enum_type->NewInstanceByNumber(factory, number);
  }
};

absl::StatusOr<Persistent<const EnumValue>> EnumValue::New(
    const Persistent<const EnumType>& enum_type, ValueFactory& value_factory,
    EnumType::ConstantId id) {
  CEL_ASSIGN_OR_RETURN(
      auto enum_value,
      absl::visit(EnumType::NewInstanceVisitor{enum_type, value_factory},
                  id.data_));
  if (!enum_value->type_) {
    // In case somebody is caching, we avoid setting the type_ if it has already
    // been set, to avoid a race condition where one CPU sees a half written
    // pointer.
    const_cast<EnumValue&>(*enum_value).type_ = enum_type;
  }
  return enum_value;
}

bool EnumValue::Equals(const Value& other) const {
  return kind() == other.kind() && type() == other.type() &&
         number() == internal::down_cast<const EnumValue&>(other).number();
}

void EnumValue::HashValue(absl::HashState state) const {
  absl::HashState::combine(std::move(state), type(), number());
}

struct StructValue::SetFieldVisitor final {
  StructValue& struct_value;
  const Persistent<const Value>& value;

  absl::Status operator()(absl::string_view name) const {
    return struct_value.SetFieldByName(name, value);
  }

  absl::Status operator()(int64_t number) const {
    return struct_value.SetFieldByNumber(number, value);
  }
};

struct StructValue::GetFieldVisitor final {
  const StructValue& struct_value;
  ValueFactory& value_factory;

  absl::StatusOr<Persistent<const Value>> operator()(
      absl::string_view name) const {
    return struct_value.GetFieldByName(value_factory, name);
  }

  absl::StatusOr<Persistent<const Value>> operator()(int64_t number) const {
    return struct_value.GetFieldByNumber(value_factory, number);
  }
};

struct StructValue::HasFieldVisitor final {
  const StructValue& struct_value;

  absl::StatusOr<bool> operator()(absl::string_view name) const {
    return struct_value.HasFieldByName(name);
  }

  absl::StatusOr<bool> operator()(int64_t number) const {
    return struct_value.HasFieldByNumber(number);
  }
};

absl::StatusOr<Persistent<StructValue>> StructValue::New(
    const Persistent<const StructType>& struct_type,
    ValueFactory& value_factory) {
  TypedStructValueFactory factory(value_factory, struct_type);
  CEL_ASSIGN_OR_RETURN(auto struct_value, struct_type->NewInstance(factory));
  if (!struct_value->type_) {
    // In case somebody is caching, we avoid setting the type_ if it has already
    // been set, to avoid a race condition where one CPU sees a half written
    // pointer.
    const_cast<StructValue&>(*struct_value).type_ = struct_type;
  }
  return struct_value;
}

absl::Status StructValue::SetField(FieldId field,
                                   const Persistent<const Value>& value) {
  return absl::visit(SetFieldVisitor{*this, value}, field.data_);
}

absl::StatusOr<Persistent<const Value>> StructValue::GetField(
    ValueFactory& value_factory, FieldId field) const {
  return absl::visit(GetFieldVisitor{*this, value_factory}, field.data_);
}

absl::StatusOr<bool> StructValue::HasField(FieldId field) const {
  return absl::visit(HasFieldVisitor{*this}, field.data_);
}

Persistent<const Type> TypeValue::type() const {
  return PersistentHandleFactory<const Type>::MakeUnmanaged<const TypeType>(
      TypeType::Get());
}

std::string TypeValue::DebugString() const { return value()->DebugString(); }

void TypeValue::CopyTo(Value& address) const {
  CEL_COPY_TO_IMPL(TypeValue, *this, address);
}

void TypeValue::MoveTo(Value& address) {
  CEL_MOVE_TO_IMPL(TypeValue, *this, address);
}

bool TypeValue::Equals(const Value& other) const {
  return kind() == other.kind() &&
         value() == internal::down_cast<const TypeValue&>(other).value();
}

void TypeValue::HashValue(absl::HashState state) const {
  absl::HashState::combine(std::move(state), type(), value());
}

namespace base_internal {

absl::Cord InlinedCordBytesValue::ToCord(bool reference_counted) const {
  static_cast<void>(reference_counted);
  return value_;
}

void InlinedCordBytesValue::CopyTo(Value& address) const {
  CEL_COPY_TO_IMPL(InlinedCordBytesValue, *this, address);
}

void InlinedCordBytesValue::MoveTo(Value& address) {
  CEL_MOVE_TO_IMPL(InlinedCordBytesValue, *this, address);
}

typename InlinedCordBytesValue::Rep InlinedCordBytesValue::rep() const {
  return Rep(absl::in_place_type<std::reference_wrapper<const absl::Cord>>,
             std::cref(value_));
}

absl::Cord InlinedStringViewBytesValue::ToCord(bool reference_counted) const {
  static_cast<void>(reference_counted);
  return absl::Cord(value_);
}

void InlinedStringViewBytesValue::CopyTo(Value& address) const {
  CEL_COPY_TO_IMPL(InlinedStringViewBytesValue, *this, address);
}

void InlinedStringViewBytesValue::MoveTo(Value& address) {
  CEL_MOVE_TO_IMPL(InlinedStringViewBytesValue, *this, address);
}

typename InlinedStringViewBytesValue::Rep InlinedStringViewBytesValue::rep()
    const {
  return Rep(absl::in_place_type<absl::string_view>, value_);
}

std::pair<size_t, size_t> StringBytesValue::SizeAndAlignment() const {
  return std::make_pair(sizeof(StringBytesValue), alignof(StringBytesValue));
}

absl::Cord StringBytesValue::ToCord(bool reference_counted) const {
  if (reference_counted) {
    Ref();
    return absl::MakeCordFromExternal(absl::string_view(value_),
                                      [this]() { Unref(); });
  }
  return absl::Cord(value_);
}

typename StringBytesValue::Rep StringBytesValue::rep() const {
  return Rep(absl::in_place_type<absl::string_view>, absl::string_view(value_));
}

std::pair<size_t, size_t> ExternalDataBytesValue::SizeAndAlignment() const {
  return std::make_pair(sizeof(ExternalDataBytesValue),
                        alignof(ExternalDataBytesValue));
}

absl::Cord ExternalDataBytesValue::ToCord(bool reference_counted) const {
  if (reference_counted) {
    Ref();
    return absl::MakeCordFromExternal(
        absl::string_view(static_cast<const char*>(value_.data), value_.size),
        [this]() { Unref(); });
  }
  return absl::Cord(
      absl::string_view(static_cast<const char*>(value_.data), value_.size));
}

typename ExternalDataBytesValue::Rep ExternalDataBytesValue::rep() const {
  return Rep(
      absl::in_place_type<absl::string_view>,
      absl::string_view(static_cast<const char*>(value_.data), value_.size));
}

absl::Cord InlinedCordStringValue::ToCord(bool reference_counted) const {
  static_cast<void>(reference_counted);
  return value_;
}

void InlinedCordStringValue::CopyTo(Value& address) const {
  CEL_COPY_TO_IMPL(InlinedCordStringValue, *this, address);
}

void InlinedCordStringValue::MoveTo(Value& address) {
  CEL_MOVE_TO_IMPL(InlinedCordStringValue, *this, address);
}

typename InlinedCordStringValue::Rep InlinedCordStringValue::rep() const {
  return Rep(absl::in_place_type<std::reference_wrapper<const absl::Cord>>,
             std::cref(value_));
}

absl::Cord InlinedStringViewStringValue::ToCord(bool reference_counted) const {
  static_cast<void>(reference_counted);
  return absl::Cord(value_);
}

void InlinedStringViewStringValue::CopyTo(Value& address) const {
  CEL_COPY_TO_IMPL(InlinedStringViewStringValue, *this, address);
}

void InlinedStringViewStringValue::MoveTo(Value& address) {
  CEL_MOVE_TO_IMPL(InlinedStringViewStringValue, *this, address);
}

typename InlinedStringViewStringValue::Rep InlinedStringViewStringValue::rep()
    const {
  return Rep(absl::in_place_type<absl::string_view>, value_);
}

std::pair<size_t, size_t> StringStringValue::SizeAndAlignment() const {
  return std::make_pair(sizeof(StringStringValue), alignof(StringStringValue));
}

absl::Cord StringStringValue::ToCord(bool reference_counted) const {
  if (reference_counted) {
    Ref();
    return absl::MakeCordFromExternal(absl::string_view(value_),
                                      [this]() { Unref(); });
  }
  return absl::Cord(value_);
}

typename StringStringValue::Rep StringStringValue::rep() const {
  return Rep(absl::in_place_type<absl::string_view>, absl::string_view(value_));
}

std::pair<size_t, size_t> ExternalDataStringValue::SizeAndAlignment() const {
  return std::make_pair(sizeof(ExternalDataStringValue),
                        alignof(ExternalDataStringValue));
}

absl::Cord ExternalDataStringValue::ToCord(bool reference_counted) const {
  if (reference_counted) {
    Ref();
    return absl::MakeCordFromExternal(
        absl::string_view(static_cast<const char*>(value_.data), value_.size),
        [this]() { Unref(); });
  }
  return absl::Cord(
      absl::string_view(static_cast<const char*>(value_.data), value_.size));
}

typename ExternalDataStringValue::Rep ExternalDataStringValue::rep() const {
  return Rep(
      absl::in_place_type<absl::string_view>,
      absl::string_view(static_cast<const char*>(value_.data), value_.size));
}

}  // namespace base_internal

}  // namespace cel
