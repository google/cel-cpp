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

#include "runtime/standard/equality_functions.h"

#include <cstdint>
#include <functional>
#include <optional>
#include <utility>

#include "absl/base/macros.h"
#include "absl/functional/function_ref.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "base/builtins.h"
#include "base/function_adapter.h"
#include "base/handle.h"
#include "base/kind.h"
#include "base/type.h"
#include "base/types/list_type.h"
#include "base/value_factory.h"
#include "base/values/bool_value.h"
#include "base/values/bytes_value.h"
#include "base/values/double_value.h"
#include "base/values/duration_value.h"
#include "base/values/error_value.h"
#include "base/values/int_value.h"
#include "base/values/map_value.h"
#include "base/values/null_value.h"
#include "base/values/string_value.h"
#include "base/values/struct_value.h"
#include "base/values/timestamp_value.h"
#include "base/values/type_value.h"
#include "base/values/uint_value.h"
#include "base/values/unknown_value.h"
#include "internal/number.h"
#include "internal/status_macros.h"
#include "runtime/function_registry.h"
#include "runtime/internal/errors.h"
#include "runtime/register_function_helper.h"
#include "runtime/runtime_options.h"

namespace cel {
namespace {

using ::cel::builtin::kEqual;
using ::cel::builtin::kInequal;
using ::cel::internal::Number;

// Declaration for the functors for generic equality operator.
// Equal only defined for same-typed values.
// Nullopt is returned if equality is not defined.
struct HomogenousEqualProvider {
  static constexpr bool kIsHeterogeneous = false;
  absl::StatusOr<absl::optional<bool>> operator()(
      ValueFactory& value_factory, const Handle<Value>& lhs,
      const Handle<Value>& rhs) const;
};

// Equal defined between compatible types.
// Nullopt is returned if equality is not defined.
struct HeterogeneousEqualProvider {
  static constexpr bool kIsHeterogeneous = true;

  absl::StatusOr<absl::optional<bool>> operator()(
      ValueFactory& value_factory, const Handle<Value>& lhs,
      const Handle<Value>& rhs) const;
};

// Comparison template functions
template <class Type>
absl::optional<bool> Inequal(Type lhs, Type rhs) {
  return lhs != rhs;
}

template <>
absl::optional<bool> Inequal(const StringValue& lhs, const StringValue& rhs) {
  return !lhs.Equals(rhs);
}

template <>
absl::optional<bool> Inequal(const BytesValue& lhs, const BytesValue& rhs) {
  return !lhs.Equals(rhs);
}

template <>
absl::optional<bool> Inequal(const NullValue&, const NullValue&) {
  return false;
}

template <>
absl::optional<bool> Inequal(const TypeValue& lhs, const TypeValue& rhs) {
  return lhs.name() != rhs.name();
}

template <class Type>
absl::optional<bool> Equal(Type lhs, Type rhs) {
  return lhs == rhs;
}

template <>
absl::optional<bool> Equal(const StringValue& lhs, const StringValue& rhs) {
  return lhs.Equals(rhs);
}

template <>
absl::optional<bool> Equal(const BytesValue& lhs, const BytesValue& rhs) {
  return lhs.Equals(rhs);
}

template <>
absl::optional<bool> Equal(const NullValue&, const NullValue&) {
  return true;
}

template <>
absl::optional<bool> Equal(const TypeValue& lhs, const TypeValue& rhs) {
  return lhs.name() == rhs.name();
}

// Equality for lists. Template parameter provides either heterogeneous or
// homogenous equality for comparing members.
template <typename EqualsProvider>
absl::StatusOr<absl::optional<bool>> ListEqual(ValueFactory& factory,
                                               const ListValue& lhs,
                                               const ListValue& rhs) {
  if (&lhs == &rhs) {
    return true;
  }
  if (lhs.Size() != rhs.Size()) {
    return false;
  }

  for (int i = 0; i < lhs.Size(); ++i) {
    CEL_ASSIGN_OR_RETURN(auto lhs_i, lhs.Get(factory, i));
    CEL_ASSIGN_OR_RETURN(auto rhs_i, rhs.Get(factory, i));
    CEL_ASSIGN_OR_RETURN(absl::optional<bool> eq,
                         EqualsProvider()(factory, lhs_i, rhs_i));
    if (!eq.has_value() || !*eq) {
      return eq;
    }
  }
  return true;
}

absl::optional<Number> NumberFromValue(const Value& value) {
  if (value.Is<IntValue>()) {
    return Number::FromInt64(value.As<IntValue>().NativeValue());
  } else if (value.Is<UintValue>()) {
    return Number::FromUint64(value.As<UintValue>().NativeValue());
  } else if (value.Is<DoubleValue>()) {
    return Number::FromDouble(value.As<DoubleValue>().NativeValue());
  }

  return absl::nullopt;
}

absl::StatusOr<absl::optional<Handle<Value>>> CheckAlternativeNumericType(
    ValueFactory& value_factory, const Handle<Value>& key,
    const MapValue& rhs) {
  absl::optional<Number> number = NumberFromValue(*key);

  if (!number.has_value()) {
    return absl::nullopt;
  }

  if (!key->Is<IntValue>() && number->LosslessConvertibleToInt()) {
    Handle<Value> entry;
    bool ok;
    CEL_ASSIGN_OR_RETURN(
        std::tie(entry, ok),
        rhs.Find(value_factory, value_factory.CreateIntValue(number->AsInt())));
    if (ok) {
      return entry;
    }
  }

  if (!key->Is<UintValue>() && number->LosslessConvertibleToUint()) {
    Handle<Value> entry;
    bool ok;
    CEL_ASSIGN_OR_RETURN(std::tie(entry, ok),
                         rhs.Find(value_factory, value_factory.CreateUintValue(
                                                     number->AsUint())));
    if (ok) {
      return entry;
    }
  }

  return absl::nullopt;
}

// Equality for maps. Template parameter provides either heterogeneous or
// homogenous equality for comparing values.
template <typename EqualsProvider>
absl::StatusOr<absl::optional<bool>> MapEqual(ValueFactory& value_factory,
                                              const MapValue& lhs,
                                              const MapValue& rhs) {
  if (&lhs == &rhs) {
    return true;
  }
  if (lhs.size() != rhs.size()) {
    return false;
  }

  CEL_ASSIGN_OR_RETURN(auto iter, lhs.NewIterator(value_factory));

  while (iter->HasNext()) {
    CEL_ASSIGN_OR_RETURN(auto lhs_key, iter->Next());

    Handle<Value> rhs_value;
    bool rhs_ok;
    CEL_ASSIGN_OR_RETURN(std::tie(rhs_value, rhs_ok),
                         rhs.Find(value_factory, lhs_key));

    if (!rhs_ok && EqualsProvider::kIsHeterogeneous) {
      CEL_ASSIGN_OR_RETURN(
          auto maybe_rhs_value,
          CheckAlternativeNumericType(value_factory, lhs_key, rhs));
      rhs_ok = maybe_rhs_value.has_value();
      if (rhs_ok) {
        rhs_value = std::move(*maybe_rhs_value);
      }
    }
    if (!rhs_ok) {
      return false;
    }

    CEL_ASSIGN_OR_RETURN(auto lhs_value, lhs.Get(value_factory, lhs_key));
    CEL_ASSIGN_OR_RETURN(absl::optional<bool> eq,
                         EqualsProvider()(value_factory, lhs_value, rhs_value));

    if (!eq.has_value() || !*eq) {
      return eq;
    }
  }

  return true;
}

// Helper for wrapping ==/!= implementations.
// Name should point to a static constexpr string so the lambda capture is safe.
template <typename Type, typename Op>
std::function<Handle<Value>(cel::ValueFactory& factory, Type, Type)>
WrapComparison(Op op, absl::string_view name) {
  return [op = std::move(op), name](cel::ValueFactory& factory, Type lhs,
                                    Type rhs) -> Handle<Value> {
    absl::optional<bool> result = op(lhs, rhs);

    if (result.has_value()) {
      return factory.CreateBoolValue(*result);
    }

    return factory.CreateErrorValue(
        cel::runtime_internal::CreateNoMatchingOverloadError(name));
  };
}

// Helper method
//
// Registers all equality functions for template parameters type.
template <class Type>
absl::Status RegisterEqualityFunctionsForType(cel::FunctionRegistry& registry) {
  using FunctionAdapter =
      cel::RegisterHelper<BinaryFunctionAdapter<Handle<Value>, Type, Type>>;
  // Inequality
  CEL_RETURN_IF_ERROR(FunctionAdapter::RegisterGlobalOverload(
      kInequal, WrapComparison<Type>(&Inequal<Type>, kInequal), registry));

  // Equality
  CEL_RETURN_IF_ERROR(FunctionAdapter::RegisterGlobalOverload(
      kEqual, WrapComparison<Type>(&Equal<Type>, kEqual), registry));

  return absl::OkStatus();
}

template <typename Type, typename Op>
auto ComplexEquality(Op&& op) {
  return [op = std::forward<Op>(op)](
             cel::ValueFactory& f, const Type& t1,
             const Type& t2) -> absl::StatusOr<Handle<Value>> {
    CEL_ASSIGN_OR_RETURN(absl::optional<bool> result, op(f, t1, t2));
    if (!result.has_value()) {
      return f.CreateErrorValue(
          cel::runtime_internal::CreateNoMatchingOverloadError(kEqual));
    }
    return f.CreateBoolValue(*result);
  };
}

template <typename Type, typename Op>
auto ComplexInequality(Op&& op) {
  return [op = std::forward<Op>(op)](cel::ValueFactory& f, Type t1,
                                     Type t2) -> absl::StatusOr<Handle<Value>> {
    CEL_ASSIGN_OR_RETURN(absl::optional<bool> result, op(f, t1, t2));
    if (!result.has_value()) {
      return f.CreateErrorValue(
          cel::runtime_internal::CreateNoMatchingOverloadError(kInequal));
    }
    return f.CreateBoolValue(!*result);
  };
}

template <class Type>
absl::Status RegisterComplexEqualityFunctionsForType(
    absl::FunctionRef<absl::StatusOr<absl::optional<bool>>(ValueFactory&, Type,
                                                           Type)>
        op,
    cel::FunctionRegistry& registry) {
  using FunctionAdapter = cel::RegisterHelper<
      BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>, Type, Type>>;
  // Inequality
  CEL_RETURN_IF_ERROR(FunctionAdapter::RegisterGlobalOverload(
      kInequal, ComplexInequality<Type>(op), registry));

  // Equality
  CEL_RETURN_IF_ERROR(FunctionAdapter::RegisterGlobalOverload(
      kEqual, ComplexEquality<Type>(op), registry));

  return absl::OkStatus();
}

absl::Status RegisterHomogenousEqualityFunctions(
    cel::FunctionRegistry& registry) {
  CEL_RETURN_IF_ERROR(RegisterEqualityFunctionsForType<bool>(registry));

  CEL_RETURN_IF_ERROR(RegisterEqualityFunctionsForType<int64_t>(registry));

  CEL_RETURN_IF_ERROR(RegisterEqualityFunctionsForType<uint64_t>(registry));

  CEL_RETURN_IF_ERROR(RegisterEqualityFunctionsForType<double>(registry));

  CEL_RETURN_IF_ERROR(
      RegisterEqualityFunctionsForType<const cel::StringValue&>(registry));

  CEL_RETURN_IF_ERROR(
      RegisterEqualityFunctionsForType<const cel::BytesValue&>(registry));

  CEL_RETURN_IF_ERROR(
      RegisterEqualityFunctionsForType<absl::Duration>(registry));

  CEL_RETURN_IF_ERROR(RegisterEqualityFunctionsForType<absl::Time>(registry));

  CEL_RETURN_IF_ERROR(
      RegisterEqualityFunctionsForType<const cel::NullValue&>(registry));

  CEL_RETURN_IF_ERROR(
      RegisterEqualityFunctionsForType<const cel::TypeValue&>(registry));

  CEL_RETURN_IF_ERROR(
      RegisterComplexEqualityFunctionsForType<const cel::ListValue&>(
          &ListEqual<HomogenousEqualProvider>, registry));

  CEL_RETURN_IF_ERROR(
      RegisterComplexEqualityFunctionsForType<const cel::MapValue&>(
          &MapEqual<HomogenousEqualProvider>, registry));

  return absl::OkStatus();
}

absl::Status RegisterNullMessageEqualityFunctions(FunctionRegistry& registry) {
  // equals
  CEL_RETURN_IF_ERROR(
      (cel::RegisterHelper<
          BinaryFunctionAdapter<bool, const StructValue&, const NullValue&>>::
           RegisterGlobalOverload(
               kEqual,
               [](ValueFactory&, const StructValue&, const NullValue&) {
                 return false;
               },
               registry)));

  CEL_RETURN_IF_ERROR(
      (cel::RegisterHelper<
          BinaryFunctionAdapter<bool, const NullValue&, const StructValue&>>::
           RegisterGlobalOverload(
               kEqual,
               [](ValueFactory&, const NullValue&, const StructValue&) {
                 return false;
               },
               registry)));

  // inequals
  CEL_RETURN_IF_ERROR(
      (cel::RegisterHelper<
          BinaryFunctionAdapter<bool, const StructValue&, const NullValue&>>::
           RegisterGlobalOverload(
               kInequal,
               [](ValueFactory&, const StructValue&, const NullValue&) {
                 return true;
               },
               registry)));

  return cel::RegisterHelper<
      BinaryFunctionAdapter<bool, const NullValue&, const StructValue&>>::
      RegisterGlobalOverload(
          kInequal,
          [](ValueFactory&, const NullValue&, const StructValue&) {
            return true;
          },
          registry);
}

template <typename EqualsProvider>
absl::StatusOr<absl::optional<bool>> HomogenousValueEqual(
    ValueFactory& factory, const Handle<Value>& v1, const Handle<Value>& v2) {
  if (v1->kind() != v2->kind()) {
    return absl::nullopt;
  }

  switch (v1->kind()) {
    case ValueKind::kBool:
      return Equal<bool>(v1->As<BoolValue>().NativeValue(),
                         v2->As<BoolValue>().NativeValue());
    case ValueKind::kNull:
      return Equal<const NullValue&>(v1->As<NullValue>(), v2->As<NullValue>());
    case ValueKind::kInt:
      return Equal<int64_t>(v1->As<IntValue>().NativeValue(),
                            v2->As<IntValue>().NativeValue());
    case ValueKind::kUint:
      return Equal<uint64_t>(v1->As<UintValue>().NativeValue(),
                             v2->As<UintValue>().NativeValue());
    case ValueKind::kDouble:
      return Equal<double>(v1->As<DoubleValue>().NativeValue(),
                           v2->As<DoubleValue>().NativeValue());
    case ValueKind::kDuration:
      return Equal<absl::Duration>(v1->As<DurationValue>().NativeValue(),
                                   v2->As<DurationValue>().NativeValue());
    case ValueKind::kTimestamp:
      return Equal<absl::Time>(v1->As<TimestampValue>().NativeValue(),
                               v2->As<TimestampValue>().NativeValue());
    case ValueKind::kCelType:
      return Equal<const TypeValue&>(v1->As<TypeValue>(), v2->As<TypeValue>());
    case ValueKind::kString:
      return Equal<const StringValue&>(v1->As<StringValue>(),
                                       v2->As<StringValue>());
    case ValueKind::kBytes:
      return Equal<const cel::BytesValue&>(v1->As<cel::BytesValue>(),
                                           v2->As<cel::BytesValue>());
    case ValueKind::kList:
      return ListEqual<EqualsProvider>(factory, v1->As<ListValue>(),
                                       v2->As<ListValue>());
    case ValueKind::kMap:
      return MapEqual<EqualsProvider>(factory, v1->As<MapValue>(),
                                      v2->As<MapValue>());
    default:

      return absl::nullopt;
  }
}

absl::StatusOr<Handle<Value>> EqualOverloadImpl(ValueFactory& factory,
                                                const Handle<Value>& lhs,
                                                const Handle<Value>& rhs) {
  CEL_ASSIGN_OR_RETURN(absl::optional<bool> result,
                       runtime_internal::ValueEqualImpl(factory, lhs, rhs));
  if (result.has_value()) {
    return factory.CreateBoolValue(*result);
  }
  return factory.CreateErrorValue(
      cel::runtime_internal::CreateNoMatchingOverloadError(kEqual));
}

absl::StatusOr<Handle<Value>> InequalOverloadImpl(ValueFactory& factory,
                                                  const Handle<Value>& lhs,
                                                  const Handle<Value>& rhs) {
  CEL_ASSIGN_OR_RETURN(absl::optional<bool> result,
                       runtime_internal::ValueEqualImpl(factory, lhs, rhs));
  if (result.has_value()) {
    return factory.CreateBoolValue(!*result);
  }
  return factory.CreateErrorValue(
      cel::runtime_internal::CreateNoMatchingOverloadError(kInequal));
}

absl::Status RegisterHeterogeneousEqualityFunctions(
    cel::FunctionRegistry& registry) {
  using Adapter = cel::RegisterHelper<
      BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>, const Handle<Value>&,
                            const Handle<Value>&>>;
  CEL_RETURN_IF_ERROR(
      Adapter::RegisterGlobalOverload(kEqual, &EqualOverloadImpl, registry));

  CEL_RETURN_IF_ERROR(Adapter::RegisterGlobalOverload(
      kInequal, &InequalOverloadImpl, registry));

  return absl::OkStatus();
}

absl::StatusOr<absl::optional<bool>> HomogenousEqualProvider::operator()(
    ValueFactory& factory, const Handle<Value>& lhs,
    const Handle<Value>& rhs) const {
  return HomogenousValueEqual<HomogenousEqualProvider>(factory, lhs, rhs);
}

absl::StatusOr<absl::optional<bool>> HeterogeneousEqualProvider::operator()(
    ValueFactory& factory, const Handle<Value>& lhs,
    const Handle<Value>& rhs) const {
  return runtime_internal::ValueEqualImpl(factory, lhs, rhs);
}

}  // namespace

namespace runtime_internal {

absl::StatusOr<absl::optional<bool>> ValueEqualImpl(ValueFactory& value_factory,
                                                    const Handle<Value>& v1,
                                                    const Handle<Value>& v2) {
  if (v1->kind() == v2->kind()) {
    if (v1->Is<StructValue>() && v2->Is<StructValue>()) {
      CEL_ASSIGN_OR_RETURN(
          Handle<Value> result,
          v1->As<StructValue>().Equals(value_factory, v2->As<StructValue>()));
      if (result->Is<BoolValue>()) {
        return result->As<BoolValue>().NativeValue();
      }
      return false;
    }
    return HomogenousValueEqual<HeterogeneousEqualProvider>(value_factory, v1,
                                                            v2);
  }

  absl::optional<Number> lhs = NumberFromValue(*v1);
  absl::optional<Number> rhs = NumberFromValue(*v2);

  if (rhs.has_value() && lhs.has_value()) {
    return *lhs == *rhs;
  }

  // TODO(uncreated-issue/6): It's currently possible for the interpreter to create a
  // map containing an Error. Return no matching overload to propagate an error
  // instead of a false result.
  if (v1->Is<ErrorValue>() || v1->Is<UnknownValue>() || v2->Is<ErrorValue>() ||
      v2->Is<UnknownValue>()) {
    return absl::nullopt;
  }

  return false;
}

}  // namespace runtime_internal

absl::Status RegisterEqualityFunctions(FunctionRegistry& registry,
                                       const RuntimeOptions& options) {
  if (options.enable_heterogeneous_equality) {
    // Heterogeneous equality uses one generic overload that delegates to the
    // right equality implementation at runtime.
    CEL_RETURN_IF_ERROR(RegisterHeterogeneousEqualityFunctions(registry));
  } else {
    CEL_RETURN_IF_ERROR(RegisterHomogenousEqualityFunctions(registry));

    CEL_RETURN_IF_ERROR(RegisterNullMessageEqualityFunctions(registry));
  }
  return absl::OkStatus();
}

}  // namespace cel
