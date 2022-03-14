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

#include "base/value.h"

#include <algorithm>
#include <cmath>
#include <functional>
#include <limits>
#include <string>
#include <type_traits>
#include <utility>

#include "absl/hash/hash_testing.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/time/time.h"
#include "base/memory_manager.h"
#include "base/type.h"
#include "base/type_factory.h"
#include "base/value_factory.h"
#include "internal/strings.h"
#include "internal/testing.h"
#include "internal/time.h"

namespace cel {
namespace {

using cel::internal::StatusIs;

template <typename T>
Persistent<T> Must(absl::StatusOr<Persistent<T>> status_or_handle) {
  return std::move(status_or_handle).value();
}

class TestTypeFactory final : public TypeFactory {
 public:
  TestTypeFactory() : TypeFactory(MemoryManager::Global()) {}
};

class TestValueFactory final : public ValueFactory {
 public:
  TestValueFactory() : ValueFactory(MemoryManager::Global()) {}
};

template <class T>
constexpr void IS_INITIALIZED(T&) {}

TEST(Value, HandleSize) {
  // Advisory test to ensure we attempt to keep the size of Value handles under
  // 32 bytes. As of the time of writing they are 24 bytes.
  EXPECT_LE(sizeof(base_internal::ValueHandleData), 32);
}

TEST(Value, TransientHandleTypeTraits) {
  EXPECT_TRUE(std::is_default_constructible_v<Transient<Value>>);
  EXPECT_TRUE(std::is_copy_constructible_v<Transient<Value>>);
  EXPECT_TRUE(std::is_move_constructible_v<Transient<Value>>);
  EXPECT_TRUE(std::is_copy_assignable_v<Transient<Value>>);
  EXPECT_TRUE(std::is_move_assignable_v<Transient<Value>>);
  EXPECT_TRUE(std::is_swappable_v<Transient<Value>>);
  EXPECT_TRUE(std::is_default_constructible_v<Transient<const Value>>);
  EXPECT_TRUE(std::is_copy_constructible_v<Transient<const Value>>);
  EXPECT_TRUE(std::is_move_constructible_v<Transient<const Value>>);
  EXPECT_TRUE(std::is_copy_assignable_v<Transient<const Value>>);
  EXPECT_TRUE(std::is_move_assignable_v<Transient<const Value>>);
  EXPECT_TRUE(std::is_swappable_v<Transient<const Value>>);
}

TEST(Value, PersistentHandleTypeTraits) {
  EXPECT_TRUE(std::is_default_constructible_v<Persistent<Value>>);
  EXPECT_TRUE(std::is_copy_constructible_v<Persistent<Value>>);
  EXPECT_TRUE(std::is_move_constructible_v<Persistent<Value>>);
  EXPECT_TRUE(std::is_copy_assignable_v<Persistent<Value>>);
  EXPECT_TRUE(std::is_move_assignable_v<Persistent<Value>>);
  EXPECT_TRUE(std::is_swappable_v<Persistent<Value>>);
  EXPECT_TRUE(std::is_default_constructible_v<Persistent<const Value>>);
  EXPECT_TRUE(std::is_copy_constructible_v<Persistent<const Value>>);
  EXPECT_TRUE(std::is_move_constructible_v<Persistent<const Value>>);
  EXPECT_TRUE(std::is_copy_assignable_v<Persistent<const Value>>);
  EXPECT_TRUE(std::is_move_assignable_v<Persistent<const Value>>);
  EXPECT_TRUE(std::is_swappable_v<Persistent<const Value>>);
}

TEST(Value, DefaultConstructor) {
  TestValueFactory value_factory;
  Transient<const Value> value;
  EXPECT_EQ(value, value_factory.GetNullValue());
}

struct ConstructionAssignmentTestCase final {
  std::string name;
  std::function<Persistent<const Value>(ValueFactory&)> default_value;
};

using ConstructionAssignmentTest =
    testing::TestWithParam<ConstructionAssignmentTestCase>;

TEST_P(ConstructionAssignmentTest, CopyConstructor) {
  const auto& test_case = GetParam();
  TestValueFactory value_factory;
  Persistent<const Value> from(test_case.default_value(value_factory));
  Persistent<const Value> to(from);
  IS_INITIALIZED(to);
  EXPECT_EQ(to, test_case.default_value(value_factory));
}

TEST_P(ConstructionAssignmentTest, MoveConstructor) {
  const auto& test_case = GetParam();
  TestValueFactory value_factory;
  Persistent<const Value> from(test_case.default_value(value_factory));
  Persistent<const Value> to(std::move(from));
  IS_INITIALIZED(from);
  EXPECT_EQ(from, value_factory.GetNullValue());
  EXPECT_EQ(to, test_case.default_value(value_factory));
}

TEST_P(ConstructionAssignmentTest, CopyAssignment) {
  const auto& test_case = GetParam();
  TestValueFactory value_factory;
  Persistent<const Value> from(test_case.default_value(value_factory));
  Persistent<const Value> to;
  to = from;
  EXPECT_EQ(to, from);
}

TEST_P(ConstructionAssignmentTest, MoveAssignment) {
  const auto& test_case = GetParam();
  TestValueFactory value_factory;
  Persistent<const Value> from(test_case.default_value(value_factory));
  Persistent<const Value> to;
  to = std::move(from);
  IS_INITIALIZED(from);
  EXPECT_EQ(from, value_factory.GetNullValue());
  EXPECT_EQ(to, test_case.default_value(value_factory));
}

INSTANTIATE_TEST_SUITE_P(
    ConstructionAssignmentTest, ConstructionAssignmentTest,
    testing::ValuesIn<ConstructionAssignmentTestCase>({
        {"Null",
         [](ValueFactory& value_factory) -> Persistent<const Value> {
           return value_factory.GetNullValue();
         }},
        {"Bool",
         [](ValueFactory& value_factory) -> Persistent<const Value> {
           return value_factory.CreateBoolValue(false);
         }},
        {"Int",
         [](ValueFactory& value_factory) -> Persistent<const Value> {
           return value_factory.CreateIntValue(0);
         }},
        {"Uint",
         [](ValueFactory& value_factory) -> Persistent<const Value> {
           return value_factory.CreateUintValue(0);
         }},
        {"Double",
         [](ValueFactory& value_factory) -> Persistent<const Value> {
           return value_factory.CreateDoubleValue(0.0);
         }},
        {"Duration",
         [](ValueFactory& value_factory) -> Persistent<const Value> {
           return Must(value_factory.CreateDurationValue(absl::ZeroDuration()));
         }},
        {"Timestamp",
         [](ValueFactory& value_factory) -> Persistent<const Value> {
           return Must(value_factory.CreateTimestampValue(absl::UnixEpoch()));
         }},
        {"Error",
         [](ValueFactory& value_factory) -> Persistent<const Value> {
           return value_factory.CreateErrorValue(absl::CancelledError());
         }},
        {"Bytes",
         [](ValueFactory& value_factory) -> Persistent<const Value> {
           return Must(value_factory.CreateBytesValue(0));
         }},
    }),
    [](const testing::TestParamInfo<ConstructionAssignmentTestCase>& info) {
      return info.param.name;
    });

TEST(Value, Swap) {
  TestValueFactory value_factory;
  Persistent<const Value> lhs = value_factory.CreateIntValue(0);
  Persistent<const Value> rhs = value_factory.CreateUintValue(0);
  std::swap(lhs, rhs);
  EXPECT_EQ(lhs, value_factory.CreateUintValue(0));
  EXPECT_EQ(rhs, value_factory.CreateIntValue(0));
}

TEST(NullValue, DebugString) {
  TestValueFactory value_factory;
  EXPECT_EQ(value_factory.GetNullValue()->DebugString(), "null");
}

TEST(BoolValue, DebugString) {
  TestValueFactory value_factory;
  EXPECT_EQ(value_factory.CreateBoolValue(false)->DebugString(), "false");
  EXPECT_EQ(value_factory.CreateBoolValue(true)->DebugString(), "true");
}

TEST(IntValue, DebugString) {
  TestValueFactory value_factory;
  EXPECT_EQ(value_factory.CreateIntValue(-1)->DebugString(), "-1");
  EXPECT_EQ(value_factory.CreateIntValue(0)->DebugString(), "0");
  EXPECT_EQ(value_factory.CreateIntValue(1)->DebugString(), "1");
  EXPECT_EQ(value_factory.CreateIntValue(std::numeric_limits<int64_t>::min())
                ->DebugString(),
            "-9223372036854775808");
  EXPECT_EQ(value_factory.CreateIntValue(std::numeric_limits<int64_t>::max())
                ->DebugString(),
            "9223372036854775807");
}

TEST(UintValue, DebugString) {
  TestValueFactory value_factory;
  EXPECT_EQ(value_factory.CreateUintValue(0)->DebugString(), "0u");
  EXPECT_EQ(value_factory.CreateUintValue(1)->DebugString(), "1u");
  EXPECT_EQ(value_factory.CreateUintValue(std::numeric_limits<uint64_t>::max())
                ->DebugString(),
            "18446744073709551615u");
}

TEST(DoubleValue, DebugString) {
  TestValueFactory value_factory;
  EXPECT_EQ(value_factory.CreateDoubleValue(-1.0)->DebugString(), "-1.0");
  EXPECT_EQ(value_factory.CreateDoubleValue(0.0)->DebugString(), "0.0");
  EXPECT_EQ(value_factory.CreateDoubleValue(1.0)->DebugString(), "1.0");
  EXPECT_EQ(value_factory.CreateDoubleValue(-1.1)->DebugString(), "-1.1");
  EXPECT_EQ(value_factory.CreateDoubleValue(0.1)->DebugString(), "0.1");
  EXPECT_EQ(value_factory.CreateDoubleValue(1.1)->DebugString(), "1.1");
  EXPECT_EQ(value_factory.CreateDoubleValue(-9007199254740991.0)->DebugString(),
            "-9.0072e+15");
  EXPECT_EQ(value_factory.CreateDoubleValue(9007199254740991.0)->DebugString(),
            "9.0072e+15");
  EXPECT_EQ(value_factory.CreateDoubleValue(-9007199254740991.1)->DebugString(),
            "-9.0072e+15");
  EXPECT_EQ(value_factory.CreateDoubleValue(9007199254740991.1)->DebugString(),
            "9.0072e+15");
  EXPECT_EQ(value_factory.CreateDoubleValue(9007199254740991.1)->DebugString(),
            "9.0072e+15");

  EXPECT_EQ(
      value_factory.CreateDoubleValue(std::numeric_limits<double>::quiet_NaN())
          ->DebugString(),
      "nan");
  EXPECT_EQ(
      value_factory.CreateDoubleValue(std::numeric_limits<double>::infinity())
          ->DebugString(),
      "+infinity");
  EXPECT_EQ(
      value_factory.CreateDoubleValue(-std::numeric_limits<double>::infinity())
          ->DebugString(),
      "-infinity");
}

TEST(DurationValue, DebugString) {
  TestValueFactory value_factory;
  EXPECT_EQ(DurationValue::Zero(value_factory)->DebugString(),
            internal::FormatDuration(absl::ZeroDuration()).value());
}

TEST(TimestampValue, DebugString) {
  TestValueFactory value_factory;
  EXPECT_EQ(TimestampValue::UnixEpoch(value_factory)->DebugString(),
            internal::FormatTimestamp(absl::UnixEpoch()).value());
}

// The below tests could be made parameterized but doing so requires the
// extension for struct member initiation by name for it to be worth it. That
// feature is not available in C++17.

TEST(Value, Error) {
  TestValueFactory value_factory;
  TestTypeFactory type_factory;
  auto error_value = value_factory.CreateErrorValue(absl::CancelledError());
  EXPECT_TRUE(error_value.Is<ErrorValue>());
  EXPECT_FALSE(error_value.Is<NullValue>());
  EXPECT_EQ(error_value, error_value);
  EXPECT_EQ(error_value,
            value_factory.CreateErrorValue(absl::CancelledError()));
  EXPECT_EQ(error_value->value(), absl::CancelledError());
}

TEST(Value, Bool) {
  TestValueFactory value_factory;
  TestTypeFactory type_factory;
  auto false_value = BoolValue::False(value_factory);
  EXPECT_TRUE(false_value.Is<BoolValue>());
  EXPECT_FALSE(false_value.Is<NullValue>());
  EXPECT_EQ(false_value, false_value);
  EXPECT_EQ(false_value, value_factory.CreateBoolValue(false));
  EXPECT_EQ(false_value->kind(), Kind::kBool);
  EXPECT_EQ(false_value->type(), type_factory.GetBoolType());
  EXPECT_FALSE(false_value->value());

  auto true_value = BoolValue::True(value_factory);
  EXPECT_TRUE(true_value.Is<BoolValue>());
  EXPECT_FALSE(true_value.Is<NullValue>());
  EXPECT_EQ(true_value, true_value);
  EXPECT_EQ(true_value, value_factory.CreateBoolValue(true));
  EXPECT_EQ(true_value->kind(), Kind::kBool);
  EXPECT_EQ(true_value->type(), type_factory.GetBoolType());
  EXPECT_TRUE(true_value->value());

  EXPECT_NE(false_value, true_value);
  EXPECT_NE(true_value, false_value);
}

TEST(Value, Int) {
  TestValueFactory value_factory;
  TestTypeFactory type_factory;
  auto zero_value = value_factory.CreateIntValue(0);
  EXPECT_TRUE(zero_value.Is<IntValue>());
  EXPECT_FALSE(zero_value.Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, value_factory.CreateIntValue(0));
  EXPECT_EQ(zero_value->kind(), Kind::kInt);
  EXPECT_EQ(zero_value->type(), type_factory.GetIntType());
  EXPECT_EQ(zero_value->value(), 0);

  auto one_value = value_factory.CreateIntValue(1);
  EXPECT_TRUE(one_value.Is<IntValue>());
  EXPECT_FALSE(one_value.Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, value_factory.CreateIntValue(1));
  EXPECT_EQ(one_value->kind(), Kind::kInt);
  EXPECT_EQ(one_value->type(), type_factory.GetIntType());
  EXPECT_EQ(one_value->value(), 1);

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST(Value, Uint) {
  TestValueFactory value_factory;
  TestTypeFactory type_factory;
  auto zero_value = value_factory.CreateUintValue(0);
  EXPECT_TRUE(zero_value.Is<UintValue>());
  EXPECT_FALSE(zero_value.Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, value_factory.CreateUintValue(0));
  EXPECT_EQ(zero_value->kind(), Kind::kUint);
  EXPECT_EQ(zero_value->type(), type_factory.GetUintType());
  EXPECT_EQ(zero_value->value(), 0);

  auto one_value = value_factory.CreateUintValue(1);
  EXPECT_TRUE(one_value.Is<UintValue>());
  EXPECT_FALSE(one_value.Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, value_factory.CreateUintValue(1));
  EXPECT_EQ(one_value->kind(), Kind::kUint);
  EXPECT_EQ(one_value->type(), type_factory.GetUintType());
  EXPECT_EQ(one_value->value(), 1);

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST(Value, Double) {
  TestValueFactory value_factory;
  TestTypeFactory type_factory;
  auto zero_value = value_factory.CreateDoubleValue(0.0);
  EXPECT_TRUE(zero_value.Is<DoubleValue>());
  EXPECT_FALSE(zero_value.Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, value_factory.CreateDoubleValue(0.0));
  EXPECT_EQ(zero_value->kind(), Kind::kDouble);
  EXPECT_EQ(zero_value->type(), type_factory.GetDoubleType());
  EXPECT_EQ(zero_value->value(), 0.0);

  auto one_value = value_factory.CreateDoubleValue(1.0);
  EXPECT_TRUE(one_value.Is<DoubleValue>());
  EXPECT_FALSE(one_value.Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, value_factory.CreateDoubleValue(1.0));
  EXPECT_EQ(one_value->kind(), Kind::kDouble);
  EXPECT_EQ(one_value->type(), type_factory.GetDoubleType());
  EXPECT_EQ(one_value->value(), 1.0);

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST(Value, Duration) {
  TestValueFactory value_factory;
  TestTypeFactory type_factory;
  auto zero_value =
      Must(value_factory.CreateDurationValue(absl::ZeroDuration()));
  EXPECT_TRUE(zero_value.Is<DurationValue>());
  EXPECT_FALSE(zero_value.Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value,
            Must(value_factory.CreateDurationValue(absl::ZeroDuration())));
  EXPECT_EQ(zero_value->kind(), Kind::kDuration);
  EXPECT_EQ(zero_value->type(), type_factory.GetDurationType());
  EXPECT_EQ(zero_value->value(), absl::ZeroDuration());

  auto one_value = Must(value_factory.CreateDurationValue(
      absl::ZeroDuration() + absl::Nanoseconds(1)));
  EXPECT_TRUE(one_value.Is<DurationValue>());
  EXPECT_FALSE(one_value.Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value->kind(), Kind::kDuration);
  EXPECT_EQ(one_value->type(), type_factory.GetDurationType());
  EXPECT_EQ(one_value->value(), absl::ZeroDuration() + absl::Nanoseconds(1));

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);

  EXPECT_THAT(value_factory.CreateDurationValue(absl::InfiniteDuration()),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(Value, Timestamp) {
  TestValueFactory value_factory;
  TestTypeFactory type_factory;
  auto zero_value = Must(value_factory.CreateTimestampValue(absl::UnixEpoch()));
  EXPECT_TRUE(zero_value.Is<TimestampValue>());
  EXPECT_FALSE(zero_value.Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value,
            Must(value_factory.CreateTimestampValue(absl::UnixEpoch())));
  EXPECT_EQ(zero_value->kind(), Kind::kTimestamp);
  EXPECT_EQ(zero_value->type(), type_factory.GetTimestampType());
  EXPECT_EQ(zero_value->value(), absl::UnixEpoch());

  auto one_value = Must(value_factory.CreateTimestampValue(
      absl::UnixEpoch() + absl::Nanoseconds(1)));
  EXPECT_TRUE(one_value.Is<TimestampValue>());
  EXPECT_FALSE(one_value.Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value->kind(), Kind::kTimestamp);
  EXPECT_EQ(one_value->type(), type_factory.GetTimestampType());
  EXPECT_EQ(one_value->value(), absl::UnixEpoch() + absl::Nanoseconds(1));

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);

  EXPECT_THAT(value_factory.CreateTimestampValue(absl::InfiniteFuture()),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(Value, BytesFromString) {
  TestValueFactory value_factory;
  TestTypeFactory type_factory;
  auto zero_value = Must(value_factory.CreateBytesValue(std::string("0")));
  EXPECT_TRUE(zero_value.Is<BytesValue>());
  EXPECT_FALSE(zero_value.Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, Must(value_factory.CreateBytesValue(std::string("0"))));
  EXPECT_EQ(zero_value->kind(), Kind::kBytes);
  EXPECT_EQ(zero_value->type(), type_factory.GetBytesType());
  EXPECT_EQ(zero_value->ToString(), "0");

  auto one_value = Must(value_factory.CreateBytesValue(std::string("1")));
  EXPECT_TRUE(one_value.Is<BytesValue>());
  EXPECT_FALSE(one_value.Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, Must(value_factory.CreateBytesValue(std::string("1"))));
  EXPECT_EQ(one_value->kind(), Kind::kBytes);
  EXPECT_EQ(one_value->type(), type_factory.GetBytesType());
  EXPECT_EQ(one_value->ToString(), "1");

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST(Value, BytesFromStringView) {
  TestValueFactory value_factory;
  TestTypeFactory type_factory;
  auto zero_value =
      Must(value_factory.CreateBytesValue(absl::string_view("0")));
  EXPECT_TRUE(zero_value.Is<BytesValue>());
  EXPECT_FALSE(zero_value.Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value,
            Must(value_factory.CreateBytesValue(absl::string_view("0"))));
  EXPECT_EQ(zero_value->kind(), Kind::kBytes);
  EXPECT_EQ(zero_value->type(), type_factory.GetBytesType());
  EXPECT_EQ(zero_value->ToString(), "0");

  auto one_value = Must(value_factory.CreateBytesValue(absl::string_view("1")));
  EXPECT_TRUE(one_value.Is<BytesValue>());
  EXPECT_FALSE(one_value.Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value,
            Must(value_factory.CreateBytesValue(absl::string_view("1"))));
  EXPECT_EQ(one_value->kind(), Kind::kBytes);
  EXPECT_EQ(one_value->type(), type_factory.GetBytesType());
  EXPECT_EQ(one_value->ToString(), "1");

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST(Value, BytesFromCord) {
  TestValueFactory value_factory;
  TestTypeFactory type_factory;
  auto zero_value = Must(value_factory.CreateBytesValue(absl::Cord("0")));
  EXPECT_TRUE(zero_value.Is<BytesValue>());
  EXPECT_FALSE(zero_value.Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, Must(value_factory.CreateBytesValue(absl::Cord("0"))));
  EXPECT_EQ(zero_value->kind(), Kind::kBytes);
  EXPECT_EQ(zero_value->type(), type_factory.GetBytesType());
  EXPECT_EQ(zero_value->ToCord(), "0");

  auto one_value = Must(value_factory.CreateBytesValue(absl::Cord("1")));
  EXPECT_TRUE(one_value.Is<BytesValue>());
  EXPECT_FALSE(one_value.Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, Must(value_factory.CreateBytesValue(absl::Cord("1"))));
  EXPECT_EQ(one_value->kind(), Kind::kBytes);
  EXPECT_EQ(one_value->type(), type_factory.GetBytesType());
  EXPECT_EQ(one_value->ToCord(), "1");

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST(Value, BytesFromLiteral) {
  TestValueFactory value_factory;
  TestTypeFactory type_factory;
  auto zero_value = Must(value_factory.CreateBytesValue("0"));
  EXPECT_TRUE(zero_value.Is<BytesValue>());
  EXPECT_FALSE(zero_value.Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, Must(value_factory.CreateBytesValue("0")));
  EXPECT_EQ(zero_value->kind(), Kind::kBytes);
  EXPECT_EQ(zero_value->type(), type_factory.GetBytesType());
  EXPECT_EQ(zero_value->ToString(), "0");

  auto one_value = Must(value_factory.CreateBytesValue("1"));
  EXPECT_TRUE(one_value.Is<BytesValue>());
  EXPECT_FALSE(one_value.Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, Must(value_factory.CreateBytesValue("1")));
  EXPECT_EQ(one_value->kind(), Kind::kBytes);
  EXPECT_EQ(one_value->type(), type_factory.GetBytesType());
  EXPECT_EQ(one_value->ToString(), "1");

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST(Value, BytesFromExternal) {
  TestValueFactory value_factory;
  TestTypeFactory type_factory;
  auto zero_value = Must(value_factory.CreateBytesValue("0", []() {}));
  EXPECT_TRUE(zero_value.Is<BytesValue>());
  EXPECT_FALSE(zero_value.Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, Must(value_factory.CreateBytesValue("0", []() {})));
  EXPECT_EQ(zero_value->kind(), Kind::kBytes);
  EXPECT_EQ(zero_value->type(), type_factory.GetBytesType());
  EXPECT_EQ(zero_value->ToString(), "0");

  auto one_value = Must(value_factory.CreateBytesValue("1", []() {}));
  EXPECT_TRUE(one_value.Is<BytesValue>());
  EXPECT_FALSE(one_value.Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, Must(value_factory.CreateBytesValue("1", []() {})));
  EXPECT_EQ(one_value->kind(), Kind::kBytes);
  EXPECT_EQ(one_value->type(), type_factory.GetBytesType());
  EXPECT_EQ(one_value->ToString(), "1");

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST(Value, StringFromString) {
  TestValueFactory value_factory;
  TestTypeFactory type_factory;
  auto zero_value = Must(value_factory.CreateStringValue(std::string("0")));
  EXPECT_TRUE(zero_value.Is<StringValue>());
  EXPECT_FALSE(zero_value.Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value,
            Must(value_factory.CreateStringValue(std::string("0"))));
  EXPECT_EQ(zero_value->kind(), Kind::kString);
  EXPECT_EQ(zero_value->type(), type_factory.GetStringType());
  EXPECT_EQ(zero_value->ToString(), "0");

  auto one_value = Must(value_factory.CreateStringValue(std::string("1")));
  EXPECT_TRUE(one_value.Is<StringValue>());
  EXPECT_FALSE(one_value.Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, Must(value_factory.CreateStringValue(std::string("1"))));
  EXPECT_EQ(one_value->kind(), Kind::kString);
  EXPECT_EQ(one_value->type(), type_factory.GetStringType());
  EXPECT_EQ(one_value->ToString(), "1");

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST(Value, StringFromStringView) {
  TestValueFactory value_factory;
  TestTypeFactory type_factory;
  auto zero_value =
      Must(value_factory.CreateStringValue(absl::string_view("0")));
  EXPECT_TRUE(zero_value.Is<StringValue>());
  EXPECT_FALSE(zero_value.Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value,
            Must(value_factory.CreateStringValue(absl::string_view("0"))));
  EXPECT_EQ(zero_value->kind(), Kind::kString);
  EXPECT_EQ(zero_value->type(), type_factory.GetStringType());
  EXPECT_EQ(zero_value->ToString(), "0");

  auto one_value =
      Must(value_factory.CreateStringValue(absl::string_view("1")));
  EXPECT_TRUE(one_value.Is<StringValue>());
  EXPECT_FALSE(one_value.Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value,
            Must(value_factory.CreateStringValue(absl::string_view("1"))));
  EXPECT_EQ(one_value->kind(), Kind::kString);
  EXPECT_EQ(one_value->type(), type_factory.GetStringType());
  EXPECT_EQ(one_value->ToString(), "1");

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST(Value, StringFromCord) {
  TestValueFactory value_factory;
  TestTypeFactory type_factory;
  auto zero_value = Must(value_factory.CreateStringValue(absl::Cord("0")));
  EXPECT_TRUE(zero_value.Is<StringValue>());
  EXPECT_FALSE(zero_value.Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, Must(value_factory.CreateStringValue(absl::Cord("0"))));
  EXPECT_EQ(zero_value->kind(), Kind::kString);
  EXPECT_EQ(zero_value->type(), type_factory.GetStringType());
  EXPECT_EQ(zero_value->ToCord(), "0");

  auto one_value = Must(value_factory.CreateStringValue(absl::Cord("1")));
  EXPECT_TRUE(one_value.Is<StringValue>());
  EXPECT_FALSE(one_value.Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, Must(value_factory.CreateStringValue(absl::Cord("1"))));
  EXPECT_EQ(one_value->kind(), Kind::kString);
  EXPECT_EQ(one_value->type(), type_factory.GetStringType());
  EXPECT_EQ(one_value->ToCord(), "1");

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST(Value, StringFromLiteral) {
  TestValueFactory value_factory;
  TestTypeFactory type_factory;
  auto zero_value = Must(value_factory.CreateStringValue("0"));
  EXPECT_TRUE(zero_value.Is<StringValue>());
  EXPECT_FALSE(zero_value.Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, Must(value_factory.CreateStringValue("0")));
  EXPECT_EQ(zero_value->kind(), Kind::kString);
  EXPECT_EQ(zero_value->type(), type_factory.GetStringType());
  EXPECT_EQ(zero_value->ToString(), "0");

  auto one_value = Must(value_factory.CreateStringValue("1"));
  EXPECT_TRUE(one_value.Is<StringValue>());
  EXPECT_FALSE(one_value.Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, Must(value_factory.CreateStringValue("1")));
  EXPECT_EQ(one_value->kind(), Kind::kString);
  EXPECT_EQ(one_value->type(), type_factory.GetStringType());
  EXPECT_EQ(one_value->ToString(), "1");

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST(Value, StringFromExternal) {
  TestValueFactory value_factory;
  TestTypeFactory type_factory;
  auto zero_value = Must(value_factory.CreateStringValue("0", []() {}));
  EXPECT_TRUE(zero_value.Is<StringValue>());
  EXPECT_FALSE(zero_value.Is<NullValue>());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, Must(value_factory.CreateStringValue("0", []() {})));
  EXPECT_EQ(zero_value->kind(), Kind::kString);
  EXPECT_EQ(zero_value->type(), type_factory.GetStringType());
  EXPECT_EQ(zero_value->ToString(), "0");

  auto one_value = Must(value_factory.CreateStringValue("1", []() {}));
  EXPECT_TRUE(one_value.Is<StringValue>());
  EXPECT_FALSE(one_value.Is<NullValue>());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, Must(value_factory.CreateStringValue("1", []() {})));
  EXPECT_EQ(one_value->kind(), Kind::kString);
  EXPECT_EQ(one_value->type(), type_factory.GetStringType());
  EXPECT_EQ(one_value->ToString(), "1");

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

Persistent<const BytesValue> MakeStringBytes(ValueFactory& value_factory,
                                             absl::string_view value) {
  return Must(value_factory.CreateBytesValue(value));
}

Persistent<const BytesValue> MakeCordBytes(ValueFactory& value_factory,
                                           absl::string_view value) {
  return Must(value_factory.CreateBytesValue(absl::Cord(value)));
}

Persistent<const BytesValue> MakeExternalBytes(ValueFactory& value_factory,
                                               absl::string_view value) {
  return Must(value_factory.CreateBytesValue(value, []() {}));
}

struct BytesConcatTestCase final {
  std::string lhs;
  std::string rhs;
};

using BytesConcatTest = testing::TestWithParam<BytesConcatTestCase>;

TEST_P(BytesConcatTest, Concat) {
  const BytesConcatTestCase& test_case = GetParam();
  TestValueFactory value_factory;
  EXPECT_TRUE(
      Must(BytesValue::Concat(value_factory,
                              MakeStringBytes(value_factory, test_case.lhs),
                              MakeStringBytes(value_factory, test_case.rhs)))
          ->Equals(test_case.lhs + test_case.rhs));
  EXPECT_TRUE(
      Must(BytesValue::Concat(value_factory,
                              MakeStringBytes(value_factory, test_case.lhs),
                              MakeCordBytes(value_factory, test_case.rhs)))
          ->Equals(test_case.lhs + test_case.rhs));
  EXPECT_TRUE(
      Must(BytesValue::Concat(value_factory,
                              MakeStringBytes(value_factory, test_case.lhs),
                              MakeExternalBytes(value_factory, test_case.rhs)))
          ->Equals(test_case.lhs + test_case.rhs));
  EXPECT_TRUE(
      Must(BytesValue::Concat(value_factory,
                              MakeCordBytes(value_factory, test_case.lhs),
                              MakeStringBytes(value_factory, test_case.rhs)))
          ->Equals(test_case.lhs + test_case.rhs));
  EXPECT_TRUE(
      Must(BytesValue::Concat(value_factory,
                              MakeCordBytes(value_factory, test_case.lhs),
                              MakeCordBytes(value_factory, test_case.rhs)))
          ->Equals(test_case.lhs + test_case.rhs));
  EXPECT_TRUE(
      Must(BytesValue::Concat(value_factory,
                              MakeCordBytes(value_factory, test_case.lhs),
                              MakeExternalBytes(value_factory, test_case.rhs)))
          ->Equals(test_case.lhs + test_case.rhs));
  EXPECT_TRUE(
      Must(BytesValue::Concat(value_factory,
                              MakeExternalBytes(value_factory, test_case.lhs),
                              MakeStringBytes(value_factory, test_case.rhs)))
          ->Equals(test_case.lhs + test_case.rhs));
  EXPECT_TRUE(
      Must(BytesValue::Concat(value_factory,
                              MakeExternalBytes(value_factory, test_case.lhs),
                              MakeCordBytes(value_factory, test_case.rhs)))
          ->Equals(test_case.lhs + test_case.rhs));
  EXPECT_TRUE(
      Must(BytesValue::Concat(value_factory,
                              MakeExternalBytes(value_factory, test_case.lhs),
                              MakeExternalBytes(value_factory, test_case.rhs)))
          ->Equals(test_case.lhs + test_case.rhs));
}

INSTANTIATE_TEST_SUITE_P(BytesConcatTest, BytesConcatTest,
                         testing::ValuesIn<BytesConcatTestCase>({
                             {"", ""},
                             {"", std::string("\0", 1)},
                             {std::string("\0", 1), ""},
                             {std::string("\0", 1), std::string("\0", 1)},
                             {"", "foo"},
                             {"foo", ""},
                             {"foo", "foo"},
                             {"bar", "foo"},
                             {"foo", "bar"},
                             {"bar", "bar"},
                         }));

struct BytesSizeTestCase final {
  std::string data;
  size_t size;
};

using BytesSizeTest = testing::TestWithParam<BytesSizeTestCase>;

TEST_P(BytesSizeTest, Size) {
  const BytesSizeTestCase& test_case = GetParam();
  TestValueFactory value_factory;
  EXPECT_EQ(MakeStringBytes(value_factory, test_case.data)->size(),
            test_case.size);
  EXPECT_EQ(MakeCordBytes(value_factory, test_case.data)->size(),
            test_case.size);
  EXPECT_EQ(MakeExternalBytes(value_factory, test_case.data)->size(),
            test_case.size);
}

INSTANTIATE_TEST_SUITE_P(BytesSizeTest, BytesSizeTest,
                         testing::ValuesIn<BytesSizeTestCase>({
                             {"", 0},
                             {"1", 1},
                             {"foo", 3},
                             {"\xef\xbf\xbd", 3},
                         }));

struct BytesEmptyTestCase final {
  std::string data;
  bool empty;
};

using BytesEmptyTest = testing::TestWithParam<BytesEmptyTestCase>;

TEST_P(BytesEmptyTest, Empty) {
  const BytesEmptyTestCase& test_case = GetParam();
  TestValueFactory value_factory;
  EXPECT_EQ(MakeStringBytes(value_factory, test_case.data)->empty(),
            test_case.empty);
  EXPECT_EQ(MakeCordBytes(value_factory, test_case.data)->empty(),
            test_case.empty);
  EXPECT_EQ(MakeExternalBytes(value_factory, test_case.data)->empty(),
            test_case.empty);
}

INSTANTIATE_TEST_SUITE_P(BytesEmptyTest, BytesEmptyTest,
                         testing::ValuesIn<BytesEmptyTestCase>({
                             {"", true},
                             {std::string("\0", 1), false},
                             {"1", false},
                         }));

struct BytesEqualsTestCase final {
  std::string lhs;
  std::string rhs;
  bool equals;
};

using BytesEqualsTest = testing::TestWithParam<BytesEqualsTestCase>;

TEST_P(BytesEqualsTest, Equals) {
  const BytesEqualsTestCase& test_case = GetParam();
  TestValueFactory value_factory;
  EXPECT_EQ(MakeStringBytes(value_factory, test_case.lhs)
                ->Equals(MakeStringBytes(value_factory, test_case.rhs)),
            test_case.equals);
  EXPECT_EQ(MakeStringBytes(value_factory, test_case.lhs)
                ->Equals(MakeCordBytes(value_factory, test_case.rhs)),
            test_case.equals);
  EXPECT_EQ(MakeStringBytes(value_factory, test_case.lhs)
                ->Equals(MakeExternalBytes(value_factory, test_case.rhs)),
            test_case.equals);
  EXPECT_EQ(MakeCordBytes(value_factory, test_case.lhs)
                ->Equals(MakeStringBytes(value_factory, test_case.rhs)),
            test_case.equals);
  EXPECT_EQ(MakeCordBytes(value_factory, test_case.lhs)
                ->Equals(MakeCordBytes(value_factory, test_case.rhs)),
            test_case.equals);
  EXPECT_EQ(MakeCordBytes(value_factory, test_case.lhs)
                ->Equals(MakeExternalBytes(value_factory, test_case.rhs)),
            test_case.equals);
  EXPECT_EQ(MakeExternalBytes(value_factory, test_case.lhs)
                ->Equals(MakeStringBytes(value_factory, test_case.rhs)),
            test_case.equals);
  EXPECT_EQ(MakeExternalBytes(value_factory, test_case.lhs)
                ->Equals(MakeCordBytes(value_factory, test_case.rhs)),
            test_case.equals);
  EXPECT_EQ(MakeExternalBytes(value_factory, test_case.lhs)
                ->Equals(MakeExternalBytes(value_factory, test_case.rhs)),
            test_case.equals);
}

INSTANTIATE_TEST_SUITE_P(BytesEqualsTest, BytesEqualsTest,
                         testing::ValuesIn<BytesEqualsTestCase>({
                             {"", "", true},
                             {"", std::string("\0", 1), false},
                             {std::string("\0", 1), "", false},
                             {std::string("\0", 1), std::string("\0", 1), true},
                             {"", "foo", false},
                             {"foo", "", false},
                             {"foo", "foo", true},
                             {"bar", "foo", false},
                             {"foo", "bar", false},
                             {"bar", "bar", true},
                         }));

struct BytesCompareTestCase final {
  std::string lhs;
  std::string rhs;
  int compare;
};

using BytesCompareTest = testing::TestWithParam<BytesCompareTestCase>;

int NormalizeCompareResult(int compare) { return std::clamp(compare, -1, 1); }

TEST_P(BytesCompareTest, Equals) {
  const BytesCompareTestCase& test_case = GetParam();
  TestValueFactory value_factory;
  EXPECT_EQ(NormalizeCompareResult(
                MakeStringBytes(value_factory, test_case.lhs)
                    ->Compare(MakeStringBytes(value_factory, test_case.rhs))),
            test_case.compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeStringBytes(value_factory, test_case.lhs)
                    ->Compare(MakeCordBytes(value_factory, test_case.rhs))),
            test_case.compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeStringBytes(value_factory, test_case.lhs)
                    ->Compare(MakeExternalBytes(value_factory, test_case.rhs))),
            test_case.compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeCordBytes(value_factory, test_case.lhs)
                    ->Compare(MakeStringBytes(value_factory, test_case.rhs))),
            test_case.compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeCordBytes(value_factory, test_case.lhs)
                    ->Compare(MakeCordBytes(value_factory, test_case.rhs))),
            test_case.compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeCordBytes(value_factory, test_case.lhs)
                    ->Compare(MakeExternalBytes(value_factory, test_case.rhs))),
            test_case.compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeExternalBytes(value_factory, test_case.lhs)
                    ->Compare(MakeStringBytes(value_factory, test_case.rhs))),
            test_case.compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeExternalBytes(value_factory, test_case.lhs)
                    ->Compare(MakeCordBytes(value_factory, test_case.rhs))),
            test_case.compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeExternalBytes(value_factory, test_case.lhs)
                    ->Compare(MakeExternalBytes(value_factory, test_case.rhs))),
            test_case.compare);
}

INSTANTIATE_TEST_SUITE_P(BytesCompareTest, BytesCompareTest,
                         testing::ValuesIn<BytesCompareTestCase>({
                             {"", "", 0},
                             {"", std::string("\0", 1), -1},
                             {std::string("\0", 1), "", 1},
                             {std::string("\0", 1), std::string("\0", 1), 0},
                             {"", "foo", -1},
                             {"foo", "", 1},
                             {"foo", "foo", 0},
                             {"bar", "foo", -1},
                             {"foo", "bar", 1},
                             {"bar", "bar", 0},
                         }));

struct BytesDebugStringTestCase final {
  std::string data;
};

using BytesDebugStringTest = testing::TestWithParam<BytesDebugStringTestCase>;

TEST_P(BytesDebugStringTest, ToCord) {
  const BytesDebugStringTestCase& test_case = GetParam();
  TestValueFactory value_factory;
  EXPECT_EQ(MakeStringBytes(value_factory, test_case.data)->DebugString(),
            internal::FormatBytesLiteral(test_case.data));
  EXPECT_EQ(MakeCordBytes(value_factory, test_case.data)->DebugString(),
            internal::FormatBytesLiteral(test_case.data));
  EXPECT_EQ(MakeExternalBytes(value_factory, test_case.data)->DebugString(),
            internal::FormatBytesLiteral(test_case.data));
}

INSTANTIATE_TEST_SUITE_P(BytesDebugStringTest, BytesDebugStringTest,
                         testing::ValuesIn<BytesDebugStringTestCase>({
                             {""},
                             {"1"},
                             {"foo"},
                             {"\xef\xbf\xbd"},
                         }));

struct BytesToStringTestCase final {
  std::string data;
};

using BytesToStringTest = testing::TestWithParam<BytesToStringTestCase>;

TEST_P(BytesToStringTest, ToString) {
  const BytesToStringTestCase& test_case = GetParam();
  TestValueFactory value_factory;
  EXPECT_EQ(MakeStringBytes(value_factory, test_case.data)->ToString(),
            test_case.data);
  EXPECT_EQ(MakeCordBytes(value_factory, test_case.data)->ToString(),
            test_case.data);
  EXPECT_EQ(MakeExternalBytes(value_factory, test_case.data)->ToString(),
            test_case.data);
}

INSTANTIATE_TEST_SUITE_P(BytesToStringTest, BytesToStringTest,
                         testing::ValuesIn<BytesToStringTestCase>({
                             {""},
                             {"1"},
                             {"foo"},
                             {"\xef\xbf\xbd"},
                         }));

struct BytesToCordTestCase final {
  std::string data;
};

using BytesToCordTest = testing::TestWithParam<BytesToCordTestCase>;

TEST_P(BytesToCordTest, ToCord) {
  const BytesToCordTestCase& test_case = GetParam();
  TestValueFactory value_factory;
  EXPECT_EQ(MakeStringBytes(value_factory, test_case.data)->ToCord(),
            test_case.data);
  EXPECT_EQ(MakeCordBytes(value_factory, test_case.data)->ToCord(),
            test_case.data);
  EXPECT_EQ(MakeExternalBytes(value_factory, test_case.data)->ToCord(),
            test_case.data);
}

INSTANTIATE_TEST_SUITE_P(BytesToCordTest, BytesToCordTest,
                         testing::ValuesIn<BytesToCordTestCase>({
                             {""},
                             {"1"},
                             {"foo"},
                             {"\xef\xbf\xbd"},
                         }));

Persistent<const StringValue> MakeStringString(ValueFactory& value_factory,
                                               absl::string_view value) {
  return Must(value_factory.CreateStringValue(value));
}

Persistent<const StringValue> MakeCordString(ValueFactory& value_factory,
                                             absl::string_view value) {
  return Must(value_factory.CreateStringValue(absl::Cord(value)));
}

Persistent<const StringValue> MakeExternalString(ValueFactory& value_factory,
                                                 absl::string_view value) {
  return Must(value_factory.CreateStringValue(value, []() {}));
}

struct StringConcatTestCase final {
  std::string lhs;
  std::string rhs;
};

using StringConcatTest = testing::TestWithParam<StringConcatTestCase>;

TEST_P(StringConcatTest, Concat) {
  const StringConcatTestCase& test_case = GetParam();
  TestValueFactory value_factory;
  EXPECT_TRUE(
      Must(StringValue::Concat(value_factory,
                               MakeStringString(value_factory, test_case.lhs),
                               MakeStringString(value_factory, test_case.rhs)))
          ->Equals(test_case.lhs + test_case.rhs));
  EXPECT_TRUE(
      Must(StringValue::Concat(value_factory,
                               MakeStringString(value_factory, test_case.lhs),
                               MakeCordString(value_factory, test_case.rhs)))
          ->Equals(test_case.lhs + test_case.rhs));
  EXPECT_TRUE(
      Must(StringValue::Concat(
               value_factory, MakeStringString(value_factory, test_case.lhs),
               MakeExternalString(value_factory, test_case.rhs)))
          ->Equals(test_case.lhs + test_case.rhs));
  EXPECT_TRUE(
      Must(StringValue::Concat(value_factory,
                               MakeCordString(value_factory, test_case.lhs),
                               MakeStringString(value_factory, test_case.rhs)))
          ->Equals(test_case.lhs + test_case.rhs));
  EXPECT_TRUE(
      Must(StringValue::Concat(value_factory,
                               MakeCordString(value_factory, test_case.lhs),
                               MakeCordString(value_factory, test_case.rhs)))
          ->Equals(test_case.lhs + test_case.rhs));
  EXPECT_TRUE(
      Must(StringValue::Concat(
               value_factory, MakeCordString(value_factory, test_case.lhs),
               MakeExternalString(value_factory, test_case.rhs)))
          ->Equals(test_case.lhs + test_case.rhs));
  EXPECT_TRUE(
      Must(StringValue::Concat(value_factory,
                               MakeExternalString(value_factory, test_case.lhs),
                               MakeStringString(value_factory, test_case.rhs)))
          ->Equals(test_case.lhs + test_case.rhs));
  EXPECT_TRUE(
      Must(StringValue::Concat(value_factory,
                               MakeExternalString(value_factory, test_case.lhs),
                               MakeCordString(value_factory, test_case.rhs)))
          ->Equals(test_case.lhs + test_case.rhs));
  EXPECT_TRUE(
      Must(StringValue::Concat(
               value_factory, MakeExternalString(value_factory, test_case.lhs),
               MakeExternalString(value_factory, test_case.rhs)))
          ->Equals(test_case.lhs + test_case.rhs));
}

INSTANTIATE_TEST_SUITE_P(StringConcatTest, StringConcatTest,
                         testing::ValuesIn<StringConcatTestCase>({
                             {"", ""},
                             {"", std::string("\0", 1)},
                             {std::string("\0", 1), ""},
                             {std::string("\0", 1), std::string("\0", 1)},
                             {"", "foo"},
                             {"foo", ""},
                             {"foo", "foo"},
                             {"bar", "foo"},
                             {"foo", "bar"},
                             {"bar", "bar"},
                         }));

struct StringSizeTestCase final {
  std::string data;
  size_t size;
};

using StringSizeTest = testing::TestWithParam<StringSizeTestCase>;

TEST_P(StringSizeTest, Size) {
  const StringSizeTestCase& test_case = GetParam();
  TestValueFactory value_factory;
  EXPECT_EQ(MakeStringString(value_factory, test_case.data)->size(),
            test_case.size);
  EXPECT_EQ(MakeCordString(value_factory, test_case.data)->size(),
            test_case.size);
  EXPECT_EQ(MakeExternalString(value_factory, test_case.data)->size(),
            test_case.size);
}

INSTANTIATE_TEST_SUITE_P(StringSizeTest, StringSizeTest,
                         testing::ValuesIn<StringSizeTestCase>({
                             {"", 0},
                             {"1", 1},
                             {"foo", 3},
                             {"\xef\xbf\xbd", 1},
                         }));

struct StringEmptyTestCase final {
  std::string data;
  bool empty;
};

using StringEmptyTest = testing::TestWithParam<StringEmptyTestCase>;

TEST_P(StringEmptyTest, Empty) {
  const StringEmptyTestCase& test_case = GetParam();
  TestValueFactory value_factory;
  EXPECT_EQ(MakeStringString(value_factory, test_case.data)->empty(),
            test_case.empty);
  EXPECT_EQ(MakeCordString(value_factory, test_case.data)->empty(),
            test_case.empty);
  EXPECT_EQ(MakeExternalString(value_factory, test_case.data)->empty(),
            test_case.empty);
}

INSTANTIATE_TEST_SUITE_P(StringEmptyTest, StringEmptyTest,
                         testing::ValuesIn<StringEmptyTestCase>({
                             {"", true},
                             {std::string("\0", 1), false},
                             {"1", false},
                         }));

struct StringEqualsTestCase final {
  std::string lhs;
  std::string rhs;
  bool equals;
};

using StringEqualsTest = testing::TestWithParam<StringEqualsTestCase>;

TEST_P(StringEqualsTest, Equals) {
  const StringEqualsTestCase& test_case = GetParam();
  TestValueFactory value_factory;
  EXPECT_EQ(MakeStringString(value_factory, test_case.lhs)
                ->Equals(MakeStringString(value_factory, test_case.rhs)),
            test_case.equals);
  EXPECT_EQ(MakeStringString(value_factory, test_case.lhs)
                ->Equals(MakeCordString(value_factory, test_case.rhs)),
            test_case.equals);
  EXPECT_EQ(MakeStringString(value_factory, test_case.lhs)
                ->Equals(MakeExternalString(value_factory, test_case.rhs)),
            test_case.equals);
  EXPECT_EQ(MakeCordString(value_factory, test_case.lhs)
                ->Equals(MakeStringString(value_factory, test_case.rhs)),
            test_case.equals);
  EXPECT_EQ(MakeCordString(value_factory, test_case.lhs)
                ->Equals(MakeCordString(value_factory, test_case.rhs)),
            test_case.equals);
  EXPECT_EQ(MakeCordString(value_factory, test_case.lhs)
                ->Equals(MakeExternalString(value_factory, test_case.rhs)),
            test_case.equals);
  EXPECT_EQ(MakeExternalString(value_factory, test_case.lhs)
                ->Equals(MakeStringString(value_factory, test_case.rhs)),
            test_case.equals);
  EXPECT_EQ(MakeExternalString(value_factory, test_case.lhs)
                ->Equals(MakeCordString(value_factory, test_case.rhs)),
            test_case.equals);
  EXPECT_EQ(MakeExternalString(value_factory, test_case.lhs)
                ->Equals(MakeExternalString(value_factory, test_case.rhs)),
            test_case.equals);
}

INSTANTIATE_TEST_SUITE_P(StringEqualsTest, StringEqualsTest,
                         testing::ValuesIn<StringEqualsTestCase>({
                             {"", "", true},
                             {"", std::string("\0", 1), false},
                             {std::string("\0", 1), "", false},
                             {std::string("\0", 1), std::string("\0", 1), true},
                             {"", "foo", false},
                             {"foo", "", false},
                             {"foo", "foo", true},
                             {"bar", "foo", false},
                             {"foo", "bar", false},
                             {"bar", "bar", true},
                         }));

struct StringCompareTestCase final {
  std::string lhs;
  std::string rhs;
  int compare;
};

using StringCompareTest = testing::TestWithParam<StringCompareTestCase>;

TEST_P(StringCompareTest, Equals) {
  const StringCompareTestCase& test_case = GetParam();
  TestValueFactory value_factory;
  EXPECT_EQ(NormalizeCompareResult(
                MakeStringString(value_factory, test_case.lhs)
                    ->Compare(MakeStringString(value_factory, test_case.rhs))),
            test_case.compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeStringString(value_factory, test_case.lhs)
                    ->Compare(MakeCordString(value_factory, test_case.rhs))),
            test_case.compare);
  EXPECT_EQ(
      NormalizeCompareResult(
          MakeStringString(value_factory, test_case.lhs)
              ->Compare(MakeExternalString(value_factory, test_case.rhs))),
      test_case.compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeCordString(value_factory, test_case.lhs)
                    ->Compare(MakeStringString(value_factory, test_case.rhs))),
            test_case.compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeCordString(value_factory, test_case.lhs)
                    ->Compare(MakeCordString(value_factory, test_case.rhs))),
            test_case.compare);
  EXPECT_EQ(NormalizeCompareResult(MakeCordString(value_factory, test_case.lhs)
                                       ->Compare(MakeExternalString(
                                           value_factory, test_case.rhs))),
            test_case.compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeExternalString(value_factory, test_case.lhs)
                    ->Compare(MakeStringString(value_factory, test_case.rhs))),
            test_case.compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeExternalString(value_factory, test_case.lhs)
                    ->Compare(MakeCordString(value_factory, test_case.rhs))),
            test_case.compare);
  EXPECT_EQ(
      NormalizeCompareResult(
          MakeExternalString(value_factory, test_case.lhs)
              ->Compare(MakeExternalString(value_factory, test_case.rhs))),
      test_case.compare);
}

INSTANTIATE_TEST_SUITE_P(StringCompareTest, StringCompareTest,
                         testing::ValuesIn<StringCompareTestCase>({
                             {"", "", 0},
                             {"", std::string("\0", 1), -1},
                             {std::string("\0", 1), "", 1},
                             {std::string("\0", 1), std::string("\0", 1), 0},
                             {"", "foo", -1},
                             {"foo", "", 1},
                             {"foo", "foo", 0},
                             {"bar", "foo", -1},
                             {"foo", "bar", 1},
                             {"bar", "bar", 0},
                         }));

struct StringDebugStringTestCase final {
  std::string data;
};

using StringDebugStringTest = testing::TestWithParam<StringDebugStringTestCase>;

TEST_P(StringDebugStringTest, ToCord) {
  const StringDebugStringTestCase& test_case = GetParam();
  TestValueFactory value_factory;
  EXPECT_EQ(MakeStringString(value_factory, test_case.data)->DebugString(),
            internal::FormatStringLiteral(test_case.data));
  EXPECT_EQ(MakeCordString(value_factory, test_case.data)->DebugString(),
            internal::FormatStringLiteral(test_case.data));
  EXPECT_EQ(MakeExternalString(value_factory, test_case.data)->DebugString(),
            internal::FormatStringLiteral(test_case.data));
}

INSTANTIATE_TEST_SUITE_P(StringDebugStringTest, StringDebugStringTest,
                         testing::ValuesIn<StringDebugStringTestCase>({
                             {""},
                             {"1"},
                             {"foo"},
                             {"\xef\xbf\xbd"},
                         }));

struct StringToStringTestCase final {
  std::string data;
};

using StringToStringTest = testing::TestWithParam<StringToStringTestCase>;

TEST_P(StringToStringTest, ToString) {
  const StringToStringTestCase& test_case = GetParam();
  TestValueFactory value_factory;
  EXPECT_EQ(MakeStringString(value_factory, test_case.data)->ToString(),
            test_case.data);
  EXPECT_EQ(MakeCordString(value_factory, test_case.data)->ToString(),
            test_case.data);
  EXPECT_EQ(MakeExternalString(value_factory, test_case.data)->ToString(),
            test_case.data);
}

INSTANTIATE_TEST_SUITE_P(StringToStringTest, StringToStringTest,
                         testing::ValuesIn<StringToStringTestCase>({
                             {""},
                             {"1"},
                             {"foo"},
                             {"\xef\xbf\xbd"},
                         }));

struct StringToCordTestCase final {
  std::string data;
};

using StringToCordTest = testing::TestWithParam<StringToCordTestCase>;

TEST_P(StringToCordTest, ToCord) {
  const StringToCordTestCase& test_case = GetParam();
  TestValueFactory value_factory;
  EXPECT_EQ(MakeStringString(value_factory, test_case.data)->ToCord(),
            test_case.data);
  EXPECT_EQ(MakeCordString(value_factory, test_case.data)->ToCord(),
            test_case.data);
  EXPECT_EQ(MakeExternalString(value_factory, test_case.data)->ToCord(),
            test_case.data);
}

INSTANTIATE_TEST_SUITE_P(StringToCordTest, StringToCordTest,
                         testing::ValuesIn<StringToCordTestCase>({
                             {""},
                             {"1"},
                             {"foo"},
                             {"\xef\xbf\xbd"},
                         }));

TEST(Value, SupportsAbslHash) {
  TestValueFactory value_factory;
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      Persistent<const Value>(value_factory.GetNullValue()),
      Persistent<const Value>(
          value_factory.CreateErrorValue(absl::CancelledError())),
      Persistent<const Value>(value_factory.CreateBoolValue(false)),
      Persistent<const Value>(value_factory.CreateIntValue(0)),
      Persistent<const Value>(value_factory.CreateUintValue(0)),
      Persistent<const Value>(value_factory.CreateDoubleValue(0.0)),
      Persistent<const Value>(
          Must(value_factory.CreateDurationValue(absl::ZeroDuration()))),
      Persistent<const Value>(
          Must(value_factory.CreateTimestampValue(absl::UnixEpoch()))),
      Persistent<const Value>(value_factory.GetBytesValue()),
      Persistent<const Value>(Must(value_factory.CreateBytesValue("foo"))),
      Persistent<const Value>(
          Must(value_factory.CreateBytesValue(absl::Cord("bar")))),
      Persistent<const Value>(value_factory.GetStringValue()),
      Persistent<const Value>(Must(value_factory.CreateStringValue("foo"))),
      Persistent<const Value>(
          Must(value_factory.CreateStringValue(absl::Cord("bar")))),
  }));
}

}  // namespace
}  // namespace cel
