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

#ifndef THIRD_PARTY_CEL_CPP_BASE_VALUES_ERROR_VALUE_H_
#define THIRD_PARTY_CEL_CPP_BASE_VALUES_ERROR_VALUE_H_

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "base/handle.h"
#include "base/internal/data.h"
#include "base/kind.h"
#include "base/type.h"
#include "base/types/error_type.h"
#include "base/value.h"
#include "common/any.h"
#include "common/json.h"

namespace cel {

class ErrorValue final
    : public Value,
      public base_internal::InlineData,
      public base_internal::EnableHandleFromThis<ErrorValue> {
 public:
  ABSL_ATTRIBUTE_PURE_FUNCTION static std::string DebugString(
      const absl::Status& value);

  static constexpr ValueKind kKind = ValueKind::kError;

  static bool Is(const Value& value) { return value.kind() == kKind; }

  using Value::Is;

  static const ErrorValue& Cast(const Value& value) {
    ABSL_DCHECK(Is(value)) << "cannot cast " << value.type()->name()
                           << " to error";
    return static_cast<const ErrorValue&>(value);
  }

  constexpr ValueKind kind() const { return kKind; }

  Handle<ErrorType> type() const { return ErrorType::Get(); }

  std::string DebugString() const;

  absl::StatusOr<Any> ConvertToAny(ValueFactory&) const;

  absl::StatusOr<Json> ConvertToJson(ValueFactory&) const;

  absl::StatusOr<Handle<Value>> ConvertToType(ValueFactory&,
                                              const Handle<Type>&) const;

  absl::StatusOr<Handle<Value>> Equals(ValueFactory& value_factory,
                                       const Value& other) const;

  const absl::Status& NativeValue() const ABSL_ATTRIBUTE_LIFETIME_BOUND;

 private:
  friend class ValueHandle;
  template <size_t Size, size_t Align>
  friend struct base_internal::AnyData;
  friend struct interop_internal::ErrorValueAccess;

  static constexpr uintptr_t kMetadata =
      base_internal::kStoredInline |
      (static_cast<uintptr_t>(kKind) << base_internal::kKindShift);

  explicit ErrorValue(absl::Status value)
      : base_internal::InlineData(kMetadata), value_(std::move(value)) {}

  explicit ErrorValue(const absl::Status* value_ptr)
      : base_internal::InlineData(kMetadata | base_internal::kTrivial),
        value_ptr_(value_ptr) {}

  ErrorValue(const ErrorValue& other) : ErrorValue(other.value_) {
    // Only called when `other.value_` is the active member.
  }

  ErrorValue(ErrorValue&& other) : ErrorValue(std::move(other.value_)) {
    // Only called when `other.value_` is the active member.
  }

  ~ErrorValue() {
    // Only called when `value_` is the active member.
    value_.~Status();
  }

  ErrorValue& operator=(const ErrorValue& other) {
    // Only called when `value_` and `other.value_` are the active members.
    if (ABSL_PREDICT_TRUE(this != &other)) {
      value_ = other.value_;
    }
    return *this;
  }

  ErrorValue& operator=(ErrorValue&& other) {
    // Only called when `value_` and `other.value_` are the active members.
    if (ABSL_PREDICT_TRUE(this != &other)) {
      value_ = std::move(other.value_);
    }
    return *this;
  }

  union {
    absl::Status value_;
    const absl::Status* value_ptr_;
  };
};

CEL_INTERNAL_VALUE_DECL(ErrorValue);

namespace base_internal {

template <>
struct ValueTraits<ErrorValue> {
  using type = ErrorValue;

  using type_type = ErrorType;

  using underlying_type = absl::Status;

  static std::string DebugString(const underlying_type& value) {
    return type::DebugString(value);
  }

  static std::string DebugString(const type& value) {
    return value.DebugString();
  }

  static Handle<type> Wrap(ValueFactory& value_factory, underlying_type value);

  static underlying_type Unwrap(underlying_type value) { return value; }

  static underlying_type Unwrap(const Handle<type>& value) {
    return Unwrap(value->NativeValue());
  }
};

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_VALUES_ERROR_VALUE_H_
