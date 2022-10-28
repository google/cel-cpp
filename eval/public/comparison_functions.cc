// Copyright 2021 Google LLC
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

#include "eval/public/comparison_functions.h"

#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <optional>
#include <type_traits>
#include <vector>

#include "absl/status/status.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_number.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/portable_cel_function_adapter.h"
#include "internal/status_macros.h"

namespace google::api::expr::runtime {

namespace {

using ::google::protobuf::Arena;

// Comparison template functions
template <class Type>
bool LessThan(Arena*, Type t1, Type t2) {
  return (t1 < t2);
}

template <class Type>
bool LessThanOrEqual(Arena*, Type t1, Type t2) {
  return (t1 <= t2);
}

template <class Type>
bool GreaterThan(Arena* arena, Type t1, Type t2) {
  return LessThan(arena, t2, t1);
}

template <class Type>
bool GreaterThanOrEqual(Arena* arena, Type t1, Type t2) {
  return LessThanOrEqual(arena, t2, t1);
}

// Duration comparison specializations
template <>
bool LessThan(Arena*, absl::Duration t1, absl::Duration t2) {
  return absl::operator<(t1, t2);
}

template <>
bool LessThanOrEqual(Arena*, absl::Duration t1, absl::Duration t2) {
  return absl::operator<=(t1, t2);
}

template <>
bool GreaterThan(Arena*, absl::Duration t1, absl::Duration t2) {
  return absl::operator>(t1, t2);
}

template <>
bool GreaterThanOrEqual(Arena*, absl::Duration t1, absl::Duration t2) {
  return absl::operator>=(t1, t2);
}

// Timestamp comparison specializations
template <>
bool LessThan(Arena*, absl::Time t1, absl::Time t2) {
  return absl::operator<(t1, t2);
}

template <>
bool LessThanOrEqual(Arena*, absl::Time t1, absl::Time t2) {
  return absl::operator<=(t1, t2);
}

template <>
bool GreaterThan(Arena*, absl::Time t1, absl::Time t2) {
  return absl::operator>(t1, t2);
}

template <>
bool GreaterThanOrEqual(Arena*, absl::Time t1, absl::Time t2) {
  return absl::operator>=(t1, t2);
}

template <typename T, typename U>
bool CrossNumericLessThan(Arena* arena, T t, U u) {
  return CelNumber(t) < CelNumber(u);
}

template <typename T, typename U>
bool CrossNumericGreaterThan(Arena* arena, T t, U u) {
  return CelNumber(t) > CelNumber(u);
}

template <typename T, typename U>
bool CrossNumericLessOrEqualTo(Arena* arena, T t, U u) {
  return CelNumber(t) <= CelNumber(u);
}

template <typename T, typename U>
bool CrossNumericGreaterOrEqualTo(Arena* arena, T t, U u) {
  return CelNumber(t) >= CelNumber(u);
}

template <typename Type, typename Op>
std::function<CelValue(Arena*, Type, Type)> WrapComparison(Op op) {
  return [op = std::move(op)](Arena* arena, Type lhs, Type rhs) -> CelValue {
    absl::optional<bool> result = op(lhs, rhs);

    if (result.has_value()) {
      return CelValue::CreateBool(*result);
    }

    return CreateNoMatchingOverloadError(arena);
  };
}


template <typename T, typename U>
absl::Status RegisterSymmetricFunction(
    absl::string_view name, std::function<bool(google::protobuf::Arena*, T, U)> fn,
    CelFunctionRegistry* registry) {
  CEL_RETURN_IF_ERROR(registry->Register(
      PortableBinaryFunctionAdapter<bool, T, U>::Create(name, false, fn)));

  // the symmetric version
  CEL_RETURN_IF_ERROR(
      registry->Register(PortableBinaryFunctionAdapter<bool, U, T>::Create(
          name, false,
          [fn](google::protobuf::Arena* arena, U u, T t) { return fn(arena, t, u); })));

  return absl::OkStatus();
}

template <class Type>
absl::Status RegisterOrderingFunctionsForType(CelFunctionRegistry* registry) {
  using FunctionAdapter = PortableBinaryFunctionAdapter<bool, Type, Type>;
  // Less than
  // Extra paranthesis needed for Macros with multiple template arguments.
  CEL_RETURN_IF_ERROR(registry->Register(
      FunctionAdapter::Create(builtin::kLess, false, LessThan<Type>)));

  // Less than or Equal
  CEL_RETURN_IF_ERROR(registry->Register(FunctionAdapter::Create(
      builtin::kLessOrEqual, false, LessThanOrEqual<Type>)));

  // Greater than
  CEL_RETURN_IF_ERROR(registry->Register(
      FunctionAdapter::Create(builtin::kGreater, false, GreaterThan<Type>)));

  // Greater than or Equal
  CEL_RETURN_IF_ERROR(registry->Register(FunctionAdapter::Create(
      builtin::kGreaterOrEqual, false, GreaterThanOrEqual<Type>)));

  return absl::OkStatus();
}

// Registers all comparison functions for template parameter type.
template <class Type>
absl::Status RegisterComparisonFunctionsForType(CelFunctionRegistry* registry) {
  CEL_RETURN_IF_ERROR(RegisterOrderingFunctionsForType<Type>(registry));

  return absl::OkStatus();
}

absl::Status RegisterHomogenousComparisonFunctions(
    CelFunctionRegistry* registry) {
  CEL_RETURN_IF_ERROR(RegisterComparisonFunctionsForType<bool>(registry));

  CEL_RETURN_IF_ERROR(RegisterComparisonFunctionsForType<int64_t>(registry));

  CEL_RETURN_IF_ERROR(RegisterComparisonFunctionsForType<uint64_t>(registry));

  CEL_RETURN_IF_ERROR(RegisterComparisonFunctionsForType<double>(registry));

  CEL_RETURN_IF_ERROR(
      RegisterComparisonFunctionsForType<CelValue::StringHolder>(registry));

  CEL_RETURN_IF_ERROR(
      RegisterComparisonFunctionsForType<CelValue::BytesHolder>(registry));

  CEL_RETURN_IF_ERROR(
      RegisterComparisonFunctionsForType<absl::Duration>(registry));

  CEL_RETURN_IF_ERROR(RegisterComparisonFunctionsForType<absl::Time>(registry));

  return absl::OkStatus();
}

template <typename T, typename U>
absl::Status RegisterCrossNumericComparisons(CelFunctionRegistry* registry) {
  using FunctionAdapter = PortableBinaryFunctionAdapter<bool, T, U>;
  CEL_RETURN_IF_ERROR(registry->Register(FunctionAdapter::Create(
      builtin::kLess, /*receiver_style=*/false, &CrossNumericLessThan<T, U>)));
  CEL_RETURN_IF_ERROR(registry->Register(
      FunctionAdapter::Create(builtin::kGreater, /*receiver_style=*/false,
                              &CrossNumericGreaterThan<T, U>)));
  CEL_RETURN_IF_ERROR(registry->Register(FunctionAdapter::Create(
      builtin::kGreaterOrEqual, /*receiver_style=*/false,
      &CrossNumericGreaterOrEqualTo<T, U>)));
  CEL_RETURN_IF_ERROR(registry->Register(
      FunctionAdapter::Create(builtin::kLessOrEqual, /*receiver_style=*/false,
                              &CrossNumericLessOrEqualTo<T, U>)));
  return absl::OkStatus();
}

absl::Status RegisterHeterogeneousComparisonFunctions(
    CelFunctionRegistry* registry) {

  CEL_RETURN_IF_ERROR(
      (RegisterCrossNumericComparisons<double, int64_t>(registry)));
  CEL_RETURN_IF_ERROR(
      (RegisterCrossNumericComparisons<double, uint64_t>(registry)));

  CEL_RETURN_IF_ERROR(
      (RegisterCrossNumericComparisons<uint64_t, double>(registry)));
  CEL_RETURN_IF_ERROR(
      (RegisterCrossNumericComparisons<uint64_t, int64_t>(registry)));

  CEL_RETURN_IF_ERROR(
      (RegisterCrossNumericComparisons<int64_t, double>(registry)));
  CEL_RETURN_IF_ERROR(
      (RegisterCrossNumericComparisons<int64_t, uint64_t>(registry)));

  CEL_RETURN_IF_ERROR(RegisterOrderingFunctionsForType<bool>(registry));
  CEL_RETURN_IF_ERROR(RegisterOrderingFunctionsForType<int64_t>(registry));
  CEL_RETURN_IF_ERROR(RegisterOrderingFunctionsForType<uint64_t>(registry));
  CEL_RETURN_IF_ERROR(RegisterOrderingFunctionsForType<double>(registry));
  CEL_RETURN_IF_ERROR(
      RegisterOrderingFunctionsForType<CelValue::StringHolder>(registry));
  CEL_RETURN_IF_ERROR(
      RegisterOrderingFunctionsForType<CelValue::BytesHolder>(registry));
  CEL_RETURN_IF_ERROR(
      RegisterOrderingFunctionsForType<absl::Duration>(registry));
  CEL_RETURN_IF_ERROR(RegisterOrderingFunctionsForType<absl::Time>(registry));

  return absl::OkStatus();
}
}  // namespace


absl::Status RegisterComparisonFunctions(CelFunctionRegistry* registry,
                                         const InterpreterOptions& options) {
  if (options.enable_heterogeneous_equality) {
    // Heterogeneous equality uses one generic overload that delegates to the
    // right equality implementation at runtime.
    CEL_RETURN_IF_ERROR(RegisterHeterogeneousComparisonFunctions(registry));
  } else {
    CEL_RETURN_IF_ERROR(RegisterHomogenousComparisonFunctions(registry));
  }
  return absl::OkStatus();
}

}  // namespace google::api::expr::runtime
