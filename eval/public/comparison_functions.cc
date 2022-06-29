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
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "eval/eval/mutable_list_impl.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_number.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/message_wrapper.h"
#include "eval/public/portable_cel_function_adapter.h"
#include "eval/public/structs/legacy_type_adapter.h"
#include "eval/public/structs/legacy_type_info_apis.h"
#include "internal/casts.h"
#include "internal/overflow.h"
#include "internal/status_macros.h"
#include "internal/time.h"
#include "internal/utf8.h"
#include "re2/re2.h"

namespace google::api::expr::runtime {

namespace {

using ::google::protobuf::Arena;

// Forward declaration of the functors for generic equality operator.
// Equal only defined for same-typed values.
struct HomogenousEqualProvider {
  absl::optional<bool> operator()(const CelValue& v1, const CelValue& v2) const;
};

// Equal defined between compatible types.
struct HeterogeneousEqualProvider {
  absl::optional<bool> operator()(const CelValue& v1, const CelValue& v2) const;
};

// Comparison template functions
template <class Type>
absl::optional<bool> Inequal(Type t1, Type t2) {
  return t1 != t2;
}

template <class Type>
absl::optional<bool> Equal(Type t1, Type t2) {
  return t1 == t2;
}

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
absl::optional<bool> Inequal(absl::Duration t1, absl::Duration t2) {
  return absl::operator!=(t1, t2);
}

template <>
absl::optional<bool> Equal(absl::Duration t1, absl::Duration t2) {
  return absl::operator==(t1, t2);
}

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
absl::optional<bool> Inequal(absl::Time t1, absl::Time t2) {
  return absl::operator!=(t1, t2);
}

template <>
absl::optional<bool> Equal(absl::Time t1, absl::Time t2) {
  return absl::operator==(t1, t2);
}

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

bool MessageNullEqual(Arena* arena, MessageWrapper t1, CelValue::NullType) {
  // messages should never be null.
  return false;
}

bool MessageNullInequal(Arena* arena, MessageWrapper t1, CelValue::NullType) {
  // messages should never be null.
  return true;
}

// Equality for lists. Template parameter provides either heterogeneous or
// homogenous equality for comparing members.
template <typename EqualsProvider>
absl::optional<bool> ListEqual(const CelList* t1, const CelList* t2) {
  if (t1 == t2) {
    return true;
  }
  int index_size = t1->size();
  if (t2->size() != index_size) {
    return false;
  }

  for (int i = 0; i < index_size; i++) {
    CelValue e1 = (*t1)[i];
    CelValue e2 = (*t2)[i];
    absl::optional<bool> eq = EqualsProvider()(e1, e2);
    if (eq.has_value()) {
      if (!(*eq)) {
        return false;
      }
    } else {
      // Propagate that the equality is undefined.
      return eq;
    }
  }

  return true;
}

// Homogeneous CelList specific overload implementation for CEL ==.
template <>
absl::optional<bool> Equal(const CelList* t1, const CelList* t2) {
  return ListEqual<HomogenousEqualProvider>(t1, t2);
}

// Homogeneous CelList specific overload implementation for CEL !=.
template <>
absl::optional<bool> Inequal(const CelList* t1, const CelList* t2) {
  absl::optional<bool> eq = Equal<const CelList*>(t1, t2);
  if (eq.has_value()) {
    return !*eq;
  }
  return eq;
}

// Equality for maps. Template parameter provides either heterogeneous or
// homogenous equality for comparing values.
template <typename EqualsProvider>
absl::optional<bool> MapEqual(const CelMap* t1, const CelMap* t2) {
  if (t1 == t2) {
    return true;
  }
  if (t1->size() != t2->size()) {
    return false;
  }

  auto list_keys = t1->ListKeys();
  if (!list_keys.ok()) {
    return absl::nullopt;
  }
  const CelList* keys = *list_keys;
  for (int i = 0; i < keys->size(); i++) {
    CelValue key = (*keys)[i];
    CelValue v1 = (*t1)[key].value();
    absl::optional<CelValue> v2 = (*t2)[key];
    if (!v2.has_value()) {
      auto number = GetNumberFromCelValue(key);
      if (!number.has_value()) {
        return false;
      }
      if (!key.IsInt64() && number->LosslessConvertibleToInt()) {
        CelValue int_key = CelValue::CreateInt64(number->AsInt());
        absl::optional<bool> eq = EqualsProvider()(key, int_key);
        if (eq.has_value() && *eq) {
          v2 = (*t2)[int_key];
        }
      }
      if (!key.IsUint64() && !v2.has_value() &&
          number->LosslessConvertibleToUint()) {
        CelValue uint_key = CelValue::CreateUint64(number->AsUint());
        absl::optional<bool> eq = EqualsProvider()(key, uint_key);
        if (eq.has_value() && *eq) {
          v2 = (*t2)[uint_key];
        }
      }
    }
    if (!v2.has_value()) {
      return false;
    }
    absl::optional<bool> eq = EqualsProvider()(v1, *v2);
    if (!eq.has_value() || !*eq) {
      // Shortcircuit on value comparison errors and 'false' results.
      return eq;
    }
  }

  return true;
}

// Homogeneous CelMap specific overload implementation for CEL ==.
template <>
absl::optional<bool> Equal(const CelMap* t1, const CelMap* t2) {
  return MapEqual<HomogenousEqualProvider>(t1, t2);
}

// Homogeneous CelMap specific overload implementation for CEL !=.
template <>
absl::optional<bool> Inequal(const CelMap* t1, const CelMap* t2) {
  absl::optional<bool> eq = Equal<const CelMap*>(t1, t2);
  if (eq.has_value()) {
    // Propagate comparison errors.
    return !*eq;
  }
  return absl::nullopt;
}

bool MessageEqual(const CelValue::MessageWrapper& m1,
                  const CelValue::MessageWrapper& m2) {
  const LegacyTypeInfoApis* lhs_type_info = m1.legacy_type_info();
  const LegacyTypeInfoApis* rhs_type_info = m2.legacy_type_info();

  if (lhs_type_info->GetTypename(m1) != rhs_type_info->GetTypename(m2)) {
    return false;
  }

  const LegacyTypeAccessApis* accessor = lhs_type_info->GetAccessApis(m1);

  if (accessor == nullptr) {
    return false;
  }

  return accessor->IsEqualTo(m1, m2);
}

// Generic equality for CEL values of the same type.
// EqualityProvider is used for equality among members of container types.
template <class EqualityProvider>
absl::optional<bool> HomogenousCelValueEqual(const CelValue& t1,
                                             const CelValue& t2) {
  if (t1.type() != t2.type()) {
    return absl::nullopt;
  }
  switch (t1.type()) {
    case CelValue::Type::kNullType:
      return Equal<CelValue::NullType>(CelValue::NullType(),
                                       CelValue::NullType());
    case CelValue::Type::kBool:
      return Equal<bool>(t1.BoolOrDie(), t2.BoolOrDie());
    case CelValue::Type::kInt64:
      return Equal<int64_t>(t1.Int64OrDie(), t2.Int64OrDie());
    case CelValue::Type::kUint64:
      return Equal<uint64_t>(t1.Uint64OrDie(), t2.Uint64OrDie());
    case CelValue::Type::kDouble:
      return Equal<double>(t1.DoubleOrDie(), t2.DoubleOrDie());
    case CelValue::Type::kString:
      return Equal<CelValue::StringHolder>(t1.StringOrDie(), t2.StringOrDie());
    case CelValue::Type::kBytes:
      return Equal<CelValue::BytesHolder>(t1.BytesOrDie(), t2.BytesOrDie());
    case CelValue::Type::kDuration:
      return Equal<absl::Duration>(t1.DurationOrDie(), t2.DurationOrDie());
    case CelValue::Type::kTimestamp:
      return Equal<absl::Time>(t1.TimestampOrDie(), t2.TimestampOrDie());
    case CelValue::Type::kList:
      return ListEqual<EqualityProvider>(t1.ListOrDie(), t2.ListOrDie());
    case CelValue::Type::kMap:
      return MapEqual<EqualityProvider>(t1.MapOrDie(), t2.MapOrDie());
    case CelValue::Type::kCelType:
      return Equal<CelValue::CelTypeHolder>(t1.CelTypeOrDie(),
                                            t2.CelTypeOrDie());
    default:
      break;
  }
  return absl::nullopt;
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

// Helper method
//
// Registers all equality functions for template parameters type.
template <class Type>
absl::Status RegisterEqualityFunctionsForType(CelFunctionRegistry* registry) {
  // Inequality
  absl::Status status =
      PortableFunctionAdapter<CelValue, Type, Type>::CreateAndRegister(
          builtin::kInequal, false, WrapComparison<Type>(&Inequal<Type>),
          registry);
  if (!status.ok()) return status;

  // Equality
  status = PortableFunctionAdapter<CelValue, Type, Type>::CreateAndRegister(
      builtin::kEqual, false, WrapComparison<Type>(&Equal<Type>), registry);
  return status;
}

template <typename T, typename U>
absl::Status RegisterSymmetricFunction(
    absl::string_view name, std::function<bool(google::protobuf::Arena*, T, U)> fn,
    CelFunctionRegistry* registry) {
  CEL_RETURN_IF_ERROR((PortableFunctionAdapter<bool, T, U>::CreateAndRegister(
      name, false, fn, registry)));

  // the symmetric version
  CEL_RETURN_IF_ERROR((PortableFunctionAdapter<bool, U, T>::CreateAndRegister(
      name, false,
      [fn](google::protobuf::Arena* arena, U u, T t) { return fn(arena, t, u); },
      registry)));

  return absl::OkStatus();
}

template <class Type>
absl::Status RegisterOrderingFunctionsForType(CelFunctionRegistry* registry) {
  // Less than
  // Extra paranthesis needed for Macros with multiple template arguments.
  CEL_RETURN_IF_ERROR(
      (PortableFunctionAdapter<bool, Type, Type>::CreateAndRegister(
          builtin::kLess, false, LessThan<Type>, registry)));

  // Less than or Equal
  CEL_RETURN_IF_ERROR(
      (PortableFunctionAdapter<bool, Type, Type>::CreateAndRegister(
          builtin::kLessOrEqual, false, LessThanOrEqual<Type>, registry)));

  // Greater than
  CEL_RETURN_IF_ERROR(
      (PortableFunctionAdapter<bool, Type, Type>::CreateAndRegister(
          builtin::kGreater, false, GreaterThan<Type>, registry)));

  // Greater than or Equal
  CEL_RETURN_IF_ERROR(
      (PortableFunctionAdapter<bool, Type, Type>::CreateAndRegister(
          builtin::kGreaterOrEqual, false, GreaterThanOrEqual<Type>,
          registry)));

  return absl::OkStatus();
}

// Registers all comparison functions for template parameter type.
template <class Type>
absl::Status RegisterComparisonFunctionsForType(CelFunctionRegistry* registry) {
  CEL_RETURN_IF_ERROR(RegisterEqualityFunctionsForType<Type>(registry));

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

  // Null only supports equality/inequality by default.
  CEL_RETURN_IF_ERROR(
      RegisterEqualityFunctionsForType<CelValue::NullType>(registry));

  CEL_RETURN_IF_ERROR(
      RegisterEqualityFunctionsForType<const CelList*>(registry));

  CEL_RETURN_IF_ERROR(
      RegisterEqualityFunctionsForType<const CelMap*>(registry));

  CEL_RETURN_IF_ERROR(
      RegisterEqualityFunctionsForType<CelValue::CelTypeHolder>(registry));

  return absl::OkStatus();
}

absl::Status RegisterNullMessageEqualityFunctions(
    CelFunctionRegistry* registry) {
  CEL_RETURN_IF_ERROR(
      (RegisterSymmetricFunction<MessageWrapper, CelValue::NullType>(
          builtin::kEqual, MessageNullEqual, registry)));
  CEL_RETURN_IF_ERROR(
      (RegisterSymmetricFunction<MessageWrapper, CelValue::NullType>(
          builtin::kInequal, MessageNullInequal, registry)));

  return absl::OkStatus();
}

// Wrapper around CelValueEqualImpl to work with the PortableFunctionAdapter
// template. Implements CEL ==,
CelValue GeneralizedEqual(Arena* arena, CelValue t1, CelValue t2) {
  absl::optional<bool> result = CelValueEqualImpl(t1, t2);
  if (result.has_value()) {
    return CelValue::CreateBool(*result);
  }
  // Note: With full heterogeneous equality enabled, this only happens for
  // containers containing special value types (errors, unknowns).
  return CreateNoMatchingOverloadError(arena, builtin::kEqual);
}

// Wrapper around CelValueEqualImpl to work with the PortableFunctionAdapter
// template. Implements CEL !=.
CelValue GeneralizedInequal(Arena* arena, CelValue t1, CelValue t2) {
  absl::optional<bool> result = CelValueEqualImpl(t1, t2);
  if (result.has_value()) {
    return CelValue::CreateBool(!*result);
  }
  return CreateNoMatchingOverloadError(arena, builtin::kInequal);
}

template <typename T, typename U>
absl::Status RegisterCrossNumericComparisons(CelFunctionRegistry* registry) {
  CEL_RETURN_IF_ERROR((PortableFunctionAdapter<bool, T, U>::CreateAndRegister(
      builtin::kLess, /*receiver_style=*/false, &CrossNumericLessThan<T, U>,
      registry)));
  CEL_RETURN_IF_ERROR((PortableFunctionAdapter<bool, T, U>::CreateAndRegister(
      builtin::kGreater, /*receiver_style=*/false,
      &CrossNumericGreaterThan<T, U>, registry)));
  CEL_RETURN_IF_ERROR((PortableFunctionAdapter<bool, T, U>::CreateAndRegister(
      builtin::kGreaterOrEqual, /*receiver_style=*/false,
      &CrossNumericGreaterOrEqualTo<T, U>, registry)));
  CEL_RETURN_IF_ERROR((PortableFunctionAdapter<bool, T, U>::CreateAndRegister(
      builtin::kLessOrEqual, /*receiver_style=*/false,
      &CrossNumericLessOrEqualTo<T, U>, registry)));
  return absl::OkStatus();
}

absl::Status RegisterHeterogeneousComparisonFunctions(
    CelFunctionRegistry* registry) {
  CEL_RETURN_IF_ERROR(
      (PortableFunctionAdapter<CelValue, CelValue, CelValue>::CreateAndRegister(
          builtin::kEqual, /*receiver_style=*/false, &GeneralizedEqual,
          registry)));
  CEL_RETURN_IF_ERROR(
      (PortableFunctionAdapter<CelValue, CelValue, CelValue>::CreateAndRegister(
          builtin::kInequal, /*receiver_style=*/false, &GeneralizedInequal,
          registry)));

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

absl::optional<bool> HomogenousEqualProvider::operator()(
    const CelValue& v1, const CelValue& v2) const {
  return HomogenousCelValueEqual<HomogenousEqualProvider>(v1, v2);
}

absl::optional<bool> HeterogeneousEqualProvider::operator()(
    const CelValue& v1, const CelValue& v2) const {
  return CelValueEqualImpl(v1, v2);
}

}  // namespace

// Equal operator is defined for all types at plan time. Runtime delegates to
// the correct implementation for types or returns nullopt if the comparison
// isn't defined.
absl::optional<bool> CelValueEqualImpl(const CelValue& v1, const CelValue& v2) {
  if (v1.type() == v2.type()) {
    // Message equality is only defined if heterogeneous comparions are enabled
    // to preserve the legacy behavior for equality.
    if (CelValue::MessageWrapper lhs, rhs;
        v1.GetValue(&lhs) && v2.GetValue(&rhs)) {
      return MessageEqual(lhs, rhs);
    }
    return HomogenousCelValueEqual<HeterogeneousEqualProvider>(v1, v2);
  }

  absl::optional<CelNumber> lhs = GetNumberFromCelValue(v1);
  absl::optional<CelNumber> rhs = GetNumberFromCelValue(v2);

  if (rhs.has_value() && lhs.has_value()) {
    return *lhs == *rhs;
  }

  // TODO(issues/5): It's currently possible for the interpreter to create a
  // map containing an Error. Return no matching overload to propagate an error
  // instead of a false result.
  if (v1.IsError() || v1.IsUnknownSet() || v2.IsError() || v2.IsUnknownSet()) {
    return absl::nullopt;
  }

  return false;
}

absl::Status RegisterComparisonFunctions(CelFunctionRegistry* registry,
                                         const InterpreterOptions& options) {
  if (options.enable_heterogeneous_equality) {
    // Heterogeneous equality uses one generic overload that delegates to the
    // right equality implementation at runtime.
    CEL_RETURN_IF_ERROR(RegisterHeterogeneousComparisonFunctions(registry));
  } else {
    CEL_RETURN_IF_ERROR(RegisterHomogenousComparisonFunctions(registry));

    CEL_RETURN_IF_ERROR(RegisterNullMessageEqualityFunctions(registry));
  }
  return absl::OkStatus();
}

}  // namespace google::api::expr::runtime
