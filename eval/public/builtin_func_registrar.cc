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
#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>
#include <string>
#include <vector>

#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "base/function_adapter.h"
#include "base/handle.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "base/values/bytes_value.h"
#include "base/values/string_value.h"
#include "eval/eval/mutable_list_impl.h"
#include "eval/internal/interop.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_number.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/comparison_functions.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/equality_function_registrar.h"
#include "eval/public/logical_function_registrar.h"
#include "eval/public/portable_cel_function_adapter.h"
#include "internal/casts.h"
#include "internal/overflow.h"
#include "internal/proto_time_encoding.h"
#include "internal/status_macros.h"
#include "internal/time.h"
#include "internal/utf8.h"
#include "re2/re2.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::BinaryFunctionAdapter;
using ::cel::BytesValue;
using ::cel::Handle;
using ::cel::StringValue;
using ::cel::UnaryFunctionAdapter;
using ::cel::Value;
using ::cel::ValueFactory;
using ::cel::internal::EncodeDurationToString;
using ::cel::internal::EncodeTimeToString;
using ::cel::internal::MaxTimestamp;
using ::google::protobuf::Arena;

// Time representing `9999-12-31T23:59:59.999999999Z`.
const absl::Time kMaxTime = MaxTimestamp();

// Template functions providing arithmetic operations
template <class Type>
Handle<Value> Add(ValueFactory&, Type v0, Type v1);

template <>
Handle<Value> Add<int64_t>(ValueFactory& value_factory, int64_t v0,
                           int64_t v1) {
  auto sum = cel::internal::CheckedAdd(v0, v1);
  if (!sum.ok()) {
    return value_factory.CreateErrorValue(sum.status());
  }
  return value_factory.CreateIntValue(*sum);
}

template <>
Handle<Value> Add<uint64_t>(ValueFactory& value_factory, uint64_t v0,
                            uint64_t v1) {
  auto sum = cel::internal::CheckedAdd(v0, v1);
  if (!sum.ok()) {
    return value_factory.CreateErrorValue(sum.status());
  }
  return value_factory.CreateUintValue(*sum);
}

template <>
Handle<Value> Add<double>(ValueFactory& value_factory, double v0, double v1) {
  return value_factory.CreateDoubleValue(v0 + v1);
}

template <class Type>
Handle<Value> Sub(ValueFactory&, Type v0, Type v1);

template <>
Handle<Value> Sub<int64_t>(ValueFactory& value_factory, int64_t v0,
                           int64_t v1) {
  auto diff = cel::internal::CheckedSub(v0, v1);
  if (!diff.ok()) {
    return value_factory.CreateErrorValue(diff.status());
  }
  return value_factory.CreateIntValue(*diff);
}

template <>
Handle<Value> Sub<uint64_t>(ValueFactory& value_factory, uint64_t v0,
                            uint64_t v1) {
  auto diff = cel::internal::CheckedSub(v0, v1);
  if (!diff.ok()) {
    return value_factory.CreateErrorValue(diff.status());
  }
  return value_factory.CreateUintValue(*diff);
}

template <>
Handle<Value> Sub<double>(ValueFactory& value_factory, double v0, double v1) {
  return value_factory.CreateDoubleValue(v0 - v1);
}

template <class Type>
Handle<Value> Mul(ValueFactory&, Type v0, Type v1);

template <>
Handle<Value> Mul<int64_t>(ValueFactory& value_factory, int64_t v0,
                           int64_t v1) {
  auto prod = cel::internal::CheckedMul(v0, v1);
  if (!prod.ok()) {
    return value_factory.CreateErrorValue(prod.status());
  }
  return value_factory.CreateIntValue(*prod);
}

template <>
Handle<Value> Mul<uint64_t>(ValueFactory& value_factory, uint64_t v0,
                            uint64_t v1) {
  auto prod = cel::internal::CheckedMul(v0, v1);
  if (!prod.ok()) {
    return value_factory.CreateErrorValue(prod.status());
  }
  return value_factory.CreateUintValue(*prod);
}

template <>
Handle<Value> Mul<double>(ValueFactory& value_factory, double v0, double v1) {
  return value_factory.CreateDoubleValue(v0 * v1);
}

template <class Type>
Handle<Value> Div(ValueFactory&, Type v0, Type v1);

// Division operations for integer types should check for
// division by 0
template <>
Handle<Value> Div<int64_t>(ValueFactory& value_factory, int64_t v0,
                           int64_t v1) {
  auto quot = cel::internal::CheckedDiv(v0, v1);
  if (!quot.ok()) {
    return value_factory.CreateErrorValue(quot.status());
  }
  return value_factory.CreateIntValue(*quot);
}

// Division operations for integer types should check for
// division by 0
template <>
Handle<Value> Div<uint64_t>(ValueFactory& value_factory, uint64_t v0,
                            uint64_t v1) {
  auto quot = cel::internal::CheckedDiv(v0, v1);
  if (!quot.ok()) {
    return value_factory.CreateErrorValue(quot.status());
  }
  return value_factory.CreateUintValue(*quot);
}

template <>
Handle<Value> Div<double>(ValueFactory& value_factory, double v0, double v1) {
  static_assert(std::numeric_limits<double>::is_iec559,
                "Division by zero for doubles must be supported");

  // For double, division will result in +/- inf
  return value_factory.CreateDoubleValue(v0 / v1);
}

// Modulo operation
template <class Type>
Handle<Value> Modulo(ValueFactory& value_factory, Type v0, Type v1);

// Modulo operations for integer types should check for
// division by 0
template <>
Handle<Value> Modulo<int64_t>(ValueFactory& value_factory, int64_t v0,
                              int64_t v1) {
  auto mod = cel::internal::CheckedMod(v0, v1);
  if (!mod.ok()) {
    return value_factory.CreateErrorValue(mod.status());
  }
  return value_factory.CreateIntValue(*mod);
}

template <>
Handle<Value> Modulo<uint64_t>(ValueFactory& value_factory, uint64_t v0,
                               uint64_t v1) {
  auto mod = cel::internal::CheckedMod(v0, v1);
  if (!mod.ok()) {
    return value_factory.CreateErrorValue(mod.status());
  }
  return value_factory.CreateUintValue(*mod);
}

// Helper method
// Registers all arithmetic functions for template parameter type.
template <class Type>
absl::Status RegisterArithmeticFunctionsForType(CelFunctionRegistry* registry) {
  using FunctionAdapter = cel::BinaryFunctionAdapter<Handle<Value>, Type, Type>;
  CEL_RETURN_IF_ERROR(registry->Register(
      FunctionAdapter::CreateDescriptor(builtin::kAdd, false),
      FunctionAdapter::WrapFunction(&Add<Type>)));

  CEL_RETURN_IF_ERROR(registry->Register(
      FunctionAdapter::CreateDescriptor(builtin::kSubtract, false),
      FunctionAdapter::WrapFunction(&Sub<Type>)));

  CEL_RETURN_IF_ERROR(registry->Register(
      FunctionAdapter::CreateDescriptor(builtin::kMultiply, false),
      FunctionAdapter::WrapFunction(&Mul<Type>)));

  return registry->Register(
      FunctionAdapter::CreateDescriptor(builtin::kDivide, false),
      FunctionAdapter::WrapFunction(&Div<Type>));
}

// Register basic Arithmetic functions for numeric types.
absl::Status RegisterNumericArithmeticFunctions(
    CelFunctionRegistry* registry, const InterpreterOptions& options) {
  CEL_RETURN_IF_ERROR(RegisterArithmeticFunctionsForType<int64_t>(registry));
  CEL_RETURN_IF_ERROR(RegisterArithmeticFunctionsForType<uint64_t>(registry));
  CEL_RETURN_IF_ERROR(RegisterArithmeticFunctionsForType<double>(registry));

  // Modulo
  CEL_RETURN_IF_ERROR(registry->Register(
      BinaryFunctionAdapter<Handle<Value>, int64_t, int64_t>::CreateDescriptor(
          builtin::kModulo, false),
      BinaryFunctionAdapter<Handle<Value>, int64_t, int64_t>::WrapFunction(
          &Modulo<int64_t>)));

  CEL_RETURN_IF_ERROR(registry->Register(
      BinaryFunctionAdapter<Handle<Value>, uint64_t,
                            uint64_t>::CreateDescriptor(builtin::kModulo,
                                                        false),
      BinaryFunctionAdapter<Handle<Value>, uint64_t, uint64_t>::WrapFunction(
          &Modulo<uint64_t>)));

  // Negation group
  CEL_RETURN_IF_ERROR(registry->Register(
      UnaryFunctionAdapter<Handle<Value>, int64_t>::CreateDescriptor(
          builtin::kNeg, false),
      UnaryFunctionAdapter<Handle<Value>, int64_t>::WrapFunction(
          [](ValueFactory& value_factory, int64_t value) -> Handle<Value> {
            auto inv = cel::internal::CheckedNegation(value);
            if (!inv.ok()) {
              return value_factory.CreateErrorValue(inv.status());
            }
            return value_factory.CreateIntValue(*inv);
          })));

  return registry->Register(
      UnaryFunctionAdapter<double, double>::CreateDescriptor(builtin::kNeg,
                                                             false),
      UnaryFunctionAdapter<double, double>::WrapFunction(
          [](ValueFactory&, double value) -> double { return -value; }));
}

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

// AppendList will append the elements in value2 to value1.
//
// This call will only be invoked within comprehensions where `value1` is an
// intermediate result which cannot be directly assigned or co-mingled with a
// user-provided list.
const CelList* AppendList(Arena* arena, const CelList* value1,
                          const CelList* value2) {
  // The `value1` object cannot be directly addressed and is an intermediate
  // variable. Once the comprehension completes this value will in effect be
  // treated as immutable.
  MutableListImpl* mutable_list = const_cast<MutableListImpl*>(
      cel::internal::down_cast<const MutableListImpl*>(value1));
  for (int i = 0; i < value2->size(); i++) {
    mutable_list->Append((*value2).Get(arena, i));
  }
  return mutable_list;
}

// Concatenation for StringHolder type.
absl::StatusOr<Handle<StringValue>> ConcatString(
    ValueFactory& factory, const Handle<StringValue>& value1,
    const Handle<StringValue>& value2) {
  return factory.CreateStringValue(
      absl::StrCat(value1->ToString(), value2->ToString()));
}

// Concatenation for BytesHolder type.
absl::StatusOr<Handle<cel::BytesValue>> ConcatBytes(
    ValueFactory& factory, const Handle<BytesValue>& value1,
    const Handle<BytesValue>& value2) {
  return factory.CreateBytesValue(
      absl::StrCat(value1->ToString(), value2->ToString()));
}

// Concatenation for CelList type.
const CelList* ConcatList(Arena* arena, const CelList* value1,
                          const CelList* value2) {
  std::vector<CelValue> joined_values;

  int size1 = value1->size();
  int size2 = value2->size();
  joined_values.reserve(size1 + size2);

  for (int i = 0; i < size1; i++) {
    joined_values.push_back((*value1).Get(arena, i));
  }
  for (int i = 0; i < size2; i++) {
    joined_values.push_back((*value2).Get(arena, i));
  }

  auto concatenated =
      Arena::Create<ContainerBackedListImpl>(arena, joined_values);
  return concatenated;
}

// Timestamp
absl::Status FindTimeBreakdown(absl::Time timestamp, absl::string_view tz,
                               absl::TimeZone::CivilInfo* breakdown) {
  absl::TimeZone time_zone;

  // Early return if there is no timezone.
  if (tz.empty()) {
    *breakdown = time_zone.At(timestamp);
    return absl::OkStatus();
  }

  // Check to see whether the timezone is an IANA timezone.
  if (absl::LoadTimeZone(tz, &time_zone)) {
    *breakdown = time_zone.At(timestamp);
    return absl::OkStatus();
  }

  // Check for times of the format: [+-]HH:MM and convert them into durations
  // specified as [+-]HHhMMm.
  if (absl::StrContains(tz, ":")) {
    std::string dur = absl::StrCat(tz, "m");
    absl::StrReplaceAll({{":", "h"}}, &dur);
    absl::Duration d;
    if (absl::ParseDuration(dur, &d)) {
      timestamp += d;
      *breakdown = time_zone.At(timestamp);
      return absl::OkStatus();
    }
  }

  // Otherwise, error.
  return absl::InvalidArgumentError("Invalid timezone");
}

CelValue GetTimeBreakdownPart(
    Arena* arena, absl::Time timestamp, absl::string_view tz,
    const std::function<CelValue(const absl::TimeZone::CivilInfo&)>&
        extractor_func) {
  absl::TimeZone::CivilInfo breakdown;
  auto status = FindTimeBreakdown(timestamp, tz, &breakdown);

  if (!status.ok()) {
    return CreateErrorValue(arena, status);
  }

  return extractor_func(breakdown);
}

CelValue GetFullYear(Arena* arena, absl::Time timestamp, absl::string_view tz) {
  return GetTimeBreakdownPart(
      arena, timestamp, tz, [](const absl::TimeZone::CivilInfo& breakdown) {
        return CelValue::CreateInt64(breakdown.cs.year());
      });
}

CelValue GetMonth(Arena* arena, absl::Time timestamp, absl::string_view tz) {
  return GetTimeBreakdownPart(
      arena, timestamp, tz, [](const absl::TimeZone::CivilInfo& breakdown) {
        return CelValue::CreateInt64(breakdown.cs.month() - 1);
      });
}

CelValue GetDayOfYear(Arena* arena, absl::Time timestamp,
                      absl::string_view tz) {
  return GetTimeBreakdownPart(
      arena, timestamp, tz, [](const absl::TimeZone::CivilInfo& breakdown) {
        return CelValue::CreateInt64(
            absl::GetYearDay(absl::CivilDay(breakdown.cs)) - 1);
      });
}

CelValue GetDayOfMonth(Arena* arena, absl::Time timestamp,
                       absl::string_view tz) {
  return GetTimeBreakdownPart(
      arena, timestamp, tz, [](const absl::TimeZone::CivilInfo& breakdown) {
        return CelValue::CreateInt64(breakdown.cs.day() - 1);
      });
}

CelValue GetDate(Arena* arena, absl::Time timestamp, absl::string_view tz) {
  return GetTimeBreakdownPart(
      arena, timestamp, tz, [](const absl::TimeZone::CivilInfo& breakdown) {
        return CelValue::CreateInt64(breakdown.cs.day());
      });
}

CelValue GetDayOfWeek(Arena* arena, absl::Time timestamp,
                      absl::string_view tz) {
  return GetTimeBreakdownPart(
      arena, timestamp, tz, [](const absl::TimeZone::CivilInfo& breakdown) {
        absl::Weekday weekday = absl::GetWeekday(breakdown.cs);

        // get day of week from the date in UTC, zero-based, zero for Sunday,
        // based on GetDayOfWeek CEL function definition.
        int weekday_num = static_cast<int>(weekday);
        weekday_num = (weekday_num == 6) ? 0 : weekday_num + 1;
        return CelValue::CreateInt64(weekday_num);
      });
}

CelValue GetHours(Arena* arena, absl::Time timestamp, absl::string_view tz) {
  return GetTimeBreakdownPart(
      arena, timestamp, tz, [](const absl::TimeZone::CivilInfo& breakdown) {
        return CelValue::CreateInt64(breakdown.cs.hour());
      });
}

CelValue GetMinutes(Arena* arena, absl::Time timestamp, absl::string_view tz) {
  return GetTimeBreakdownPart(
      arena, timestamp, tz, [](const absl::TimeZone::CivilInfo& breakdown) {
        return CelValue::CreateInt64(breakdown.cs.minute());
      });
}

CelValue GetSeconds(Arena* arena, absl::Time timestamp, absl::string_view tz) {
  return GetTimeBreakdownPart(
      arena, timestamp, tz, [](const absl::TimeZone::CivilInfo& breakdown) {
        return CelValue::CreateInt64(breakdown.cs.second());
      });
}

CelValue GetMilliseconds(Arena* arena, absl::Time timestamp,
                         absl::string_view tz) {
  return GetTimeBreakdownPart(
      arena, timestamp, tz, [](const absl::TimeZone::CivilInfo& breakdown) {
        return CelValue::CreateInt64(
            absl::ToInt64Milliseconds(breakdown.subsecond));
      });
}

CelValue CreateDurationFromString(Arena* arena,
                                  CelValue::StringHolder dur_str) {
  absl::Duration d;
  if (!absl::ParseDuration(dur_str.value(), &d)) {
    return CreateErrorValue(arena, "String to Duration conversion failed",
                            absl::StatusCode::kInvalidArgument);
  }

  return CelValue::CreateDuration(d);
}

bool StringContains(ValueFactory&, const Handle<StringValue>& value,
                    const Handle<StringValue>& substr) {
  return absl::StrContains(value->ToString(), substr->ToString());
}

bool StringEndsWith(ValueFactory&, const Handle<StringValue>& value,
                    const Handle<StringValue>& suffix) {
  return absl::EndsWith(value->ToString(), suffix->ToString());
}

bool StringStartsWith(ValueFactory&, const Handle<StringValue>& value,
                      const Handle<StringValue>& prefix) {
  return absl::StartsWith(value->ToString(), prefix->ToString());
}

absl::Status RegisterSetMembershipFunctions(CelFunctionRegistry* registry,
                                            const InterpreterOptions& options) {
  constexpr std::array<absl::string_view, 3> in_operators = {
      builtin::kIn,            // @in for map and list types.
      builtin::kInFunction,    // deprecated in() -- for backwards compat
      builtin::kInDeprecated,  // deprecated _in_ -- for backwards compat
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

absl::Status RegisterStringFunctions(CelFunctionRegistry* registry,
                                     const InterpreterOptions& options) {
  // Basic substring tests (contains, startsWith, endsWith)
  for (bool receiver_style : {true, false}) {
    CEL_RETURN_IF_ERROR(registry->Register(
        BinaryFunctionAdapter<bool, const Handle<StringValue>&,
                              const Handle<StringValue>&>::
            CreateDescriptor(builtin::kStringContains, receiver_style),
        BinaryFunctionAdapter<
            bool, const Handle<StringValue>&,
            const Handle<StringValue>&>::WrapFunction(StringContains)));

    CEL_RETURN_IF_ERROR(registry->Register(
        BinaryFunctionAdapter<bool, const Handle<StringValue>&,
                              const Handle<StringValue>&>::
            CreateDescriptor(builtin::kStringEndsWith, receiver_style),
        BinaryFunctionAdapter<
            bool, const Handle<StringValue>&,
            const Handle<StringValue>&>::WrapFunction(StringEndsWith)));

    CEL_RETURN_IF_ERROR(registry->Register(
        BinaryFunctionAdapter<bool, const Handle<StringValue>&,
                              const Handle<StringValue>&>::
            CreateDescriptor(builtin::kStringStartsWith, receiver_style),
        BinaryFunctionAdapter<
            bool, const Handle<StringValue>&,
            const Handle<StringValue>&>::WrapFunction(StringStartsWith)));
  }

  // matches function if enabled.
  if (options.enable_regex) {
    auto regex_matches =
        [max_size = options.regex_max_program_size](
            ValueFactory& value_factory, const Handle<StringValue>& target,
            const Handle<StringValue>& regex) -> Handle<Value> {
      RE2 re2(regex->ToString());
      if (max_size > 0 && re2.ProgramSize() > max_size) {
        return value_factory.CreateErrorValue(
            absl::InvalidArgumentError("exceeded RE2 max program size"));
      }
      if (!re2.ok()) {
        return value_factory.CreateErrorValue(
            absl::InvalidArgumentError("invalid regex for match"));
      }
      return value_factory.CreateBoolValue(
          RE2::PartialMatch(target->ToString(), re2));
    };

    // bind str.matches(re) and matches(str, re)
    for (bool receiver_style : {true, false}) {
      using MatchFnAdapter =
          BinaryFunctionAdapter<Handle<Value>, const Handle<StringValue>&,
                                const Handle<StringValue>&>;
      CEL_RETURN_IF_ERROR(
          registry->Register(MatchFnAdapter::CreateDescriptor(
                                 builtin::kRegexMatch, receiver_style),
                             MatchFnAdapter::WrapFunction(regex_matches)));
    }
  }  // if options.enable_regex

  // string concatenation if enabled
  if (options.enable_string_concat) {
    using StrCatFnAdapter =
        BinaryFunctionAdapter<absl::StatusOr<Handle<StringValue>>,
                              const Handle<StringValue>&,
                              const Handle<StringValue>&>;
    CEL_RETURN_IF_ERROR(registry->Register(
        StrCatFnAdapter::CreateDescriptor(builtin::kAdd, false),
        StrCatFnAdapter::WrapFunction(&ConcatString)));

    using BytesCatFnAdapter =
        BinaryFunctionAdapter<absl::StatusOr<Handle<BytesValue>>,
                              const Handle<BytesValue>&,
                              const Handle<BytesValue>&>;
    CEL_RETURN_IF_ERROR(registry->Register(
        BytesCatFnAdapter::CreateDescriptor(builtin::kAdd, false),
        BytesCatFnAdapter::WrapFunction(&ConcatBytes)));
  }

  // String size
  auto size_func = [](ValueFactory& value_factory,
                      const Handle<StringValue>& value) -> Handle<Value> {
    auto [count, valid] = ::cel::internal::Utf8Validate(value->ToString());
    if (!valid) {
      return value_factory.CreateErrorValue(
          absl::InvalidArgumentError("invalid utf-8 string"));
    }
    return value_factory.CreateIntValue(count);
  };

  // receiver style = true/false
  // Support global and receiver style size() operations on strings.
  using StrSizeFnAdapter =
      UnaryFunctionAdapter<Handle<Value>, const Handle<StringValue>&>;
  CEL_RETURN_IF_ERROR(
      registry->Register(StrSizeFnAdapter::CreateDescriptor(
                             builtin::kSize, /*receiver_style=*/true),
                         StrSizeFnAdapter::WrapFunction(size_func)));
  CEL_RETURN_IF_ERROR(
      registry->Register(StrSizeFnAdapter::CreateDescriptor(
                             builtin::kSize, /*receiver_style=*/false),
                         StrSizeFnAdapter::WrapFunction(size_func)));

  // Bytes size
  auto bytes_size_func = [](ValueFactory&,
                            const Handle<BytesValue>& value) -> int64_t {
    return value->size();
  };
  // receiver style = true/false
  // Support global and receiver style size() operations on bytes.
  using BytesSizeFnAdapter =
      UnaryFunctionAdapter<int64_t, const Handle<BytesValue>&>;
  CEL_RETURN_IF_ERROR(
      registry->Register(BytesSizeFnAdapter::CreateDescriptor(
                             builtin::kSize, /*receiver_style=*/true),
                         BytesSizeFnAdapter::WrapFunction(bytes_size_func)));
  CEL_RETURN_IF_ERROR(
      registry->Register(BytesSizeFnAdapter::CreateDescriptor(
                             builtin::kSize, /*receiver_style=*/false),
                         BytesSizeFnAdapter::WrapFunction(bytes_size_func)));

  return absl::OkStatus();
}

absl::Status RegisterTimestampFunctions(CelFunctionRegistry* registry,
                                        const InterpreterOptions& options) {
  auto status =
      registry->Register(PortableBinaryFunctionAdapter<CelValue, absl::Time,
                                                       CelValue::StringHolder>::
                             Create(builtin::kFullYear, true,
                                    [](Arena* arena, absl::Time ts,
                                       CelValue::StringHolder tz) -> CelValue {
                                      return GetFullYear(arena, ts, tz.value());
                                    }));
  if (!status.ok()) return status;

  status = registry->Register(
      PortableUnaryFunctionAdapter<CelValue, absl::Time>::Create(
          builtin::kFullYear, true,
          [](Arena* arena, absl::Time ts) -> CelValue {
            return GetFullYear(arena, ts, "");
          }));
  if (!status.ok()) return status;

  status =
      registry->Register(PortableBinaryFunctionAdapter<CelValue, absl::Time,
                                                       CelValue::StringHolder>::
                             Create(builtin::kMonth, true,
                                    [](Arena* arena, absl::Time ts,
                                       CelValue::StringHolder tz) -> CelValue {
                                      return GetMonth(arena, ts, tz.value());
                                    }));
  if (!status.ok()) return status;

  status = registry->Register(
      PortableUnaryFunctionAdapter<CelValue, absl::Time>::Create(
          builtin::kMonth, true, [](Arena* arena, absl::Time ts) -> CelValue {
            return GetMonth(arena, ts, "");
          }));
  if (!status.ok()) return status;

  status = registry->Register(
      PortableBinaryFunctionAdapter<CelValue, absl::Time,
                                    CelValue::StringHolder>::
          Create(builtin::kDayOfYear, true,
                 [](Arena* arena, absl::Time ts,
                    CelValue::StringHolder tz) -> CelValue {
                   return GetDayOfYear(arena, ts, tz.value());
                 }));
  if (!status.ok()) return status;

  status = registry->Register(
      PortableUnaryFunctionAdapter<CelValue, absl::Time>::Create(
          builtin::kDayOfYear, true,
          [](Arena* arena, absl::Time ts) -> CelValue {
            return GetDayOfYear(arena, ts, "");
          }));
  if (!status.ok()) return status;

  status = registry->Register(
      PortableBinaryFunctionAdapter<CelValue, absl::Time,
                                    CelValue::StringHolder>::
          Create(builtin::kDayOfMonth, true,
                 [](Arena* arena, absl::Time ts,
                    CelValue::StringHolder tz) -> CelValue {
                   return GetDayOfMonth(arena, ts, tz.value());
                 }));
  if (!status.ok()) return status;

  status = registry->Register(
      PortableUnaryFunctionAdapter<CelValue, absl::Time>::Create(
          builtin::kDayOfMonth, true,
          [](Arena* arena, absl::Time ts) -> CelValue {
            return GetDayOfMonth(arena, ts, "");
          }));
  if (!status.ok()) return status;

  status =
      registry->Register(PortableBinaryFunctionAdapter<CelValue, absl::Time,
                                                       CelValue::StringHolder>::
                             Create(builtin::kDate, true,
                                    [](Arena* arena, absl::Time ts,
                                       CelValue::StringHolder tz) -> CelValue {
                                      return GetDate(arena, ts, tz.value());
                                    }));
  if (!status.ok()) return status;

  status = registry->Register(
      PortableUnaryFunctionAdapter<CelValue, absl::Time>::Create(
          builtin::kDate, true, [](Arena* arena, absl::Time ts) -> CelValue {
            return GetDate(arena, ts, "");
          }));
  if (!status.ok()) return status;

  status = registry->Register(
      PortableBinaryFunctionAdapter<CelValue, absl::Time,
                                    CelValue::StringHolder>::
          Create(builtin::kDayOfWeek, true,
                 [](Arena* arena, absl::Time ts,
                    CelValue::StringHolder tz) -> CelValue {
                   return GetDayOfWeek(arena, ts, tz.value());
                 }));
  if (!status.ok()) return status;

  status = registry->Register(
      PortableUnaryFunctionAdapter<CelValue, absl::Time>::Create(
          builtin::kDayOfWeek, true,
          [](Arena* arena, absl::Time ts) -> CelValue {
            return GetDayOfWeek(arena, ts, "");
          }));
  if (!status.ok()) return status;

  status =
      registry->Register(PortableBinaryFunctionAdapter<CelValue, absl::Time,
                                                       CelValue::StringHolder>::
                             Create(builtin::kHours, true,
                                    [](Arena* arena, absl::Time ts,
                                       CelValue::StringHolder tz) -> CelValue {
                                      return GetHours(arena, ts, tz.value());
                                    }));
  if (!status.ok()) return status;

  status = registry->Register(
      PortableUnaryFunctionAdapter<CelValue, absl::Time>::Create(
          builtin::kHours, true, [](Arena* arena, absl::Time ts) -> CelValue {
            return GetHours(arena, ts, "");
          }));
  if (!status.ok()) return status;

  status =
      registry->Register(PortableBinaryFunctionAdapter<CelValue, absl::Time,
                                                       CelValue::StringHolder>::
                             Create(builtin::kMinutes, true,
                                    [](Arena* arena, absl::Time ts,
                                       CelValue::StringHolder tz) -> CelValue {
                                      return GetMinutes(arena, ts, tz.value());
                                    }));
  if (!status.ok()) return status;

  status = registry->Register(
      PortableUnaryFunctionAdapter<CelValue, absl::Time>::Create(
          builtin::kMinutes, true, [](Arena* arena, absl::Time ts) -> CelValue {
            return GetMinutes(arena, ts, "");
          }));
  if (!status.ok()) return status;

  status =
      registry->Register(PortableBinaryFunctionAdapter<CelValue, absl::Time,
                                                       CelValue::StringHolder>::
                             Create(builtin::kSeconds, true,
                                    [](Arena* arena, absl::Time ts,
                                       CelValue::StringHolder tz) -> CelValue {
                                      return GetSeconds(arena, ts, tz.value());
                                    }));
  if (!status.ok()) return status;

  status = registry->Register(
      PortableUnaryFunctionAdapter<CelValue, absl::Time>::Create(
          builtin::kSeconds, true, [](Arena* arena, absl::Time ts) -> CelValue {
            return GetSeconds(arena, ts, "");
          }));
  if (!status.ok()) return status;

  status = registry->Register(
      PortableBinaryFunctionAdapter<CelValue, absl::Time,
                                    CelValue::StringHolder>::
          Create(builtin::kMilliseconds, true,
                 [](Arena* arena, absl::Time ts,
                    CelValue::StringHolder tz) -> CelValue {
                   return GetMilliseconds(arena, ts, tz.value());
                 }));
  if (!status.ok()) return status;

  return registry->Register(
      PortableUnaryFunctionAdapter<CelValue, absl::Time>::Create(
          builtin::kMilliseconds, true,
          [](Arena* arena, absl::Time ts) -> CelValue {
            return GetMilliseconds(arena, ts, "");
          }));
}

absl::Status RegisterBytesConversionFunctions(CelFunctionRegistry* registry,
                                              const InterpreterOptions&) {
  // bytes -> bytes
  auto status = registry->Register(
      PortableUnaryFunctionAdapter<
          CelValue::BytesHolder,
          CelValue::BytesHolder>::Create(builtin::kBytes, false,
                                         [](Arena*, CelValue::BytesHolder value)
                                             -> CelValue::BytesHolder {
                                           return value;
                                         }));
  if (!status.ok()) return status;

  // string -> bytes
  return registry->Register(
      PortableUnaryFunctionAdapter<CelValue, CelValue::StringHolder>::Create(
          builtin::kBytes, false,
          [](Arena* arena, CelValue::StringHolder value) -> CelValue {
            return CelValue::CreateBytesView(value.value());
          }));
}

absl::Status RegisterDoubleConversionFunctions(CelFunctionRegistry* registry,
                                               const InterpreterOptions&) {
  // double -> double
  auto status =
      registry->Register(PortableUnaryFunctionAdapter<double, double>::Create(
          builtin::kDouble, false, [](Arena*, double v) { return v; }));
  if (!status.ok()) return status;

  // int -> double
  status =
      registry->Register(PortableUnaryFunctionAdapter<double, int64_t>::Create(
          builtin::kDouble, false,
          [](Arena*, int64_t v) { return static_cast<double>(v); }));
  if (!status.ok()) return status;

  // string -> double
  status = registry->Register(
      PortableUnaryFunctionAdapter<CelValue, CelValue::StringHolder>::Create(
          builtin::kDouble, false, [](Arena* arena, CelValue::StringHolder s) {
            double result;
            if (absl::SimpleAtod(s.value(), &result)) {
              return CelValue::CreateDouble(result);
            } else {
              return CreateErrorValue(arena, "cannot convert string to double",
                                      absl::StatusCode::kInvalidArgument);
            }
          }));
  if (!status.ok()) return status;

  // uint -> double
  return registry->Register(
      PortableUnaryFunctionAdapter<double, uint64_t>::Create(
          builtin::kDouble, false,
          [](Arena*, uint64_t v) { return static_cast<double>(v); }));
}

absl::Status RegisterIntConversionFunctions(CelFunctionRegistry* registry,
                                            const InterpreterOptions&) {
  // bool -> int
  auto status =
      registry->Register(PortableUnaryFunctionAdapter<int64_t, bool>::Create(
          builtin::kInt, false,
          [](Arena*, bool v) { return static_cast<int64_t>(v); }));
  if (!status.ok()) return status;

  // double -> int
  status =
      registry->Register(PortableUnaryFunctionAdapter<CelValue, double>::Create(
          builtin::kInt, false, [](Arena* arena, double v) {
            auto conv = cel::internal::CheckedDoubleToInt64(v);
            if (!conv.ok()) {
              return CreateErrorValue(arena, conv.status());
            }
            return CelValue::CreateInt64(*conv);
          }));
  if (!status.ok()) return status;

  // int -> int
  status =
      registry->Register(PortableUnaryFunctionAdapter<int64_t, int64_t>::Create(
          builtin::kInt, false, [](Arena*, int64_t v) { return v; }));
  if (!status.ok()) return status;

  // string -> int
  status = registry->Register(
      PortableUnaryFunctionAdapter<CelValue, CelValue::StringHolder>::Create(
          builtin::kInt, false, [](Arena* arena, CelValue::StringHolder s) {
            int64_t result;
            if (!absl::SimpleAtoi(s.value(), &result)) {
              return CreateErrorValue(arena, "cannot convert string to int",
                                      absl::StatusCode::kInvalidArgument);
            }
            return CelValue::CreateInt64(result);
          }));
  if (!status.ok()) return status;

  // time -> int
  status = registry->Register(
      PortableUnaryFunctionAdapter<int64_t, absl::Time>::Create(
          builtin::kInt, false,
          [](Arena*, absl::Time t) { return absl::ToUnixSeconds(t); }));
  if (!status.ok()) return status;

  // uint -> int
  return registry->Register(
      PortableUnaryFunctionAdapter<CelValue, uint64_t>::Create(
          builtin::kInt, false, [](Arena* arena, uint64_t v) {
            auto conv = cel::internal::CheckedUint64ToInt64(v);
            if (!conv.ok()) {
              return CreateErrorValue(arena, conv.status());
            }
            return CelValue::CreateInt64(*conv);
          }));
}

absl::Status RegisterStringConversionFunctions(
    CelFunctionRegistry* registry, const InterpreterOptions& options) {
  // May be optionally disabled to reduce potential allocs.
  if (!options.enable_string_conversion) {
    return absl::OkStatus();
  }

  auto status = registry->Register(
      PortableUnaryFunctionAdapter<CelValue, CelValue::BytesHolder>::Create(
          builtin::kString, false,
          [](Arena* arena, CelValue::BytesHolder value) -> CelValue {
            if (::cel::internal::Utf8IsValid(value.value())) {
              return CelValue::CreateStringView(value.value());
            }
            return CreateErrorValue(arena, "invalid UTF-8 bytes value",
                                    absl::StatusCode::kInvalidArgument);
          }));
  if (!status.ok()) return status;

  // double -> string
  status = registry->Register(
      PortableUnaryFunctionAdapter<CelValue::StringHolder, double>::Create(
          builtin::kString, false,
          [](Arena* arena, double value) -> CelValue::StringHolder {
            return CelValue::StringHolder(
                Arena::Create<std::string>(arena, absl::StrCat(value)));
          }));
  if (!status.ok()) return status;

  // int -> string
  status = registry->Register(
      PortableUnaryFunctionAdapter<CelValue::StringHolder, int64_t>::Create(
          builtin::kString, false,
          [](Arena* arena, int64_t value) -> CelValue::StringHolder {
            return CelValue::StringHolder(
                Arena::Create<std::string>(arena, absl::StrCat(value)));
          }));
  if (!status.ok()) return status;

  // string -> string
  status = registry->Register(
      PortableUnaryFunctionAdapter<CelValue::StringHolder,
                                   CelValue::StringHolder>::
          Create(builtin::kString, false,
                 [](Arena*, CelValue::StringHolder value)
                     -> CelValue::StringHolder { return value; }));
  if (!status.ok()) return status;

  // uint -> string
  status = registry->Register(
      PortableUnaryFunctionAdapter<CelValue::StringHolder, uint64_t>::Create(
          builtin::kString, false,
          [](Arena* arena, uint64_t value) -> CelValue::StringHolder {
            return CelValue::StringHolder(
                Arena::Create<std::string>(arena, absl::StrCat(value)));
          }));
  if (!status.ok()) return status;

  // duration -> string
  status = registry->Register(
      PortableUnaryFunctionAdapter<CelValue, absl::Duration>::Create(
          builtin::kString, false,
          [](Arena* arena, absl::Duration value) -> CelValue {
            auto encode = EncodeDurationToString(value);
            if (!encode.ok()) {
              return CreateErrorValue(arena, encode.status());
            }
            return CelValue::CreateString(CelValue::StringHolder(
                Arena::Create<std::string>(arena, *encode)));
          }));
  if (!status.ok()) return status;

  // timestamp -> string
  return registry->Register(
      PortableUnaryFunctionAdapter<CelValue, absl::Time>::Create(
          builtin::kString, false,
          [](Arena* arena, absl::Time value) -> CelValue {
            auto encode = EncodeTimeToString(value);
            if (!encode.ok()) {
              return CreateErrorValue(arena, encode.status());
            }
            return CelValue::CreateString(CelValue::StringHolder(
                Arena::Create<std::string>(arena, *encode)));
          }));
}

absl::Status RegisterUintConversionFunctions(CelFunctionRegistry* registry,
                                             const InterpreterOptions&) {
  // double -> uint
  auto status =
      registry->Register(PortableUnaryFunctionAdapter<CelValue, double>::Create(
          builtin::kUint, false, [](Arena* arena, double v) {
            auto conv = cel::internal::CheckedDoubleToUint64(v);
            if (!conv.ok()) {
              return CreateErrorValue(arena, conv.status());
            }
            return CelValue::CreateUint64(*conv);
          }));
  if (!status.ok()) return status;

  // int -> uint
  status = registry->Register(
      PortableUnaryFunctionAdapter<CelValue, int64_t>::Create(
          builtin::kUint, false, [](Arena* arena, int64_t v) {
            auto conv = cel::internal::CheckedInt64ToUint64(v);
            if (!conv.ok()) {
              return CreateErrorValue(arena, conv.status());
            }
            return CelValue::CreateUint64(*conv);
          }));
  if (!status.ok()) return status;

  // string -> uint
  status = registry->Register(
      PortableUnaryFunctionAdapter<CelValue, CelValue::StringHolder>::Create(
          builtin::kUint, false, [](Arena* arena, CelValue::StringHolder s) {
            uint64_t result;
            if (!absl::SimpleAtoi(s.value(), &result)) {
              return CreateErrorValue(arena, "doesn't convert to a string",
                                      absl::StatusCode::kInvalidArgument);
            }
            return CelValue::CreateUint64(result);
          }));
  if (!status.ok()) return status;

  // uint -> uint
  return registry->Register(
      PortableUnaryFunctionAdapter<uint64_t, uint64_t>::Create(
          builtin::kUint, false, [](Arena*, uint64_t v) { return v; }));
}

absl::Status RegisterConversionFunctions(CelFunctionRegistry* registry,
                                         const InterpreterOptions& options) {
  auto status = RegisterBytesConversionFunctions(registry, options);
  if (!status.ok()) return status;

  status = RegisterDoubleConversionFunctions(registry, options);
  if (!status.ok()) return status;

  // duration() conversion from string.
  status = registry->Register(
      PortableUnaryFunctionAdapter<CelValue, CelValue::StringHolder>::Create(
          builtin::kDuration, false, CreateDurationFromString));
  if (!status.ok()) return status;

  // dyn() identity function.
  // TODO(issues/102): strip dyn() function references at type-check time.
  status = registry->Register(
      PortableUnaryFunctionAdapter<CelValue, CelValue>::Create(
          builtin::kDyn, false,
          [](Arena*, CelValue value) -> CelValue { return value; }));

  status = RegisterIntConversionFunctions(registry, options);
  if (!status.ok()) return status;

  status = RegisterStringConversionFunctions(registry, options);
  if (!status.ok()) return status;

  // timestamp conversion from int.
  status = registry->Register(
      PortableUnaryFunctionAdapter<CelValue, int64_t>::Create(
          builtin::kTimestamp, false,
          [](Arena*, int64_t epoch_seconds) -> CelValue {
            return CelValue::CreateTimestamp(
                absl::FromUnixSeconds(epoch_seconds));
          }));

  // timestamp() conversion from string.
  bool enable_timestamp_duration_overflow_errors =
      options.enable_timestamp_duration_overflow_errors;
  status = registry->Register(
      PortableUnaryFunctionAdapter<CelValue, CelValue::StringHolder>::Create(
          builtin::kTimestamp, false,
          [=](Arena* arena, CelValue::StringHolder time_str) -> CelValue {
            absl::Time ts;
            if (!absl::ParseTime(absl::RFC3339_full, time_str.value(), &ts,
                                 nullptr)) {
              return CreateErrorValue(arena,
                                      "String to Timestamp conversion failed",
                                      absl::StatusCode::kInvalidArgument);
            }
            if (enable_timestamp_duration_overflow_errors) {
              if (ts < absl::UniversalEpoch() || ts > kMaxTime) {
                return CreateErrorValue(arena, "timestamp overflow",
                                        absl::StatusCode::kOutOfRange);
              }
            }
            return CelValue::CreateTimestamp(ts);
          }));
  if (!status.ok()) return status;

  return RegisterUintConversionFunctions(registry, options);
}

absl::Status RegisterCheckedTimeArithmeticFunctions(
    CelFunctionRegistry* registry) {
  CEL_RETURN_IF_ERROR(registry->Register(
      BinaryFunctionAdapter<Handle<Value>, absl::Time,
                            absl::Duration>::CreateDescriptor(builtin::kAdd,
                                                              false),
      BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>, absl::Time,
                            absl::Duration>::
          WrapFunction([](ValueFactory& value_factory, absl::Time t1,
                          absl::Duration d2) -> absl::StatusOr<Handle<Value>> {
            auto sum = cel::internal::CheckedAdd(t1, d2);
            if (!sum.ok()) {
              return value_factory.CreateErrorValue(sum.status());
            }
            return value_factory.CreateTimestampValue(*sum);
          })));

  CEL_RETURN_IF_ERROR(registry->Register(
      BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>, absl::Duration,
                            absl::Time>::CreateDescriptor(builtin::kAdd, false),
      BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>, absl::Duration,
                            absl::Time>::
          WrapFunction([](ValueFactory& value_factory, absl::Duration d2,
                          absl::Time t1) -> absl::StatusOr<Handle<Value>> {
            auto sum = cel::internal::CheckedAdd(t1, d2);
            if (!sum.ok()) {
              return value_factory.CreateErrorValue(sum.status());
            }
            return value_factory.CreateTimestampValue(*sum);
          })));

  CEL_RETURN_IF_ERROR(registry->Register(
      BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>, absl::Duration,
                            absl::Duration>::CreateDescriptor(builtin::kAdd,
                                                              false),
      BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>, absl::Duration,
                            absl::Duration>::
          WrapFunction([](ValueFactory& value_factory, absl::Duration d1,
                          absl::Duration d2) -> absl::StatusOr<Handle<Value>> {
            auto sum = cel::internal::CheckedAdd(d1, d2);
            if (!sum.ok()) {
              return value_factory.CreateErrorValue(sum.status());
            }
            return value_factory.CreateDurationValue(*sum);
          })));

  CEL_RETURN_IF_ERROR(registry->Register(
      BinaryFunctionAdapter<
          absl::StatusOr<Handle<Value>>, absl::Time,
          absl::Duration>::CreateDescriptor(builtin::kSubtract, false),
      BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>, absl::Time,
                            absl::Duration>::
          WrapFunction([](ValueFactory& value_factory, absl::Time t1,
                          absl::Duration d2) -> absl::StatusOr<Handle<Value>> {
            auto diff = cel::internal::CheckedSub(t1, d2);
            if (!diff.ok()) {
              return value_factory.CreateErrorValue(diff.status());
            }
            return value_factory.CreateTimestampValue(*diff);
          })));

  CEL_RETURN_IF_ERROR(registry->Register(
      BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>, absl::Time,
                            absl::Time>::CreateDescriptor(builtin::kSubtract,
                                                          false),
      BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>, absl::Time,
                            absl::Time>::
          WrapFunction([](ValueFactory& value_factory, absl::Time t1,
                          absl::Time t2) -> absl::StatusOr<Handle<Value>> {
            auto diff = cel::internal::CheckedSub(t1, t2);
            if (!diff.ok()) {
              return value_factory.CreateErrorValue(diff.status());
            }
            return value_factory.CreateDurationValue(*diff);
          })));

  CEL_RETURN_IF_ERROR(registry->Register(
      BinaryFunctionAdapter<
          absl::StatusOr<Handle<Value>>, absl::Duration,
          absl::Duration>::CreateDescriptor(builtin::kSubtract, false),
      BinaryFunctionAdapter<absl::StatusOr<Handle<Value>>, absl::Duration,
                            absl::Duration>::
          WrapFunction([](ValueFactory& value_factory, absl::Duration d1,
                          absl::Duration d2) -> absl::StatusOr<Handle<Value>> {
            auto diff = cel::internal::CheckedSub(d1, d2);
            if (!diff.ok()) {
              return value_factory.CreateErrorValue(diff.status());
            }
            return value_factory.CreateDurationValue(*diff);
          })));

  return absl::OkStatus();
}

absl::Status RegisterUncheckedTimeArithmeticFunctions(
    CelFunctionRegistry* registry) {
  // TODO(issues/5): deprecate unchecked time math functions when clients no
  // longer depend on them.
  CEL_RETURN_IF_ERROR(registry->Register(
      BinaryFunctionAdapter<Handle<Value>, absl::Time,
                            absl::Duration>::CreateDescriptor(builtin::kAdd,
                                                              false),
      BinaryFunctionAdapter<Handle<Value>, absl::Time, absl::Duration>::
          WrapFunction([](ValueFactory&, absl::Time t1,
                          absl::Duration d2) -> Handle<Value> {
            return cel::interop_internal::CreateTimestampValue(t1 + d2);
          })));

  CEL_RETURN_IF_ERROR(registry->Register(
      BinaryFunctionAdapter<Handle<Value>, absl::Duration,
                            absl::Time>::CreateDescriptor(builtin::kAdd, false),
      BinaryFunctionAdapter<Handle<Value>, absl::Duration, absl::Time>::
          WrapFunction([](ValueFactory&, absl::Duration d2,
                          absl::Time t1) -> Handle<Value> {
            return cel::interop_internal::CreateTimestampValue(t1 + d2);
          })));

  CEL_RETURN_IF_ERROR(registry->Register(
      BinaryFunctionAdapter<Handle<Value>, absl::Duration,
                            absl::Duration>::CreateDescriptor(builtin::kAdd,
                                                              false),
      BinaryFunctionAdapter<Handle<Value>, absl::Duration, absl::Duration>::
          WrapFunction([](ValueFactory&, absl::Duration d1,
                          absl::Duration d2) -> Handle<Value> {
            return cel::interop_internal::CreateDurationValue(d1 + d2);
          })));

  CEL_RETURN_IF_ERROR(registry->Register(
      BinaryFunctionAdapter<Handle<Value>, absl::Time, absl::Duration>::
          CreateDescriptor(builtin::kSubtract, false),

      BinaryFunctionAdapter<Handle<Value>, absl::Time, absl::Duration>::
          WrapFunction(

              [](ValueFactory&, absl::Time t1,
                 absl::Duration d2) -> Handle<Value> {
                return cel::interop_internal::CreateTimestampValue(t1 - d2);
              })));

  CEL_RETURN_IF_ERROR(registry->Register(
      BinaryFunctionAdapter<Handle<Value>, absl::Time,
                            absl::Time>::CreateDescriptor(builtin::kSubtract,
                                                          false),
      BinaryFunctionAdapter<Handle<Value>, absl::Time, absl::Time>::
          WrapFunction(

              [](ValueFactory&, absl::Time t1, absl::Time t2) -> Handle<Value> {
                return cel::interop_internal::CreateDurationValue(t1 - t2);
              })));

  CEL_RETURN_IF_ERROR(registry->Register(
      BinaryFunctionAdapter<Handle<Value>, absl::Duration, absl::Duration>::
          CreateDescriptor(builtin::kSubtract, false),
      BinaryFunctionAdapter<Handle<Value>, absl::Duration, absl::Duration>::
          WrapFunction([](ValueFactory&, absl::Duration d1,
                          absl::Duration d2) -> Handle<Value> {
            return cel::interop_internal::CreateDurationValue(d1 - d2);
          })));

  return absl::OkStatus();
}

absl::Status RegisterTimeFunctions(CelFunctionRegistry* registry,
                                   const InterpreterOptions& options) {
  CEL_RETURN_IF_ERROR(RegisterTimestampFunctions(registry, options));

  // Special arithmetic operators for Timestamp and Duration
  if (options.enable_timestamp_duration_overflow_errors) {
    CEL_RETURN_IF_ERROR(RegisterCheckedTimeArithmeticFunctions(registry));
  } else {
    CEL_RETURN_IF_ERROR(RegisterUncheckedTimeArithmeticFunctions(registry));
  }

  // duration breakdown accessor functions
  using DurationAccessorFunction =
      UnaryFunctionAdapter<int64_t, absl::Duration>;
  CEL_RETURN_IF_ERROR(registry->Register(
      DurationAccessorFunction::CreateDescriptor(builtin::kHours, true),
      DurationAccessorFunction::WrapFunction(
          [](ValueFactory&, absl::Duration d) -> int64_t {
            return absl::ToInt64Hours(d);
          })));

  CEL_RETURN_IF_ERROR(registry->Register(
      DurationAccessorFunction::CreateDescriptor(builtin::kMinutes, true),
      DurationAccessorFunction::WrapFunction(
          [](ValueFactory&, absl::Duration d) -> int64_t {
            return absl::ToInt64Minutes(d);
          })));

  CEL_RETURN_IF_ERROR(registry->Register(
      DurationAccessorFunction::CreateDescriptor(builtin::kSeconds, true),
      DurationAccessorFunction::WrapFunction(
          [](ValueFactory&, absl::Duration d) -> int64_t {
            return absl::ToInt64Seconds(d);
          })));

  CEL_RETURN_IF_ERROR(registry->Register(
      DurationAccessorFunction::CreateDescriptor(builtin::kMilliseconds, true),
      DurationAccessorFunction::WrapFunction(
          [](ValueFactory&, absl::Duration d) -> int64_t {
            constexpr int64_t millis_per_second = 1000L;
            return absl::ToInt64Milliseconds(d) % millis_per_second;
          })));

  return absl::OkStatus();
}

}  // namespace

absl::Status RegisterBuiltinFunctions(CelFunctionRegistry* registry,
                                      const InterpreterOptions& options) {
  CEL_RETURN_IF_ERROR(registry->RegisterAll(
      {
          &RegisterEqualityFunctions,
          &RegisterComparisonFunctions,
          &RegisterLogicalFunctions,
          &RegisterNumericArithmeticFunctions,
          &RegisterConversionFunctions,
          &RegisterTimeFunctions,
          &RegisterStringFunctions,
      },
      options));

  // List size
  auto list_size_func = [](Arena*, const CelList* cel_list) -> int64_t {
    return (*cel_list).size();
  };
  // receiver style = true/false
  // Support both the global and receiver style size() for lists.
  auto status = registry->Register(
      PortableUnaryFunctionAdapter<int64_t, const CelList*>::Create(
          builtin::kSize, true, list_size_func));
  if (!status.ok()) return status;
  status = registry->Register(
      PortableUnaryFunctionAdapter<int64_t, const CelList*>::Create(
          builtin::kSize, false, list_size_func));
  if (!status.ok()) return status;

  // Map size
  auto map_size_func = [](Arena*, const CelMap* cel_map) -> int64_t {
    return (*cel_map).size();
  };
  // receiver style = true/false
  status = registry->Register(
      PortableUnaryFunctionAdapter<int64_t, const CelMap*>::Create(
          builtin::kSize, true, map_size_func));
  if (!status.ok()) return status;
  status = registry->Register(
      PortableUnaryFunctionAdapter<int64_t, const CelMap*>::Create(
          builtin::kSize, false, map_size_func));
  if (!status.ok()) return status;

  // Register set membership tests with the 'in' operator and its variants.
  status = RegisterSetMembershipFunctions(registry, options);
  if (!status.ok()) return status;

  if (options.enable_list_concat) {
    status = registry->Register(
        PortableBinaryFunctionAdapter<const CelList*, const CelList*,
                                      const CelList*>::Create(builtin::kAdd,
                                                              false,
                                                              ConcatList));
    if (!status.ok()) return status;
  }

  status =
      registry->Register(PortableBinaryFunctionAdapter<
                         const CelList*, const CelList*,
                         const CelList*>::Create(builtin::kRuntimeListAppend,
                                                 false, AppendList));
  if (!status.ok()) return status;

  return registry->Register(
      PortableUnaryFunctionAdapter<CelValue::CelTypeHolder, CelValue>::Create(
          builtin::kType, false,
          [](Arena*, CelValue value) -> CelValue::CelTypeHolder {
            return value.ObtainCelType().CelTypeOrDie();
          }));
}

}  // namespace google::api::expr::runtime
