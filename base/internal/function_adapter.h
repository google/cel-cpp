// Copyright 2023 Google LLC
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
//
// Definitions for implementation details of the function adapter utility.

#ifndef THIRD_PARTY_CEL_CPP_BASE_INTERNAL_FUNCTION_ADAPTER_H_
#define THIRD_PARTY_CEL_CPP_BASE_INTERNAL_FUNCTION_ADAPTER_H_

#include <type_traits>
#include <utility>

#include "absl/status/status.h"
#include "absl/time/time.h"
#include "base/kind.h"
#include "base/value_factory.h"
#include "base/values/bool_value.h"
#include "base/values/bytes_value.h"
#include "base/values/duration_value.h"
#include "base/values/int_value.h"
#include "base/values/timestamp_value.h"
#include "base/values/uint_value.h"
#include "internal/status_macros.h"

namespace cel::internal {

// Helper for triggering static asserts in an unspecialized template overload.
template <typename T>
struct UnhandledType : std::false_type {};

// Adapts the type param Type to the appropriate Kind.
// A static assertion fails if the provided type does not map to a cel::Value
// kind.
// TODO(issues/5): Add support for remaining kinds.
template <typename Type>
constexpr Kind AdaptedKind() {
  static_assert(UnhandledType<Type>::value,
                "Unsupported primitive type to cel::Kind conversion");
  return Kind::kNotForUseWithExhaustiveSwitchStatements;
}

template <>
constexpr Kind AdaptedKind<int64_t>() {
  return Kind::kInt64;
}

template <>
constexpr Kind AdaptedKind<uint64_t>() {
  return Kind::kUint64;
}

template <>
constexpr Kind AdaptedKind<double>() {
  return Kind::kDouble;
}

template <>
constexpr Kind AdaptedKind<bool>() {
  return Kind::kBool;
}

template <>
constexpr Kind AdaptedKind<absl::Time>() {
  return Kind::kTimestamp;
}

template <>
constexpr Kind AdaptedKind<absl::Duration>() {
  return Kind::kDuration;
}

template <>
constexpr Kind AdaptedKind<Handle<Value>>() {
  return Kind::kAny;
}

template <>
constexpr Kind AdaptedKind<Handle<StringValue>>() {
  return Kind::kString;
}

template <>
constexpr Kind AdaptedKind<Handle<BytesValue>>() {
  return Kind::kBytes;
}

template <>
constexpr Kind AdaptedKind<const Handle<Value>&>() {
  return Kind::kAny;
}

template <>
constexpr Kind AdaptedKind<const Handle<StringValue>&>() {
  return Kind::kString;
}

template <>
constexpr Kind AdaptedKind<const Handle<BytesValue>&>() {
  return Kind::kBytes;
}

// Adapt a Handle<Value> to its corresponding argument type in a wrapped c++
// function.
struct HandleToAdaptedVisitor {
  absl::Status operator()(int64_t* out) {
    if (!input.Is<IntValue>()) {
      return absl::InvalidArgumentError("expected int value");
    }
    *out = input.As<IntValue>()->value();
    return absl::OkStatus();
  }

  absl::Status operator()(uint64_t* out) {
    if (!input.Is<UintValue>()) {
      return absl::InvalidArgumentError("expected uint value");
    }
    *out = input.As<UintValue>()->value();
    return absl::OkStatus();
  }

  absl::Status operator()(double* out) {
    if (!input.Is<DoubleValue>()) {
      return absl::InvalidArgumentError("expected double value");
    }
    *out = input.As<DoubleValue>()->value();
    return absl::OkStatus();
  }

  absl::Status operator()(bool* out) {
    if (!input.Is<BoolValue>()) {
      return absl::InvalidArgumentError("expected bool value");
    }
    *out = input.As<BoolValue>()->value();
    return absl::OkStatus();
  }

  absl::Status operator()(absl::Time* out) {
    if (!input.Is<TimestampValue>()) {
      return absl::InvalidArgumentError("expected timestamp value");
    }
    *out = input.As<TimestampValue>()->value();
    return absl::OkStatus();
  }

  absl::Status operator()(absl::Duration* out) {
    if (!input.Is<DurationValue>()) {
      return absl::InvalidArgumentError("expected duration value");
    }
    *out = input.As<DurationValue>()->value();
    return absl::OkStatus();
  }

  absl::Status operator()(Handle<Value>* out) {
    *out = input;
    return absl::OkStatus();
  }

  absl::Status operator()(const Handle<Value>** out) {
    *out = &input;
    return absl::OkStatus();
  }

  // Used to implement adapter for pass by const reference functions.
  template <typename T>
  absl::Status operator()(const Handle<T>** out) {
    if (!input.Is<T>()) {
      return absl::InvalidArgumentError(
          absl::StrCat("expected ", KindToString(T::kKind), " value"));
    }
    *out = &(input.As<T>());
    return absl::OkStatus();
  }

  template <typename T>
  absl::Status operator()(Handle<T>* out) {
    const Handle<T>* out_ptr;
    CEL_RETURN_IF_ERROR(this->operator()(&out_ptr));
    *out = *out_ptr;
    return absl::OkStatus();
  }

  const Handle<Value>& input;
};

// Adapts the return value of a wrapped C++ function to its corresponding
// Handle<Value> representation.
struct AdaptedToHandleVisitor {
  absl::StatusOr<Handle<Value>> operator()(int64_t in) {
    return value_factory.CreateIntValue(in);
  }

  absl::StatusOr<Handle<Value>> operator()(uint64_t in) {
    return value_factory.CreateUintValue(in);
  }

  absl::StatusOr<Handle<Value>> operator()(double in) {
    return value_factory.CreateDoubleValue(in);
  }

  absl::StatusOr<Handle<Value>> operator()(bool in) {
    return value_factory.CreateBoolValue(in);
  }

  absl::StatusOr<Handle<Value>> operator()(absl::Time in) {
    return value_factory.CreateTimestampValue(in);
  }

  absl::StatusOr<Handle<Value>> operator()(absl::Duration in) {
    return value_factory.CreateDurationValue(in);
  }

  absl::StatusOr<Handle<Value>> operator()(Handle<Value> in) { return in; }

  template <typename T>
  absl::StatusOr<Handle<Value>> operator()(Handle<T> in) {
    return in;
  }

  // Special case for StatusOr<T> return value -- wrap the underlying value if
  // present, otherwise return the status.
  template <typename T>
  absl::StatusOr<Handle<Value>> operator()(absl::StatusOr<T> wrapped) {
    CEL_ASSIGN_OR_RETURN(auto value, wrapped);
    return this->operator()(std::move(value));
  }

  cel::ValueFactory& value_factory;
};

}  // namespace cel::internal

#endif  // THIRD_PARTY_CEL_CPP_BASE_INTERNAL_FUNCTION_ADAPTER_H_
