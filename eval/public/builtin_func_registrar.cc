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
#include "eval/eval/mutable_list_impl.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_number.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/comparison_functions.h"
#include "eval/public/containers/container_backed_list_impl.h"
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

using ::cel::internal::EncodeDurationToString;
using ::cel::internal::EncodeTimeToString;
using ::cel::internal::MaxTimestamp;
using ::google::protobuf::Arena;

// Time representing `9999-12-31T23:59:59.999999999Z`.
const absl::Time kMaxTime = MaxTimestamp();

// Template functions providing arithmetic operations
template <class Type>
CelValue Add(Arena*, Type v0, Type v1);

template <>
CelValue Add<int64_t>(Arena* arena, int64_t v0, int64_t v1) {
  auto sum = cel::internal::CheckedAdd(v0, v1);
  if (!sum.ok()) {
    return CreateErrorValue(arena, sum.status());
  }
  return CelValue::CreateInt64(*sum);
}

template <>
CelValue Add<uint64_t>(Arena* arena, uint64_t v0, uint64_t v1) {
  auto sum = cel::internal::CheckedAdd(v0, v1);
  if (!sum.ok()) {
    return CreateErrorValue(arena, sum.status());
  }
  return CelValue::CreateUint64(*sum);
}

template <>
CelValue Add<double>(Arena*, double v0, double v1) {
  return CelValue::CreateDouble(v0 + v1);
}

template <class Type>
CelValue Sub(Arena*, Type v0, Type v1);

template <>
CelValue Sub<int64_t>(Arena* arena, int64_t v0, int64_t v1) {
  auto diff = cel::internal::CheckedSub(v0, v1);
  if (!diff.ok()) {
    return CreateErrorValue(arena, diff.status());
  }
  return CelValue::CreateInt64(*diff);
}

template <>
CelValue Sub<uint64_t>(Arena* arena, uint64_t v0, uint64_t v1) {
  auto diff = cel::internal::CheckedSub(v0, v1);
  if (!diff.ok()) {
    return CreateErrorValue(arena, diff.status());
  }
  return CelValue::CreateUint64(*diff);
}

template <>
CelValue Sub<double>(Arena*, double v0, double v1) {
  return CelValue::CreateDouble(v0 - v1);
}

template <class Type>
CelValue Mul(Arena*, Type v0, Type v1);

template <>
CelValue Mul<int64_t>(Arena* arena, int64_t v0, int64_t v1) {
  auto prod = cel::internal::CheckedMul(v0, v1);
  if (!prod.ok()) {
    return CreateErrorValue(arena, prod.status());
  }
  return CelValue::CreateInt64(*prod);
}

template <>
CelValue Mul<uint64_t>(Arena* arena, uint64_t v0, uint64_t v1) {
  auto prod = cel::internal::CheckedMul(v0, v1);
  if (!prod.ok()) {
    return CreateErrorValue(arena, prod.status());
  }
  return CelValue::CreateUint64(*prod);
}

template <>
CelValue Mul<double>(Arena*, double v0, double v1) {
  return CelValue::CreateDouble(v0 * v1);
}

template <class Type>
CelValue Div(Arena* arena, Type v0, Type v1);

// Division operations for integer types should check for
// division by 0
template <>
CelValue Div<int64_t>(Arena* arena, int64_t v0, int64_t v1) {
  auto quot = cel::internal::CheckedDiv(v0, v1);
  if (!quot.ok()) {
    return CreateErrorValue(arena, quot.status());
  }
  return CelValue::CreateInt64(*quot);
}

// Division operations for integer types should check for
// division by 0
template <>
CelValue Div<uint64_t>(Arena* arena, uint64_t v0, uint64_t v1) {
  auto quot = cel::internal::CheckedDiv(v0, v1);
  if (!quot.ok()) {
    return CreateErrorValue(arena, quot.status());
  }
  return CelValue::CreateUint64(*quot);
}

template <>
CelValue Div<double>(Arena*, double v0, double v1) {
  static_assert(std::numeric_limits<double>::is_iec559,
                "Division by zero for doubles must be supported");

  // For double, division will result in +/- inf
  return CelValue::CreateDouble(v0 / v1);
}

// Modulo operation
template <class Type>
CelValue Modulo(Arena* arena, Type v0, Type v1);

// Modulo operations for integer types should check for
// division by 0
template <>
CelValue Modulo<int64_t>(Arena* arena, int64_t v0, int64_t v1) {
  auto mod = cel::internal::CheckedMod(v0, v1);
  if (!mod.ok()) {
    return CreateErrorValue(arena, mod.status());
  }
  return CelValue::CreateInt64(*mod);
}

template <>
CelValue Modulo<uint64_t>(Arena* arena, uint64_t v0, uint64_t v1) {
  auto mod = cel::internal::CheckedMod(v0, v1);
  if (!mod.ok()) {
    return CreateErrorValue(arena, mod.status());
  }
  return CelValue::CreateUint64(*mod);
}

// Helper method
// Registers all arithmetic functions for template parameter type.
template <class Type>
absl::Status RegisterArithmeticFunctionsForType(CelFunctionRegistry* registry) {
  absl::Status status =
      PortableFunctionAdapter<CelValue, Type, Type>::CreateAndRegister(
          builtin::kAdd, false, Add<Type>, registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<CelValue, Type, Type>::CreateAndRegister(
      builtin::kSubtract, false, Sub<Type>, registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<CelValue, Type, Type>::CreateAndRegister(
      builtin::kMultiply, false, Mul<Type>, registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<CelValue, Type, Type>::CreateAndRegister(
      builtin::kDivide, false, Div<Type>, registry);
  return status;
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
bool In(Arena*, T value, const CelList* list) {
  int index_size = list->size();

  for (int i = 0; i < index_size; i++) {
    CelValue element = (*list)[i];

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
    CelValue element = (*list)[i];
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
    mutable_list->Append((*value2)[i]);
  }
  return mutable_list;
}

// Concatenation for StringHolder type.
CelValue::StringHolder ConcatString(Arena* arena, CelValue::StringHolder value1,
                                    CelValue::StringHolder value2) {
  auto concatenated = Arena::Create<std::string>(
      arena, absl::StrCat(value1.value(), value2.value()));
  return CelValue::StringHolder(concatenated);
}

// Concatenation for BytesHolder type.
CelValue::BytesHolder ConcatBytes(Arena* arena, CelValue::BytesHolder value1,
                                  CelValue::BytesHolder value2) {
  auto concatenated = Arena::Create<std::string>(
      arena, absl::StrCat(value1.value(), value2.value()));
  return CelValue::BytesHolder(concatenated);
}

// Concatenation for CelList type.
const CelList* ConcatList(Arena* arena, const CelList* value1,
                          const CelList* value2) {
  std::vector<CelValue> joined_values;

  int size1 = value1->size();
  int size2 = value2->size();
  joined_values.reserve(size1 + size2);

  for (int i = 0; i < size1; i++) {
    joined_values.push_back((*value1)[i]);
  }
  for (int i = 0; i < size2; i++) {
    joined_values.push_back((*value2)[i]);
  }

  auto concatenated =
      Arena::Create<ContainerBackedListImpl>(arena, joined_values);
  return concatenated;
}

// Timestamp
const absl::Status FindTimeBreakdown(absl::Time timestamp, absl::string_view tz,
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

CelValue GetHours(Arena*, absl::Duration duration) {
  return CelValue::CreateInt64(absl::ToInt64Hours(duration));
}

CelValue GetMinutes(Arena*, absl::Duration duration) {
  return CelValue::CreateInt64(absl::ToInt64Minutes(duration));
}

CelValue GetSeconds(Arena*, absl::Duration duration) {
  return CelValue::CreateInt64(absl::ToInt64Seconds(duration));
}

CelValue GetMilliseconds(Arena*, absl::Duration duration) {
  int64_t millis_per_second = 1000L;
  return CelValue::CreateInt64(absl::ToInt64Milliseconds(duration) %
                               millis_per_second);
}

bool StringContains(Arena*, CelValue::StringHolder value,
                    CelValue::StringHolder substr) {
  return absl::StrContains(value.value(), substr.value());
}

bool StringEndsWith(Arena*, CelValue::StringHolder value,
                    CelValue::StringHolder suffix) {
  return absl::EndsWith(value.value(), suffix.value());
}

bool StringStartsWith(Arena*, CelValue::StringHolder value,
                      CelValue::StringHolder prefix) {
  return absl::StartsWith(value.value(), prefix.value());
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
        CEL_RETURN_IF_ERROR(
            (PortableFunctionAdapter<CelValue, CelValue, const CelList*>::
                 CreateAndRegister(op, false, &HeterogeneousEqualityIn,
                                   registry)));
      } else {
        CEL_RETURN_IF_ERROR(
            (PortableFunctionAdapter<bool, bool, const CelList*>::
                 CreateAndRegister(op, false, In<bool>, registry)));
        CEL_RETURN_IF_ERROR(
            (PortableFunctionAdapter<bool, int64_t, const CelList*>::
                 CreateAndRegister(op, false, In<int64_t>, registry)));
        CEL_RETURN_IF_ERROR(
            (PortableFunctionAdapter<bool, uint64_t, const CelList*>::
                 CreateAndRegister(op, false, In<uint64_t>, registry)));
        CEL_RETURN_IF_ERROR(
            (PortableFunctionAdapter<bool, double, const CelList*>::
                 CreateAndRegister(op, false, In<double>, registry)));
        CEL_RETURN_IF_ERROR(
            (PortableFunctionAdapter<
                bool, CelValue::StringHolder,
                const CelList*>::CreateAndRegister(op, false,
                                                   In<CelValue::StringHolder>,
                                                   registry)));
        CEL_RETURN_IF_ERROR(
            (PortableFunctionAdapter<
                bool, CelValue::BytesHolder,
                const CelList*>::CreateAndRegister(op, false,
                                                   In<CelValue::BytesHolder>,
                                                   registry)));
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
    auto status = PortableFunctionAdapter<
        CelValue, CelValue::StringHolder,
        const CelMap*>::CreateAndRegister(op, false, stringKeyInSet, registry);
    if (!status.ok()) return status;

    status =
        PortableFunctionAdapter<CelValue, bool,
                                const CelMap*>::CreateAndRegister(op, false,
                                                                  boolKeyInSet,
                                                                  registry);
    if (!status.ok()) return status;

    status =
        PortableFunctionAdapter<CelValue, int64_t,
                                const CelMap*>::CreateAndRegister(op, false,
                                                                  intKeyInSet,
                                                                  registry);
    if (!status.ok()) return status;

    status =
        PortableFunctionAdapter<CelValue, uint64_t,
                                const CelMap*>::CreateAndRegister(op, false,
                                                                  uintKeyInSet,
                                                                  registry);
    if (!status.ok()) return status;

    if (options.enable_heterogeneous_equality) {
      status = PortableFunctionAdapter<
          CelValue, double, const CelMap*>::CreateAndRegister(op, false,
                                                              doubleKeyInSet,
                                                              registry);
      if (!status.ok()) return status;
    }
  }
  return absl::OkStatus();
}

absl::Status RegisterStringFunctions(CelFunctionRegistry* registry,
                                     const InterpreterOptions& options) {
  auto status = PortableFunctionAdapter<
      bool, CelValue::StringHolder,
      CelValue::StringHolder>::CreateAndRegister(builtin::kStringContains,
                                                 false, StringContains,
                                                 registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<
      bool, CelValue::StringHolder,
      CelValue::StringHolder>::CreateAndRegister(builtin::kStringContains, true,
                                                 StringContains, registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<
      bool, CelValue::StringHolder,
      CelValue::StringHolder>::CreateAndRegister(builtin::kStringEndsWith,
                                                 false, StringEndsWith,
                                                 registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<
      bool, CelValue::StringHolder,
      CelValue::StringHolder>::CreateAndRegister(builtin::kStringEndsWith, true,
                                                 StringEndsWith, registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<
      bool, CelValue::StringHolder,
      CelValue::StringHolder>::CreateAndRegister(builtin::kStringStartsWith,
                                                 false, StringStartsWith,
                                                 registry);
  if (!status.ok()) return status;

  return PortableFunctionAdapter<
      bool, CelValue::StringHolder,
      CelValue::StringHolder>::CreateAndRegister(builtin::kStringStartsWith,
                                                 true, StringStartsWith,
                                                 registry);
}

absl::Status RegisterTimestampFunctions(CelFunctionRegistry* registry,
                                        const InterpreterOptions& options) {
  auto status =
      PortableFunctionAdapter<CelValue, absl::Time, CelValue::StringHolder>::
          CreateAndRegister(
              builtin::kFullYear, true,
              [](Arena* arena, absl::Time ts, CelValue::StringHolder tz)
                  -> CelValue { return GetFullYear(arena, ts, tz.value()); },
              registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<CelValue, absl::Time>::CreateAndRegister(
      builtin::kFullYear, true,
      [](Arena* arena, absl::Time ts) -> CelValue {
        return GetFullYear(arena, ts, "");
      },
      registry);
  if (!status.ok()) return status;

  status =
      PortableFunctionAdapter<CelValue, absl::Time, CelValue::StringHolder>::
          CreateAndRegister(
              builtin::kMonth, true,
              [](Arena* arena, absl::Time ts, CelValue::StringHolder tz)
                  -> CelValue { return GetMonth(arena, ts, tz.value()); },
              registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<CelValue, absl::Time>::CreateAndRegister(
      builtin::kMonth, true,
      [](Arena* arena, absl::Time ts) -> CelValue {
        return GetMonth(arena, ts, "");
      },
      registry);
  if (!status.ok()) return status;

  status =
      PortableFunctionAdapter<CelValue, absl::Time, CelValue::StringHolder>::
          CreateAndRegister(
              builtin::kDayOfYear, true,
              [](Arena* arena, absl::Time ts, CelValue::StringHolder tz)
                  -> CelValue { return GetDayOfYear(arena, ts, tz.value()); },
              registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<CelValue, absl::Time>::CreateAndRegister(
      builtin::kDayOfYear, true,
      [](Arena* arena, absl::Time ts) -> CelValue {
        return GetDayOfYear(arena, ts, "");
      },
      registry);
  if (!status.ok()) return status;

  status =
      PortableFunctionAdapter<CelValue, absl::Time, CelValue::StringHolder>::
          CreateAndRegister(
              builtin::kDayOfMonth, true,
              [](Arena* arena, absl::Time ts, CelValue::StringHolder tz)
                  -> CelValue { return GetDayOfMonth(arena, ts, tz.value()); },
              registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<CelValue, absl::Time>::CreateAndRegister(
      builtin::kDayOfMonth, true,
      [](Arena* arena, absl::Time ts) -> CelValue {
        return GetDayOfMonth(arena, ts, "");
      },
      registry);
  if (!status.ok()) return status;

  status =
      PortableFunctionAdapter<CelValue, absl::Time, CelValue::StringHolder>::
          CreateAndRegister(
              builtin::kDate, true,
              [](Arena* arena, absl::Time ts, CelValue::StringHolder tz)
                  -> CelValue { return GetDate(arena, ts, tz.value()); },
              registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<CelValue, absl::Time>::CreateAndRegister(
      builtin::kDate, true,
      [](Arena* arena, absl::Time ts) -> CelValue {
        return GetDate(arena, ts, "");
      },
      registry);
  if (!status.ok()) return status;

  status =
      PortableFunctionAdapter<CelValue, absl::Time, CelValue::StringHolder>::
          CreateAndRegister(
              builtin::kDayOfWeek, true,
              [](Arena* arena, absl::Time ts, CelValue::StringHolder tz)
                  -> CelValue { return GetDayOfWeek(arena, ts, tz.value()); },
              registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<CelValue, absl::Time>::CreateAndRegister(
      builtin::kDayOfWeek, true,
      [](Arena* arena, absl::Time ts) -> CelValue {
        return GetDayOfWeek(arena, ts, "");
      },
      registry);
  if (!status.ok()) return status;

  status =
      PortableFunctionAdapter<CelValue, absl::Time, CelValue::StringHolder>::
          CreateAndRegister(
              builtin::kHours, true,
              [](Arena* arena, absl::Time ts, CelValue::StringHolder tz)
                  -> CelValue { return GetHours(arena, ts, tz.value()); },
              registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<CelValue, absl::Time>::CreateAndRegister(
      builtin::kHours, true,
      [](Arena* arena, absl::Time ts) -> CelValue {
        return GetHours(arena, ts, "");
      },
      registry);
  if (!status.ok()) return status;

  status =
      PortableFunctionAdapter<CelValue, absl::Time, CelValue::StringHolder>::
          CreateAndRegister(
              builtin::kMinutes, true,
              [](Arena* arena, absl::Time ts, CelValue::StringHolder tz)
                  -> CelValue { return GetMinutes(arena, ts, tz.value()); },
              registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<CelValue, absl::Time>::CreateAndRegister(
      builtin::kMinutes, true,
      [](Arena* arena, absl::Time ts) -> CelValue {
        return GetMinutes(arena, ts, "");
      },
      registry);
  if (!status.ok()) return status;

  status =
      PortableFunctionAdapter<CelValue, absl::Time, CelValue::StringHolder>::
          CreateAndRegister(
              builtin::kSeconds, true,
              [](Arena* arena, absl::Time ts, CelValue::StringHolder tz)
                  -> CelValue { return GetSeconds(arena, ts, tz.value()); },
              registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<CelValue, absl::Time>::CreateAndRegister(
      builtin::kSeconds, true,
      [](Arena* arena, absl::Time ts) -> CelValue {
        return GetSeconds(arena, ts, "");
      },
      registry);
  if (!status.ok()) return status;

  status =
      PortableFunctionAdapter<CelValue, absl::Time, CelValue::StringHolder>::
          CreateAndRegister(
              builtin::kMilliseconds, true,
              [](Arena* arena, absl::Time ts,
                 CelValue::StringHolder tz) -> CelValue {
                return GetMilliseconds(arena, ts, tz.value());
              },
              registry);
  if (!status.ok()) return status;

  return PortableFunctionAdapter<CelValue, absl::Time>::CreateAndRegister(
      builtin::kMilliseconds, true,
      [](Arena* arena, absl::Time ts) -> CelValue {
        return GetMilliseconds(arena, ts, "");
      },
      registry);
}

absl::Status RegisterBytesConversionFunctions(CelFunctionRegistry* registry,
                                              const InterpreterOptions&) {
  // bytes -> bytes
  auto status =
      PortableFunctionAdapter<CelValue::BytesHolder, CelValue::BytesHolder>::
          CreateAndRegister(
              builtin::kBytes, false,
              [](Arena*, CelValue::BytesHolder value) -> CelValue::BytesHolder {
                return value;
              },
              registry);
  if (!status.ok()) return status;

  // string -> bytes
  return PortableFunctionAdapter<CelValue, CelValue::StringHolder>::
      CreateAndRegister(
          builtin::kBytes, false,
          [](Arena* arena, CelValue::StringHolder value) -> CelValue {
            return CelValue::CreateBytesView(value.value());
          },
          registry);
}

absl::Status RegisterDoubleConversionFunctions(CelFunctionRegistry* registry,
                                               const InterpreterOptions&) {
  // double -> double
  auto status = PortableFunctionAdapter<double, double>::CreateAndRegister(
      builtin::kDouble, false, [](Arena*, double v) { return v; }, registry);
  if (!status.ok()) return status;

  // int -> double
  status = PortableFunctionAdapter<double, int64_t>::CreateAndRegister(
      builtin::kDouble, false,
      [](Arena*, int64_t v) { return static_cast<double>(v); }, registry);
  if (!status.ok()) return status;

  // string -> double
  status = PortableFunctionAdapter<CelValue, CelValue::StringHolder>::
      CreateAndRegister(
          builtin::kDouble, false,
          [](Arena* arena, CelValue::StringHolder s) {
            double result;
            if (absl::SimpleAtod(s.value(), &result)) {
              return CelValue::CreateDouble(result);
            } else {
              return CreateErrorValue(arena, "cannot convert string to double",
                                      absl::StatusCode::kInvalidArgument);
            }
          },
          registry);
  if (!status.ok()) return status;

  // uint -> double
  return PortableFunctionAdapter<double, uint64_t>::CreateAndRegister(
      builtin::kDouble, false,
      [](Arena*, uint64_t v) { return static_cast<double>(v); }, registry);
}

absl::Status RegisterIntConversionFunctions(CelFunctionRegistry* registry,
                                            const InterpreterOptions&) {
  // bool -> int
  auto status = PortableFunctionAdapter<int64_t, bool>::CreateAndRegister(
      builtin::kInt, false,
      [](Arena*, bool v) { return static_cast<int64_t>(v); }, registry);
  if (!status.ok()) return status;

  // double -> int
  status = PortableFunctionAdapter<CelValue, double>::CreateAndRegister(
      builtin::kInt, false,
      [](Arena* arena, double v) {
        auto conv = cel::internal::CheckedDoubleToInt64(v);
        if (!conv.ok()) {
          return CreateErrorValue(arena, conv.status());
        }
        return CelValue::CreateInt64(*conv);
      },
      registry);
  if (!status.ok()) return status;

  // int -> int
  status = PortableFunctionAdapter<int64_t, int64_t>::CreateAndRegister(
      builtin::kInt, false, [](Arena*, int64_t v) { return v; }, registry);
  if (!status.ok()) return status;

  // string -> int
  status = PortableFunctionAdapter<CelValue, CelValue::StringHolder>::
      CreateAndRegister(
          builtin::kInt, false,
          [](Arena* arena, CelValue::StringHolder s) {
            int64_t result;
            if (!absl::SimpleAtoi(s.value(), &result)) {
              return CreateErrorValue(arena, "cannot convert string to int",
                                      absl::StatusCode::kInvalidArgument);
            }
            return CelValue::CreateInt64(result);
          },
          registry);
  if (!status.ok()) return status;

  // time -> int
  status = PortableFunctionAdapter<int64_t, absl::Time>::CreateAndRegister(
      builtin::kInt, false,
      [](Arena*, absl::Time t) { return absl::ToUnixSeconds(t); }, registry);
  if (!status.ok()) return status;

  // uint -> int
  return PortableFunctionAdapter<CelValue, uint64_t>::CreateAndRegister(
      builtin::kInt, false,
      [](Arena* arena, uint64_t v) {
        auto conv = cel::internal::CheckedUint64ToInt64(v);
        if (!conv.ok()) {
          return CreateErrorValue(arena, conv.status());
        }
        return CelValue::CreateInt64(*conv);
      },
      registry);
}

absl::Status RegisterStringConversionFunctions(
    CelFunctionRegistry* registry, const InterpreterOptions& options) {
  // May be optionally disabled to reduce potential allocs.
  if (!options.enable_string_conversion) {
    return absl::OkStatus();
  }

  auto status = PortableFunctionAdapter<CelValue, CelValue::BytesHolder>::
      CreateAndRegister(
          builtin::kString, false,
          [](Arena* arena, CelValue::BytesHolder value) -> CelValue {
            if (::cel::internal::Utf8IsValid(value.value())) {
              return CelValue::CreateStringView(value.value());
            }
            return CreateErrorValue(arena, "invalid UTF-8 bytes value",
                                    absl::StatusCode::kInvalidArgument);
          },
          registry);
  if (!status.ok()) return status;

  // double -> string
  status = PortableFunctionAdapter<CelValue::StringHolder, double>::
      CreateAndRegister(
          builtin::kString, false,
          [](Arena* arena, double value) -> CelValue::StringHolder {
            return CelValue::StringHolder(
                Arena::Create<std::string>(arena, absl::StrCat(value)));
          },
          registry);
  if (!status.ok()) return status;

  // int -> string
  status = PortableFunctionAdapter<CelValue::StringHolder, int64_t>::
      CreateAndRegister(
          builtin::kString, false,
          [](Arena* arena, int64_t value) -> CelValue::StringHolder {
            return CelValue::StringHolder(
                Arena::Create<std::string>(arena, absl::StrCat(value)));
          },
          registry);
  if (!status.ok()) return status;

  // string -> string
  status =
      PortableFunctionAdapter<CelValue::StringHolder, CelValue::StringHolder>::
          CreateAndRegister(
              builtin::kString, false,
              [](Arena*, CelValue::StringHolder value)
                  -> CelValue::StringHolder { return value; },
              registry);
  if (!status.ok()) return status;

  // uint -> string
  status = PortableFunctionAdapter<CelValue::StringHolder, uint64_t>::
      CreateAndRegister(
          builtin::kString, false,
          [](Arena* arena, uint64_t value) -> CelValue::StringHolder {
            return CelValue::StringHolder(
                Arena::Create<std::string>(arena, absl::StrCat(value)));
          },
          registry);
  if (!status.ok()) return status;

  // duration -> string
  status = PortableFunctionAdapter<CelValue, absl::Duration>::CreateAndRegister(
      builtin::kString, false,
      [](Arena* arena, absl::Duration value) -> CelValue {
        auto encode = EncodeDurationToString(value);
        if (!encode.ok()) {
          return CreateErrorValue(arena, encode.status());
        }
        return CelValue::CreateString(
            CelValue::StringHolder(Arena::Create<std::string>(arena, *encode)));
      },
      registry);
  if (!status.ok()) return status;

  // timestamp -> string
  return PortableFunctionAdapter<CelValue, absl::Time>::CreateAndRegister(
      builtin::kString, false,
      [](Arena* arena, absl::Time value) -> CelValue {
        auto encode = EncodeTimeToString(value);
        if (!encode.ok()) {
          return CreateErrorValue(arena, encode.status());
        }
        return CelValue::CreateString(
            CelValue::StringHolder(Arena::Create<std::string>(arena, *encode)));
      },
      registry);
}

absl::Status RegisterUintConversionFunctions(CelFunctionRegistry* registry,
                                             const InterpreterOptions&) {
  // double -> uint
  auto status = PortableFunctionAdapter<CelValue, double>::CreateAndRegister(
      builtin::kUint, false,
      [](Arena* arena, double v) {
        auto conv = cel::internal::CheckedDoubleToUint64(v);
        if (!conv.ok()) {
          return CreateErrorValue(arena, conv.status());
        }
        return CelValue::CreateUint64(*conv);
      },
      registry);
  if (!status.ok()) return status;

  // int -> uint
  status = PortableFunctionAdapter<CelValue, int64_t>::CreateAndRegister(
      builtin::kUint, false,
      [](Arena* arena, int64_t v) {
        auto conv = cel::internal::CheckedInt64ToUint64(v);
        if (!conv.ok()) {
          return CreateErrorValue(arena, conv.status());
        }
        return CelValue::CreateUint64(*conv);
      },
      registry);
  if (!status.ok()) return status;

  // string -> uint
  status = PortableFunctionAdapter<CelValue, CelValue::StringHolder>::
      CreateAndRegister(
          builtin::kUint, false,
          [](Arena* arena, CelValue::StringHolder s) {
            uint64_t result;
            if (!absl::SimpleAtoi(s.value(), &result)) {
              return CreateErrorValue(arena, "doesn't convert to a string",
                                      absl::StatusCode::kInvalidArgument);
            }
            return CelValue::CreateUint64(result);
          },
          registry);
  if (!status.ok()) return status;

  // uint -> uint
  return PortableFunctionAdapter<uint64_t, uint64_t>::CreateAndRegister(
      builtin::kUint, false, [](Arena*, uint64_t v) { return v; }, registry);
}

absl::Status RegisterConversionFunctions(CelFunctionRegistry* registry,
                                         const InterpreterOptions& options) {
  auto status = RegisterBytesConversionFunctions(registry, options);
  if (!status.ok()) return status;

  status = RegisterDoubleConversionFunctions(registry, options);
  if (!status.ok()) return status;

  // duration() conversion from string.
  status = PortableFunctionAdapter<CelValue, CelValue::StringHolder>::
      CreateAndRegister(builtin::kDuration, false, CreateDurationFromString,
                        registry);
  if (!status.ok()) return status;

  // dyn() identity function.
  // TODO(issues/102): strip dyn() function references at type-check time.
  status = PortableFunctionAdapter<CelValue, CelValue>::CreateAndRegister(
      builtin::kDyn, false,
      [](Arena*, CelValue value) -> CelValue { return value; }, registry);

  status = RegisterIntConversionFunctions(registry, options);
  if (!status.ok()) return status;

  status = RegisterStringConversionFunctions(registry, options);
  if (!status.ok()) return status;

  // timestamp conversion from int.
  status = PortableFunctionAdapter<CelValue, int64_t>::CreateAndRegister(
      builtin::kTimestamp, false,
      [](Arena*, int64_t epoch_seconds) -> CelValue {
        return CelValue::CreateTimestamp(absl::FromUnixSeconds(epoch_seconds));
      },
      registry);

  // timestamp() conversion from string.
  bool enable_timestamp_duration_overflow_errors =
      options.enable_timestamp_duration_overflow_errors;
  status = PortableFunctionAdapter<CelValue, CelValue::StringHolder>::
      CreateAndRegister(
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
          },
          registry);
  if (!status.ok()) return status;

  return RegisterUintConversionFunctions(registry, options);
}

}  // namespace

absl::Status RegisterBuiltinFunctions(CelFunctionRegistry* registry,
                                      const InterpreterOptions& options) {
  // logical NOT
  absl::Status status = PortableFunctionAdapter<bool, bool>::CreateAndRegister(
      builtin::kNot, false, [](Arena*, bool value) -> bool { return !value; },
      registry);
  if (!status.ok()) return status;

  // Negation group
  status = PortableFunctionAdapter<CelValue, int64_t>::CreateAndRegister(
      builtin::kNeg, false,
      [](Arena* arena, int64_t value) -> CelValue {
        auto inv = cel::internal::CheckedNegation(value);
        if (!inv.ok()) {
          return CreateErrorValue(arena, inv.status());
        }
        return CelValue::CreateInt64(*inv);
      },
      registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<double, double>::CreateAndRegister(
      builtin::kNeg, false,
      [](Arena*, double value) -> double { return -value; }, registry);
  if (!status.ok()) return status;

  CEL_RETURN_IF_ERROR(RegisterComparisonFunctions(registry, options));

  status = RegisterConversionFunctions(registry, options);
  if (!status.ok()) return status;

  // Strictness
  status = PortableFunctionAdapter<bool, bool>::CreateAndRegister(
      builtin::kNotStrictlyFalse, false,
      [](Arena*, bool value) -> bool { return value; }, registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<bool, const CelError*>::CreateAndRegister(
      builtin::kNotStrictlyFalse, false,
      [](Arena*, const CelError*) -> bool { return true; }, registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<bool, const UnknownSet*>::CreateAndRegister(
      builtin::kNotStrictlyFalse, false,
      [](Arena*, const UnknownSet*) -> bool { return true; }, registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<bool, bool>::CreateAndRegister(
      builtin::kNotStrictlyFalseDeprecated, false,
      [](Arena*, bool value) -> bool { return value; }, registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<bool, const CelError*>::CreateAndRegister(
      builtin::kNotStrictlyFalseDeprecated, false,
      [](Arena*, const CelError*) -> bool { return true; }, registry);
  if (!status.ok()) return status;

  // String size
  auto size_func = [](Arena* arena, CelValue::StringHolder value) -> CelValue {
    absl::string_view str = value.value();
    auto [count, valid] = ::cel::internal::Utf8Validate(str);
    if (!valid) {
      return CreateErrorValue(arena, "invalid utf-8 string",
                              absl::StatusCode::kInvalidArgument);
    }
    return CelValue::CreateInt64(static_cast<int64_t>(count));
  };
  // receiver style = true/false
  // Support global and receiver style size() operations on strings.
  status = PortableFunctionAdapter<
      CelValue, CelValue::StringHolder>::CreateAndRegister(builtin::kSize, true,
                                                           size_func, registry);
  if (!status.ok()) return status;
  status = PortableFunctionAdapter<
      CelValue, CelValue::StringHolder>::CreateAndRegister(builtin::kSize,
                                                           false, size_func,
                                                           registry);
  if (!status.ok()) return status;

  // Bytes size
  auto bytes_size_func = [](Arena*, CelValue::BytesHolder value) -> int64_t {
    return value.value().size();
  };
  // receiver style = true/false
  // Support global and receiver style size() operations on bytes.
  status = PortableFunctionAdapter<
      int64_t, CelValue::BytesHolder>::CreateAndRegister(builtin::kSize, true,
                                                         bytes_size_func,
                                                         registry);
  if (!status.ok()) return status;
  status = PortableFunctionAdapter<
      int64_t, CelValue::BytesHolder>::CreateAndRegister(builtin::kSize, false,
                                                         bytes_size_func,
                                                         registry);
  if (!status.ok()) return status;

  // List size
  auto list_size_func = [](Arena*, const CelList* cel_list) -> int64_t {
    return (*cel_list).size();
  };
  // receiver style = true/false
  // Support both the global and receiver style size() for lists.
  status = PortableFunctionAdapter<int64_t, const CelList*>::CreateAndRegister(
      builtin::kSize, true, list_size_func, registry);
  if (!status.ok()) return status;
  status = PortableFunctionAdapter<int64_t, const CelList*>::CreateAndRegister(
      builtin::kSize, false, list_size_func, registry);
  if (!status.ok()) return status;

  // Map size
  auto map_size_func = [](Arena*, const CelMap* cel_map) -> int64_t {
    return (*cel_map).size();
  };
  // receiver style = true/false
  status = PortableFunctionAdapter<int64_t, const CelMap*>::CreateAndRegister(
      builtin::kSize, true, map_size_func, registry);
  if (!status.ok()) return status;
  status = PortableFunctionAdapter<int64_t, const CelMap*>::CreateAndRegister(
      builtin::kSize, false, map_size_func, registry);
  if (!status.ok()) return status;

  // Register set membership tests with the 'in' operator and its variants.
  status = RegisterSetMembershipFunctions(registry, options);
  if (!status.ok()) return status;

  // basic Arithmetic functions for numeric types
  status = RegisterArithmeticFunctionsForType<int64_t>(registry);
  if (!status.ok()) return status;

  status = RegisterArithmeticFunctionsForType<uint64_t>(registry);
  if (!status.ok()) return status;

  status = RegisterArithmeticFunctionsForType<double>(registry);
  if (!status.ok()) return status;

  bool enable_timestamp_duration_overflow_errors =
      options.enable_timestamp_duration_overflow_errors;
  // Special arithmetic operators for Timestamp and Duration
  status = PortableFunctionAdapter<CelValue, absl::Time, absl::Duration>::
      CreateAndRegister(
          builtin::kAdd, false,
          [=](Arena* arena, absl::Time t1, absl::Duration d2) -> CelValue {
            if (enable_timestamp_duration_overflow_errors) {
              auto sum = cel::internal::CheckedAdd(t1, d2);
              if (!sum.ok()) {
                return CreateErrorValue(arena, sum.status());
              }
              return CelValue::CreateTimestamp(*sum);
            }
            return CelValue::CreateTimestamp(t1 + d2);
          },
          registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<CelValue, absl::Duration, absl::Time>::
      CreateAndRegister(
          builtin::kAdd, false,
          [=](Arena* arena, absl::Duration d2, absl::Time t1) -> CelValue {
            if (enable_timestamp_duration_overflow_errors) {
              auto sum = cel::internal::CheckedAdd(t1, d2);
              if (!sum.ok()) {
                return CreateErrorValue(arena, sum.status());
              }
              return CelValue::CreateTimestamp(*sum);
            }
            return CelValue::CreateTimestamp(t1 + d2);
          },
          registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<CelValue, absl::Duration, absl::Duration>::
      CreateAndRegister(
          builtin::kAdd, false,
          [=](Arena* arena, absl::Duration d1, absl::Duration d2) -> CelValue {
            if (enable_timestamp_duration_overflow_errors) {
              auto sum = cel::internal::CheckedAdd(d1, d2);
              if (!sum.ok()) {
                return CreateErrorValue(arena, sum.status());
              }
              return CelValue::CreateDuration(*sum);
            }
            return CelValue::CreateDuration(d1 + d2);
          },
          registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<CelValue, absl::Time, absl::Duration>::
      CreateAndRegister(
          builtin::kSubtract, false,
          [=](Arena* arena, absl::Time t1, absl::Duration d2) -> CelValue {
            if (enable_timestamp_duration_overflow_errors) {
              auto diff = cel::internal::CheckedSub(t1, d2);
              if (!diff.ok()) {
                return CreateErrorValue(arena, diff.status());
              }
              return CelValue::CreateTimestamp(*diff);
            }
            return CelValue::CreateTimestamp(t1 - d2);
          },
          registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<CelValue, absl::Time, absl::Time>::
      CreateAndRegister(
          builtin::kSubtract, false,
          [=](Arena* arena, absl::Time t1, absl::Time t2) -> CelValue {
            if (enable_timestamp_duration_overflow_errors) {
              auto diff = cel::internal::CheckedSub(t1, t2);
              if (!diff.ok()) {
                return CreateErrorValue(arena, diff.status());
              }
              return CelValue::CreateDuration(*diff);
            }
            return CelValue::CreateDuration(t1 - t2);
          },
          registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<CelValue, absl::Duration, absl::Duration>::
      CreateAndRegister(
          builtin::kSubtract, false,
          [=](Arena* arena, absl::Duration d1, absl::Duration d2) -> CelValue {
            if (enable_timestamp_duration_overflow_errors) {
              auto diff = cel::internal::CheckedSub(d1, d2);
              if (!diff.ok()) {
                return CreateErrorValue(arena, diff.status());
              }
              return CelValue::CreateDuration(*diff);
            }
            return CelValue::CreateDuration(d1 - d2);
          },
          registry);
  if (!status.ok()) return status;

  // Concat group
  if (options.enable_string_concat) {
    status = PortableFunctionAdapter<
        CelValue::StringHolder, CelValue::StringHolder,
        CelValue::StringHolder>::CreateAndRegister(builtin::kAdd, false,
                                                   ConcatString, registry);
    if (!status.ok()) return status;

    status = PortableFunctionAdapter<
        CelValue::BytesHolder, CelValue::BytesHolder,
        CelValue::BytesHolder>::CreateAndRegister(builtin::kAdd, false,
                                                  ConcatBytes, registry);
    if (!status.ok()) return status;
  }

  if (options.enable_list_concat) {
    status = PortableFunctionAdapter<
        const CelList*, const CelList*,
        const CelList*>::CreateAndRegister(builtin::kAdd, false, ConcatList,
                                           registry);
    if (!status.ok()) return status;
  }

  // Global matches function.
  if (options.enable_regex) {
    auto regex_matches = [max_size = options.regex_max_program_size](
                             Arena* arena, CelValue::StringHolder target,
                             CelValue::StringHolder regex) -> CelValue {
      RE2 re2(re2::StringPiece(regex.value().data(), regex.value().size()));
      if (max_size > 0 && re2.ProgramSize() > max_size) {
        return CreateErrorValue(arena, "exceeded RE2 max program size",
                                absl::StatusCode::kInvalidArgument);
      }
      if (!re2.ok()) {
        return CreateErrorValue(arena, "invalid_argument",
                                absl::StatusCode::kInvalidArgument);
      }
      return CelValue::CreateBool(RE2::PartialMatch(re2::StringPiece(target.value().data(), target.value().size()), re2));
    };

    status = PortableFunctionAdapter<
        CelValue, CelValue::StringHolder,
        CelValue::StringHolder>::CreateAndRegister(builtin::kRegexMatch, false,
                                                   regex_matches, registry);
    if (!status.ok()) return status;

    // Receiver-style matches function.
    status = PortableFunctionAdapter<
        CelValue, CelValue::StringHolder,
        CelValue::StringHolder>::CreateAndRegister(builtin::kRegexMatch, true,
                                                   regex_matches, registry);
    if (!status.ok()) return status;
  }

  status =
      PortableFunctionAdapter<const CelList*, const CelList*, const CelList*>::
          CreateAndRegister(builtin::kRuntimeListAppend, false, AppendList,
                            registry);
  if (!status.ok()) return status;

  status = RegisterStringFunctions(registry, options);
  if (!status.ok()) return status;

  // Modulo
  status =
      PortableFunctionAdapter<CelValue, int64_t, int64_t>::CreateAndRegister(
          builtin::kModulo, false, Modulo<int64_t>, registry);
  if (!status.ok()) return status;

  status =
      PortableFunctionAdapter<CelValue, uint64_t, uint64_t>::CreateAndRegister(
          builtin::kModulo, false, Modulo<uint64_t>, registry);
  if (!status.ok()) return status;

  status = RegisterTimestampFunctions(registry, options);
  if (!status.ok()) return status;

  // duration functions
  status = PortableFunctionAdapter<CelValue, absl::Duration>::CreateAndRegister(
      builtin::kHours, true,
      [](Arena* arena, absl::Duration d) -> CelValue {
        return GetHours(arena, d);
      },
      registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<CelValue, absl::Duration>::CreateAndRegister(
      builtin::kMinutes, true,
      [](Arena* arena, absl::Duration d) -> CelValue {
        return GetMinutes(arena, d);
      },
      registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<CelValue, absl::Duration>::CreateAndRegister(
      builtin::kSeconds, true,
      [](Arena* arena, absl::Duration d) -> CelValue {
        return GetSeconds(arena, d);
      },
      registry);
  if (!status.ok()) return status;

  status = PortableFunctionAdapter<CelValue, absl::Duration>::CreateAndRegister(
      builtin::kMilliseconds, true,
      [](Arena* arena, absl::Duration d) -> CelValue {
        return GetMilliseconds(arena, d);
      },
      registry);
  if (!status.ok()) return status;

  return PortableFunctionAdapter<CelValue::CelTypeHolder, CelValue>::
      CreateAndRegister(
          builtin::kType, false,
          [](Arena*, CelValue value) -> CelValue::CelTypeHolder {
            return value.ObtainCelType().CelTypeOrDie();
          },
          registry);
}

}  // namespace google::api::expr::runtime
