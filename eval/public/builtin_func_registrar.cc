#include "eval/public/builtin_func_registrar.h"

#include <cmath>
#include <functional>
#include <limits>
#include <vector>

#include "absl/numeric/int128.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_function_adapter.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_options.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "internal/proto_util.h"
#include "re2/re2.h"
#include "base/unilib.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

using google::protobuf::Arena;

namespace {

const int64_t kIntMax = std::numeric_limits<int64_t>::max();
const int64_t kIntMin = std::numeric_limits<int64_t>::lowest();
const uint64_t kUintMax = std::numeric_limits<uint64_t>::max();

// Returns the number of UTF8 codepoints within a string.
// The input string must first be checked to see if it is valid UTF8.
static int UTF8CodepointCount(absl::string_view str) {
  int n = 0;
  // Increment the codepoint count on non-trail-byte characters.
  for (const auto p : str) {
    n += (*reinterpret_cast<const signed char*>(&p) >= -0x40);
  }
  return n;
}

// Comparison template functions
template <class Type>
CelValue Inequal(Arena*, Type t1, Type t2) {
  return CelValue::CreateBool(t1 != t2);
}

template <class Type>
CelValue Equal(Arena*, Type t1, Type t2) {
  return CelValue::CreateBool(t1 == t2);
}

// Forward declaration of the generic equality operator
template <>
CelValue Equal(Arena*, CelValue t1, CelValue t2);

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
CelValue Inequal(Arena*, absl::Duration t1, absl::Duration t2) {
  return CelValue::CreateBool(absl::operator!=(t1, t2));
}

template <>
CelValue Equal(Arena*, absl::Duration t1, absl::Duration t2) {
  return CelValue::CreateBool(absl::operator==(t1, t2));
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
CelValue Inequal(Arena*, absl::Time t1, absl::Time t2) {
  return CelValue::CreateBool(absl::operator!=(t1, t2));
}

template <>
CelValue Equal(Arena*, absl::Time t1, absl::Time t2) {
  return CelValue::CreateBool(absl::operator==(t1, t2));
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

// Message specializations
template <>
CelValue Inequal(Arena* arena, const google::protobuf::Message* t1,
                 const google::protobuf::Message* t2) {
  if (t1 == nullptr) {
    return CelValue::CreateBool(t2 != nullptr);
  }
  if (t2 == nullptr) {
    return CelValue::CreateBool(true);
  }
  return CreateNoMatchingOverloadError(arena, builtin::kInequal);
}

template <>
CelValue Equal(Arena* arena, const google::protobuf::Message* t1,
               const google::protobuf::Message* t2) {
  if (t1 == nullptr) {
    return CelValue::CreateBool(t2 == nullptr);
  }
  if (t2 == nullptr) {
    return CelValue::CreateBool(false);
  }
  return CreateNoMatchingOverloadError(arena, builtin::kEqual);
}

// Equality specialization for lists
template <>
CelValue Equal(Arena* arena, const CelList* t1, const CelList* t2) {
  int index_size = t1->size();
  if (t2->size() != index_size) {
    return CelValue::CreateBool(false);
  }

  for (int i = 0; i < index_size; i++) {
    CelValue e1 = (*t1)[i];
    CelValue e2 = (*t2)[i];
    const CelValue eq = Equal<CelValue>(arena, e1, e2);
    if (eq.IsBool()) {
      if (!eq.BoolOrDie()) {
        return CelValue::CreateBool(false);
      }
    } else {
      // propagate errors
      return eq;
    }
  }

  return CelValue::CreateBool(true);
}

template <>
CelValue Inequal(Arena* arena, const CelList* t1, const CelList* t2) {
  const CelValue eq = Equal<const CelList*>(arena, t1, t2);
  if (eq.IsBool()) {
    return CelValue::CreateBool(!eq.BoolOrDie());
  }
  return eq;
}

// Equality specialization for maps
template <>
CelValue Equal(Arena* arena, const CelMap* t1, const CelMap* t2) {
  if (t1->size() != t2->size()) {
    return CelValue::CreateBool(false);
  }

  const CelList* keys = t1->ListKeys();
  for (int i = 0; i < keys->size(); i++) {
    CelValue key = (*keys)[i];
    CelValue v1 = (*t1)[key].value();
    absl::optional<CelValue> v2 = (*t2)[key];
    if (!v2.has_value()) {
      return CelValue::CreateBool(false);
    }
    const CelValue eq = Equal<CelValue>(arena, v1, v2.value());
    if (eq.IsBool()) {
      if (!eq.BoolOrDie()) {
        return CelValue::CreateBool(false);
      }
    } else {
      // propagate errors
      return eq;
    }
  }

  return CelValue::CreateBool(true);
}

template <>
CelValue Inequal(Arena* arena, const CelMap* t1, const CelMap* t2) {
  const CelValue eq = Equal<const CelMap*>(arena, t1, t2);
  if (eq.IsBool()) {
    return CelValue::CreateBool(!eq.BoolOrDie());
  }
  return eq;
}

// Generic equality for CEL values
template <>
CelValue Equal(Arena* arena, CelValue t1, CelValue t2) {
  if (t1.type() != t2.type()) {
    // This is used to implement inequal for some types so we can't determine
    // the function.
    return CreateNoMatchingOverloadError(arena);
  }
  switch (t1.type()) {
    case CelValue::Type::kBool:
      return Equal<bool>(arena, t1.BoolOrDie(), t2.BoolOrDie());
    case CelValue::Type::kInt64:
      return Equal<int64_t>(arena, t1.Int64OrDie(), t2.Int64OrDie());
    case CelValue::Type::kUint64:
      return Equal<uint64_t>(arena, t1.Uint64OrDie(), t2.Uint64OrDie());
    case CelValue::Type::kDouble:
      return Equal<double>(arena, t1.DoubleOrDie(), t2.DoubleOrDie());
    case CelValue::Type::kString:
      return Equal<CelValue::StringHolder>(arena, t1.StringOrDie(),
                                           t2.StringOrDie());
    case CelValue::Type::kBytes:
      return Equal<CelValue::BytesHolder>(arena, t1.BytesOrDie(),
                                          t2.BytesOrDie());
    case CelValue::Type::kMessage:
      return Equal<const google::protobuf::Message*>(arena, t1.MessageOrDie(),
                                           t2.MessageOrDie());
    case CelValue::Type::kDuration:
      return Equal<absl::Duration>(arena, t1.DurationOrDie(),
                                   t2.DurationOrDie());
    case CelValue::Type::kTimestamp:
      return Equal<absl::Time>(arena, t1.TimestampOrDie(), t2.TimestampOrDie());
    case CelValue::Type::kList:
      return Equal<const CelList*>(arena, t1.ListOrDie(), t2.ListOrDie());
    case CelValue::Type::kMap:
      return Equal<const CelMap*>(arena, t1.MapOrDie(), t2.MapOrDie());
    case CelValue::Type::kCelType:
      return Equal<CelValue::CelTypeHolder>(arena, t1.CelTypeOrDie(),
                                            t2.CelTypeOrDie());
    default:
      break;
  }
  return CreateNoMatchingOverloadError(arena);
}

// Helper method
//
// Registers all equality functions for template parameters type.
template <class Type>
absl::Status RegisterEqualityFunctionsForType(CelFunctionRegistry* registry) {
  // Inequality
  absl::Status status =
      FunctionAdapter<CelValue, Type, Type>::CreateAndRegister(
          builtin::kInequal, false, Inequal<Type>, registry);
  if (!status.ok()) return status;

  // Equality
  status = FunctionAdapter<CelValue, Type, Type>::CreateAndRegister(
      builtin::kEqual, false, Equal<Type>, registry);
  return status;
}

// Registers all comparison functions for template parameter type.
template <class Type>
absl::Status RegisterComparisonFunctionsForType(CelFunctionRegistry* registry) {
  absl::Status status = RegisterEqualityFunctionsForType<Type>(registry);
  if (!status.ok()) return status;

  // Less than
  status = FunctionAdapter<bool, Type, Type>::CreateAndRegister(
      builtin::kLess, false, LessThan<Type>, registry);
  if (!status.ok()) return status;

  // Less than or Equal
  status = FunctionAdapter<bool, Type, Type>::CreateAndRegister(
      builtin::kLessOrEqual, false, LessThanOrEqual<Type>, registry);
  if (!status.ok()) return status;

  // Greater than
  status = FunctionAdapter<bool, Type, Type>::CreateAndRegister(
      builtin::kGreater, false, GreaterThan<Type>, registry);
  if (!status.ok()) return status;

  // Greater than or Equal
  return FunctionAdapter<bool, Type, Type>::CreateAndRegister(
      builtin::kGreaterOrEqual, false, GreaterThanOrEqual<Type>, registry);
}

// Template functions providing arithmetic operations
template <class Type>
CelValue Add(Arena*, Type v0, Type v1);

template <>
CelValue Add<int64_t>(Arena* arena, int64_t v0, int64_t v1) {
  absl::int128 bv = v0;
  bv += v1;
  if (bv < kIntMin || bv > kIntMax) {
    return CreateErrorValue(arena, "integer overflow",
                            absl::StatusCode::kOutOfRange);
  }
  return CelValue::CreateInt64(absl::Int128Low64(bv));
}

template <>
CelValue Add<uint64_t>(Arena* arena, uint64_t v0, uint64_t v1) {
  absl::uint128 bv = v0;
  bv += v1;
  if (bv > kUintMax) {
    return CreateErrorValue(arena, "unsigned integer overflow",
                            absl::StatusCode::kOutOfRange);
  }
  return CelValue::CreateUint64(absl::Uint128Low64(bv));
}

template <>
CelValue Add<double>(Arena*, double v0, double v1) {
  return CelValue::CreateDouble(v0 + v1);
}

template <class Type>
CelValue Sub(Arena*, Type v0, Type v1);

template <>
CelValue Sub<int64_t>(Arena* arena, int64_t v0, int64_t v1) {
  absl::int128 bv = v0;
  bv -= v1;
  if (bv < kIntMin || bv > kIntMax) {
    return CreateErrorValue(arena, "integer overflow",
                            absl::StatusCode::kOutOfRange);
  }
  return CelValue::CreateInt64(absl::Int128Low64(bv));
}

template <>
CelValue Sub<uint64_t>(Arena* arena, uint64_t v0, uint64_t v1) {
  if (v1 > v0) {
    return CreateErrorValue(arena, "unsigned integer overflow",
                            absl::StatusCode::kOutOfRange);
  }
  return CelValue::CreateUint64(v0 - v1);
}

template <>
CelValue Sub<double>(Arena*, double v0, double v1) {
  return CelValue::CreateDouble(v0 - v1);
}

template <class Type>
CelValue Mul(Arena*, Type v0, Type v1);

template <>
CelValue Mul<int64_t>(Arena* arena, int64_t v0, int64_t v1) {
  absl::int128 bv = v0;
  bv *= v1;
  if (bv < kIntMin || bv > kIntMax) {
    return CreateErrorValue(arena, "integer overflow",
                            absl::StatusCode::kOutOfRange);
  }
  return CelValue::CreateInt64(absl::Int128Low64(bv));
}

template <>
CelValue Mul<uint64_t>(Arena* arena, uint64_t v0, uint64_t v1) {
  absl::uint128 bv = v0;
  bv *= v1;
  if (bv > kUintMax) {
    return CreateErrorValue(arena, "unsigned integer overflow",
                            absl::StatusCode::kOutOfRange);
  }
  return CelValue::CreateUint64(absl::Uint128Low64(bv));
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
  // For integral types, zero check is essential, to avoid
  // floating pointer exception.
  if (v1 == 0) {
    // TODO(issues/25) Which code?
    return CreateErrorValue(arena, "Division by 0");
  }
  // Overflow case for two's complement: -INT_MIN/-1
  if (v0 == kIntMin && v1 == -1) {
    return CreateErrorValue(arena, "integer overflow",
                            absl::StatusCode::kOutOfRange);
  }
  return CelValue::CreateInt64(v0 / v1);
}

// Division operations for integer types should check for
// division by 0
template <>
CelValue Div<uint64_t>(Arena* arena, uint64_t v0, uint64_t v1) {
  // For integral types, zero check is essential, to avoid
  // floating pointer exception.
  if (v1 == 0) {
    // TODO(issues/25) Which code?
    return CreateErrorValue(arena, "Division by 0");
  }
  return CelValue::CreateUint64(v0 / v1);
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
CelValue Modulo(Arena* arena, Type value, Type value2);

// Modulo operations for integer types should check for
// division by 0
template <>
CelValue Modulo<int64_t>(Arena* arena, int64_t value, int64_t value2) {
  if (value2 == 0) {
    return CreateErrorValue(arena, "Modulo by 0");
  }
  // Handle the two's complement case.
  if (value == kIntMin && value2 == -1) {
    return CreateErrorValue(arena, "integer overflow",
                            absl::StatusCode::kOutOfRange);
  }
  return CelValue::CreateInt64(value % value2);
}

template <>
CelValue Modulo<uint64_t>(Arena* arena, uint64_t value, uint64_t value2) {
  if (value2 == 0) {
    return CreateErrorValue(arena, "Modulo by 0");
  }

  return CelValue::CreateUint64(value % value2);
}

// Helper method
// Registers all arithmetic functions for template parameter type.
template <class Type>
absl::Status RegisterArithmeticFunctionsForType(CelFunctionRegistry* registry) {
  absl::Status status =
      FunctionAdapter<CelValue, Type, Type>::CreateAndRegister(
          builtin::kAdd, false, Add<Type>, registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, Type, Type>::CreateAndRegister(
      builtin::kSubtract, false, Sub<Type>, registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, Type, Type>::CreateAndRegister(
      builtin::kMultiply, false, Mul<Type>, registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, Type, Type>::CreateAndRegister(
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

CelValue CreateTimestampFromString(Arena* arena,
                                   CelValue::StringHolder time_str) {
  absl::Time ts;
  if (!absl::ParseTime(absl::RFC3339_full, time_str.value(), &ts, nullptr)) {
    return CreateErrorValue(arena, "String to Timestamp conversion failed",
                            absl::StatusCode::kInvalidArgument);
  }
  return CelValue::CreateTimestamp(ts);
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

absl::Status RegisterComparisonFunctions(CelFunctionRegistry* registry,
                                         const InterpreterOptions& options) {
  auto status = RegisterComparisonFunctionsForType<bool>(registry);
  if (!status.ok()) return status;

  status = RegisterComparisonFunctionsForType<int64_t>(registry);
  if (!status.ok()) return status;

  status = RegisterComparisonFunctionsForType<uint64_t>(registry);
  if (!status.ok()) return status;

  status = RegisterComparisonFunctionsForType<double>(registry);
  if (!status.ok()) return status;

  status = RegisterComparisonFunctionsForType<CelValue::StringHolder>(registry);
  if (!status.ok()) return status;

  status = RegisterComparisonFunctionsForType<CelValue::BytesHolder>(registry);
  if (!status.ok()) return status;

  status = RegisterComparisonFunctionsForType<absl::Duration>(registry);
  if (!status.ok()) return status;

  status = RegisterComparisonFunctionsForType<absl::Time>(registry);
  if (!status.ok()) return status;

  status = RegisterEqualityFunctionsForType<const google::protobuf::Message*>(registry);
  if (!status.ok()) return status;

  status = RegisterEqualityFunctionsForType<const CelList*>(registry);
  if (!status.ok()) return status;

  status = RegisterEqualityFunctionsForType<const CelMap*>(registry);
  if (!status.ok()) return status;

  return RegisterEqualityFunctionsForType<CelValue::CelTypeHolder>(registry);
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
      auto status =
          FunctionAdapter<bool, bool, const CelList*>::CreateAndRegister(
              op, false, In<bool>, registry);
      if (!status.ok()) return status;
      status = FunctionAdapter<bool, int64_t, const CelList*>::CreateAndRegister(
          op, false, In<int64_t>, registry);
      if (!status.ok()) return status;
      status = FunctionAdapter<bool, uint64_t, const CelList*>::CreateAndRegister(
          op, false, In<uint64_t>, registry);
      if (!status.ok()) return status;
      status = FunctionAdapter<bool, double, const CelList*>::CreateAndRegister(
          op, false, In<double>, registry);
      if (!status.ok()) return status;
      status = FunctionAdapter<bool, CelValue::StringHolder, const CelList*>::
          CreateAndRegister(op, false, In<CelValue::StringHolder>, registry);
      if (!status.ok()) return status;
      status = FunctionAdapter<bool, CelValue::BytesHolder, const CelList*>::
          CreateAndRegister(op, false, In<CelValue::BytesHolder>, registry);
      if (!status.ok()) return status;
    }
  }

  auto boolKeyInSet = [](Arena* arena, bool key,
                         const CelMap* cel_map) -> CelValue {
    const auto& result = cel_map->Has(CelValue::CreateBool(key));
    if (!result.ok()) {
      return CreateErrorValue(arena, result.status());
    }
    return CelValue::CreateBool(*result);
  };
  auto intKeyInSet = [](Arena* arena, int64_t key,
                        const CelMap* cel_map) -> CelValue {
    const auto& result = cel_map->Has(CelValue::CreateInt64(key));
    if (!result.ok()) {
      return CreateErrorValue(arena, result.status());
    }
    return CelValue::CreateBool(*result);
  };
  auto stringKeyInSet = [](Arena* arena, CelValue::StringHolder key,
                           const CelMap* cel_map) -> CelValue {
    const auto& result = cel_map->Has(CelValue::CreateString(key));
    if (!result.ok()) {
      return CreateErrorValue(arena, result.status());
    }
    return CelValue::CreateBool(*result);
  };
  auto uintKeyInSet = [](Arena* arena, uint64_t key,
                         const CelMap* cel_map) -> CelValue {
    const auto& result = cel_map->Has(CelValue::CreateUint64(key));
    if (!result.ok()) {
      return CreateErrorValue(arena, result.status());
    }
    return CelValue::CreateBool(*result);
  };

  for (auto op : in_operators) {
    auto status =
        FunctionAdapter<CelValue, CelValue::StringHolder,
                        const CelMap*>::CreateAndRegister(op, false,
                                                          stringKeyInSet,
                                                          registry);
    if (!status.ok()) return status;

    status = FunctionAdapter<CelValue, bool, const CelMap*>::CreateAndRegister(
        op, false, boolKeyInSet, registry);
    if (!status.ok()) return status;

    status = FunctionAdapter<CelValue, int64_t, const CelMap*>::CreateAndRegister(
        op, false, intKeyInSet, registry);
    if (!status.ok()) return status;

    status =
        FunctionAdapter<CelValue, uint64_t, const CelMap*>::CreateAndRegister(
            op, false, uintKeyInSet, registry);
    if (!status.ok()) return status;
  }
  return absl::OkStatus();
}

absl::Status RegisterStringFunctions(CelFunctionRegistry* registry,
                                     const InterpreterOptions& options) {
  auto status =
      FunctionAdapter<bool, CelValue::StringHolder, CelValue::StringHolder>::
          CreateAndRegister(builtin::kStringContains, false, StringContains,
                            registry);
  if (!status.ok()) return status;

  status =
      FunctionAdapter<bool, CelValue::StringHolder, CelValue::StringHolder>::
          CreateAndRegister(builtin::kStringContains, true, StringContains,
                            registry);
  if (!status.ok()) return status;

  status =
      FunctionAdapter<bool, CelValue::StringHolder, CelValue::StringHolder>::
          CreateAndRegister(builtin::kStringEndsWith, false, StringEndsWith,
                            registry);
  if (!status.ok()) return status;

  status =
      FunctionAdapter<bool, CelValue::StringHolder, CelValue::StringHolder>::
          CreateAndRegister(builtin::kStringEndsWith, true, StringEndsWith,
                            registry);
  if (!status.ok()) return status;

  status =
      FunctionAdapter<bool, CelValue::StringHolder, CelValue::StringHolder>::
          CreateAndRegister(builtin::kStringStartsWith, false, StringStartsWith,
                            registry);
  if (!status.ok()) return status;

  return FunctionAdapter<bool, CelValue::StringHolder, CelValue::StringHolder>::
      CreateAndRegister(builtin::kStringStartsWith, true, StringStartsWith,
                        registry);
}

absl::Status RegisterTimestampFunctions(CelFunctionRegistry* registry,
                                        const InterpreterOptions& options) {
  auto status = FunctionAdapter<CelValue, absl::Time, CelValue::StringHolder>::
      CreateAndRegister(
          builtin::kFullYear, true,
          [](Arena* arena, absl::Time ts, CelValue::StringHolder tz)
              -> CelValue { return GetFullYear(arena, ts, tz.value()); },
          registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Time>::CreateAndRegister(
      builtin::kFullYear, true,
      [](Arena* arena, absl::Time ts) -> CelValue {
        return GetFullYear(arena, ts, "");
      },
      registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Time, CelValue::StringHolder>::
      CreateAndRegister(
          builtin::kMonth, true,
          [](Arena* arena, absl::Time ts, CelValue::StringHolder tz)
              -> CelValue { return GetMonth(arena, ts, tz.value()); },
          registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Time>::CreateAndRegister(
      builtin::kMonth, true,
      [](Arena* arena, absl::Time ts) -> CelValue {
        return GetMonth(arena, ts, "");
      },
      registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Time, CelValue::StringHolder>::
      CreateAndRegister(
          builtin::kDayOfYear, true,
          [](Arena* arena, absl::Time ts, CelValue::StringHolder tz)
              -> CelValue { return GetDayOfYear(arena, ts, tz.value()); },
          registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Time>::CreateAndRegister(
      builtin::kDayOfYear, true,
      [](Arena* arena, absl::Time ts) -> CelValue {
        return GetDayOfYear(arena, ts, "");
      },
      registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Time, CelValue::StringHolder>::
      CreateAndRegister(
          builtin::kDayOfMonth, true,
          [](Arena* arena, absl::Time ts, CelValue::StringHolder tz)
              -> CelValue { return GetDayOfMonth(arena, ts, tz.value()); },
          registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Time>::CreateAndRegister(
      builtin::kDayOfMonth, true,
      [](Arena* arena, absl::Time ts) -> CelValue {
        return GetDayOfMonth(arena, ts, "");
      },
      registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Time, CelValue::StringHolder>::
      CreateAndRegister(
          builtin::kDate, true,
          [](Arena* arena, absl::Time ts, CelValue::StringHolder tz)
              -> CelValue { return GetDate(arena, ts, tz.value()); },
          registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Time>::CreateAndRegister(
      builtin::kDate, true,
      [](Arena* arena, absl::Time ts) -> CelValue {
        return GetDate(arena, ts, "");
      },
      registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Time, CelValue::StringHolder>::
      CreateAndRegister(
          builtin::kDayOfWeek, true,
          [](Arena* arena, absl::Time ts, CelValue::StringHolder tz)
              -> CelValue { return GetDayOfWeek(arena, ts, tz.value()); },
          registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Time>::CreateAndRegister(
      builtin::kDayOfWeek, true,
      [](Arena* arena, absl::Time ts) -> CelValue {
        return GetDayOfWeek(arena, ts, "");
      },
      registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Time, CelValue::StringHolder>::
      CreateAndRegister(
          builtin::kHours, true,
          [](Arena* arena, absl::Time ts, CelValue::StringHolder tz)
              -> CelValue { return GetHours(arena, ts, tz.value()); },
          registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Time>::CreateAndRegister(
      builtin::kHours, true,
      [](Arena* arena, absl::Time ts) -> CelValue {
        return GetHours(arena, ts, "");
      },
      registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Time, CelValue::StringHolder>::
      CreateAndRegister(
          builtin::kMinutes, true,
          [](Arena* arena, absl::Time ts, CelValue::StringHolder tz)
              -> CelValue { return GetMinutes(arena, ts, tz.value()); },
          registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Time>::CreateAndRegister(
      builtin::kMinutes, true,
      [](Arena* arena, absl::Time ts) -> CelValue {
        return GetMinutes(arena, ts, "");
      },
      registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Time, CelValue::StringHolder>::
      CreateAndRegister(
          builtin::kSeconds, true,
          [](Arena* arena, absl::Time ts, CelValue::StringHolder tz)
              -> CelValue { return GetSeconds(arena, ts, tz.value()); },
          registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Time>::CreateAndRegister(
      builtin::kSeconds, true,
      [](Arena* arena, absl::Time ts) -> CelValue {
        return GetSeconds(arena, ts, "");
      },
      registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Time, CelValue::StringHolder>::
      CreateAndRegister(
          builtin::kMilliseconds, true,
          [](Arena* arena, absl::Time ts, CelValue::StringHolder tz)
              -> CelValue { return GetMilliseconds(arena, ts, tz.value()); },
          registry);
  if (!status.ok()) return status;

  return FunctionAdapter<CelValue, absl::Time>::CreateAndRegister(
      builtin::kMilliseconds, true,
      [](Arena* arena, absl::Time ts) -> CelValue {
        return GetMilliseconds(arena, ts, "");
      },
      registry);
}

absl::Status RegisterBytesConversionFunctions(CelFunctionRegistry* registry,
                                              const InterpreterOptions&) {
  // bytes -> bytes
  auto status = FunctionAdapter<CelValue::BytesHolder, CelValue::BytesHolder>::
      CreateAndRegister(
          builtin::kBytes, false,
          [](Arena*, CelValue::BytesHolder value) -> CelValue::BytesHolder {
            return value;
          },
          registry);
  if (!status.ok()) return status;

  // string -> bytes
  return FunctionAdapter<CelValue, CelValue::StringHolder>::CreateAndRegister(
      builtin::kBytes, false,
      [](Arena* arena, CelValue::StringHolder value) -> CelValue {
        return CelValue::CreateBytesView(value.value());
      },
      registry);
}

absl::Status RegisterDoubleConversionFunctions(CelFunctionRegistry* registry,
                                               const InterpreterOptions&) {
  // double -> double
  auto status = FunctionAdapter<double, double>::CreateAndRegister(
      builtin::kDouble, false, [](Arena*, double v) { return v; }, registry);
  if (!status.ok()) return status;

  // int -> double
  status = FunctionAdapter<double, int64_t>::CreateAndRegister(
      builtin::kDouble, false,
      [](Arena*, int64_t v) { return static_cast<double>(v); }, registry);
  if (!status.ok()) return status;

  // string -> double
  status = FunctionAdapter<CelValue, CelValue::StringHolder>::CreateAndRegister(
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
  return FunctionAdapter<double, uint64_t>::CreateAndRegister(
      builtin::kDouble, false,
      [](Arena*, uint64_t v) { return static_cast<double>(v); }, registry);
}

absl::Status RegisterIntConversionFunctions(CelFunctionRegistry* registry,
                                            const InterpreterOptions&) {
  // bool -> int
  auto status = FunctionAdapter<int64_t, bool>::CreateAndRegister(
      builtin::kInt, false,
      [](Arena*, bool v) { return static_cast<int64_t>(v); }, registry);
  if (!status.ok()) return status;

  // double -> int
  status = FunctionAdapter<CelValue, double>::CreateAndRegister(
      builtin::kInt, false,
      [](Arena* arena, double v) {
        // NaN and -+infinite numbers cannot be represented as int values,
        // nor can double values which exceed the integer 64-bit range.
        if (!std::isfinite(v) || v > static_cast<double>(kIntMax) ||
            v < static_cast<double>(kIntMin)) {
          return CreateErrorValue(arena, "double out of int range",
                                  absl::StatusCode::kInvalidArgument);
        }
        return CelValue::CreateInt64(static_cast<int64_t>(v));
      },
      registry);
  if (!status.ok()) return status;

  // int -> int
  status = FunctionAdapter<int64_t, int64_t>::CreateAndRegister(
      builtin::kInt, false, [](Arena*, int64_t v) { return v; }, registry);
  if (!status.ok()) return status;

  // string -> int
  status = FunctionAdapter<CelValue, CelValue::StringHolder>::CreateAndRegister(
      builtin::kInt, false,
      [](Arena* arena, CelValue::StringHolder s) {
        int64_t result;
        if (absl::SimpleAtoi(s.value(), &result)) {
          return CelValue::CreateInt64(result);
        } else {
          return CreateErrorValue(arena, "cannot convert string to int",
                                  absl::StatusCode::kInvalidArgument);
        }
      },
      registry);
  if (!status.ok()) return status;

  // time -> int
  status = FunctionAdapter<int64_t, absl::Time>::CreateAndRegister(
      builtin::kInt, false,
      [](Arena*, absl::Time t) { return absl::ToUnixSeconds(t); }, registry);
  if (!status.ok()) return status;

  // uint -> int
  return FunctionAdapter<CelValue, uint64_t>::CreateAndRegister(
      builtin::kInt, false,
      [](Arena* arena, uint64_t v) {
        if (v > static_cast<uint64_t>(kIntMax)) {
          return CreateErrorValue(arena, "uint out of int range",
                                  absl::StatusCode::kInvalidArgument);
        }
        return CelValue::CreateInt64(static_cast<int64_t>(v));
      },
      registry);
}

absl::Status RegisterStringConversionFunctions(
    CelFunctionRegistry* registry, const InterpreterOptions& options) {
  // May be optionally disabled to reduce potential allocs.
  if (!options.enable_string_conversion) {
    return absl::OkStatus();
  }

  auto status =
      FunctionAdapter<CelValue, CelValue::BytesHolder>::CreateAndRegister(
          builtin::kString, false,
          [](Arena* arena, CelValue::BytesHolder value) -> CelValue {
            if (UniLib::IsStructurallyValid(value.value())) {
              return CelValue::CreateStringView(value.value());
            }
            return CreateErrorValue(arena, "invalid UTF-8 bytes value",
                                    absl::StatusCode::kInvalidArgument);
          },
          registry);
  if (!status.ok()) return status;

  // double -> string
  status = FunctionAdapter<CelValue::StringHolder, double>::CreateAndRegister(
      builtin::kString, false,
      [](Arena* arena, double value) -> CelValue::StringHolder {
        return CelValue::StringHolder(
            Arena::Create<std::string>(arena, absl::StrCat(value)));
      },
      registry);
  if (!status.ok()) return status;

  // int -> string
  status = FunctionAdapter<CelValue::StringHolder, int64_t>::CreateAndRegister(
      builtin::kString, false,
      [](Arena* arena, int64_t value) -> CelValue::StringHolder {
        return CelValue::StringHolder(
            Arena::Create<std::string>(arena, absl::StrCat(value)));
      },
      registry);
  if (!status.ok()) return status;

  // string -> string
  status = FunctionAdapter<CelValue::StringHolder, CelValue::StringHolder>::
      CreateAndRegister(
          builtin::kString, false,
          [](Arena*, CelValue::StringHolder value) -> CelValue::StringHolder {
            return value;
          },
          registry);
  if (!status.ok()) return status;

  // uint -> string
  status = FunctionAdapter<CelValue::StringHolder, uint64_t>::CreateAndRegister(
      builtin::kString, false,
      [](Arena* arena, uint64_t value) -> CelValue::StringHolder {
        return CelValue::StringHolder(
            Arena::Create<std::string>(arena, absl::StrCat(value)));
      },
      registry);
  if (!status.ok()) return status;

  // duration -> string
  status = FunctionAdapter<CelValue, absl::Duration>::CreateAndRegister(
      builtin::kString, false,
      [](Arena* arena, absl::Duration value) -> CelValue {
        auto encode =
            google::api::expr::internal::EncodeDurationToString(value);
        if (!encode.ok()) {
          const auto& status = encode.status();
          return CreateErrorValue(arena, status.message(), status.code());
        }
        return CelValue::CreateString(CelValue::StringHolder(
            Arena::Create<std::string>(arena, encode.value())));
      },
      registry);
  if (!status.ok()) return status;

  // timestamp -> string
  return FunctionAdapter<CelValue, absl::Time>::CreateAndRegister(
      builtin::kString, false,
      [](Arena* arena, absl::Time value) -> CelValue {
        auto encode = google::api::expr::internal::EncodeTimeToString(value);
        if (!encode.ok()) {
          const auto& status = encode.status();
          return CreateErrorValue(arena, status.message(), status.code());
        }
        return CelValue::CreateString(CelValue::StringHolder(
            Arena::Create<std::string>(arena, encode.value())));
      },
      registry);
}

absl::Status RegisterUintConversionFunctions(CelFunctionRegistry* registry,
                                             const InterpreterOptions&) {
  // double -> uint
  auto status = FunctionAdapter<CelValue, double>::CreateAndRegister(
      builtin::kUint, false,
      [](Arena* arena, double v) {
        // NaN and -+infinite numbers cannot be represented as uint values,
        // nor doubles that exceed the uint64_t range. In some limited cases,
        // like 1.84467e+19, the value appears to fit within the uint64_t range
        // but type conversion results in rounding that overflows.
        //
        // Note, the double is checked to make sure it is not greater than 2^64
        // before it is converted to a uint128 value, as the type conversion
        // may check-fail for some double inputs that exceed this value.
        if (!std::isfinite(v) || v < 0 || v > std::ldexp(1.0, 64) ||
            absl::uint128(v) > kUintMax) {
          return CreateErrorValue(arena, "double out of uint range",
                                  absl::StatusCode::kInvalidArgument);
        }
        return CelValue::CreateUint64(static_cast<uint64_t>(v));
      },
      registry);
  if (!status.ok()) return status;

  // int -> uint
  status = FunctionAdapter<CelValue, int64_t>::CreateAndRegister(
      builtin::kUint, false,
      [](Arena* arena, int64_t v) {
        if (v < 0) {
          return CreateErrorValue(arena, "int out of uint range",
                                  absl::StatusCode::kInvalidArgument);
        }
        return CelValue::CreateUint64(static_cast<uint64_t>(v));
      },
      registry);
  if (!status.ok()) return status;

  // string -> uint
  status = FunctionAdapter<CelValue, CelValue::StringHolder>::CreateAndRegister(
      builtin::kUint, false,
      [](Arena* arena, CelValue::StringHolder s) {
        uint64_t result;
        if (absl::SimpleAtoi(s.value(), &result)) {
          return CelValue::CreateUint64(result);
        } else {
          return CreateErrorValue(arena, "doesn't convert to a string",
                                  absl::StatusCode::kInvalidArgument);
        }
      },
      registry);
  if (!status.ok()) return status;

  // uint -> uint
  return FunctionAdapter<uint64_t, uint64_t>::CreateAndRegister(
      builtin::kUint, false, [](Arena*, uint64_t v) { return v; }, registry);
}

absl::Status RegisterConversionFunctions(CelFunctionRegistry* registry,
                                         const InterpreterOptions& options) {
  auto status = RegisterBytesConversionFunctions(registry, options);
  if (!status.ok()) return status;

  status = RegisterDoubleConversionFunctions(registry, options);
  if (!status.ok()) return status;

  // duration() conversion from string.
  status = FunctionAdapter<CelValue, CelValue::StringHolder>::CreateAndRegister(
      builtin::kDuration, false, CreateDurationFromString, registry);
  if (!status.ok()) return status;

  // dyn() identity function.
  // TODO(issues/102): strip dyn() function references at type-check time.
  status = FunctionAdapter<CelValue, CelValue>::CreateAndRegister(
      builtin::kDyn, false,
      [](Arena*, CelValue value) -> CelValue { return value; }, registry);

  status = RegisterIntConversionFunctions(registry, options);
  if (!status.ok()) return status;

  status = RegisterStringConversionFunctions(registry, options);
  if (!status.ok()) return status;

  // timestamp() conversion from string.
  status = FunctionAdapter<CelValue, CelValue::StringHolder>::CreateAndRegister(
      builtin::kTimestamp, false, CreateTimestampFromString, registry);
  if (!status.ok()) return status;

  return RegisterUintConversionFunctions(registry, options);
}

}  // namespace

absl::Status RegisterBuiltinFunctions(CelFunctionRegistry* registry,
                                      const InterpreterOptions& options) {
  // logical NOT
  absl::Status status = FunctionAdapter<bool, bool>::CreateAndRegister(
      builtin::kNot, false, [](Arena*, bool value) -> bool { return !value; },
      registry);
  if (!status.ok()) return status;

  // Negation group
  status = FunctionAdapter<CelValue, int64_t>::CreateAndRegister(
      builtin::kNeg, false,
      [](Arena* arena, int64_t value) -> CelValue {
        // Handle overflow from two's complement.
        if (value == kIntMin) {
          return CreateErrorValue(arena, "integer overflow",
                                  absl::StatusCode::kOutOfRange);
        }
        return CelValue::CreateInt64(-value);
      },
      registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<double, double>::CreateAndRegister(
      builtin::kNeg, false,
      [](Arena*, double value) -> double { return -value; }, registry);
  if (!status.ok()) return status;

  status = RegisterComparisonFunctions(registry, options);
  if (!status.ok()) return status;

  status = RegisterConversionFunctions(registry, options);
  if (!status.ok()) return status;

  // Strictness
  status = FunctionAdapter<bool, bool>::CreateAndRegister(
      builtin::kNotStrictlyFalse, false,
      [](Arena*, bool value) -> bool { return value; }, registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<bool, const CelError*>::CreateAndRegister(
      builtin::kNotStrictlyFalse, false,
      [](Arena*, const CelError*) -> bool { return true; }, registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<bool, const UnknownSet*>::CreateAndRegister(
      builtin::kNotStrictlyFalse, false,
      [](Arena*, const UnknownSet*) -> bool { return true; }, registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<bool, bool>::CreateAndRegister(
      builtin::kNotStrictlyFalseDeprecated, false,
      [](Arena*, bool value) -> bool { return value; }, registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<bool, const CelError*>::CreateAndRegister(
      builtin::kNotStrictlyFalseDeprecated, false,
      [](Arena*, const CelError*) -> bool { return true; }, registry);
  if (!status.ok()) return status;

  // String size
  auto size_func = [=](Arena* arena, CelValue::StringHolder value) -> CelValue {
    absl::string_view str = value.value();
    if (options.enable_string_size_as_unicode_codepoints) {
      if (!UniLib::IsStructurallyValid(str)) {
        return CreateErrorValue(arena, "invalid utf-8 string",
                                absl::StatusCode::kInvalidArgument);
      }
      return CelValue::CreateInt64(UTF8CodepointCount(str));
    }
    return CelValue::CreateInt64(str.size());
  };
  // receiver style = true/false
  // Support global and receiver style size() operations on strings.
  status = FunctionAdapter<CelValue, CelValue::StringHolder>::CreateAndRegister(
      builtin::kSize, true, size_func, registry);
  if (!status.ok()) return status;
  status = FunctionAdapter<CelValue, CelValue::StringHolder>::CreateAndRegister(
      builtin::kSize, false, size_func, registry);
  if (!status.ok()) return status;

  // Bytes size
  auto bytes_size_func = [](Arena*, CelValue::BytesHolder value) -> int64_t {
    return value.value().size();
  };
  // receiver style = true/false
  // Support global and receiver style size() operations on bytes.
  status = FunctionAdapter<int64_t, CelValue::BytesHolder>::CreateAndRegister(
      builtin::kSize, true, bytes_size_func, registry);
  if (!status.ok()) return status;
  status = FunctionAdapter<int64_t, CelValue::BytesHolder>::CreateAndRegister(
      builtin::kSize, false, bytes_size_func, registry);
  if (!status.ok()) return status;

  // List size
  auto list_size_func = [](Arena*, const CelList* cel_list) -> int64_t {
    return (*cel_list).size();
  };
  // receiver style = true/false
  // Support both the global and receiver style size() for lists.
  status = FunctionAdapter<int64_t, const CelList*>::CreateAndRegister(
      builtin::kSize, true, list_size_func, registry);
  if (!status.ok()) return status;
  status = FunctionAdapter<int64_t, const CelList*>::CreateAndRegister(
      builtin::kSize, false, list_size_func, registry);
  if (!status.ok()) return status;

  // Map size
  auto map_size_func = [](Arena*, const CelMap* cel_map) -> int64_t {
    return (*cel_map).size();
  };
  // receiver style = true/false
  status = FunctionAdapter<int64_t, const CelMap*>::CreateAndRegister(
      builtin::kSize, true, map_size_func, registry);
  if (!status.ok()) return status;
  status = FunctionAdapter<int64_t, const CelMap*>::CreateAndRegister(
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

  // Special arithmetic operators for Timestamp and Duration
  status =
      FunctionAdapter<CelValue, absl::Time, absl::Duration>::CreateAndRegister(
          builtin::kAdd, false,
          [](Arena*, absl::Time t1, absl::Duration d2) -> CelValue {
            return CelValue::CreateTimestamp(t1 + d2);
          },
          registry);
  if (!status.ok()) return status;

  status =
      FunctionAdapter<CelValue, absl::Duration, absl::Time>::CreateAndRegister(
          builtin::kAdd, false,
          [](Arena*, absl::Duration d2, absl::Time t1) -> CelValue {
            return CelValue::CreateTimestamp(t1 + d2);
          },
          registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Duration, absl::Duration>::
      CreateAndRegister(
          builtin::kAdd, false,
          [](Arena*, absl::Duration d1, absl::Duration d2) -> CelValue {
            return CelValue::CreateDuration(d1 + d2);
          },
          registry);
  if (!status.ok()) return status;

  status =
      FunctionAdapter<CelValue, absl::Time, absl::Duration>::CreateAndRegister(
          builtin::kSubtract, false,
          [](Arena*, absl::Time t1, absl::Duration d2) -> CelValue {
            return CelValue::CreateTimestamp(t1 - d2);
          },
          registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Time, absl::Time>::CreateAndRegister(
      builtin::kSubtract, false,
      [](Arena*, absl::Time t1, absl::Time t2) -> CelValue {
        return CelValue::CreateDuration(t1 - t2);
      },
      registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Duration, absl::Duration>::
      CreateAndRegister(
          builtin::kSubtract, false,
          [](Arena*, absl::Duration d1, absl::Duration d2) -> CelValue {
            return CelValue::CreateDuration(d1 - d2);
          },
          registry);
  if (!status.ok()) return status;

  // Concat group
  if (options.enable_string_concat) {
    status = FunctionAdapter<
        CelValue::StringHolder, CelValue::StringHolder,
        CelValue::StringHolder>::CreateAndRegister(builtin::kAdd, false,
                                                   ConcatString, registry);
    if (!status.ok()) return status;

    status =
        FunctionAdapter<CelValue::BytesHolder, CelValue::BytesHolder,
                        CelValue::BytesHolder>::CreateAndRegister(builtin::kAdd,
                                                                  false,
                                                                  ConcatBytes,
                                                                  registry);
    if (!status.ok()) return status;
  }

  if (options.enable_list_concat) {
    status =
        FunctionAdapter<const CelList*, const CelList*,
                        const CelList*>::CreateAndRegister(builtin::kAdd, false,
                                                           ConcatList,
                                                           registry);
    if (!status.ok()) return status;
  }

  // Global matches function.
  if (options.enable_regex) {
    auto regex_matches = [max_size = options.regex_max_program_size](
                             Arena* arena, CelValue::StringHolder target,
                             CelValue::StringHolder regex) -> CelValue {
      RE2 re2(regex.value().data());
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

    status = FunctionAdapter<
        CelValue, CelValue::StringHolder,
        CelValue::StringHolder>::CreateAndRegister(builtin::kRegexMatch, false,
                                                   regex_matches, registry);
    if (!status.ok()) return status;

    // Receiver-style matches function.
    status = FunctionAdapter<
        CelValue, CelValue::StringHolder,
        CelValue::StringHolder>::CreateAndRegister(builtin::kRegexMatch, true,
                                                   regex_matches, registry);
    if (!status.ok()) return status;
  }

  status = RegisterStringFunctions(registry, options);
  if (!status.ok()) return status;

  // Modulo
  status = FunctionAdapter<CelValue, int64_t, int64_t>::CreateAndRegister(
      builtin::kModulo, false, Modulo<int64_t>, registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, uint64_t, uint64_t>::CreateAndRegister(
      builtin::kModulo, false, Modulo<uint64_t>, registry);
  if (!status.ok()) return status;

  status = RegisterTimestampFunctions(registry, options);
  if (!status.ok()) return status;

  // duration functions
  status = FunctionAdapter<CelValue, absl::Duration>::CreateAndRegister(
      builtin::kHours, true,
      [](Arena* arena, absl::Duration d) -> CelValue {
        return GetHours(arena, d);
      },
      registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Duration>::CreateAndRegister(
      builtin::kMinutes, true,
      [](Arena* arena, absl::Duration d) -> CelValue {
        return GetMinutes(arena, d);
      },
      registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Duration>::CreateAndRegister(
      builtin::kSeconds, true,
      [](Arena* arena, absl::Duration d) -> CelValue {
        return GetSeconds(arena, d);
      },
      registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Duration>::CreateAndRegister(
      builtin::kMilliseconds, true,
      [](Arena* arena, absl::Duration d) -> CelValue {
        return GetMilliseconds(arena, d);
      },
      registry);
  if (!status.ok()) return status;

  return FunctionAdapter<CelValue::CelTypeHolder, CelValue>::CreateAndRegister(
      builtin::kType, false,
      [](Arena*, CelValue value) -> CelValue::CelTypeHolder {
        return value.ObtainCelType().CelTypeOrDie();
      },
      registry);
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
