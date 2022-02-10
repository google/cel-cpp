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
#include <cmath>
#include <cstdlib>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/call_once.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/container/inlined_vector.h"
#include "absl/hash/hash.h"
#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "base/internal/value.h"
#include "internal/reference_counted.h"
#include "internal/status_macros.h"
#include "internal/strings.h"
#include "internal/time.h"

namespace cel {

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

// SimpleValues holds common values that are frequently needed and should not be
// constructed everytime they are required, usually because they would require a
// heap allocation. An example of this is an empty byte string.
struct SimpleValues final {
 public:
  SimpleValues() = default;

  SimpleValues(const SimpleValues&) = delete;

  SimpleValues(SimpleValues&&) = delete;

  SimpleValues& operator=(const SimpleValues&) = delete;

  SimpleValues& operator=(SimpleValues&&) = delete;

  Value empty_bytes;
};

ABSL_CONST_INIT absl::once_flag simple_values_once;
ABSL_CONST_INIT SimpleValues* simple_values = nullptr;

}  // namespace

Value Value::Error(const absl::Status& status) {
  ABSL_ASSERT(!status.ok());
  if (ABSL_PREDICT_FALSE(status.ok())) {
    return Value(absl::UnknownError(
        "If you are seeing this message the caller attempted to construct an "
        "error value from a successful status. Refusing to fail "
        "successfully."));
  }
  return Value(status);
}

absl::StatusOr<Value> Value::Duration(absl::Duration value) {
  CEL_RETURN_IF_ERROR(internal::ValidateDuration(value));
  int64_t seconds = absl::IDivDuration(value, absl::Seconds(1), &value);
  int64_t nanoseconds = absl::IDivDuration(value, absl::Nanoseconds(1), &value);
  return Value(Kind::kDuration, seconds,
               absl::bit_cast<uint32_t>(static_cast<int32_t>(nanoseconds)));
}

absl::StatusOr<Value> Value::Timestamp(absl::Time value) {
  CEL_RETURN_IF_ERROR(internal::ValidateTimestamp(value));
  absl::Duration duration = value - absl::UnixEpoch();
  int64_t seconds = absl::IDivDuration(duration, absl::Seconds(1), &duration);
  int64_t nanoseconds =
      absl::IDivDuration(duration, absl::Nanoseconds(1), &duration);
  return Value(Kind::kTimestamp, seconds,
               absl::bit_cast<uint32_t>(static_cast<int32_t>(nanoseconds)));
}

Value::Value(const Value& other) {
  // metadata_ is currently equal to the simple null type.
  // content_ is zero initialized.
  switch (other.kind()) {
    case Kind::kNullType:
      // `this` is already the null value, do nothing.
      return;
    case Kind::kBool:
      ABSL_FALLTHROUGH_INTENDED;
    case Kind::kInt:
      ABSL_FALLTHROUGH_INTENDED;
    case Kind::kUint:
      ABSL_FALLTHROUGH_INTENDED;
    case Kind::kDouble:
      ABSL_FALLTHROUGH_INTENDED;
    case Kind::kDuration:
      ABSL_FALLTHROUGH_INTENDED;
    case Kind::kTimestamp:
      // `other` is a simple value and simple type. We only need to trivially
      // copy metadata_ and content_.
      metadata_.CopyFrom(other.metadata_);
      content_.construct_trivial_value(other.content_.trivial_value());
      return;
    case Kind::kError:
      // `other` is an error value and a simple type. We need to trivially copy
      // metadata_ and copy construct the error value to content_.
      metadata_.CopyFrom(other.metadata_);
      content_.construct_error_value(other.content_.error_value());
      return;
    case Kind::kBytes:
      // `other` is a reffed value and a simple type. We need to trivially copy
      // metadata_ and copy construct the reffed value to content_.
      metadata_.CopyFrom(other.metadata_);
      content_.construct_reffed_value(other.content_.reffed_value());
      return;
    default:
      // TODO(issues/5): remove after implementing other kinds
      std::abort();
  }
}

Value::Value(Value&& other) {
  // metadata_ is currently equal to the simple null type.
  // content_ is currently zero initialized.
  switch (other.kind()) {
    case Kind::kNullType:
      // `this` and `other` are already the null value, do nothing.
      return;
    case Kind::kBool:
      ABSL_FALLTHROUGH_INTENDED;
    case Kind::kInt:
      ABSL_FALLTHROUGH_INTENDED;
    case Kind::kUint:
      ABSL_FALLTHROUGH_INTENDED;
    case Kind::kDouble:
      ABSL_FALLTHROUGH_INTENDED;
    case Kind::kDuration:
      ABSL_FALLTHROUGH_INTENDED;
    case Kind::kTimestamp:
      // `other` is a simple value and simple type. Trivially copy and then
      // clear metadata_ and content_, making `other` equivalent to `Value()` or
      // `Value::Null()`.
      metadata_.MoveFrom(std::move(other.metadata_));
      content_.construct_trivial_value(other.content_.trivial_value());
      other.content_.destruct_trivial_value();
      break;
    case Kind::kError:
      // `other` is an error value and simple type. Trivially copy and then
      // clear metadata_ and copy construct and then clear content_, making
      // `other` equivalent to `Value()` or `Value::Null()`.
      metadata_.MoveFrom(std::move(other.metadata_));
      content_.construct_error_value(other.content_.error_value());
      other.content_.destruct_error_value();
      break;
    case Kind::kBytes:
      // `other` is a reffed value and simple type. Trivially copy and then
      // clear metadata_ and trivially move content_, making
      // `other` equivalent to `Value()` or `Value::Null()`.
      metadata_.MoveFrom(std::move(other.metadata_));
      content_.adopt_reffed_value(other.content_.release_reffed_value());
      break;
    default:
      // TODO(issues/5): remove after implementing other kinds
      std::abort();
  }
}

Value::~Value() { Destruct(this); }

Value& Value::operator=(const Value& other) {
  if (ABSL_PREDICT_TRUE(this != std::addressof(other))) {
    switch (other.kind()) {
      case Kind::kNullType:
        ABSL_FALLTHROUGH_INTENDED;
      case Kind::kBool:
        ABSL_FALLTHROUGH_INTENDED;
      case Kind::kInt:
        ABSL_FALLTHROUGH_INTENDED;
      case Kind::kUint:
        ABSL_FALLTHROUGH_INTENDED;
      case Kind::kDouble:
        ABSL_FALLTHROUGH_INTENDED;
      case Kind::kDuration:
        ABSL_FALLTHROUGH_INTENDED;
      case Kind::kTimestamp:
        // `this` could be a simple value, an error value, or a reffed value.
        // First we destruct resetting `this` to `Value()`. Then we perform the
        // equivalent work of the copy constructor.
        Destruct(this);
        metadata_.CopyFrom(other.metadata_);
        content_.construct_trivial_value(other.content_.trivial_value());
        break;
      case Kind::kError:
        if (kind() == Kind::kError) {
          // `this` and `other` are error values. Perform a copy assignment
          // which is faster than destructing and copy constructing.
          content_.assign_error_value(other.content_.error_value());
        } else {
          // `this` could be a simple value or a reffed value. First we destruct
          // resetting `this` to `Value()`. Then we perform the equivalent work
          // of the copy constructor.
          Destruct(this);
          content_.construct_error_value(other.content_.error_value());
        }
        // Always copy metadata, for forward compatibility in case other bits
        // are added.
        metadata_.CopyFrom(other.metadata_);
        break;
      case Kind::kBytes: {
        // `this` could be a simple value, an error value, or a reffed value.
        // First we destruct resetting `this` to `Value()`. Then we perform the
        // equivalent work of the copy constructor.
        base_internal::BaseValue* reffed_value =
            internal::Ref(other.content_.reffed_value());
        Destruct(this);
        metadata_.CopyFrom(other.metadata_);
        // Adopt is typically used for moves, but in this case we already
        // increment the reference count, so it is equivalent to a move.
        content_.adopt_reffed_value(reffed_value);
      } break;
      default:
        // TODO(issues/5): remove after implementing other kinds
        std::abort();
    }
  }
  return *this;
}

Value& Value::operator=(Value&& other) {
  if (ABSL_PREDICT_TRUE(this != std::addressof(other))) {
    switch (other.kind()) {
      case Kind::kNullType:
        ABSL_FALLTHROUGH_INTENDED;
      case Kind::kBool:
        ABSL_FALLTHROUGH_INTENDED;
      case Kind::kInt:
        ABSL_FALLTHROUGH_INTENDED;
      case Kind::kUint:
        ABSL_FALLTHROUGH_INTENDED;
      case Kind::kDouble:
        ABSL_FALLTHROUGH_INTENDED;
      case Kind::kDuration:
        ABSL_FALLTHROUGH_INTENDED;
      case Kind::kTimestamp:
        // `this` could be a simple value, an error value, or a reffed value.
        // First we destruct resetting `this` to `Value()`. Then we perform the
        // equivalent work of the move constructor.
        Destruct(this);
        metadata_.MoveFrom(std::move(other.metadata_));
        content_.construct_trivial_value(other.content_.trivial_value());
        other.content_.destruct_trivial_value();
        break;
      case Kind::kError:
        if (kind() == Kind::kError) {
          // `this` and `other` are error values. Perform a copy assignment
          // which is faster than destructing and copy constructing. `other`
          // will be reset below.
          content_.assign_error_value(other.content_.error_value());
        } else {
          // `this` could be a simple value or a reffed value. First we destruct
          // resetting `this` to `Value()`. Then we perform the equivalent work
          // of the copy constructor.
          Destruct(this);
          content_.construct_error_value(other.content_.error_value());
        }
        // Always copy metadata, for forward compatibility in case other bits
        // are added.
        metadata_.CopyFrom(other.metadata_);
        // Reset `other` to `Value()`.
        Destruct(std::addressof(other));
        break;
      case Kind::kBytes:
        // `this` could be a simple value, an error value, or a reffed value.
        // First we destruct resetting `this` to `Value()`. Then we perform the
        // equivalent work of the move constructor.
        Destruct(this);
        metadata_.MoveFrom(std::move(other.metadata_));
        content_.adopt_reffed_value(other.content_.release_reffed_value());
        break;
      default:
        // TODO(issues/5): remove after implementing other kinds
        std::abort();
    }
  }
  return *this;
}

std::string Value::DebugString() const {
  switch (kind()) {
    case Kind::kNullType:
      return "null";
    case Kind::kBool:
      return AsBool() ? "true" : "false";
    case Kind::kInt:
      return absl::StrCat(AsInt());
    case Kind::kUint:
      return absl::StrCat(AsUint(), "u");
    case Kind::kDouble: {
      if (std::isfinite(AsDouble())) {
        if (static_cast<double>(static_cast<int64_t>(AsDouble())) !=
            AsDouble()) {
          // The double is not representable as a whole number, so use
          // absl::StrCat which will add decimal places.
          return absl::StrCat(AsDouble());
        }
        // absl::StrCat historically would represent 0.0 as 0, and we want the
        // decimal places so ZetaSQL correctly assumes the type as double
        // instead of int64_t.
        std::string stringified = absl::StrCat(AsDouble());
        if (!absl::StrContains(stringified, '.')) {
          absl::StrAppend(&stringified, ".0");
        } else {
          // absl::StrCat has a decimal now? Use it directly.
        }
        return stringified;
      }
      if (std::isnan(AsDouble())) {
        return "nan";
      }
      if (std::signbit(AsDouble())) {
        return "-infinity";
      }
      return "+infinity";
    }
    case Kind::kDuration:
      return internal::FormatDuration(AsDuration()).value();
    case Kind::kTimestamp:
      return internal::FormatTimestamp(AsTimestamp()).value();
    case Kind::kError:
      return AsError().ToString();
    case Kind::kBytes:
      return content_.reffed_value()->DebugString();
    default:
      // TODO(issues/5): remove after implementing other kinds
      std::abort();
  }
}

void Value::InitializeSingletons() {
  absl::call_once(simple_values_once, []() {
    ABSL_ASSERT(simple_values == nullptr);
    simple_values = new SimpleValues();
    simple_values->empty_bytes = Value(Kind::kBytes, new cel::Bytes());
  });
}

void Value::Destruct(Value* dest) {
  // Perform any deallocations or destructions necessary and reset the state
  // of `dest` to `Value()` making it the null value.
  switch (dest->kind()) {
    case Kind::kNullType:
      return;
    case Kind::kBool:
      ABSL_FALLTHROUGH_INTENDED;
    case Kind::kInt:
      ABSL_FALLTHROUGH_INTENDED;
    case Kind::kUint:
      ABSL_FALLTHROUGH_INTENDED;
    case Kind::kDouble:
      ABSL_FALLTHROUGH_INTENDED;
    case Kind::kDuration:
      ABSL_FALLTHROUGH_INTENDED;
    case Kind::kTimestamp:
      dest->content_.destruct_trivial_value();
      break;
    case Kind::kError:
      dest->content_.destruct_error_value();
      break;
    case Kind::kBytes:
      dest->content_.destruct_reffed_value();
      break;
    default:
      // TODO(issues/5): remove after implementing other kinds
      std::abort();
  }
  dest->metadata_.Reset();
}

void Value::HashValue(absl::HashState state) const {
  state = absl::HashState::combine(std::move(state), type());
  switch (kind()) {
    case Kind::kNullType:
      absl::HashState::combine(std::move(state), 0);
      return;
    case Kind::kBool:
      absl::HashState::combine(std::move(state), AsBool());
      return;
    case Kind::kInt:
      absl::HashState::combine(std::move(state), AsInt());
      return;
    case Kind::kUint:
      absl::HashState::combine(std::move(state), AsUint());
      return;
    case Kind::kDouble:
      absl::HashState::combine(std::move(state), AsDouble());
      return;
    case Kind::kDuration:
      absl::HashState::combine(std::move(state), AsDuration());
      return;
    case Kind::kTimestamp:
      absl::HashState::combine(std::move(state), AsTimestamp());
      return;
    case Kind::kError:
      StatusHashValue(std::move(state), AsError());
      return;
    case Kind::kBytes:
      content_.reffed_value()->HashValue(std::move(state));
      return;
    default:
      // TODO(issues/5): remove after implementing other kinds
      std::abort();
  }
}

bool Value::Equals(const Value& other) const {
  // Comparing types is not enough as type may only compare the type name,
  // which could be the same in separate environments but different kinds. So
  // we also compare the kinds.
  if (kind() != other.kind() || type() != other.type()) {
    return false;
  }
  switch (kind()) {
    case Kind::kNullType:
      return true;
    case Kind::kBool:
      return AsBool() == other.AsBool();
    case Kind::kInt:
      return AsInt() == other.AsInt();
    case Kind::kUint:
      return AsUint() == other.AsUint();
    case Kind::kDouble:
      return AsDouble() == other.AsDouble();
    case Kind::kDuration:
      return AsDuration() == other.AsDuration();
    case Kind::kTimestamp:
      return AsTimestamp() == other.AsTimestamp();
    case Kind::kError:
      return AsError() == other.AsError();
    case Kind::kBytes:
      return content_.reffed_value()->Equals(other);
    default:
      // TODO(issues/5): remove after implementing other kinds
      std::abort();
  }
}

void Value::Swap(Value& other) {
  // TODO(issues/5): Optimize this after other values are implemented
  Value tmp(std::move(other));
  other = std::move(*this);
  *this = std::move(tmp);
}

namespace {

constexpr absl::string_view ExternalDataToStringView(
    const base_internal::ExternalData& external_data) {
  return absl::string_view(static_cast<const char*>(external_data.data),
                           external_data.size);
}

struct DebugStringVisitor final {
  std::string operator()(const std::string& value) const {
    return internal::FormatBytesLiteral(value);
  }

  std::string operator()(const absl::Cord& value) const {
    absl::string_view flat;
    if (value.GetFlat(&flat)) {
      return internal::FormatBytesLiteral(flat);
    }
    return internal::FormatBytesLiteral(value.ToString());
  }

  std::string operator()(const base_internal::ExternalData& value) const {
    return internal::FormatBytesLiteral(ExternalDataToStringView(value));
  }
};

struct ToCordReleaser final {
  void operator()() const { internal::Unref(refcnt); }

  const internal::ReferenceCounted* refcnt;
};

struct ToStringVisitor final {
  std::string operator()(const std::string& value) const { return value; }

  std::string operator()(const absl::Cord& value) const {
    return value.ToString();
  }

  std::string operator()(const base_internal::ExternalData& value) const {
    return std::string(static_cast<const char*>(value.data), value.size);
  }
};

struct ToCordVisitor final {
  const internal::ReferenceCounted* refcnt;

  absl::Cord operator()(const std::string& value) const {
    internal::Ref(refcnt);
    return absl::MakeCordFromExternal(value, ToCordReleaser{refcnt});
  }

  absl::Cord operator()(const absl::Cord& value) const { return value; }

  absl::Cord operator()(const base_internal::ExternalData& value) const {
    internal::Ref(refcnt);
    return absl::MakeCordFromExternal(ExternalDataToStringView(value),
                                      ToCordReleaser{refcnt});
  }
};

struct SizeVisitor final {
  size_t operator()(const std::string& value) const { return value.size(); }

  size_t operator()(const absl::Cord& value) const { return value.size(); }

  size_t operator()(const base_internal::ExternalData& value) const {
    return value.size;
  }
};

struct EmptyVisitor final {
  bool operator()(const std::string& value) const { return value.empty(); }

  bool operator()(const absl::Cord& value) const { return value.empty(); }

  bool operator()(const base_internal::ExternalData& value) const {
    return value.size == 0;
  }
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

  bool operator()(const std::string& value) const {
    return EqualsImpl(value, ref_);
  }

  bool operator()(const absl::Cord& value) const {
    return EqualsImpl(value, ref_);
  }

  bool operator()(const base_internal::ExternalData& value) const {
    return EqualsImpl(ExternalDataToStringView(value), ref_);
  }

 private:
  const T& ref_;
};

template <>
class EqualsVisitor<Bytes> final {
 public:
  explicit EqualsVisitor(const Bytes& ref) : ref_(ref) {}

  bool operator()(const std::string& value) const { return ref_.Equals(value); }

  bool operator()(const absl::Cord& value) const { return ref_.Equals(value); }

  bool operator()(const base_internal::ExternalData& value) const {
    return ref_.Equals(ExternalDataToStringView(value));
  }

 private:
  const Bytes& ref_;
};

template <typename T>
class CompareVisitor final {
 public:
  explicit CompareVisitor(const T& ref) : ref_(ref) {}

  int operator()(const std::string& value) const {
    return CompareImpl(value, ref_);
  }

  int operator()(const absl::Cord& value) const {
    return CompareImpl(value, ref_);
  }

  int operator()(const base_internal::ExternalData& value) const {
    return CompareImpl(ExternalDataToStringView(value), ref_);
  }

 private:
  const T& ref_;
};

template <>
class CompareVisitor<Bytes> final {
 public:
  explicit CompareVisitor(const Bytes& ref) : ref_(ref) {}

  int operator()(const std::string& value) const { return ref_.Compare(value); }

  int operator()(const absl::Cord& value) const { return ref_.Compare(value); }

  int operator()(absl::string_view value) const { return ref_.Compare(value); }

  int operator()(const base_internal::ExternalData& value) const {
    return ref_.Compare(ExternalDataToStringView(value));
  }

 private:
  const Bytes& ref_;
};

class HashValueVisitor final {
 public:
  explicit HashValueVisitor(absl::HashState state) : state_(std::move(state)) {}

  void operator()(const std::string& value) {
    absl::HashState::combine(std::move(state_), value);
  }

  void operator()(const absl::Cord& value) {
    absl::HashState::combine(std::move(state_), value);
  }

  void operator()(const base_internal::ExternalData& value) {
    absl::HashState::combine(std::move(state_),
                             ExternalDataToStringView(value));
  }

 private:
  absl::HashState state_;
};

}  // namespace

Value Bytes::Empty() {
  Value::InitializeSingletons();
  return simple_values->empty_bytes;
}

Value Bytes::New(std::string value) {
  if (value.empty()) {
    return Empty();
  }
  return Value(Kind::kBytes, new Bytes(std::move(value)));
}

Value Bytes::New(absl::Cord value) {
  if (value.empty()) {
    return Empty();
  }
  return Value(Kind::kBytes, new Bytes(std::move(value)));
}

Value Bytes::Concat(const Bytes& lhs, const Bytes& rhs) {
  absl::Cord value;
  value.Append(lhs.ToCord());
  value.Append(rhs.ToCord());
  return New(std::move(value));
}

size_t Bytes::size() const { return absl::visit(SizeVisitor{}, data_); }

bool Bytes::empty() const { return absl::visit(EmptyVisitor{}, data_); }

bool Bytes::Equals(absl::string_view bytes) const {
  return absl::visit(EqualsVisitor<absl::string_view>(bytes), data_);
}

bool Bytes::Equals(const absl::Cord& bytes) const {
  return absl::visit(EqualsVisitor<absl::Cord>(bytes), data_);
}

bool Bytes::Equals(const Bytes& bytes) const {
  return absl::visit(EqualsVisitor<Bytes>(*this), bytes.data_);
}

int Bytes::Compare(absl::string_view bytes) const {
  return absl::visit(CompareVisitor<absl::string_view>(bytes), data_);
}

int Bytes::Compare(const absl::Cord& bytes) const {
  return absl::visit(CompareVisitor<absl::Cord>(bytes), data_);
}

int Bytes::Compare(const Bytes& bytes) const {
  return absl::visit(CompareVisitor<Bytes>(*this), bytes.data_);
}

std::string Bytes::ToString() const {
  return absl::visit(ToStringVisitor{}, data_);
}

absl::Cord Bytes::ToCord() const {
  return absl::visit(ToCordVisitor{this}, data_);
}

std::string Bytes::DebugString() const {
  return absl::visit(DebugStringVisitor{}, data_);
}

bool Bytes::Equals(const Value& value) const {
  ABSL_ASSERT(value.IsBytes());
  return absl::visit(EqualsVisitor<Bytes>(*this), value.AsBytes().data_);
}

void Bytes::HashValue(absl::HashState state) const {
  absl::visit(HashValueVisitor(std::move(state)), data_);
}

}  // namespace cel
