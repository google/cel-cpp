#include "eval/public/builtin_func_registrar.h"

#include <functional>

#include "google/protobuf/util/time_util.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "eval/eval/container_backed_list_impl.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_function_adapter.h"
#include "re2/re2.h"
#include "base/canonical_errors.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

using google::protobuf::Duration;
using google::protobuf::Timestamp;
using google::protobuf::Arena;
using google::protobuf::util::TimeUtil;

namespace {

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
  return CelValue::CreateBool(operator!=(t1, t2));
}

template <>
CelValue Equal(Arena*, absl::Duration t1, absl::Duration t2) {
  return CelValue::CreateBool(operator==(t1, t2));
}

template <>
bool LessThan(Arena*, absl::Duration t1, absl::Duration t2) {
  return operator<(t1, t2);
}

template <>
bool LessThanOrEqual(Arena*, absl::Duration t1, absl::Duration t2) {
  return operator<=(t1, t2);
}

template <>
bool GreaterThan(Arena*, absl::Duration t1, absl::Duration t2) {
  return operator>(t1, t2);
}

template <>
bool GreaterThanOrEqual(Arena*, absl::Duration t1, absl::Duration t2) {
  return operator>=(t1, t2);
}

// Timestamp comparison specializations
template <>
CelValue Inequal(Arena*, absl::Time t1, absl::Time t2) {
  return CelValue::CreateBool(operator!=(t1, t2));
}

template <>
CelValue Equal(Arena*, absl::Time t1, absl::Time t2) {
  return CelValue::CreateBool(operator==(t1, t2));
}

template <>
bool LessThan(Arena*, absl::Time t1, absl::Time t2) {
  return operator<(t1, t2);
}

template <>
bool LessThanOrEqual(Arena*, absl::Time t1, absl::Time t2) {
  return operator<=(t1, t2);
}

template <>
bool GreaterThan(Arena*, absl::Time t1, absl::Time t2) {
  return operator>(t1, t2);
}

template <>
bool GreaterThanOrEqual(Arena*, absl::Time t1, absl::Time t2) {
  return operator>=(t1, t2);
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
  return CreateNoMatchingOverloadError(arena);
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
  return CreateNoMatchingOverloadError(arena);
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
    default:
      break;
  }
  return CreateNoMatchingOverloadError(arena);
}

// Helper method
//
// Registers all equality functions for template parameters type.
template <class Type>
::cel_base::Status RegisterEqualityFunctionsForType(CelFunctionRegistry* registry) {
  // Inequality
  ::cel_base::Status status =
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
::cel_base::Status RegisterComparisonFunctionsForType(
    CelFunctionRegistry* registry) {
  ::cel_base::Status status = RegisterEqualityFunctionsForType<Type>(registry);
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
  status = FunctionAdapter<bool, Type, Type>::CreateAndRegister(
      builtin::kGreaterOrEqual, false, GreaterThanOrEqual<Type>, registry);
  if (!status.ok()) return status;

  return ::cel_base::OkStatus();
}

// Template functions providing arithmetic operations
template <class Type>
Type Add(Arena*, Type v0, Type v1) {
  return v0 + v1;
}

template <class Type>
Type Sub(Arena*, Type v0, Type v1) {
  return v0 - v1;
}

template <class Type>
Type Mul(Arena*, Type v0, Type v1) {
  return v0 * v1;
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
::cel_base::Status RegisterArithmeticFunctionsForType(
    CelFunctionRegistry* registry) {
  cel_base::Status status = FunctionAdapter<Type, Type, Type>::CreateAndRegister(
      builtin::kAdd, false, Add<Type>, registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<Type, Type, Type>::CreateAndRegister(
      builtin::kSubtract, false, Sub<Type>, registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<Type, Type, Type>::CreateAndRegister(
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
const cel_base::Status FindTimeBreakdown(absl::Time timestamp, absl::string_view tz,
                                     absl::TimeZone::CivilInfo* breakdown) {
  absl::TimeZone time_zone;

  if (!tz.empty()) {
    bool found = absl::LoadTimeZone(std::string(tz), &time_zone);
    if (!found) {
      return cel_base::InvalidArgumentError("Invalid timezone");
    }
  }

  *breakdown = time_zone.At(timestamp);
  return cel_base::OkStatus();
}

CelValue GetTimeBreakdownPart(
    Arena* arena, absl::Time timestamp, absl::string_view tz,
    const std::function<CelValue(const absl::TimeZone::CivilInfo&)>&
        extractor_func) {
  absl::TimeZone::CivilInfo breakdown;
  auto status = FindTimeBreakdown(timestamp, tz, &breakdown);

  if (!status.ok()) {
    return CreateErrorValue(arena, status.message());
  }

  return extractor_func(breakdown);
}

CelValue CreateTimestampFromString(Arena* arena,
                                   CelValue::StringHolder time_str) {
  absl::Time ts;
  if (!absl::ParseTime(absl::RFC3339_full, std::string(time_str.value()), &ts,
                       nullptr)) {
    return CreateErrorValue(arena, "String to Timestamp conversion failed",
                            cel_base::StatusCode::kInvalidArgument);
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
        absl::Weekday weekday = absl::GetWeekday(absl::CivilDay(breakdown.cs));

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
  if (!absl::ParseDuration(std::string(dur_str.value()), &d)) {
    return CreateErrorValue(arena, "String to Duration conversion failed",
                            cel_base::StatusCode::kInvalidArgument);
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

}  // namespace

::cel_base::Status RegisterBuiltinFunctions(CelFunctionRegistry* registry,
                                        const InterpreterOptions& options) {
  // logical NOT
  cel_base::Status status = FunctionAdapter<bool, bool>::CreateAndRegister(
      builtin::kNot, false, [](Arena*, bool value) -> bool { return !value; },
      registry);
  if (!status.ok()) return status;

  // Negation group
  status = FunctionAdapter<int64_t, int64_t>::CreateAndRegister(
      builtin::kNeg, false, [](Arena*, int64_t value) -> int64_t { return -value; },
      registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<double, double>::CreateAndRegister(
      builtin::kNeg, false,
      [](Arena*, double value) -> double { return -value; }, registry);
  if (!status.ok()) return status;

  status = RegisterComparisonFunctionsForType<bool>(registry);
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

  // Logical AND
  // This implementation is used when short-circuiting is off.
  status = FunctionAdapter<bool, bool, bool>::CreateAndRegister(
      builtin::kAnd, false,
      [](Arena*, bool value1, bool value2) -> bool { return value1 && value2; },
      registry);
  if (!status.ok()) return status;

  // Special case: one of arguments is error.
  status = FunctionAdapter<CelValue, const CelError*, bool>::CreateAndRegister(
      builtin::kAnd, false,
      [](Arena*, const CelError* value1, bool value2) {
        return (value2) ? CelValue::CreateError(value1)
                        : CelValue::CreateBool(false);
      },
      registry);
  if (!status.ok()) return status;

  // Special case: one of arguments is error.
  status = FunctionAdapter<CelValue, bool, const CelError*>::CreateAndRegister(
      builtin::kAnd, false,
      [](Arena*, bool value1, const CelError* value2) {
        return (value1) ? CelValue::CreateError(value2)
                        : CelValue::CreateBool(false);
      },
      registry);
  if (!status.ok()) return status;

  // Special case: both arguments are errors.
  status = FunctionAdapter<const CelError*, const CelError*, const CelError*>::
      CreateAndRegister(
          builtin::kAnd, false,
          [](Arena*, const CelError* value1, const CelError*) {
            return value1;
          },
          registry);
  if (!status.ok()) return status;

  // Logical OR
  // This implementation is used when short-circuiting is off.
  status = FunctionAdapter<bool, bool, bool>::CreateAndRegister(
      builtin::kOr, false,
      [](Arena*, bool value1, bool value2) -> bool { return value1 || value2; },
      registry);
  if (!status.ok()) return status;

  // Special case: one of arguments is error.
  status = FunctionAdapter<CelValue, const CelError*, bool>::CreateAndRegister(
      builtin::kOr, false,
      [](Arena*, const CelError* value1, bool value2) {
        return (value2) ? CelValue::CreateBool(true)
                        : CelValue::CreateError(value1);
      },
      registry);
  if (!status.ok()) return status;

  // Special case: one of arguments is error.
  status = FunctionAdapter<CelValue, bool, const CelError*>::CreateAndRegister(
      builtin::kOr, false,
      [](Arena*, bool value1, const CelError* value2) {
        return (value1) ? CelValue::CreateBool(true)
                        : CelValue::CreateError(value2);
      },
      registry);
  if (!status.ok()) return status;

  // Special case: both arguments are errors.
  status = FunctionAdapter<const CelError*, const CelError*, const CelError*>::
      CreateAndRegister(
          builtin::kOr, false,
          [](Arena*, const CelError* value1, const CelError*) {
            return value1;
          },
          registry);
  if (!status.ok()) return status;

  // Ternary operator
  // This implementation is used when short-circuiting is off.
  status =
      FunctionAdapter<CelValue, bool, CelValue, CelValue>::CreateAndRegister(
          builtin::kTernary, false,
          [](Arena*, bool cond, CelValue value1, CelValue value2) {
            return (cond) ? value1 : value2;
          },
          registry);
  if (!status.ok()) return status;

  // Ternary operator
  // Special case: condition is error
  status = FunctionAdapter<CelValue, const CelError*, CelValue, CelValue>::
      CreateAndRegister(
          builtin::kTernary, false,
          [](Arena*, const CelError* error, CelValue, CelValue) {
            return CelValue::CreateError(error);
          },
          registry);
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

  status = FunctionAdapter<bool, bool>::CreateAndRegister(
      builtin::kNotStrictlyFalseDeprecated, false,
      [](Arena*, bool value) -> bool { return value; }, registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<bool, const CelError*>::CreateAndRegister(
      builtin::kNotStrictlyFalseDeprecated, false,
      [](Arena*, const CelError*) -> bool { return true; }, registry);
  if (!status.ok()) return status;

  // String size
  auto string_size_func = [](Arena*, CelValue::StringHolder value) -> int64_t {
    return value.value().size();
  };
  // receiver style = true/false
  // Support global and receiver style size() operations on strings.
  status = FunctionAdapter<int64_t, CelValue::StringHolder>::CreateAndRegister(
      builtin::kSize, true, string_size_func, registry);
  if (!status.ok()) return status;
  status = FunctionAdapter<int64_t, CelValue::StringHolder>::CreateAndRegister(
      builtin::kSize, false, string_size_func, registry);
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

  // List in operator: @in
  if (options.enable_list_contains) {
    status = FunctionAdapter<bool, bool, const CelList*>::CreateAndRegister(
        builtin::kIn, false, In<bool>, registry);
    if (!status.ok()) return status;
    status = FunctionAdapter<bool, int64_t, const CelList*>::CreateAndRegister(
        builtin::kIn, false, In<int64_t>, registry);
    if (!status.ok()) return status;
    status = FunctionAdapter<bool, uint64_t, const CelList*>::CreateAndRegister(
        builtin::kIn, false, In<uint64_t>, registry);
    if (!status.ok()) return status;
    status = FunctionAdapter<bool, double, const CelList*>::CreateAndRegister(
        builtin::kIn, false, In<double>, registry);
    if (!status.ok()) return status;
    status = FunctionAdapter<bool, CelValue::StringHolder, const CelList*>::
        CreateAndRegister(builtin::kIn, false, In<CelValue::StringHolder>,
                          registry);
    if (!status.ok()) return status;
    status = FunctionAdapter<bool, CelValue::BytesHolder, const CelList*>::
        CreateAndRegister(builtin::kIn, false, In<CelValue::BytesHolder>,
                          registry);
    if (!status.ok()) return status;

    // List in operator: _in_ (deprecated)
    // Bindings preserved for backward compatibility with stored expressions.
    status = FunctionAdapter<bool, bool, const CelList*>::CreateAndRegister(
        builtin::kInDeprecated, false, In<bool>, registry);
    if (!status.ok()) return status;
    status = FunctionAdapter<bool, int64_t, const CelList*>::CreateAndRegister(
        builtin::kInDeprecated, false, In<int64_t>, registry);
    if (!status.ok()) return status;
    status = FunctionAdapter<bool, uint64_t, const CelList*>::CreateAndRegister(
        builtin::kInDeprecated, false, In<uint64_t>, registry);
    if (!status.ok()) return status;
    status = FunctionAdapter<bool, double, const CelList*>::CreateAndRegister(
        builtin::kInDeprecated, false, In<double>, registry);
    if (!status.ok()) return status;
    status = FunctionAdapter<bool, CelValue::StringHolder, const CelList*>::
        CreateAndRegister(builtin::kInDeprecated, false,
                          In<CelValue::StringHolder>, registry);
    if (!status.ok()) return status;
    status = FunctionAdapter<bool, CelValue::BytesHolder, const CelList*>::
        CreateAndRegister(builtin::kInDeprecated, false,
                          In<CelValue::BytesHolder>, registry);
    if (!status.ok()) return status;

    // List in() function (deprecated)
    // Bindings preserved for backward compatibility with stored expressions.
    status = FunctionAdapter<bool, bool, const CelList*>::CreateAndRegister(
        builtin::kInFunction, false, In<bool>, registry);
    if (!status.ok()) return status;
    status = FunctionAdapter<bool, int64_t, const CelList*>::CreateAndRegister(
        builtin::kInFunction, false, In<int64_t>, registry);
    if (!status.ok()) return status;
    status = FunctionAdapter<bool, uint64_t, const CelList*>::CreateAndRegister(
        builtin::kInFunction, false, In<uint64_t>, registry);
    if (!status.ok()) return status;
    status = FunctionAdapter<bool, double, const CelList*>::CreateAndRegister(
        builtin::kInFunction, false, In<double>, registry);
    if (!status.ok()) return status;
    status = FunctionAdapter<bool, CelValue::StringHolder, const CelList*>::
        CreateAndRegister(builtin::kInFunction, false,
                          In<CelValue::StringHolder>, registry);
    if (!status.ok()) return status;
    status = FunctionAdapter<bool, CelValue::BytesHolder, const CelList*>::
        CreateAndRegister(builtin::kInFunction, false,
                          In<CelValue::BytesHolder>, registry);
    if (!status.ok()) return status;
  }

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

  // Map in operator: @in
  status = FunctionAdapter<bool, CelValue::StringHolder, const CelMap*>::
      CreateAndRegister(
          builtin::kIn, false,
          [](Arena*, CelValue::StringHolder key,
             const CelMap* cel_map) -> bool {
            return (*cel_map)[CelValue::CreateString(key)].has_value();
          },
          registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<bool, int64_t, const CelMap*>::CreateAndRegister(
      builtin::kIn, false,
      [](Arena*, int64_t key, const CelMap* cel_map) -> bool {
        return (*cel_map)[CelValue::CreateInt64(key)].has_value();
      },
      registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<bool, uint64_t, const CelMap*>::CreateAndRegister(
      builtin::kIn, false,
      [](Arena*, uint64_t key, const CelMap* cel_map) -> bool {
        return (*cel_map)[CelValue::CreateUint64(key)].has_value();
      },
      registry);
  if (!status.ok()) return status;

  // Map in operators: _in_ (deprecated).
  // Bindings preserved for backward compatibility with stored expressions.
  status = FunctionAdapter<bool, int64_t, const CelMap*>::CreateAndRegister(
      builtin::kInDeprecated, false,
      [](Arena*, int64_t key, const CelMap* cel_map) -> bool {
        return (*cel_map)[CelValue::CreateInt64(key)].has_value();
      },
      registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<bool, uint64_t, const CelMap*>::CreateAndRegister(
      builtin::kInDeprecated, false,
      [](Arena*, uint64_t key, const CelMap* cel_map) -> bool {
        return (*cel_map)[CelValue::CreateUint64(key)].has_value();
      },
      registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<bool, CelValue::StringHolder, const CelMap*>::
      CreateAndRegister(
          builtin::kInDeprecated, false,
          [](Arena*, CelValue::StringHolder key,
             const CelMap* cel_map) -> bool {
            return (*cel_map)[CelValue::CreateString(key)].has_value();
          },
          registry);
  if (!status.ok()) return status;

  // Map in() function (deprecated)
  status = FunctionAdapter<bool, CelValue::StringHolder, const CelMap*>::
      CreateAndRegister(
          builtin::kInFunction, false,
          [](Arena*, CelValue::StringHolder key,
             const CelMap* cel_map) -> bool {
            return (*cel_map)[CelValue::CreateString(key)].has_value();
          },
          registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<bool, int64_t, const CelMap*>::CreateAndRegister(
      builtin::kInFunction, false,
      [](Arena*, int64_t key, const CelMap* cel_map) -> bool {
        return (*cel_map)[CelValue::CreateInt64(key)].has_value();
      },
      registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<bool, uint64_t, const CelMap*>::CreateAndRegister(
      builtin::kInFunction, false,
      [](Arena*, uint64_t key, const CelMap* cel_map) -> bool {
        return (*cel_map)[CelValue::CreateUint64(key)].has_value();
      },
      registry);
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
                                cel_base::StatusCode::kInvalidArgument);
      }
      if (!re2.ok()) {
        return CreateErrorValue(arena, "invalid_argument",
                                cel_base::StatusCode::kInvalidArgument);
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

  status =
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

  status =
      FunctionAdapter<bool, CelValue::StringHolder, CelValue::StringHolder>::
          CreateAndRegister(builtin::kStringStartsWith, true, StringStartsWith,
                            registry);
  if (!status.ok()) return status;

  // Modulo
  status = FunctionAdapter<CelValue, int64_t, int64_t>::CreateAndRegister(
      builtin::kModulo, false, Modulo<int64_t>, registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, uint64_t, uint64_t>::CreateAndRegister(
      builtin::kModulo, false, Modulo<uint64_t>, registry);
  if (!status.ok()) return status;

  // Timestamp
  //
  // timestamp() conversion from string..
  status = FunctionAdapter<CelValue, CelValue::StringHolder>::CreateAndRegister(
      builtin::kTimestamp, false, CreateTimestampFromString, registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<CelValue, absl::Time, CelValue::StringHolder>::
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

  status = FunctionAdapter<CelValue, absl::Time>::CreateAndRegister(
      builtin::kMilliseconds, true,
      [](Arena* arena, absl::Time ts) -> CelValue {
        return GetMilliseconds(arena, ts, "");
      },
      registry);
  if (!status.ok()) return status;

  // type conversion to int
  // TODO(issues/26): To return errors on loss of precision
  // (overflow/underflow) by returning StatusOr<RawType>.
  status = FunctionAdapter<int64_t, absl::Time>::CreateAndRegister(
      builtin::kInt, false,
      [](Arena*, absl::Time t) { return absl::ToUnixSeconds(t); }, registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<int64_t, double>::CreateAndRegister(
      builtin::kInt, false, [](Arena*, double v) { return (int64_t)v; },
      registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<int64_t, bool>::CreateAndRegister(
      builtin::kInt, false, [](Arena*, bool v) { return (int64_t)v; }, registry);
  if (!status.ok()) return status;

  status = FunctionAdapter<int64_t, uint64_t>::CreateAndRegister(
      builtin::kInt, false, [](Arena*, uint64_t v) { return (int64_t)v; },
      registry);
  if (!status.ok()) return status;

  // duration

  // duration() conversion from string..
  status = FunctionAdapter<CelValue, CelValue::StringHolder>::CreateAndRegister(
      builtin::kDuration, false, CreateDurationFromString, registry);
  if (!status.ok()) return status;

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

  if (options.enable_string_conversion) {
    status = FunctionAdapter<CelValue::StringHolder, int64_t>::CreateAndRegister(
        builtin::kString, false,
        [](Arena* arena, int64_t value) -> CelValue::StringHolder {
          return CelValue::StringHolder(
              Arena::Create<std::string>(arena, absl::StrCat(value)));
        },
        registry);
    if (!status.ok()) return status;

    status = FunctionAdapter<CelValue::StringHolder, uint64_t>::CreateAndRegister(
        builtin::kString, false,
        [](Arena* arena, uint64_t value) -> CelValue::StringHolder {
          return CelValue::StringHolder(
              Arena::Create<std::string>(arena, absl::StrCat(value)));
        },
        registry);
    if (!status.ok()) return status;

    status = FunctionAdapter<CelValue::StringHolder, double>::CreateAndRegister(
        builtin::kString, false,
        [](Arena* arena, double value) -> CelValue::StringHolder {
          return CelValue::StringHolder(
              Arena::Create<std::string>(arena, absl::StrCat(value)));
        },
        registry);
    if (!status.ok()) return status;

    status = FunctionAdapter<CelValue::StringHolder, CelValue::BytesHolder>::
        CreateAndRegister(
            builtin::kString, false,
            [](Arena* arena,
               CelValue::BytesHolder value) -> CelValue::StringHolder {
              return CelValue::StringHolder(
                  Arena::Create<std::string>(arena, std::string(value.value())));
            },
            registry);
    if (!status.ok()) return status;

    status = FunctionAdapter<CelValue::StringHolder, CelValue::StringHolder>::
        CreateAndRegister(
            builtin::kString, false,
            [](Arena*, CelValue::StringHolder value) -> CelValue::StringHolder {
              return value;
            },
            registry);
    if (!status.ok()) return status;
  }

  return ::cel_base::OkStatus();
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
