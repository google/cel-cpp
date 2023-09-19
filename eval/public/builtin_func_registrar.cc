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

#include "eval/public/builtin_func_registrar.h"

#include <array>
#include <cstdint>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "base/builtins.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_number.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/equality_function_registrar.h"
#include "eval/public/portable_cel_function_adapter.h"
#include "internal/status_macros.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"
#include "runtime/standard/arithmetic_functions.h"
#include "runtime/standard/comparison_functions.h"
#include "runtime/standard/container_functions.h"
#include "runtime/standard/equality_functions.h"
#include "runtime/standard/logical_functions.h"
#include "runtime/standard/regex_functions.h"
#include "runtime/standard/string_functions.h"
#include "runtime/standard/time_functions.h"
#include "runtime/standard/type_conversion_functions.h"

namespace google::api::expr::runtime {

namespace {

using ::google::protobuf::Arena;

template <class T>
bool ValueEquals(const CelValue& value, T other);

template <>
bool ValueEquals(const CelValue& value, bool other) {
  return value.IsBool() && (value.BoolOrDie() == other);
}

template <>
bool ValueEquals(const CelValue& value, int64_t other) {
  return value.IsInt64() && (value.Int64OrDie() == other);
}

template <>
bool ValueEquals(const CelValue& value, uint64_t other) {
  return value.IsUint64() && (value.Uint64OrDie() == other);
}

template <>
bool ValueEquals(const CelValue& value, double other) {
  return value.IsDouble() && (value.DoubleOrDie() == other);
}

template <>
bool ValueEquals(const CelValue& value, CelValue::StringHolder other) {
  return value.IsString() && (value.StringOrDie() == other);
}

template <>
bool ValueEquals(const CelValue& value, CelValue::BytesHolder other) {
  return value.IsBytes() && (value.BytesOrDie() == other);
}

// Template function implementing CEL in() function
template <typename T>
bool In(Arena* arena, T value, const CelList* list) {
  int index_size = list->size();

  for (int i = 0; i < index_size; i++) {
    CelValue element = (*list).Get(arena, i);

    if (ValueEquals<T>(element, value)) {
      return true;
    }
  }

  return false;
}

// Implementation for @in operator using heterogeneous equality.
CelValue HeterogeneousEqualityIn(Arena* arena, CelValue value,
                                 const CelList* list) {
  int index_size = list->size();

  for (int i = 0; i < index_size; i++) {
    CelValue element = (*list).Get(arena, i);
    absl::optional<bool> element_equals = CelValueEqualImpl(element, value);

    // If equality is undefined (e.g. duration == double), just treat as false.
    if (element_equals.has_value() && *element_equals) {
      return CelValue::CreateBool(true);
    }
  }

  return CelValue::CreateBool(false);
}

absl::Status RegisterSetMembershipFunctions(CelFunctionRegistry* registry,
                                            const InterpreterOptions& options) {
  constexpr std::array<absl::string_view, 3> in_operators = {
      cel::builtin::kIn,            // @in for map and list types.
      cel::builtin::kInFunction,    // deprecated in() -- for backwards compat
      cel::builtin::kInDeprecated,  // deprecated _in_ -- for backwards compat
  };

  if (options.enable_list_contains) {
    for (absl::string_view op : in_operators) {
      if (options.enable_heterogeneous_equality) {
        CEL_RETURN_IF_ERROR(registry->Register(
            (PortableBinaryFunctionAdapter<CelValue, CelValue, const CelList*>::
                 Create(op, false, &HeterogeneousEqualityIn))));
      } else {
        CEL_RETURN_IF_ERROR(registry->Register(
            (PortableBinaryFunctionAdapter<bool, bool, const CelList*>::Create(
                op, false, In<bool>))));
        CEL_RETURN_IF_ERROR(registry->Register(
            (PortableBinaryFunctionAdapter<
                bool, int64_t, const CelList*>::Create(op, false,
                                                       In<int64_t>))));
        CEL_RETURN_IF_ERROR(registry->Register(
            PortableBinaryFunctionAdapter<
                bool, uint64_t, const CelList*>::Create(op, false,
                                                        In<uint64_t>)));
        CEL_RETURN_IF_ERROR(registry->Register(
            PortableBinaryFunctionAdapter<bool, double, const CelList*>::Create(
                op, false, In<double>)));
        CEL_RETURN_IF_ERROR(registry->Register(
            PortableBinaryFunctionAdapter<
                bool, CelValue::StringHolder,
                const CelList*>::Create(op, false,
                                        In<CelValue::StringHolder>)));
        CEL_RETURN_IF_ERROR(registry->Register(
            PortableBinaryFunctionAdapter<
                bool, CelValue::BytesHolder,
                const CelList*>::Create(op, false, In<CelValue::BytesHolder>)));
      }
    }
  }

  auto boolKeyInSet = [options](Arena* arena, bool key,
                                const CelMap* cel_map) -> CelValue {
    const auto& result = cel_map->Has(CelValue::CreateBool(key));
    if (result.ok()) {
      return CelValue::CreateBool(*result);
    }
    if (options.enable_heterogeneous_equality) {
      return CelValue::CreateBool(false);
    }
    return CreateErrorValue(arena, result.status());
  };

  auto intKeyInSet = [options](Arena* arena, int64_t key,
                               const CelMap* cel_map) -> CelValue {
    CelValue int_key = CelValue::CreateInt64(key);
    const auto& result = cel_map->Has(int_key);
    if (options.enable_heterogeneous_equality) {
      if (result.ok() && *result) {
        return CelValue::CreateBool(*result);
      }
      absl::optional<CelNumber> number = GetNumberFromCelValue(int_key);
      if (number->LosslessConvertibleToUint()) {
        const auto& result =
            cel_map->Has(CelValue::CreateUint64(number->AsUint()));
        if (result.ok() && *result) {
          return CelValue::CreateBool(*result);
        }
      }
      return CelValue::CreateBool(false);
    }
    if (!result.ok()) {
      return CreateErrorValue(arena, result.status());
    }
    return CelValue::CreateBool(*result);
  };

  auto stringKeyInSet = [options](Arena* arena, CelValue::StringHolder key,
                                  const CelMap* cel_map) -> CelValue {
    const auto& result = cel_map->Has(CelValue::CreateString(key));
    if (result.ok()) {
      return CelValue::CreateBool(*result);
    }
    if (options.enable_heterogeneous_equality) {
      return CelValue::CreateBool(false);
    }
    return CreateErrorValue(arena, result.status());
  };

  auto uintKeyInSet = [options](Arena* arena, uint64_t key,
                                const CelMap* cel_map) -> CelValue {
    CelValue uint_key = CelValue::CreateUint64(key);
    const auto& result = cel_map->Has(uint_key);
    if (options.enable_heterogeneous_equality) {
      if (result.ok() && *result) {
        return CelValue::CreateBool(*result);
      }
      absl::optional<CelNumber> number = GetNumberFromCelValue(uint_key);
      if (number->LosslessConvertibleToInt()) {
        const auto& result =
            cel_map->Has(CelValue::CreateInt64(number->AsInt()));
        if (result.ok() && *result) {
          return CelValue::CreateBool(*result);
        }
      }
      return CelValue::CreateBool(false);
    }
    if (!result.ok()) {
      return CreateErrorValue(arena, result.status());
    }
    return CelValue::CreateBool(*result);
  };

  auto doubleKeyInSet = [](Arena* arena, double key,
                           const CelMap* cel_map) -> CelValue {
    absl::optional<CelNumber> number =
        GetNumberFromCelValue(CelValue::CreateDouble(key));
    if (number->LosslessConvertibleToInt()) {
      const auto& result = cel_map->Has(CelValue::CreateInt64(number->AsInt()));
      if (result.ok() && *result) {
        return CelValue::CreateBool(*result);
      }
    }
    if (number->LosslessConvertibleToUint()) {
      const auto& result =
          cel_map->Has(CelValue::CreateUint64(number->AsUint()));
      if (result.ok() && *result) {
        return CelValue::CreateBool(*result);
      }
    }
    return CelValue::CreateBool(false);
  };

  for (auto op : in_operators) {
    auto status = registry->Register(
        PortableBinaryFunctionAdapter<CelValue, CelValue::StringHolder,
                                      const CelMap*>::Create(op, false,
                                                             stringKeyInSet));
    if (!status.ok()) return status;

    status = registry->Register(
        PortableBinaryFunctionAdapter<CelValue, bool, const CelMap*>::Create(
            op, false, boolKeyInSet));
    if (!status.ok()) return status;

    status = registry->Register(
        PortableBinaryFunctionAdapter<CelValue, int64_t, const CelMap*>::Create(
            op, false, intKeyInSet));
    if (!status.ok()) return status;

    status = registry->Register(
        PortableBinaryFunctionAdapter<CelValue, uint64_t,
                                      const CelMap*>::Create(op, false,
                                                             uintKeyInSet));
    if (!status.ok()) return status;

    if (options.enable_heterogeneous_equality) {
      status = registry->Register(
          PortableBinaryFunctionAdapter<CelValue, double,
                                        const CelMap*>::Create(op, false,
                                                               doubleKeyInSet));
      if (!status.ok()) return status;
    }
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status RegisterBuiltinFunctions(CelFunctionRegistry* registry,
                                      const InterpreterOptions& options) {
  cel::FunctionRegistry& modern_registry = registry->InternalGetRegistry();
  cel::RuntimeOptions runtime_options = ConvertToRuntimeOptions(options);

  CEL_RETURN_IF_ERROR(
      cel::RegisterLogicalFunctions(modern_registry, runtime_options));
  CEL_RETURN_IF_ERROR(
      cel::RegisterComparisonFunctions(modern_registry, runtime_options));
  CEL_RETURN_IF_ERROR(
      cel::RegisterContainerFunctions(modern_registry, runtime_options));
  CEL_RETURN_IF_ERROR(
      cel::RegisterTypeConversionFunctions(modern_registry, runtime_options));
  CEL_RETURN_IF_ERROR(
      cel::RegisterArithmeticFunctions(modern_registry, runtime_options));
  CEL_RETURN_IF_ERROR(
      cel::RegisterTimeFunctions(modern_registry, runtime_options));
  CEL_RETURN_IF_ERROR(
      cel::RegisterStringFunctions(modern_registry, runtime_options));
  CEL_RETURN_IF_ERROR(
      cel::RegisterRegexFunctions(modern_registry, runtime_options));
  CEL_RETURN_IF_ERROR(
      cel::RegisterEqualityFunctions(modern_registry, runtime_options));

  return registry->RegisterAll(
      {
          &RegisterSetMembershipFunctions,
      },
      options);
}

}  // namespace google::api::expr::runtime
