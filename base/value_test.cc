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
#include <string>
#include <type_traits>
#include <utility>

#include "absl/hash/hash_testing.h"
#include "absl/status/status.h"
#include "absl/time/time.h"
#include "base/type.h"
#include "internal/strings.h"
#include "internal/testing.h"
#include "internal/time.h"

namespace cel {
namespace {

using cel::internal::StatusIs;

template <class T>
constexpr void IS_INITIALIZED(T&) {}

TEST(Value, TypeTraits) {
  EXPECT_TRUE(std::is_default_constructible_v<Value>);
  EXPECT_TRUE(std::is_copy_constructible_v<Value>);
  EXPECT_TRUE(std::is_move_constructible_v<Value>);
  EXPECT_TRUE(std::is_copy_assignable_v<Value>);
  EXPECT_TRUE(std::is_move_assignable_v<Value>);
  EXPECT_TRUE(std::is_swappable_v<Value>);
}

TEST(Value, DefaultConstructor) {
  Value value;
  EXPECT_EQ(value, Value::Null());
}

struct ConstructionAssignmentTestCase final {
  std::string name;
  std::function<Value()> default_value;
};

using ConstructionAssignmentTest =
    testing::TestWithParam<ConstructionAssignmentTestCase>;

TEST_P(ConstructionAssignmentTest, CopyConstructor) {
  const auto& test_case = GetParam();
  Value from(test_case.default_value());
  Value to(from);
  IS_INITIALIZED(to);
  EXPECT_EQ(to, test_case.default_value());
}

TEST_P(ConstructionAssignmentTest, MoveConstructor) {
  const auto& test_case = GetParam();
  Value from(test_case.default_value());
  Value to(std::move(from));
  IS_INITIALIZED(from);
  EXPECT_EQ(from, Value::Null());
  EXPECT_EQ(to, test_case.default_value());
}

TEST_P(ConstructionAssignmentTest, CopyAssignment) {
  const auto& test_case = GetParam();
  Value from(test_case.default_value());
  Value to;
  to = from;
  EXPECT_EQ(to, from);
}

TEST_P(ConstructionAssignmentTest, MoveAssignment) {
  const auto& test_case = GetParam();
  Value from(test_case.default_value());
  Value to;
  to = std::move(from);
  IS_INITIALIZED(from);
  EXPECT_EQ(from, Value::Null());
  EXPECT_EQ(to, test_case.default_value());
}

INSTANTIATE_TEST_SUITE_P(
    ConstructionAssignmentTest, ConstructionAssignmentTest,
    testing::ValuesIn<ConstructionAssignmentTestCase>({
        {"Null", Value::Null},
        {"Bool", Value::False},
        {"Int", []() { return Value::Int(0); }},
        {"Uint", []() { return Value::Uint(0); }},
        {"Double", []() { return Value::Double(0.0); }},
        {"Duration", []() { return Value::ZeroDuration(); }},
        {"Timestamp", []() { return Value::UnixEpoch(); }},
        {"Error", []() { return Value::Error(absl::CancelledError()); }},
        {"Bytes", Bytes::Empty},
    }),
    [](const testing::TestParamInfo<ConstructionAssignmentTestCase>& info) {
      return info.param.name;
    });

TEST(Value, Swap) {
  Value lhs = Value::Int(0);
  Value rhs = Value::Uint(0);
  std::swap(lhs, rhs);
  EXPECT_EQ(lhs, Value::Uint(0));
  EXPECT_EQ(rhs, Value::Int(0));
}

TEST(Value, NaN) { EXPECT_TRUE(std::isnan(Value::NaN().AsDouble())); }

TEST(Value, PositiveInfinity) {
  EXPECT_TRUE(std::isinf(Value::PositiveInfinity().AsDouble()));
  EXPECT_FALSE(std::signbit(Value::PositiveInfinity().AsDouble()));
}

TEST(Value, NegativeInfinity) {
  EXPECT_TRUE(std::isinf(Value::NegativeInfinity().AsDouble()));
  EXPECT_TRUE(std::signbit(Value::NegativeInfinity().AsDouble()));
}

TEST(Value, ZeroDuration) {
  EXPECT_EQ(Value::ZeroDuration().AsDuration(), absl::ZeroDuration());
}

TEST(Value, UnixEpoch) {
  EXPECT_EQ(Value::UnixEpoch().AsTimestamp(), absl::UnixEpoch());
}

TEST(Null, DebugString) { EXPECT_EQ(Value::Null().DebugString(), "null"); }

TEST(Bool, DebugString) {
  EXPECT_EQ(Value::False().DebugString(), "false");
  EXPECT_EQ(Value::True().DebugString(), "true");
}

TEST(Int, DebugString) {
  EXPECT_EQ(Value::Int(-1).DebugString(), "-1");
  EXPECT_EQ(Value::Int(0).DebugString(), "0");
  EXPECT_EQ(Value::Int(1).DebugString(), "1");
}

TEST(Uint, DebugString) {
  EXPECT_EQ(Value::Uint(0).DebugString(), "0u");
  EXPECT_EQ(Value::Uint(1).DebugString(), "1u");
}

TEST(Double, DebugString) {
  EXPECT_EQ(Value::Double(-1.0).DebugString(), "-1.0");
  EXPECT_EQ(Value::Double(0.0).DebugString(), "0.0");
  EXPECT_EQ(Value::Double(1.0).DebugString(), "1.0");
  EXPECT_EQ(Value::Double(-1.1).DebugString(), "-1.1");
  EXPECT_EQ(Value::Double(0.1).DebugString(), "0.1");
  EXPECT_EQ(Value::Double(1.1).DebugString(), "1.1");

  EXPECT_EQ(Value::NaN().DebugString(), "nan");
  EXPECT_EQ(Value::PositiveInfinity().DebugString(), "+infinity");
  EXPECT_EQ(Value::NegativeInfinity().DebugString(), "-infinity");
}

TEST(Duration, DebugString) {
  EXPECT_EQ(Value::ZeroDuration().DebugString(),
            internal::FormatDuration(absl::ZeroDuration()).value());
}

TEST(Timestamp, DebugString) {
  EXPECT_EQ(Value::UnixEpoch().DebugString(),
            internal::FormatTimestamp(absl::UnixEpoch()).value());
}

// The below tests could be made parameterized but doing so requires the
// extension for struct member initiation by name for it to be worth it. That
// feature is not available in C++17.

TEST(Value, Error) {
  Value error_value = Value::Error(absl::CancelledError());
  EXPECT_TRUE(error_value.IsError());
  EXPECT_EQ(error_value, error_value);
  EXPECT_EQ(error_value, Value::Error(absl::CancelledError()));
  EXPECT_EQ(error_value.AsError(), absl::CancelledError());
}

TEST(Value, Bool) {
  Value false_value = Value::False();
  EXPECT_TRUE(false_value.IsBool());
  EXPECT_EQ(false_value, false_value);
  EXPECT_EQ(false_value, Value::Bool(false));
  EXPECT_EQ(false_value.kind(), Kind::kBool);
  EXPECT_EQ(false_value.type(), Type::Bool());
  EXPECT_FALSE(false_value.AsBool());

  Value true_value = Value::True();
  EXPECT_TRUE(true_value.IsBool());
  EXPECT_EQ(true_value, true_value);
  EXPECT_EQ(true_value, Value::Bool(true));
  EXPECT_EQ(true_value.kind(), Kind::kBool);
  EXPECT_EQ(true_value.type(), Type::Bool());
  EXPECT_TRUE(true_value.AsBool());

  EXPECT_NE(false_value, true_value);
  EXPECT_NE(true_value, false_value);
}

TEST(Value, Int) {
  Value zero_value = Value::Int(0);
  EXPECT_TRUE(zero_value.IsInt());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, Value::Int(0));
  EXPECT_EQ(zero_value.kind(), Kind::kInt);
  EXPECT_EQ(zero_value.type(), Type::Int());
  EXPECT_EQ(zero_value.AsInt(), 0);

  Value one_value = Value::Int(1);
  EXPECT_TRUE(one_value.IsInt());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, Value::Int(1));
  EXPECT_EQ(one_value.kind(), Kind::kInt);
  EXPECT_EQ(one_value.type(), Type::Int());
  EXPECT_EQ(one_value.AsInt(), 1);

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST(Value, Uint) {
  Value zero_value = Value::Uint(0);
  EXPECT_TRUE(zero_value.IsUint());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, Value::Uint(0));
  EXPECT_EQ(zero_value.kind(), Kind::kUint);
  EXPECT_EQ(zero_value.type(), Type::Uint());
  EXPECT_EQ(zero_value.AsUint(), 0);

  Value one_value = Value::Uint(1);
  EXPECT_TRUE(one_value.IsUint());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, Value::Uint(1));
  EXPECT_EQ(one_value.kind(), Kind::kUint);
  EXPECT_EQ(one_value.type(), Type::Uint());
  EXPECT_EQ(one_value.AsUint(), 1);

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST(Value, Double) {
  Value zero_value = Value::Double(0.0);
  EXPECT_TRUE(zero_value.IsDouble());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, Value::Double(0.0));
  EXPECT_EQ(zero_value.kind(), Kind::kDouble);
  EXPECT_EQ(zero_value.type(), Type::Double());
  EXPECT_EQ(zero_value.AsDouble(), 0.0);

  Value one_value = Value::Double(1.0);
  EXPECT_TRUE(one_value.IsDouble());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, Value::Double(1.0));
  EXPECT_EQ(one_value.kind(), Kind::kDouble);
  EXPECT_EQ(one_value.type(), Type::Double());
  EXPECT_EQ(one_value.AsDouble(), 1.0);

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST(Value, Duration) {
  Value zero_value = Value::ZeroDuration();
  EXPECT_TRUE(zero_value.IsDuration());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, Value::ZeroDuration());
  EXPECT_EQ(zero_value.kind(), Kind::kDuration);
  EXPECT_EQ(zero_value.type(), Type::Duration());
  EXPECT_EQ(zero_value.AsDuration(), absl::ZeroDuration());

  ASSERT_OK_AND_ASSIGN(Value one_value, Value::Duration(absl::ZeroDuration() +
                                                        absl::Nanoseconds(1)));
  EXPECT_TRUE(one_value.IsDuration());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value.kind(), Kind::kDuration);
  EXPECT_EQ(one_value.type(), Type::Duration());
  EXPECT_EQ(one_value.AsDuration(),
            absl::ZeroDuration() + absl::Nanoseconds(1));

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);

  EXPECT_THAT(Value::Duration(absl::InfiniteDuration()),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(Value, Timestamp) {
  Value zero_value = Value::UnixEpoch();
  EXPECT_TRUE(zero_value.IsTimestamp());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, Value::UnixEpoch());
  EXPECT_EQ(zero_value.kind(), Kind::kTimestamp);
  EXPECT_EQ(zero_value.type(), Type::Timestamp());
  EXPECT_EQ(zero_value.AsTimestamp(), absl::UnixEpoch());

  ASSERT_OK_AND_ASSIGN(Value one_value, Value::Timestamp(absl::UnixEpoch() +
                                                         absl::Nanoseconds(1)));
  EXPECT_TRUE(one_value.IsTimestamp());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value.kind(), Kind::kTimestamp);
  EXPECT_EQ(one_value.type(), Type::Timestamp());
  EXPECT_EQ(one_value.AsTimestamp(), absl::UnixEpoch() + absl::Nanoseconds(1));

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);

  EXPECT_THAT(Value::Timestamp(absl::InfiniteFuture()),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(Value, BytesFromString) {
  Value zero_value = Bytes::New(std::string("0"));
  EXPECT_TRUE(zero_value.IsBytes());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, Bytes::New(std::string("0")));
  EXPECT_EQ(zero_value.kind(), Kind::kBytes);
  EXPECT_EQ(zero_value.type(), Type::Bytes());
  EXPECT_EQ(zero_value.AsBytes().ToString(), "0");

  Value one_value = Bytes::New(std::string("1"));
  EXPECT_TRUE(one_value.IsBytes());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, Bytes::New(std::string("1")));
  EXPECT_EQ(one_value.kind(), Kind::kBytes);
  EXPECT_EQ(one_value.type(), Type::Bytes());
  EXPECT_EQ(one_value.AsBytes().ToString(), "1");

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST(Value, BytesFromStringView) {
  Value zero_value = Bytes::New(absl::string_view("0"));
  EXPECT_TRUE(zero_value.IsBytes());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, Bytes::New(absl::string_view("0")));
  EXPECT_EQ(zero_value.kind(), Kind::kBytes);
  EXPECT_EQ(zero_value.type(), Type::Bytes());
  EXPECT_EQ(zero_value.AsBytes().ToString(), "0");

  Value one_value = Bytes::New(absl::string_view("1"));
  EXPECT_TRUE(one_value.IsBytes());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, Bytes::New(absl::string_view("1")));
  EXPECT_EQ(one_value.kind(), Kind::kBytes);
  EXPECT_EQ(one_value.type(), Type::Bytes());
  EXPECT_EQ(one_value.AsBytes().ToString(), "1");

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST(Value, BytesFromCord) {
  Value zero_value = Bytes::New(absl::Cord("0"));
  EXPECT_TRUE(zero_value.IsBytes());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, Bytes::New(absl::Cord("0")));
  EXPECT_EQ(zero_value.kind(), Kind::kBytes);
  EXPECT_EQ(zero_value.type(), Type::Bytes());
  EXPECT_EQ(zero_value.AsBytes().ToCord(), "0");

  Value one_value = Bytes::New(absl::Cord("1"));
  EXPECT_TRUE(one_value.IsBytes());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, Bytes::New(absl::Cord("1")));
  EXPECT_EQ(one_value.kind(), Kind::kBytes);
  EXPECT_EQ(one_value.type(), Type::Bytes());
  EXPECT_EQ(one_value.AsBytes().ToCord(), "1");

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

TEST(Value, BytesFromLiteral) {
  Value zero_value = Bytes::New("0");
  EXPECT_TRUE(zero_value.IsBytes());
  EXPECT_EQ(zero_value, zero_value);
  EXPECT_EQ(zero_value, Bytes::New("0"));
  EXPECT_EQ(zero_value.kind(), Kind::kBytes);
  EXPECT_EQ(zero_value.type(), Type::Bytes());
  EXPECT_EQ(zero_value.AsBytes().ToString(), "0");

  Value one_value = Bytes::New("1");
  EXPECT_TRUE(one_value.IsBytes());
  EXPECT_EQ(one_value, one_value);
  EXPECT_EQ(one_value, Bytes::New("1"));
  EXPECT_EQ(one_value.kind(), Kind::kBytes);
  EXPECT_EQ(one_value.type(), Type::Bytes());
  EXPECT_EQ(one_value.AsBytes().ToString(), "1");

  EXPECT_NE(zero_value, one_value);
  EXPECT_NE(one_value, zero_value);
}

Value MakeStringBytes(absl::string_view value) { return Bytes::New(value); }

Value MakeCordBytes(absl::string_view value) {
  return Bytes::New(absl::Cord(value));
}

Value MakeWrappedBytes(absl::string_view value) {
  return Bytes::Wrap(value, []() {});
}

struct BytesConcatTestCase final {
  std::string lhs;
  std::string rhs;
};

using BytesConcatTest = testing::TestWithParam<BytesConcatTestCase>;

TEST_P(BytesConcatTest, Concat) {
  const BytesConcatTestCase& test_case = GetParam();
  EXPECT_TRUE(Bytes::Concat(MakeStringBytes(test_case.lhs).AsBytes(),
                            MakeStringBytes(test_case.rhs).AsBytes())
                  .AsBytes()
                  .Equals(test_case.lhs + test_case.rhs));
  EXPECT_TRUE(Bytes::Concat(MakeStringBytes(test_case.lhs).AsBytes(),
                            MakeCordBytes(test_case.rhs).AsBytes())
                  .AsBytes()
                  .Equals(test_case.lhs + test_case.rhs));
  EXPECT_TRUE(Bytes::Concat(MakeStringBytes(test_case.lhs).AsBytes(),
                            MakeWrappedBytes(test_case.rhs).AsBytes())
                  .AsBytes()
                  .Equals(test_case.lhs + test_case.rhs));
  EXPECT_TRUE(Bytes::Concat(MakeCordBytes(test_case.lhs).AsBytes(),
                            MakeStringBytes(test_case.rhs).AsBytes())
                  .AsBytes()
                  .Equals(test_case.lhs + test_case.rhs));
  EXPECT_TRUE(Bytes::Concat(MakeCordBytes(test_case.lhs).AsBytes(),
                            MakeWrappedBytes(test_case.rhs).AsBytes())
                  .AsBytes()
                  .Equals(test_case.lhs + test_case.rhs));
  EXPECT_TRUE(Bytes::Concat(MakeCordBytes(test_case.lhs).AsBytes(),
                            MakeCordBytes(test_case.rhs).AsBytes())
                  .AsBytes()
                  .Equals(test_case.lhs + test_case.rhs));
  EXPECT_TRUE(Bytes::Concat(MakeWrappedBytes(test_case.lhs).AsBytes(),
                            MakeStringBytes(test_case.rhs).AsBytes())
                  .AsBytes()
                  .Equals(test_case.lhs + test_case.rhs));
  EXPECT_TRUE(Bytes::Concat(MakeWrappedBytes(test_case.lhs).AsBytes(),
                            MakeCordBytes(test_case.rhs).AsBytes())
                  .AsBytes()
                  .Equals(test_case.lhs + test_case.rhs));
  EXPECT_TRUE(Bytes::Concat(MakeWrappedBytes(test_case.lhs).AsBytes(),
                            MakeWrappedBytes(test_case.rhs).AsBytes())
                  .AsBytes()
                  .Equals(test_case.lhs + test_case.rhs));
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
  EXPECT_EQ(MakeStringBytes(test_case.data).AsBytes().size(), test_case.size);
  EXPECT_EQ(MakeCordBytes(test_case.data).AsBytes().size(), test_case.size);
  EXPECT_EQ(MakeWrappedBytes(test_case.data).AsBytes().size(), test_case.size);
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
  EXPECT_EQ(MakeStringBytes(test_case.data).AsBytes().empty(), test_case.empty);
  EXPECT_EQ(MakeCordBytes(test_case.data).AsBytes().empty(), test_case.empty);
  EXPECT_EQ(MakeWrappedBytes(test_case.data).AsBytes().empty(),
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
  EXPECT_EQ(MakeStringBytes(test_case.lhs)
                .AsBytes()
                .Equals(MakeStringBytes(test_case.rhs).AsBytes()),
            test_case.equals);
  EXPECT_EQ(MakeStringBytes(test_case.lhs)
                .AsBytes()
                .Equals(MakeCordBytes(test_case.rhs).AsBytes()),
            test_case.equals);
  EXPECT_EQ(MakeStringBytes(test_case.lhs)
                .AsBytes()
                .Equals(MakeWrappedBytes(test_case.rhs).AsBytes()),
            test_case.equals);
  EXPECT_EQ(MakeCordBytes(test_case.lhs)
                .AsBytes()
                .Equals(MakeStringBytes(test_case.rhs).AsBytes()),
            test_case.equals);
  EXPECT_EQ(MakeCordBytes(test_case.lhs)
                .AsBytes()
                .Equals(MakeWrappedBytes(test_case.rhs).AsBytes()),
            test_case.equals);
  EXPECT_EQ(MakeCordBytes(test_case.lhs)
                .AsBytes()
                .Equals(MakeCordBytes(test_case.rhs).AsBytes()),
            test_case.equals);
  EXPECT_EQ(MakeWrappedBytes(test_case.lhs)
                .AsBytes()
                .Equals(MakeStringBytes(test_case.rhs).AsBytes()),
            test_case.equals);
  EXPECT_EQ(MakeWrappedBytes(test_case.lhs)
                .AsBytes()
                .Equals(MakeCordBytes(test_case.rhs).AsBytes()),
            test_case.equals);
  EXPECT_EQ(MakeWrappedBytes(test_case.lhs)
                .AsBytes()
                .Equals(MakeWrappedBytes(test_case.rhs).AsBytes()),
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
  EXPECT_EQ(NormalizeCompareResult(
                MakeStringBytes(test_case.lhs)
                    .AsBytes()
                    .Compare(MakeStringBytes(test_case.rhs).AsBytes())),
            test_case.compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeStringBytes(test_case.lhs)
                    .AsBytes()
                    .Compare(MakeCordBytes(test_case.rhs).AsBytes())),
            test_case.compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeStringBytes(test_case.lhs)
                    .AsBytes()
                    .Compare(MakeWrappedBytes(test_case.rhs).AsBytes())),
            test_case.compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeCordBytes(test_case.lhs)
                    .AsBytes()
                    .Compare(MakeStringBytes(test_case.rhs).AsBytes())),
            test_case.compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeCordBytes(test_case.lhs)
                    .AsBytes()
                    .Compare(MakeWrappedBytes(test_case.rhs).AsBytes())),
            test_case.compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeCordBytes(test_case.lhs)
                    .AsBytes()
                    .Compare(MakeCordBytes(test_case.rhs).AsBytes())),
            test_case.compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeWrappedBytes(test_case.lhs)
                    .AsBytes()
                    .Compare(MakeStringBytes(test_case.rhs).AsBytes())),
            test_case.compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeWrappedBytes(test_case.lhs)
                    .AsBytes()
                    .Compare(MakeCordBytes(test_case.rhs).AsBytes())),
            test_case.compare);
  EXPECT_EQ(NormalizeCompareResult(
                MakeWrappedBytes(test_case.lhs)
                    .AsBytes()
                    .Compare(MakeWrappedBytes(test_case.rhs).AsBytes())),
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
  EXPECT_EQ(MakeStringBytes(test_case.data).DebugString(),
            internal::FormatBytesLiteral(test_case.data));
  EXPECT_EQ(MakeCordBytes(test_case.data).DebugString(),
            internal::FormatBytesLiteral(test_case.data));
  EXPECT_EQ(MakeWrappedBytes(test_case.data).DebugString(),
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
  EXPECT_EQ(MakeStringBytes(test_case.data).AsBytes().ToString(),
            test_case.data);
  EXPECT_EQ(MakeCordBytes(test_case.data).AsBytes().ToString(), test_case.data);
  EXPECT_EQ(MakeWrappedBytes(test_case.data).AsBytes().ToString(),
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
  EXPECT_EQ(MakeStringBytes(test_case.data).AsBytes().ToCord(), test_case.data);
  EXPECT_EQ(MakeCordBytes(test_case.data).AsBytes().ToCord(), test_case.data);
  EXPECT_EQ(MakeWrappedBytes(test_case.data).AsBytes().ToCord(),
            test_case.data);
}

INSTANTIATE_TEST_SUITE_P(BytesToCordTest, BytesToCordTest,
                         testing::ValuesIn<BytesToCordTestCase>({
                             {""},
                             {"1"},
                             {"foo"},
                             {"\xef\xbf\xbd"},
                         }));

TEST(Value, SupportsAbslHash) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      Value::Null(),
      Value::Error(absl::CancelledError()),
      Value::Bool(false),
      Value::Int(0),
      Value::Uint(0),
      Value::Double(0.0),
      Value::ZeroDuration(),
      Value::UnixEpoch(),
      Bytes::Empty(),
      Bytes::New("foo"),
      Bytes::New(absl::Cord("bar")),
      Bytes::Wrap("baz", []() {}),
  }));
}

}  // namespace
}  // namespace cel
