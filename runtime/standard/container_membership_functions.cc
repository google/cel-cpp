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

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "base/builtins.h"
#include "base/function_adapter.h"
#include "base/handle.h"
#include "base/value.h"
#include "base/values/bool_value.h"
#include "base/values/double_value.h"
#include "base/values/int_value.h"
#include "base/values/uint_value.h"
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
bool ValueEquals(const Handle<Value>& value, T other);

template <>
bool ValueEquals(const Handle<Value>& value, bool other) {
  return value->Is<BoolValue>() && value->As<BoolValue>().value() == other;
}

template <>
bool ValueEquals(const Handle<Value>& value, int64_t other) {
  return value->Is<IntValue>() && (value->As<IntValue>().value() == other);
}

template <>
bool ValueEquals(const Handle<Value>& value, uint64_t other) {
  return value->Is<UintValue>() && (value->As<UintValue>().value() == other);
}

template <>
bool ValueEquals(const Handle<Value>& value, double other) {
  return value->Is<DoubleValue>() &&
         (value->As<DoubleValue>().value() == other);
}

template <>
bool ValueEquals(const Handle<Value>& value, const StringValue& other) {
  return value->Is<StringValue>() && (value->As<StringValue>().Equals(other));
}

template <>
bool ValueEquals(const Handle<Value>& value, const BytesValue& other) {
  return value->Is<BytesValue>() && (value->As<BytesValue>().Equals(other));
}

// Template function implementing CEL in() function
template <typename T>
absl::StatusOr<bool> In(ValueFactory& value_factory, T value,
                        const ListValue& list) {
  size_t size = list.size();
  for (int i = 0; i < size; i++) {
    CEL_ASSIGN_OR_RETURN(Handle<Value> element, list.Get(value_factory, i));
    if (ValueEquals<T>(element, value)) {
      return true;
    }
  }

  return false;
}

// Implementation for @in operator using heterogeneous equality.
absl::StatusOr<Handle<Value>> HeterogeneousEqualityIn(
    ValueFactory& value_factory, const Handle<Value>& value,
    const ListValue& list) {
  // TODO(uncreated-issue/55): the generic cel::ListValue::Contains() is almost
  // identical to the in function implementation. It should be possible to
  // consolidate, but separating the value type migration from swapping the
  // equals implementation to isolate the two changes. For the sake of
  // migration, LegacyLists use a special implementation of Contains that
  // operate using the CelValueEqualsImpl to minimize the number of modern <->
  // legacy conversions.
  if (list.Is<base_internal::LegacyListValue>()) {
    return list.Contains(value_factory, value);
  }
  CEL_ASSIGN_OR_RETURN(
      bool exists,
      list.AnyOf(value_factory,
                 [&value, &value_factory](
                     const Handle<Value>& elem) -> absl::StatusOr<bool> {
                   CEL_ASSIGN_OR_RETURN(absl::optional<bool> element_equals,
                                        runtime_internal::ValueEqualImpl(
                                            value_factory, elem, value));

                   // If equality is undefined, just consider the comparison
                   // as false and continue searching (as opposed to returning
                   // error as in element-wise equal).
                   if (!element_equals.has_value()) {
                     return false;  // continue
                   }
                   return *element_equals;
                 }));
  return value_factory.CreateBoolValue(exists);
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
          ValueFactory& factory, bool key,
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
          ValueFactory& factory, int64_t key,
          const MapValue& map_value) -> absl::StatusOr<Handle<Value>> {
    Handle<Value> int_key = factory.CreateIntValue(key);
    auto result = map_value.Has(factory, int_key);
    if (enable_heterogeneous_equality) {
      if (result.ok() && (*result)->Is<BoolValue>() &&
          (*result)->As<BoolValue>().value()) {
        return std::move(*result);
      }
      Number number = Number::FromInt64(key);
      if (number.LosslessConvertibleToUint()) {
        const auto& result =
            map_value.Has(factory, factory.CreateUintValue(number.AsUint()));
        if (result.ok() && (*result)->Is<BoolValue>() &&
            (*result)->As<BoolValue>().value()) {
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
          ValueFactory& factory, const Handle<StringValue>& key,
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
          ValueFactory& factory, uint64_t key,
          const MapValue& map_value) -> absl::StatusOr<Handle<Value>> {
    Handle<Value> uint_key = factory.CreateUintValue(key);
    const auto& result = map_value.Has(factory, uint_key);
    if (enable_heterogeneous_equality) {
      if (result.ok() && (*result)->Is<BoolValue>() &&
          (*result)->As<BoolValue>().value()) {
        return std::move(*result);
      }
      Number number = Number::FromUint64(key);
      if (number.LosslessConvertibleToInt()) {
        const auto& result =
            map_value.Has(factory, factory.CreateIntValue(number.AsInt()));
        if (result.ok() && (*result)->Is<BoolValue>() &&
            (*result)->As<BoolValue>().value()) {
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
      [](ValueFactory& factory, double key,
         const MapValue& map_value) -> absl::StatusOr<Handle<Value>> {
    Number number = Number::FromDouble(key);
    if (number.LosslessConvertibleToInt()) {
      const auto& result =
          map_value.Has(factory, factory.CreateIntValue(number.AsInt()));
      if (result.ok() && (*result)->Is<BoolValue>() &&
          (*result)->As<BoolValue>().value()) {
        return std::move(*result);
      }
    }
    if (number.LosslessConvertibleToUint()) {
      const auto& result =
          map_value.Has(factory, factory.CreateUintValue(number.AsUint()));
      if (result.ok() && (*result)->Is<BoolValue>() &&
          (*result)->As<BoolValue>().value()) {
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
