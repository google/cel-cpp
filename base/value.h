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

#include <cstdint>
#include <limits>
#include <memory>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/casts.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/variant.h"
#include "base/internal/value.h"
#include "base/kind.h"
#include "base/type.h"
#include "internal/casts.h"

namespace cel {

// A representation of a CEL value that enables reflection and introspection of
// values.
//
// TODO(issues/5): document once derived implementations stabilize
class Value final {
 public:
  // Returns the null value.
  ABSL_ATTRIBUTE_PURE_FUNCTION static Value Null() { return Value(); }

  // Constructs an error value. It is required that `status` is non-OK,
  // otherwise behavior is undefined.
  static Value Error(const absl::Status& status);

  // Returns a bool value.
  static Value Bool(bool value) { return Value(value); }

  // Returns the false bool value. Equivalent to `Value::Bool(false)`.
  ABSL_ATTRIBUTE_PURE_FUNCTION static Value False() { return Bool(false); }

  // Returns the true bool value. Equivalent to `Value::Bool(true)`.
  ABSL_ATTRIBUTE_PURE_FUNCTION static Value True() { return Bool(true); }

  // Returns an int value.
  static Value Int(int64_t value) { return Value(value); }

  // Returns a uint value.
  static Value Uint(uint64_t value) { return Value(value); }

  // Returns a double value.
  static Value Double(double value) { return Value(value); }

  // Returns a NaN double value. Equivalent to `Value::Double(NAN)`.
  ABSL_ATTRIBUTE_PURE_FUNCTION static Value NaN() {
    return Double(std::numeric_limits<double>::quiet_NaN());
  }

  // Returns a positive infinity double value. Equivalent to
  // `Value::Double(INFINITY)`.
  ABSL_ATTRIBUTE_PURE_FUNCTION static Value PositiveInfinity() {
    return Double(std::numeric_limits<double>::infinity());
  }

  // Returns a negative infinity double value. Equivalent to
  // `Value::Double(-INFINITY)`.
  ABSL_ATTRIBUTE_PURE_FUNCTION static Value NegativeInfinity() {
    return Double(-std::numeric_limits<double>::infinity());
  }

  // Returns a duration value or a `absl::StatusCode::kInvalidArgument` error if
  // the value is not in the valid range.
  static absl::StatusOr<Value> Duration(absl::Duration value);

  // Returns the zero duration value. Equivalent to
  // `Value::Duration(absl::ZeroDuration())`.
  ABSL_ATTRIBUTE_PURE_FUNCTION static Value ZeroDuration() {
    return Value(Kind::kDuration, 0, 0);
  }

  // Returns a timestamp value or a `absl::StatusCode::kInvalidArgument` error
  // if the value is not in the valid range.
  static absl::StatusOr<Value> Timestamp(absl::Time value);

  // Returns the zero timestamp value. Equivalent to
  // `Value::Timestamp(absl::UnixEpoch())`.
  ABSL_ATTRIBUTE_PURE_FUNCTION static Value UnixEpoch() {
    return Value(Kind::kTimestamp, 0, 0);
  }

  // Equivalent to `Value::Null()`.
  constexpr Value() = default;

  Value(const Value& other);

  Value(Value&& other);

  ~Value();

  Value& operator=(const Value& other);

  Value& operator=(Value&& other);

  // Returns the type of the value. If you only need the kind, prefer `kind()`.
  cel::Type type() const {
    return metadata_.simple_tag()
               ? cel::Type::Simple(metadata_.kind())
               : cel::Type(internal::Ref(metadata_.base_type()));
  }

  // Returns the kind of the value. This is equivalent to `type().kind()` but
  // faster in many scenarios. As such it should be preffered when only the kind
  // is required.
  Kind kind() const { return metadata_.kind(); }

  // True if this is the null value, false otherwise.
  bool IsNull() const { return kind() == Kind::kNullType; }

  // True if this is an error value, false otherwise.
  bool IsError() const { return kind() == Kind::kError; }

  // True if this is a bool value, false otherwise.
  bool IsBool() const { return kind() == Kind::kBool; }

  // True if this is an int value, false otherwise.
  bool IsInt() const { return kind() == Kind::kInt; }

  // True if this is a uint value, false otherwise.
  bool IsUint() const { return kind() == Kind::kUint; }

  // True if this is a double value, false otherwise.
  bool IsDouble() const { return kind() == Kind::kDouble; }

  // True if this is a duration value, false otherwise.
  bool IsDuration() const { return kind() == Kind::kDuration; }

  // True if this is a timestamp value, false otherwise.
  bool IsTimestamp() const { return kind() == Kind::kTimestamp; }

  // True if this is a bytes value, false otherwise.
  bool IsBytes() const { return kind() == Kind::kBytes; }

  // Returns the C++ error value. Requires `kind() == Kind::kError` or behavior
  // is undefined.
  const absl::Status& AsError() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_ASSERT(IsError());
    return content_.error_value();
  }

  // Returns the C++ bool value. Requires `kind() == Kind::kBool` or behavior is
  // undefined.
  bool AsBool() const {
    ABSL_ASSERT(IsBool());
    return content_.bool_value();
  }

  // Returns the C++ int value. Requires `kind() == Kind::kInt` or behavior is
  // undefined.
  int64_t AsInt() const {
    ABSL_ASSERT(IsInt());
    return content_.int_value();
  }

  // Returns the C++ uint value. Requires `kind() == Kind::kUint` or behavior is
  // undefined.
  uint64_t AsUint() const {
    ABSL_ASSERT(IsUint());
    return content_.uint_value();
  }

  // Returns the C++ double value. Requires `kind() == Kind::kDouble` or
  // behavior is undefined.
  double AsDouble() const {
    ABSL_ASSERT(IsDouble());
    return content_.double_value();
  }

  // Returns the C++ duration value. Requires `kind() == Kind::kDuration` or
  // behavior is undefined.
  absl::Duration AsDuration() const {
    ABSL_ASSERT(IsDuration());
    return absl::Seconds(content_.int_value()) +
           absl::Nanoseconds(
               absl::bit_cast<int32_t>(metadata_.extended_content()));
  }

  // Returns the C++ timestamp value. Requires `kind() == Kind::kTimestamp` or
  // behavior is undefined.
  absl::Time AsTimestamp() const {
    // Timestamp is stored as the duration since Unix Epoch.
    ABSL_ASSERT(IsTimestamp());
    return absl::UnixEpoch() + absl::Seconds(content_.int_value()) +
           absl::Nanoseconds(
               absl::bit_cast<int32_t>(metadata_.extended_content()));
  }

  std::string DebugString() const;

  const Bytes& AsBytes() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_ASSERT(IsBytes());
    return internal::down_cast<const Bytes&>(*content_.reffed_value());
  }

  template <typename H>
  friend H AbslHashValue(H state, const Value& value) {
    value.HashValue(absl::HashState::Create(&state));
    return std::move(state);
  }

  friend void swap(Value& lhs, Value& rhs) { lhs.Swap(rhs); }

  friend bool operator==(const Value& lhs, const Value& rhs) {
    return lhs.Equals(rhs);
  }

  friend bool operator!=(const Value& lhs, const Value& rhs) {
    return !operator==(lhs, rhs);
  }

 private:
  friend class Bytes;

  using Metadata = base_internal::ValueMetadata;
  using Content = base_internal::ValueContent;

  static void InitializeSingletons();

  static void Destruct(Value* dest);

  constexpr explicit Value(bool value)
      : metadata_(Kind::kBool), content_(value) {}

  constexpr explicit Value(int64_t value)
      : metadata_(Kind::kInt), content_(value) {}

  constexpr explicit Value(uint64_t value)
      : metadata_(Kind::kUint), content_(value) {}

  constexpr explicit Value(double value)
      : metadata_(Kind::kDouble), content_(value) {}

  explicit Value(const absl::Status& status)
      : metadata_(Kind::kError), content_(status) {}

  constexpr Value(Kind kind, base_internal::BaseValue* base_value)
      : metadata_(kind), content_(base_value) {}

  constexpr Value(Kind kind, int64_t content, uint32_t extended_content)
      : metadata_(kind, extended_content), content_(content) {}

  bool Equals(const Value& other) const;

  void HashValue(absl::HashState state) const;

  void Swap(Value& other);

  Metadata metadata_;
  Content content_;
};

// A CEL bytes value specific interface that can be accessed via
// `cel::Value::AsBytes`. It acts as a facade over various native
// representations and provides efficient implementations of CEL builtin
// functions.
class Bytes final : public base_internal::BaseValue {
 public:
  // Returns a bytes value which has a size of 0 and is empty.
  ABSL_ATTRIBUTE_PURE_FUNCTION static Value Empty();

  // Returns a bytes value with `value` as its contents.
  static Value New(std::string value);

  // Returns a bytes value with a copy of `value` as its contents.
  static Value New(absl::string_view value) {
    return New(std::string(value.data(), value.size()));
  }

  // Returns a bytes value with a copy of `value` as its contents.
  //
  // This is needed for `Value::Bytes("foo")` to be an unambiguous function
  // call.
  static Value New(const char* value) {
    ABSL_ASSERT(value != nullptr);
    return New(absl::string_view(value));
  }

  // Returns a bytes value with `value` as its contents.
  static Value New(absl::Cord value);

  // Returns a bytes value with `value` as its contents. Unlike `New()` this
  // does not copy `value`, instead it expects the contents pointed to by
  // `value` to live as long as the returned instance. `releaser` is used to
  // notify the caller when the contents pointed to by `value` are no longer
  // required.
  template <typename Releaser>
  static std::enable_if_t<std::is_invocable_r_v<void, Releaser>, Value> Wrap(
      absl::string_view value, Releaser&& releaser);

  static Value Concat(const Bytes& lhs, const Bytes& rhs);

  size_t size() const;

  bool empty() const;

  bool Equals(absl::string_view bytes) const;

  bool Equals(const absl::Cord& bytes) const;

  bool Equals(const Bytes& bytes) const;

  int Compare(absl::string_view bytes) const;

  int Compare(const absl::Cord& bytes) const;

  int Compare(const Bytes& bytes) const;

  std::string ToString() const;

  absl::Cord ToCord() const;

  std::string DebugString() const override;

 protected:
  bool Equals(const Value& value) const override;

  void HashValue(absl::HashState state) const override;

 private:
  friend class Value;

  Bytes() : Bytes(std::string()) {}

  explicit Bytes(std::string value)
      : base_internal::BaseValue(),
        data_(absl::in_place_index<0>, std::move(value)) {}

  explicit Bytes(absl::Cord value)
      : base_internal::BaseValue(),
        data_(absl::in_place_index<1>, std::move(value)) {}

  explicit Bytes(base_internal::ExternalData value)
      : base_internal::BaseValue(),
        data_(absl::in_place_index<2>, std::move(value)) {}

  absl::variant<std::string, absl::Cord, base_internal::ExternalData> data_;
};

template <typename Releaser>
std::enable_if_t<std::is_invocable_r_v<void, Releaser>, Value> Bytes::Wrap(
    absl::string_view value, Releaser&& releaser) {
  if (value.empty()) {
    std::forward<Releaser>(releaser)();
    return Empty();
  }
  return Value(Kind::kBytes,
               new Bytes(base_internal::ExternalData(
                   value.data(), value.size(),
                   std::make_unique<base_internal::ExternalDataReleaser>(
                       std::forward<Releaser>(releaser)))));
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUE_H_
