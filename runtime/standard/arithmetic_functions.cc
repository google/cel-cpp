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

#include "runtime/standard/arithmetic_functions.h"

#include <cstdint>
#include <limits>

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "base/builtins.h"
#include "base/function_adapter.h"
#include "common/value.h"
#include "internal/overflow.h"
#include "internal/status_macros.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"

namespace cel {
namespace {

// Template functions providing arithmetic operations
template <class Type>
Value Add(Type v0, Type v1);

template <>
Value Add<int64_t>(int64_t v0, int64_t v1) {
  if (auto sum = cel::internal::CheckedAdd(v0, v1); sum) {
    return IntValue(*sum);
  }
  return IntOverflowError();
}

template <>
Value Add<uint64_t>(uint64_t v0, uint64_t v1) {
  if (auto sum = cel::internal::CheckedAdd(v0, v1); sum) {
    return UintValue(*sum);
  }
  return UintOverflowError();
}

template <>
Value Add<double>(double v0, double v1) {
  return DoubleValue(v0 + v1);
}

template <class Type>
Value Sub(Type v0, Type v1);

template <>
Value Sub<int64_t>(int64_t v0, int64_t v1) {
  if (auto sum = cel::internal::CheckedSub(v0, v1); sum) {
    return IntValue(*sum);
  }
  return IntOverflowError();
}

template <>
Value Sub<uint64_t>(uint64_t v0, uint64_t v1) {
  if (auto sum = cel::internal::CheckedSub(v0, v1); sum) {
    return UintValue(*sum);
  }
  return UintOverflowError();
}

template <>
Value Sub<double>(double v0, double v1) {
  return DoubleValue(v0 - v1);
}

template <class Type>
Value Mul(Type v0, Type v1);

template <>
Value Mul<int64_t>(int64_t v0, int64_t v1) {
  if (auto prod = cel::internal::CheckedMul(v0, v1); prod) {
    return IntValue(*prod);
  }
  return IntOverflowError();
}

template <>
Value Mul<uint64_t>(uint64_t v0, uint64_t v1) {
  if (auto prod = cel::internal::CheckedMul(v0, v1); prod) {
    return UintValue(*prod);
  }
  return UintOverflowError();
}

template <>
Value Mul<double>(double v0, double v1) {
  return DoubleValue(v0 * v1);
}

template <class Type>
Value Div(Type v0, Type v1);

// Division operations for integer types should check for
// division by 0
template <>
Value Div<int64_t>(int64_t v0, int64_t v1) {
  if (auto quot = cel::internal::CheckedDiv(v0, v1); quot) {
    return IntValue(*quot);
  }
  return v1 != int64_t{0} ? IntOverflowError() : IntDivisionByZeroError();
}

// Division operations for integer types should check for
// division by 0
template <>
Value Div<uint64_t>(uint64_t v0, uint64_t v1) {
  if (auto quot = cel::internal::CheckedDiv(v0, v1); quot) {
    return UintValue(*quot);
  }
  return v1 != uint64_t{0} ? UintOverflowError() : UintDivisionByZeroError();
}

template <>
Value Div<double>(double v0, double v1) {
  static_assert(std::numeric_limits<double>::is_iec559,
                "Division by zero for doubles must be supported");

  // For double, division will result in +/- inf
  return DoubleValue(v0 / v1);
}

// Modulo operation
template <class Type>
Value Modulo(Type v0, Type v1);

// Modulo operations for integer types should check for
// division by 0
template <>
Value Modulo<int64_t>(int64_t v0, int64_t v1) {
  if (auto quot = cel::internal::CheckedMod(v0, v1); quot) {
    return IntValue(*quot);
  }
  return v1 != int64_t{0} ? IntOverflowError() : IntModuloByZeroError();
}

template <>
Value Modulo<uint64_t>(uint64_t v0, uint64_t v1) {
  if (auto quot = cel::internal::CheckedMod(v0, v1); quot) {
    return UintValue(*quot);
  }
  return v1 != uint64_t{0} ? UintOverflowError() : UintModuloByZeroError();
}

// Helper method
// Registers all arithmetic functions for template parameter type.
template <class Type>
absl::Status RegisterArithmeticFunctionsForType(FunctionRegistry& registry) {
  using FunctionAdapter = cel::BinaryFunctionAdapter<Value, Type, Type>;
  CEL_RETURN_IF_ERROR(registry.Register(
      FunctionAdapter::CreateDescriptor(cel::builtin::kAdd, false),
      FunctionAdapter::WrapFunction(&Add<Type>)));

  CEL_RETURN_IF_ERROR(registry.Register(
      FunctionAdapter::CreateDescriptor(cel::builtin::kSubtract, false),
      FunctionAdapter::WrapFunction(&Sub<Type>)));

  CEL_RETURN_IF_ERROR(registry.Register(
      FunctionAdapter::CreateDescriptor(cel::builtin::kMultiply, false),
      FunctionAdapter::WrapFunction(&Mul<Type>)));

  return registry.Register(
      FunctionAdapter::CreateDescriptor(cel::builtin::kDivide, false),
      FunctionAdapter::WrapFunction(&Div<Type>));
}

}  // namespace

absl::Status RegisterArithmeticFunctions(FunctionRegistry& registry,
                                         const RuntimeOptions& options) {
  CEL_RETURN_IF_ERROR(RegisterArithmeticFunctionsForType<int64_t>(registry));
  CEL_RETURN_IF_ERROR(RegisterArithmeticFunctionsForType<uint64_t>(registry));
  CEL_RETURN_IF_ERROR(RegisterArithmeticFunctionsForType<double>(registry));

  // Modulo
  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, int64_t, int64_t>::CreateDescriptor(
          cel::builtin::kModulo, false),
      BinaryFunctionAdapter<Value, int64_t, int64_t>::WrapFunction(
          &Modulo<int64_t>)));

  CEL_RETURN_IF_ERROR(registry.Register(
      BinaryFunctionAdapter<Value, uint64_t, uint64_t>::CreateDescriptor(
          cel::builtin::kModulo, false),
      BinaryFunctionAdapter<Value, uint64_t, uint64_t>::WrapFunction(
          &Modulo<uint64_t>)));

  // Negation group
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, int64_t>::CreateDescriptor(cel::builtin::kNeg,
                                                             false),
      UnaryFunctionAdapter<Value, int64_t>::WrapFunction(
          [](int64_t value) -> Value {
            if (auto inv = cel::internal::CheckedNegation(value); inv) {
              return IntValue(*inv);
            }
            return IntOverflowError();
          })));

  return registry.Register(
      UnaryFunctionAdapter<double, double>::CreateDescriptor(cel::builtin::kNeg,
                                                             false),
      UnaryFunctionAdapter<double, double>::WrapFunction(
          [](double value) -> double { return -value; }));
}

}  // namespace cel
