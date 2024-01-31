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

#include "runtime/standard/container_membership_functions.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "base/builtins.h"
#include "base/function_adapter.h"
#include "base/handle.h"
#include "base/value.h"
#include "common/value.h"
#include "internal/number.h"
#include "internal/status_macros.h"
#include "runtime/function_registry.h"
#include "runtime/register_function_helper.h"
#include "runtime/runtime_options.h"
#include "runtime/standard/equality_functions.h"

namespace cel {
namespace {

using ::cel::internal::Number;

static constexpr std::array<absl::string_view, 3> in_operators = {
    cel::builtin::kIn,            // @in for map and list types.
    cel::builtin::kInFunction,    // deprecated in() -- for backwards compat
    cel::builtin::kInDeprecated,  // deprecated _in_ -- for backwards compat
};

template <class T>
bool ValueEquals(ValueView value, T other);

template <>
bool ValueEquals(ValueView value, bool other) {
  if (auto bool_value = As<BoolValueView>(value); bool_value) {
    return bool_value->NativeValue() == other;
  }
  return false;
}

template <>
bool ValueEquals(ValueView value, int64_t other) {
  if (auto int_value = As<IntValueView>(value); int_value) {
    return int_value->NativeValue() == other;
  }
  return false;
}

template <>
bool ValueEquals(ValueView value, uint64_t other) {
  if (auto uint_value = As<UintValueView>(value); uint_value) {
    return uint_value->NativeValue() == other;
  }
  return false;
}

template <>
bool ValueEquals(ValueView value, double other) {
  if (auto double_value = As<DoubleValueView>(value); double_value) {
    return double_value->NativeValue() == other;
  }
  return false;
}

template <>
bool ValueEquals(ValueView value, const StringValue& other) {
  if (auto string_value = As<StringValueView>(value); string_value) {
    return string_value->Equals(other);
  }
  return false;
}

template <>
bool ValueEquals(ValueView value, const BytesValue& other) {
  if (auto bytes_value = As<BytesValueView>(value); bytes_value) {
    return bytes_value->Equals(other);
  }
  return false;
}

// Template function implementing CEL in() function
template <typename T>
absl::StatusOr<bool> In(ValueManager& value_factory, T value,
                        const ListValue& list) {
  size_t size = list.Size();
  Value element_scratch;
  for (int i = 0; i < size; i++) {
    CEL_ASSIGN_OR_RETURN(ValueView element,
                         list.Get(value_factory, i, element_scratch));
    if (ValueEquals<T>(element, value)) {
      return true;
    }
  }

  return false;
}

// Implementation for @in operator using heterogeneous equality.
absl::StatusOr<Handle<Value>> HeterogeneousEqualityIn(
    ValueManager& value_factory, const Handle<Value>& value,
    const ListValue& list) {
  return list.Contains(value_factory, value);
}

absl::Status RegisterListMembershipFunctions(FunctionRegistry& registry,
                                             const RuntimeOptions& options) {
  for (absl::string_view op : in_operators) {
    if (options.enable_heterogeneous_equality) {
      CEL_RETURN_IF_ERROR(
          (RegisterHelper<
              BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>,
                                    const Handle<Value>&, const ListValue&>>::
               RegisterGlobalOverload(op, &HeterogeneousEqualityIn, registry)));
    } else {
      CEL_RETURN_IF_ERROR(
          (RegisterHelper<BinaryFunctionAdapter<absl::StatusOr<bool>, bool,
                                                const ListValue&>>::
               RegisterGlobalOverload(op, In<bool>, registry)));
      CEL_RETURN_IF_ERROR(
          (RegisterHelper<BinaryFunctionAdapter<absl::StatusOr<bool>, int64_t,
                                                const ListValue&>>::
               RegisterGlobalOverload(op, In<int64_t>, registry)));
      CEL_RETURN_IF_ERROR(
          (RegisterHelper<BinaryFunctionAdapter<absl::StatusOr<bool>, uint64_t,
                                                const ListValue&>>::
               RegisterGlobalOverload(op, In<uint64_t>, registry)));
      CEL_RETURN_IF_ERROR(
          (RegisterHelper<BinaryFunctionAdapter<absl::StatusOr<bool>, double,
                                                const ListValue&>>::
               RegisterGlobalOverload(op, In<double>, registry)));
      CEL_RETURN_IF_ERROR(
          (RegisterHelper<BinaryFunctionAdapter<
               absl::StatusOr<bool>, const StringValue&, const ListValue&>>::
               RegisterGlobalOverload(op, In<const StringValue&>, registry)));
      CEL_RETURN_IF_ERROR(
          (RegisterHelper<BinaryFunctionAdapter<
               absl::StatusOr<bool>, const BytesValue&, const ListValue&>>::
               RegisterGlobalOverload(op, In<const BytesValue&>, registry)));
    }
  }
  return absl::OkStatus();
}

absl::Status RegisterMapMembershipFunctions(FunctionRegistry& registry,
                                            const RuntimeOptions& options) {
  const bool enable_heterogeneous_equality =
      options.enable_heterogeneous_equality;

  auto boolKeyInSet =
      [enable_heterogeneous_equality](
          ValueManager& factory, bool key,
          const MapValue& map_value) -> absl::StatusOr<Handle<Value>> {
    auto result = map_value.Has(factory, factory.CreateBoolValue(key));
    if (result.ok()) {
      return std::move(*result);
    }
    if (enable_heterogeneous_equality) {
      return factory.CreateBoolValue(false);
    }
    return factory.CreateErrorValue(result.status());
  };

  auto intKeyInSet =
      [enable_heterogeneous_equality](
          ValueManager& factory, int64_t key,
          const MapValue& map_value) -> absl::StatusOr<Handle<Value>> {
    Handle<Value> int_key = factory.CreateIntValue(key);
    auto result = map_value.Has(factory, int_key);
    if (enable_heterogeneous_equality) {
      if (result.ok() && (*result)->Is<BoolValue>() &&
          (*result)->As<BoolValue>().NativeValue()) {
        return std::move(*result);
      }
      Number number = Number::FromInt64(key);
      if (number.LosslessConvertibleToUint()) {
        const auto& result =
            map_value.Has(factory, factory.CreateUintValue(number.AsUint()));
        if (result.ok() && (*result)->Is<BoolValue>() &&
            (*result)->As<BoolValue>().NativeValue()) {
          return std::move(*result);
        }
      }
      return factory.CreateBoolValue(false);
    }
    if (!result.ok()) {
      return factory.CreateErrorValue(result.status());
    }
    return std::move(*result);
  };

  auto stringKeyInSet =
      [enable_heterogeneous_equality](
          ValueManager& factory, const Handle<StringValue>& key,
          const MapValue& map_value) -> absl::StatusOr<Handle<Value>> {
    auto result = map_value.Has(factory, key);
    if (result.ok()) {
      return std::move(*result);
    }
    if (enable_heterogeneous_equality) {
      return factory.CreateBoolValue(false);
    }
    return factory.CreateErrorValue(result.status());
  };

  auto uintKeyInSet =
      [enable_heterogeneous_equality](
          ValueManager& factory, uint64_t key,
          const MapValue& map_value) -> absl::StatusOr<Handle<Value>> {
    Handle<Value> uint_key = factory.CreateUintValue(key);
    const auto& result = map_value.Has(factory, uint_key);
    if (enable_heterogeneous_equality) {
      if (result.ok() && (*result)->Is<BoolValue>() &&
          (*result)->As<BoolValue>().NativeValue()) {
        return std::move(*result);
      }
      Number number = Number::FromUint64(key);
      if (number.LosslessConvertibleToInt()) {
        const auto& result =
            map_value.Has(factory, factory.CreateIntValue(number.AsInt()));
        if (result.ok() && (*result)->Is<BoolValue>() &&
            (*result)->As<BoolValue>().NativeValue()) {
          return std::move(*result);
        }
      }
      return factory.CreateBoolValue(false);
    }
    if (!result.ok()) {
      return factory.CreateErrorValue(result.status());
    }
    return std::move(*result);
  };

  auto doubleKeyInSet =
      [](ValueManager& factory, double key,
         const MapValue& map_value) -> absl::StatusOr<Handle<Value>> {
    Number number = Number::FromDouble(key);
    if (number.LosslessConvertibleToInt()) {
      const auto& result =
          map_value.Has(factory, factory.CreateIntValue(number.AsInt()));
      if (result.ok() && (*result)->Is<BoolValue>() &&
          (*result)->As<BoolValue>().NativeValue()) {
        return std::move(*result);
      }
    }
    if (number.LosslessConvertibleToUint()) {
      const auto& result =
          map_value.Has(factory, factory.CreateUintValue(number.AsUint()));
      if (result.ok() && (*result)->Is<BoolValue>() &&
          (*result)->As<BoolValue>().NativeValue()) {
        return std::move(*result);
      }
    }
    return factory.CreateBoolValue(false);
  };

  for (auto op : in_operators) {
    auto status = RegisterHelper<BinaryFunctionAdapter<
        absl::StatusOr<Handle<Value>>, const Handle<StringValue>&,
        const MapValue&>>::RegisterGlobalOverload(op, stringKeyInSet, registry);
    if (!status.ok()) return status;

    status = RegisterHelper<BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>,
                                                  bool, const MapValue&>>::
        RegisterGlobalOverload(op, boolKeyInSet, registry);
    if (!status.ok()) return status;

    status = RegisterHelper<BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>,
                                                  int64_t, const MapValue&>>::
        RegisterGlobalOverload(op, intKeyInSet, registry);
    if (!status.ok()) return status;

    status = RegisterHelper<BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>,
                                                  uint64_t, const MapValue&>>::
        RegisterGlobalOverload(op, uintKeyInSet, registry);
    if (!status.ok()) return status;

    if (enable_heterogeneous_equality) {
      status =
          RegisterHelper<BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>,
                                               double, const MapValue&>>::
              RegisterGlobalOverload(op, doubleKeyInSet, registry);
      if (!status.ok()) return status;
    }
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status RegisterContainerMembershipFunctions(
    FunctionRegistry& registry, const RuntimeOptions& options) {
  if (options.enable_list_contains) {
    CEL_RETURN_IF_ERROR(RegisterListMembershipFunctions(registry, options));
  }
  return RegisterMapMembershipFunctions(registry, options);
}

}  // namespace cel
