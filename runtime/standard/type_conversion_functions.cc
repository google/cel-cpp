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

#include "runtime/standard/type_conversion_functions.h"

#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "base/builtins.h"
#include "base/function_adapter.h"
#include "base/value.h"
#include "base/value_manager.h"
#include "internal/overflow.h"
#include "internal/status_macros.h"
#include "internal/time.h"

namespace cel {
namespace {

using ::cel::internal::EncodeDurationToJson;
using ::cel::internal::EncodeTimestampToJson;
using ::cel::internal::MaxTimestamp;

// Time representing `9999-12-31T23:59:59.999999999Z`.
const absl::Time kMaxTime = MaxTimestamp();

absl::Status RegisterIntConversionFunctions(FunctionRegistry& registry,
                                            const RuntimeOptions&) {
  // bool -> int
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<int64_t, bool>::CreateDescriptor(cel::builtin::kInt,
                                                            false),
      UnaryFunctionAdapter<int64_t, bool>::WrapFunction(
          [](ValueManager&, bool v) { return static_cast<int64_t>(v); })));

  // double -> int
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, double>::CreateDescriptor(cel::builtin::kInt,
                                                            false),
      UnaryFunctionAdapter<Value, double>::WrapFunction(
          [](ValueManager& value_factory, double v) -> Value {
            auto conv = cel::internal::CheckedDoubleToInt64(v);
            if (!conv.ok()) {
              return value_factory.CreateErrorValue(conv.status());
            }
            return value_factory.CreateIntValue(*conv);
          })));

  // int -> int
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<int64_t, int64_t>::CreateDescriptor(
          cel::builtin::kInt, false),
      UnaryFunctionAdapter<int64_t, int64_t>::WrapFunction(
          [](ValueManager&, int64_t v) { return v; })));

  // string -> int
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, const StringValue&>::CreateDescriptor(
          cel::builtin::kInt, false),
      UnaryFunctionAdapter<Value, const StringValue&>::WrapFunction(
          [](ValueManager& value_factory, const StringValue& s) -> Value {
            int64_t result;
            if (!absl::SimpleAtoi(s.ToString(), &result)) {
              return value_factory.CreateErrorValue(
                  absl::InvalidArgumentError("cannot convert string to int"));
            }
            return value_factory.CreateIntValue(result);
          })));

  // time -> int
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<int64_t, absl::Time>::CreateDescriptor(
          cel::builtin::kInt, false),
      UnaryFunctionAdapter<int64_t, absl::Time>::WrapFunction(
          [](ValueManager&, absl::Time t) { return absl::ToUnixSeconds(t); })));

  // uint -> int
  return registry.Register(
      UnaryFunctionAdapter<Value, uint64_t>::CreateDescriptor(
          cel::builtin::kInt, false),
      UnaryFunctionAdapter<Value, uint64_t>::WrapFunction(
          [](ValueManager& value_factory, uint64_t v) -> Value {
            auto conv = cel::internal::CheckedUint64ToInt64(v);
            if (!conv.ok()) {
              return value_factory.CreateErrorValue(conv.status());
            }
            return value_factory.CreateIntValue(*conv);
          }));
}

absl::Status RegisterStringConversionFunctions(FunctionRegistry& registry,
                                               const RuntimeOptions& options) {
  // May be optionally disabled to reduce potential allocs.
  if (!options.enable_string_conversion) {
    return absl::OkStatus();
  }

  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, const BytesValue&>::CreateDescriptor(
          cel::builtin::kString, false),
      UnaryFunctionAdapter<Value, const BytesValue&>::WrapFunction(
          [](ValueManager& value_factory, const BytesValue& value) -> Value {
            auto handle_or = value_factory.CreateStringValue(value.ToString());
            if (!handle_or.ok()) {
              return value_factory.CreateErrorValue(handle_or.status());
            }
            return *handle_or;
          })));

  // double -> string
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<StringValue, double>::CreateDescriptor(
          cel::builtin::kString, false),
      UnaryFunctionAdapter<StringValue, double>::WrapFunction(
          [](ValueManager& value_factory, double value) -> StringValue {
            return value_factory.CreateUncheckedStringValue(
                absl::StrCat(value));
          })));

  // int -> string
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<StringValue, int64_t>::CreateDescriptor(
          cel::builtin::kString, false),
      UnaryFunctionAdapter<StringValue, int64_t>::WrapFunction(
          [](ValueManager& value_factory, int64_t value) -> StringValue {
            return value_factory.CreateUncheckedStringValue(
                absl::StrCat(value));
          })));

  // string -> string
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<StringValue, StringValue>::CreateDescriptor(
          cel::builtin::kString, false),
      UnaryFunctionAdapter<StringValue, StringValue>::WrapFunction(
          [](ValueManager&, StringValue value) -> StringValue {
            return value;
          })));

  // uint -> string
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<StringValue, uint64_t>::CreateDescriptor(
          cel::builtin::kString, false),
      UnaryFunctionAdapter<StringValue, uint64_t>::WrapFunction(
          [](ValueManager& value_factory, uint64_t value) -> StringValue {
            return value_factory.CreateUncheckedStringValue(
                absl::StrCat(value));
          })));

  // duration -> string
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, absl::Duration>::CreateDescriptor(
          cel::builtin::kString, false),
      UnaryFunctionAdapter<Value, absl::Duration>::WrapFunction(
          [](ValueManager& value_factory, absl::Duration value) -> Value {
            auto encode = EncodeDurationToJson(value);
            if (!encode.ok()) {
              return value_factory.CreateErrorValue(encode.status());
            }
            return value_factory.CreateUncheckedStringValue(*encode);
          })));

  // timestamp -> string
  return registry.Register(
      UnaryFunctionAdapter<Value, absl::Time>::CreateDescriptor(
          cel::builtin::kString, false),
      UnaryFunctionAdapter<Value, absl::Time>::WrapFunction(
          [](ValueManager& value_factory, absl::Time value) -> Value {
            auto encode = EncodeTimestampToJson(value);
            if (!encode.ok()) {
              return value_factory.CreateErrorValue(encode.status());
            }
            return value_factory.CreateUncheckedStringValue(*encode);
          }));
}

absl::Status RegisterUintConversionFunctions(FunctionRegistry& registry,
                                             const RuntimeOptions&) {
  // double -> uint
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, double>::CreateDescriptor(cel::builtin::kUint,
                                                            false),
      UnaryFunctionAdapter<Value, double>::WrapFunction(
          [](ValueManager& value_factory, double v) -> Value {
            auto conv = cel::internal::CheckedDoubleToUint64(v);
            if (!conv.ok()) {
              return value_factory.CreateErrorValue(conv.status());
            }
            return value_factory.CreateUintValue(*conv);
          })));

  // int -> uint
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, int64_t>::CreateDescriptor(
          cel::builtin::kUint, false),
      UnaryFunctionAdapter<Value, int64_t>::WrapFunction(
          [](ValueManager& value_factory, int64_t v) -> Value {
            auto conv = cel::internal::CheckedInt64ToUint64(v);
            if (!conv.ok()) {
              return value_factory.CreateErrorValue(conv.status());
            }
            return value_factory.CreateUintValue(*conv);
          })));

  // string -> uint
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, const StringValue&>::CreateDescriptor(
          cel::builtin::kUint, false),
      UnaryFunctionAdapter<Value, const StringValue&>::WrapFunction(
          [](ValueManager& value_factory, const StringValue& s) -> Value {
            uint64_t result;
            if (!absl::SimpleAtoi(s.ToString(), &result)) {
              return value_factory.CreateErrorValue(
                  absl::InvalidArgumentError("doesn't convert to a string"));
            }
            return value_factory.CreateUintValue(result);
          })));

  // uint -> uint
  return registry.Register(
      UnaryFunctionAdapter<uint64_t, uint64_t>::CreateDescriptor(
          cel::builtin::kUint, false),
      UnaryFunctionAdapter<uint64_t, uint64_t>::WrapFunction(
          [](ValueManager&, uint64_t v) { return v; }));
}

absl::Status RegisterBytesConversionFunctions(FunctionRegistry& registry,
                                              const RuntimeOptions&) {
  // bytes -> bytes
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<BytesValue, BytesValue>::CreateDescriptor(
          cel::builtin::kBytes, false),
      UnaryFunctionAdapter<BytesValue, BytesValue>::WrapFunction(
          [](ValueManager&, BytesValue value) -> BytesValue {
            return value;
          })));

  // string -> bytes
  return registry.Register(
      UnaryFunctionAdapter<absl::StatusOr<BytesValue>, const StringValue&>::
          CreateDescriptor(cel::builtin::kBytes, false),
      UnaryFunctionAdapter<absl::StatusOr<BytesValue>, const StringValue&>::
          WrapFunction(
              [](ValueManager& value_factory, const StringValue& value) {
                return value_factory.CreateBytesValue(value.ToString());
              }));
}

absl::Status RegisterDoubleConversionFunctions(FunctionRegistry& registry,
                                               const RuntimeOptions&) {
  // double -> double
  CEL_RETURN_IF_ERROR(
      registry.Register(UnaryFunctionAdapter<double, double>::CreateDescriptor(
                            cel::builtin::kDouble, false),
                        UnaryFunctionAdapter<double, double>::WrapFunction(
                            [](ValueManager&, double v) { return v; })));

  // int -> double
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<double, int64_t>::CreateDescriptor(
          cel::builtin::kDouble, false),
      UnaryFunctionAdapter<double, int64_t>::WrapFunction(
          [](ValueManager&, int64_t v) { return static_cast<double>(v); })));

  // string -> double
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, const StringValue&>::CreateDescriptor(
          cel::builtin::kDouble, false),
      UnaryFunctionAdapter<Value, const StringValue&>::WrapFunction(
          [](ValueManager& value_factory, const StringValue& s) -> Value {
            double result;
            if (absl::SimpleAtod(s.ToString(), &result)) {
              return value_factory.CreateDoubleValue(result);
            } else {
              return value_factory.CreateErrorValue(absl::InvalidArgumentError(
                  "cannot convert string to double"));
            }
          })));

  // uint -> double
  return registry.Register(
      UnaryFunctionAdapter<double, uint64_t>::CreateDescriptor(
          cel::builtin::kDouble, false),
      UnaryFunctionAdapter<double, uint64_t>::WrapFunction(
          [](ValueManager&, uint64_t v) { return static_cast<double>(v); }));
}

Value CreateDurationFromString(ValueManager& value_factory,
                               const StringValue& dur_str) {
  absl::Duration d;
  if (!absl::ParseDuration(dur_str.ToString(), &d)) {
    return value_factory.CreateErrorValue(
        absl::InvalidArgumentError("String to Duration conversion failed"));
  }

  auto duration = value_factory.CreateDurationValue(d);

  if (!duration.ok()) {
    return value_factory.CreateErrorValue(duration.status());
  }

  return *duration;
}

absl::Status RegisterTimeConversionFunctions(FunctionRegistry& registry,
                                             const RuntimeOptions& options) {
  // duration() conversion from string.
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, const StringValue&>::CreateDescriptor(
          cel::builtin::kDuration, false),
      UnaryFunctionAdapter<Value, const StringValue&>::WrapFunction(
          CreateDurationFromString)));

  // timestamp conversion from int.
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, int64_t>::CreateDescriptor(
          cel::builtin::kTimestamp, false),
      UnaryFunctionAdapter<Value, int64_t>::WrapFunction(
          [](ValueManager& value_factory, int64_t epoch_seconds) -> Value {
            return value_factory.CreateUncheckedTimestampValue(
                absl::FromUnixSeconds(epoch_seconds));
          })));

  // timestamp() conversion from string.
  bool enable_timestamp_duration_overflow_errors =
      options.enable_timestamp_duration_overflow_errors;
  return registry.Register(
      UnaryFunctionAdapter<Value, const StringValue&>::CreateDescriptor(
          cel::builtin::kTimestamp, false),
      UnaryFunctionAdapter<Value, const StringValue&>::WrapFunction(
          [=](ValueManager& value_factory,
              const StringValue& time_str) -> Value {
            absl::Time ts;
            if (!absl::ParseTime(absl::RFC3339_full, time_str.ToString(), &ts,
                                 nullptr)) {
              return value_factory.CreateErrorValue(absl::InvalidArgumentError(
                  "String to Timestamp conversion failed"));
            }
            if (enable_timestamp_duration_overflow_errors) {
              if (ts < absl::UniversalEpoch() || ts > kMaxTime) {
                return value_factory.CreateErrorValue(
                    absl::OutOfRangeError("timestamp overflow"));
              }
            }
            return value_factory.CreateUncheckedTimestampValue(ts);
          }));
}

}  // namespace

absl::Status RegisterTypeConversionFunctions(FunctionRegistry& registry,
                                             const RuntimeOptions& options) {
  CEL_RETURN_IF_ERROR(RegisterBytesConversionFunctions(registry, options));

  CEL_RETURN_IF_ERROR(RegisterDoubleConversionFunctions(registry, options));

  CEL_RETURN_IF_ERROR(RegisterIntConversionFunctions(registry, options));

  CEL_RETURN_IF_ERROR(RegisterStringConversionFunctions(registry, options));

  CEL_RETURN_IF_ERROR(RegisterUintConversionFunctions(registry, options));

  CEL_RETURN_IF_ERROR(RegisterTimeConversionFunctions(registry, options));

  // dyn() identity function.
  // TODO(issues/102): strip dyn() function references at type-check time.
  CEL_RETURN_IF_ERROR(registry.Register(
      UnaryFunctionAdapter<Value, const Value&>::CreateDescriptor(
          cel::builtin::kDyn, false),
      UnaryFunctionAdapter<Value, const Value&>::WrapFunction(
          [](ValueManager&, const Value& value) -> Value { return value; })));

  // type(dyn) -> type
  return registry.Register(
      UnaryFunctionAdapter<Value, const Value&>::CreateDescriptor(
          cel::builtin::kType, false),
      UnaryFunctionAdapter<Value, const Value&>::WrapFunction(
          [](ValueManager& factory, const Value& value) {
            return factory.CreateTypeValue(value.GetType(factory));
          }));
}

}  // namespace cel
