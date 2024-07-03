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

#include "extensions/math_ext.h"

#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/value.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_number.h"
#include "eval/public/cel_options.h"
#include "internal/status_macros.h"
#include "runtime/function_adapter.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"

namespace cel::extensions {

namespace {

using ::google::api::expr::runtime::CelFunctionRegistry;
using ::google::api::expr::runtime::CelNumber;
using ::google::api::expr::runtime::InterpreterOptions;

static constexpr char kMathMin[] = "math.@min";
static constexpr char kMathMax[] = "math.@max";

struct ToValueVisitor {
  Value operator()(uint64_t v) const { return UintValue{v}; }
  Value operator()(int64_t v) const { return IntValue{v}; }
  Value operator()(double v) const { return DoubleValue{v}; }
};

Value NumberToValue(CelNumber number) {
  return number.visit<Value>(ToValueVisitor{});
}

absl::StatusOr<CelNumber> ValueToNumber(const Value &value,
                                        absl::string_view function) {
  if (auto int_value = As<IntValue>(value); int_value) {
    return CelNumber::FromInt64(int_value->NativeValue());
  }
  if (auto uint_value = As<UintValue>(value); uint_value) {
    return CelNumber::FromUint64(uint_value->NativeValue());
  }
  if (auto double_value = As<DoubleValue>(value); double_value) {
    return CelNumber::FromDouble(double_value->NativeValue());
  }
  return absl::InvalidArgumentError(
      absl::StrCat(function, " arguments must be numeric"));
}

CelNumber MinNumber(CelNumber v1, CelNumber v2) {
  if (v2 < v1) {
    return v2;
  }
  return v1;
}

Value MinValue(CelNumber v1, CelNumber v2) {
  return NumberToValue(MinNumber(v1, v2));
}

template <typename T>
Value Identity(ValueManager &, T v1) {
  return NumberToValue(CelNumber(v1));
}

template <typename T, typename U>
Value Min(ValueManager &, T v1, U v2) {
  return MinValue(CelNumber(v1), CelNumber(v2));
}

absl::StatusOr<Value> MinList(ValueManager &value_manager,
                              const ListValue &values) {
  CEL_ASSIGN_OR_RETURN(auto iterator, values.NewIterator(value_manager));
  if (!iterator->HasNext()) {
    return ErrorValue(
        absl::InvalidArgumentError("math.@min argument must not be empty"));
  }
  Value value;
  CEL_RETURN_IF_ERROR(iterator->Next(value_manager, value));
  absl::StatusOr<CelNumber> current = ValueToNumber(value, kMathMin);
  if (!current.ok()) {
    return ErrorValue{current.status()};
  }
  CelNumber min = *current;
  while (iterator->HasNext()) {
    CEL_RETURN_IF_ERROR(iterator->Next(value_manager, value));
    absl::StatusOr<CelNumber> other = ValueToNumber(value, kMathMin);
    if (!other.ok()) {
      return ErrorValue{other.status()};
    }
    min = MinNumber(min, *other);
  }
  return NumberToValue(min);
}

CelNumber MaxNumber(CelNumber v1, CelNumber v2) {
  if (v2 > v1) {
    return v2;
  }
  return v1;
}

Value MaxValue(CelNumber v1, CelNumber v2) {
  return NumberToValue(MaxNumber(v1, v2));
}

template <typename T, typename U>
Value Max(ValueManager &, T v1, U v2) {
  return MaxValue(CelNumber(v1), CelNumber(v2));
}

absl::StatusOr<Value> MaxList(ValueManager &value_manager,
                              const ListValue &values) {
  CEL_ASSIGN_OR_RETURN(auto iterator, values.NewIterator(value_manager));
  if (!iterator->HasNext()) {
    return ErrorValue(
        absl::InvalidArgumentError("math.@max argument must not be empty"));
  }
  Value value;
  CEL_RETURN_IF_ERROR(iterator->Next(value_manager, value));
  absl::StatusOr<CelNumber> current = ValueToNumber(value, kMathMax);
  if (!current.ok()) {
    return ErrorValue{current.status()};
  }
  CelNumber min = *current;
  while (iterator->HasNext()) {
    CEL_RETURN_IF_ERROR(iterator->Next(value_manager, value));
    absl::StatusOr<CelNumber> other = ValueToNumber(value, kMathMax);
    if (!other.ok()) {
      return ErrorValue{other.status()};
    }
    min = MaxNumber(min, *other);
  }
  return NumberToValue(min);
}

template <typename T, typename U>
absl::Status RegisterCrossNumericMin(FunctionRegistry &registry) {
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, T, U>::CreateDescriptor(
          kMathMin, /*receiver_style=*/false),
      BinaryFunctionAdapter<Value, T, U>::WrapFunction(Min<T, U>)));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, U, T>::CreateDescriptor(
          kMathMin, /*receiver_style=*/false),
      BinaryFunctionAdapter<Value, U, T>::WrapFunction(Min<U, T>)));

  return absl::OkStatus();
}

template <typename T, typename U>
absl::Status RegisterCrossNumericMax(FunctionRegistry &registry) {
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, T, U>::CreateDescriptor(
          kMathMax, /*receiver_style=*/false),
      BinaryFunctionAdapter<Value, T, U>::WrapFunction(Max<T, U>)));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, U, T>::CreateDescriptor(
          kMathMax, /*receiver_style=*/false),
      BinaryFunctionAdapter<Value, U, T>::WrapFunction(Max<U, T>)));

  return absl::OkStatus();
}

}  // namespace

absl::Status RegisterMathExtensionFunctions(FunctionRegistry &registry,
                                            const RuntimeOptions &options) {
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, int64_t>::CreateDescriptor(
          kMathMin, /*receiver_style=*/false),
      UnaryFunctionAdapter<Value, int64_t>::WrapFunction(Identity<int64_t>)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, double>::CreateDescriptor(
          kMathMin, /*receiver_style=*/false),
      UnaryFunctionAdapter<Value, double>::WrapFunction(Identity<double>)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, uint64_t>::CreateDescriptor(
          kMathMin, /*receiver_style=*/false),
      UnaryFunctionAdapter<Value, uint64_t>::WrapFunction(Identity<uint64_t>)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, int64_t, int64_t>::CreateDescriptor(
          kMathMin, /*receiver_style=*/false),
      BinaryFunctionAdapter<Value, int64_t, int64_t>::WrapFunction(
          Min<int64_t, int64_t>)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, double, double>::CreateDescriptor(
          kMathMin, /*receiver_style=*/false),
      BinaryFunctionAdapter<Value, double, double>::WrapFunction(
          Min<double, double>)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, uint64_t, uint64_t>::CreateDescriptor(
          kMathMin, /*receiver_style=*/false),
      BinaryFunctionAdapter<Value, uint64_t, uint64_t>::WrapFunction(
          Min<uint64_t, uint64_t>)));
  CEL_RETURN_IF_ERROR((RegisterCrossNumericMin<int64_t, uint64_t>(registry)));
  CEL_RETURN_IF_ERROR((RegisterCrossNumericMin<int64_t, double>(registry)));
  CEL_RETURN_IF_ERROR((RegisterCrossNumericMin<double, uint64_t>(registry)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<absl::StatusOr<Value>, ListValue>::CreateDescriptor(
          kMathMin, false),
      UnaryFunctionAdapter<absl::StatusOr<Value>, ListValue>::WrapFunction(
          MinList)));

  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, int64_t>::CreateDescriptor(
          kMathMax, /*receiver_style=*/false),
      UnaryFunctionAdapter<Value, int64_t>::WrapFunction(Identity<int64_t>)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, double>::CreateDescriptor(
          kMathMax, /*receiver_style=*/false),
      UnaryFunctionAdapter<Value, double>::WrapFunction(Identity<double>)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, uint64_t>::CreateDescriptor(
          kMathMax, /*receiver_style=*/false),
      UnaryFunctionAdapter<Value, uint64_t>::WrapFunction(Identity<uint64_t>)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, int64_t, int64_t>::CreateDescriptor(
          kMathMax, /*receiver_style=*/false),
      BinaryFunctionAdapter<Value, int64_t, int64_t>::WrapFunction(
          Max<int64_t, int64_t>)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, double, double>::CreateDescriptor(
          kMathMax, /*receiver_style=*/false),
      BinaryFunctionAdapter<Value, double, double>::WrapFunction(
          Max<double, double>)));
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, uint64_t, uint64_t>::CreateDescriptor(
          kMathMax, /*receiver_style=*/false),
      BinaryFunctionAdapter<Value, uint64_t, uint64_t>::WrapFunction(
          Max<uint64_t, uint64_t>)));
  CEL_RETURN_IF_ERROR((RegisterCrossNumericMax<int64_t, uint64_t>(registry)));
  CEL_RETURN_IF_ERROR((RegisterCrossNumericMax<int64_t, double>(registry)));
  CEL_RETURN_IF_ERROR((RegisterCrossNumericMax<double, uint64_t>(registry)));
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<absl::StatusOr<Value>, ListValue>::CreateDescriptor(
          kMathMax, false),
      UnaryFunctionAdapter<absl::StatusOr<Value>, ListValue>::WrapFunction(
          MaxList)));

  return absl::OkStatus();
}

absl::Status RegisterMathExtensionFunctions(CelFunctionRegistry *registry,
                                            const InterpreterOptions &options) {
  return RegisterMathExtensionFunctions(
      registry->InternalGetRegistry(),
      google::api::expr::runtime::ConvertToRuntimeOptions(options));
}

}  // namespace cel::extensions
