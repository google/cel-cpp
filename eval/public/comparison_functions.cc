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
#include <type_traits>
#include <vector>

#include "google/protobuf/map_field.h"
#include "google/protobuf/util/message_differencer.h"
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
#include "eval/public/cel_function_adapter.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "internal/casts.h"
#include "internal/overflow.h"
#include "internal/proto_util.h"
#include "internal/status_macros.h"
#include "internal/time.h"
#include "internal/utf8.h"
#include "re2/re2.h"

namespace google::api::expr::runtime {

namespace {

using ::google::protobuf::Arena;
using ::google::protobuf::util::MessageDifferencer;

constexpr int64_t kInt64Max = std::numeric_limits<int64_t>::max();
constexpr int64_t kInt64Min = std::numeric_limits<int64_t>::lowest();
constexpr uint64_t kUint64Max = std::numeric_limits<uint64_t>::max();
constexpr uint64_t kUintToIntMax = static_cast<uint64_t>(kInt64Max);
constexpr double kDoubleToIntMax = static_cast<double>(kInt64Max);
constexpr double kDoubleToIntMin = static_cast<double>(kInt64Min);
constexpr double kDoubleToUintMax = static_cast<double>(kUint64Max);

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

inline int32_t CompareDouble(double d1, double d2) {
  double cmp = d1 - d2;
  return cmp < 0 ? -1 : cmp > 0 ? 1 : 0;
}

int32_t CompareDoubleInt(double d, int64_t i) {
  if (d < kDoubleToIntMin) {
    return -1;
  }
  if (d > kDoubleToIntMax) {
    return 1;
  }
  return CompareDouble(d, static_cast<double>(i));
}

inline int32_t CompareIntDouble(int64_t i, double d) {
  return -CompareDoubleInt(d, i);
}

int32_t CompareDoubleUint(double d, uint64_t u) {
  if (d < 0.0) {
    return -1;
  }
  if (d > kDoubleToUintMax) {
    return 1;
  }
  return CompareDouble(d, static_cast<double>(u));
}

inline int32_t CompareUintDouble(uint64_t u, double d) {
  return -CompareDoubleUint(d, u);
}

int32_t CompareIntUint(int64_t i, uint64_t u) {
  if (i < 0 || u > kUintToIntMax) {
    return -1;
  }
  // Note, the type conversion cannot overflow as the overflow condition is
  // checked earlier as part of the special case comparison.
  int64_t cmp = i - static_cast<int64_t>(u);
  return cmp < 0 ? -1 : cmp > 0 ? 1 : 0;
}

inline int32_t CompareUintInt(uint64_t u, int64_t i) {
  return -CompareIntUint(i, u);
}

bool LessThanDoubleInt(Arena*, double d, int64_t i) {
  return CompareDoubleInt(d, i) == -1;
}

bool LessThanIntDouble(Arena*, int64_t i, double d) {
  return CompareIntDouble(i, d) == -1;
}

bool LessThanDoubleUint(Arena*, double d, uint64_t u) {
  return CompareDoubleInt(d, u) == -1;
}

bool LessThanUintDouble(Arena*, uint64_t u, double d) {
  return CompareIntDouble(u, d) == -1;
}

bool LessThanIntUint(Arena*, int64_t i, uint64_t u) {
  return CompareIntUint(i, u) == -1;
}

bool LessThanUintInt(Arena*, uint64_t u, int64_t i) {
  return CompareUintInt(u, i) == -1;
}

bool LessThanOrEqualDoubleInt(Arena*, double d, int64_t i) {
  return CompareDoubleInt(d, i) <= 0;
}

bool LessThanOrEqualIntDouble(Arena*, int64_t i, double d) {
  return CompareIntDouble(i, d) <= 0;
}

bool LessThanOrEqualDoubleUint(Arena*, double d, uint64_t u) {
  return CompareDoubleInt(d, u) <= 0;
}

bool LessThanOrEqualUintDouble(Arena*, uint64_t u, double d) {
  return CompareIntDouble(u, d) <= 0;
}

bool LessThanOrEqualIntUint(Arena*, int64_t i, uint64_t u) {
  return CompareIntUint(i, u) <= 0;
}

bool LessThanOrEqualUintInt(Arena*, uint64_t u, int64_t i) {
  return CompareUintInt(u, i) <= 0;
}

bool GreaterThanDoubleInt(Arena*, double d, int64_t i) {
  return CompareDoubleInt(d, i) == 1;
}

bool GreaterThanIntDouble(Arena*, int64_t i, double d) {
  return CompareIntDouble(i, d) == 1;
}

bool GreaterThanDoubleUint(Arena*, double d, uint64_t u) {
  return CompareDoubleInt(d, u) == 1;
}

bool GreaterThanUintDouble(Arena*, uint64_t u, double d) {
  return CompareIntDouble(u, d) == 1;
}

bool GreaterThanIntUint(Arena*, int64_t i, uint64_t u) {
  return CompareIntUint(i, u) == 1;
}

bool GreaterThanUintInt(Arena*, uint64_t u, int64_t i) {
  return CompareUintInt(u, i) == 1;
}

bool GreaterThanOrEqualDoubleInt(Arena*, double d, int64_t i) {
  return CompareDoubleInt(d, i) >= 0;
}

bool GreaterThanOrEqualIntDouble(Arena*, int64_t i, double d) {
  return CompareIntDouble(i, d) >= 0;
}

bool GreaterThanOrEqualDoubleUint(Arena*, double d, uint64_t u) {
  return CompareDoubleInt(d, u) >= 0;
}

bool GreaterThanOrEqualUintDouble(Arena*, uint64_t u, double d) {
  return CompareIntDouble(u, d) >= 0;
}

bool GreaterThanOrEqualIntUint(Arena*, int64_t i, uint64_t u) {
  return CompareIntUint(i, u) >= 0;
}

bool GreaterThanOrEqualUintInt(Arena*, uint64_t u, int64_t i) {
  return CompareUintInt(u, i) >= 0;
}

bool MessageNullEqual(Arena* arena, const google::protobuf::Message* t1,
                      CelValue::NullType) {
  // messages should never be null.
  return false;
}

bool MessageNullInequal(Arena* arena, const google::protobuf::Message* t1,
                        CelValue::NullType) {
  // messages should never be null.
  return true;
}

// Equality for lists. Template parameter provides either heterogeneous or
// homogenous equality for comparing members.
template <typename EqualsProvider>
absl::optional<bool> ListEqual(const CelList* t1, const CelList* t2) {
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
  if (t1->size() != t2->size()) {
    return false;
  }

  const CelList* keys = t1->ListKeys();
  for (int i = 0; i < keys->size(); i++) {
    CelValue key = (*keys)[i];
    CelValue v1 = (*t1)[key].value();
    absl::optional<CelValue> v2 = (*t2)[key];
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

bool MessageEqual(const google::protobuf::Message& m1, const google::protobuf::Message& m2) {
  // Equality behavior is undefined if input messages have different
  // descriptors.
  if (m1.GetDescriptor() != m2.GetDescriptor()) {
    return false;
  }
  return MessageDifferencer::Equals(m1, m2);
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
      FunctionAdapter<CelValue, Type, Type>::CreateAndRegister(
          builtin::kInequal, false, WrapComparison<Type>(&Inequal<Type>),
          registry);
  if (!status.ok()) return status;

  // Equality
  status = FunctionAdapter<CelValue, Type, Type>::CreateAndRegister(
      builtin::kEqual, false, WrapComparison<Type>(&Equal<Type>), registry);
  return status;
}

template <typename T, typename U>
absl::Status RegisterSymmetricFunction(
    absl::string_view name, std::function<bool(google::protobuf::Arena*, T, U)> fn,
    CelFunctionRegistry* registry) {
  CEL_RETURN_IF_ERROR((FunctionAdapter<bool, T, U>::CreateAndRegister(
      name, false, fn, registry)));

  // the symmetric version
  CEL_RETURN_IF_ERROR((FunctionAdapter<bool, U, T>::CreateAndRegister(
      name, false,
      [fn](google::protobuf::Arena* arena, U u, T t) { return fn(arena, t, u); },
      registry)));

  return absl::OkStatus();
}

template <class Type>
absl::Status RegisterOrderingFunctionsForType(CelFunctionRegistry* registry) {
  // Less than
  // Extra paranthesis needed for Macros with multiple template arguments.
  CEL_RETURN_IF_ERROR((FunctionAdapter<bool, Type, Type>::CreateAndRegister(
      builtin::kLess, false, LessThan<Type>, registry)));

  // Less than or Equal
  CEL_RETURN_IF_ERROR((FunctionAdapter<bool, Type, Type>::CreateAndRegister(
      builtin::kLessOrEqual, false, LessThanOrEqual<Type>, registry)));

  // Greater than
  CEL_RETURN_IF_ERROR((FunctionAdapter<bool, Type, Type>::CreateAndRegister(
      builtin::kGreater, false, GreaterThan<Type>, registry)));

  // Greater than or Equal
  CEL_RETURN_IF_ERROR((FunctionAdapter<bool, Type, Type>::CreateAndRegister(
      builtin::kGreaterOrEqual, false, GreaterThanOrEqual<Type>, registry)));

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
      (RegisterSymmetricFunction<const google::protobuf::Message*, CelValue::NullType>(
          builtin::kEqual, MessageNullEqual, registry)));
  CEL_RETURN_IF_ERROR(
      (RegisterSymmetricFunction<const google::protobuf::Message*, CelValue::NullType>(
          builtin::kInequal, MessageNullInequal, registry)));

  return absl::OkStatus();
}

// Wrapper around CelValueEqualImpl to work with the FunctionAdapter template.
// Implements CEL ==,
CelValue GeneralizedEqual(Arena* arena, CelValue t1, CelValue t2) {
  absl::optional<bool> result = CelValueEqualImpl(t1, t2);
  if (result.has_value()) {
    return CelValue::CreateBool(*result);
  }
  return CreateNoMatchingOverloadError(arena, builtin::kEqual);
}

// Wrapper around CelValueEqualImpl to work with the FunctionAdapter template.
// Implements CEL !=.
CelValue GeneralizedInequal(Arena* arena, CelValue t1, CelValue t2) {
  absl::optional<bool> result = CelValueEqualImpl(t1, t2);
  if (result.has_value()) {
    return CelValue::CreateBool(!*result);
  }
  return CreateNoMatchingOverloadError(arena, builtin::kInequal);
}

absl::Status RegisterHeterogeneousComparisonFunctions(
    CelFunctionRegistry* registry) {
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<CelValue, CelValue, CelValue>::CreateAndRegister(
          builtin::kEqual, /*receiver_style=*/false, &GeneralizedEqual,
          registry)));
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<CelValue, CelValue, CelValue>::CreateAndRegister(
          builtin::kInequal, /*receiver_style=*/false, &GeneralizedInequal,
          registry)));

  // Cross-type numeric less than operator
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<bool, double, int64_t>::CreateAndRegister(
          builtin::kLess, /*receiver_style=*/false, &LessThanDoubleInt,
          registry)));
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<bool, double, uint64_t>::CreateAndRegister(
          builtin::kLess, /*receiver_style=*/false, &LessThanDoubleUint,
          registry)));
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<bool, int64_t, uint64_t>::CreateAndRegister(
          builtin::kLess, /*receiver_style=*/false, &LessThanIntUint,
          registry)));
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<bool, int64_t, double>::CreateAndRegister(
          builtin::kLess, /*receiver_style=*/false, &LessThanIntDouble,
          registry)));
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<bool, uint64_t, double>::CreateAndRegister(
          builtin::kLess, /*receiver_style=*/false, &LessThanUintDouble,
          registry)));
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<bool, uint64_t, int64_t>::CreateAndRegister(
          builtin::kLess, /*receiver_style=*/false, &LessThanUintInt,
          registry)));

  // Cross-type numeric less than or equal operator
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<bool, double, int64_t>::CreateAndRegister(
          builtin::kLessOrEqual, /*receiver_style=*/false,
          &LessThanOrEqualDoubleInt, registry)));
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<bool, double, uint64_t>::CreateAndRegister(
          builtin::kLessOrEqual, /*receiver_style=*/false,
          &LessThanOrEqualDoubleUint, registry)));
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<bool, int64_t, uint64_t>::CreateAndRegister(
          builtin::kLessOrEqual, /*receiver_style=*/false,
          &LessThanOrEqualIntUint, registry)));
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<bool, int64_t, double>::CreateAndRegister(
          builtin::kLessOrEqual, /*receiver_style=*/false,
          &LessThanOrEqualIntDouble, registry)));
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<bool, uint64_t, double>::CreateAndRegister(
          builtin::kLessOrEqual, /*receiver_style=*/false,
          &LessThanOrEqualUintDouble, registry)));
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<bool, uint64_t, int64_t>::CreateAndRegister(
          builtin::kLessOrEqual, /*receiver_style=*/false,
          &LessThanOrEqualUintInt, registry)));

  // Cross-type numeric greater than operator
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<bool, double, int64_t>::CreateAndRegister(
          builtin::kGreater, /*receiver_style=*/false, &GreaterThanDoubleInt,
          registry)));
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<bool, double, uint64_t>::CreateAndRegister(
          builtin::kGreater, /*receiver_style=*/false, &GreaterThanDoubleUint,
          registry)));
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<bool, int64_t, uint64_t>::CreateAndRegister(
          builtin::kGreater, /*receiver_style=*/false, &GreaterThanIntUint,
          registry)));
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<bool, int64_t, double>::CreateAndRegister(
          builtin::kGreater, /*receiver_style=*/false, &GreaterThanIntDouble,
          registry)));
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<bool, uint64_t, double>::CreateAndRegister(
          builtin::kGreater, /*receiver_style=*/false, &GreaterThanUintDouble,
          registry)));
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<bool, uint64_t, int64_t>::CreateAndRegister(
          builtin::kGreater, /*receiver_style=*/false, &GreaterThanUintInt,
          registry)));

  // Cross-type numeric greater than or equal operator
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<bool, double, int64_t>::CreateAndRegister(
          builtin::kGreaterOrEqual, /*receiver_style=*/false,
          &GreaterThanOrEqualDoubleInt, registry)));
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<bool, double, uint64_t>::CreateAndRegister(
          builtin::kGreaterOrEqual, /*receiver_style=*/false,
          &GreaterThanOrEqualDoubleUint, registry)));
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<bool, int64_t, uint64_t>::CreateAndRegister(
          builtin::kGreaterOrEqual, /*receiver_style=*/false,
          &GreaterThanOrEqualIntUint, registry)));
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<bool, int64_t, double>::CreateAndRegister(
          builtin::kGreaterOrEqual, /*receiver_style=*/false,
          &GreaterThanOrEqualIntDouble, registry)));
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<bool, uint64_t, double>::CreateAndRegister(
          builtin::kGreaterOrEqual, /*receiver_style=*/false,
          &GreaterThanOrEqualUintDouble, registry)));
  CEL_RETURN_IF_ERROR(
      (FunctionAdapter<bool, uint64_t, int64_t>::CreateAndRegister(
          builtin::kGreaterOrEqual, /*receiver_style=*/false,
          &GreaterThanOrEqualUintInt, registry)));

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
    if (v1.type() == CelValue::Type::kMessage) {
      return MessageEqual(*v1.MessageOrDie(), *v2.MessageOrDie());
    }
    return HomogenousCelValueEqual<HeterogeneousEqualProvider>(v1, v2);
  }

  if (v1.type() == CelValue::Type::kNullType ||
      v2.type() == CelValue::Type::kNullType) {
    return false;
  }
  switch (v1.type()) {
    case CelValue::Type::kDouble: {
      double d;
      v1.GetValue(&d);
      if (std::isnan(d)) {
        return false;
      }
      switch (v2.type()) {
        case CelValue::Type::kInt64:
          return CompareDoubleInt(d, v2.Int64OrDie()) == 0;
        case CelValue::Type::kUint64:
          return CompareDoubleUint(d, v2.Uint64OrDie()) == 0;
        default:
          return absl::nullopt;
      }
    }
    case CelValue::Type::kInt64:
      int64_t i;
      v1.GetValue(&i);
      switch (v2.type()) {
        case CelValue::Type::kDouble: {
          double d;
          v2.GetValue(&d);
          if (std::isnan(d)) {
            return false;
          }
          return CompareIntDouble(i, d) == 0;
        }
        case CelValue::Type::kUint64:
          return CompareIntUint(i, v2.Uint64OrDie()) == 0;
        default:
          return absl::nullopt;
      }
    case CelValue::Type::kUint64:
      uint64_t u;
      v1.GetValue(&u);
      switch (v2.type()) {
        case CelValue::Type::kDouble: {
          double d;
          v2.GetValue(&d);
          if (std::isnan(d)) {
            return false;
          }
          return CompareUintDouble(u, d) == 0;
        }
        case CelValue::Type::kInt64:
          return CompareUintInt(u, v2.Int64OrDie()) == 0;
        default:
          return absl::nullopt;
      }
    default:
      return absl::nullopt;
  }
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
