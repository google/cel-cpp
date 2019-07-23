#include "v1beta1/converters.h"

#include "google/protobuf/empty.pb.h"
#include "google/type/money.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "common/value.h"
#include "internal/status_util.h"
#include "protoutil/converters.h"
#include "protoutil/type_registry.h"
#include "testutil/test_data_io.h"
#include "testutil/test_data_util.h"
#include "testdata/test_data.pb.h"

namespace google {
namespace api {
namespace expr {
namespace v1beta1 {

using protoutil::TypeRegistry;
using testdata::TestValue;

namespace {

const TypeRegistry* kReg = []() {
  auto* reg = new TypeRegistry;
  protoutil::RegisterConvertersWith(reg);
  return reg;
}();

class ValueTest : public ::testing::TestWithParam<TestValue> {
 public:
  ValueTest() { v1beta1::InitValueDifferencer(&v1beta1_differ_); }

  ::testing::AssertionResult IsEquiv(const v1beta1::ExprValue& lhs,
                                     const v1beta1::ExprValue& rhs) {
    std::string diff;
    v1beta1_differ_.ReportDifferencesToString(&diff);
    if (v1beta1_differ_.Compare(lhs, rhs)) {
      return ::testing::AssertionSuccess();
    }
    return ::testing::AssertionFailure() << diff;
  }

 private:
  google::protobuf::util::MessageDifferencer v1beta1_differ_;
};

TEST_P(ValueTest, SelfEqual) {
  for (const auto& lhs : GetParam().v1beta1()) {
    SCOPED_TRACE(lhs.ShortDebugString());
    common::Value lhs_val = ValueFrom(lhs, kReg);
    for (const auto& rhs : GetParam().v1beta1()) {
      SCOPED_TRACE(rhs.ShortDebugString());
      common::Value rhs_val = ValueFrom(rhs, kReg);
      EXPECT_EQ(lhs_val.hash_code(), rhs_val.hash_code());
      EXPECT_EQ(lhs_val, rhs_val);
    }
  }
}

TEST_P(ValueTest, RoundTrip_FromRef) {
  for (const auto& expected : GetParam().v1beta1()) {
    SCOPED_TRACE(expected.ShortDebugString());
    auto cel_value = ValueFrom(expected, kReg);
    EXPECT_TRUE(cel_value.owns_value());
    v1beta1::ExprValue actual;
    ValueTo(cel_value, &actual);
    EXPECT_TRUE(IsEquiv(expected, actual));
  }
}

TEST_P(ValueTest, RoundTrip_FromPtr) {
  for (const auto& expected : GetParam().v1beta1()) {
    SCOPED_TRACE(expected.ShortDebugString());
    auto cel_value = ValueFrom(absl::make_unique<ExprValue>(expected), kReg);
    EXPECT_TRUE(cel_value.owns_value());
    v1beta1::ExprValue actual;
    ValueTo(cel_value, &actual);
    EXPECT_TRUE(IsEquiv(expected, actual));
  }
}

TEST_P(ValueTest, RoundTrip_FromMove) {
  for (const auto& expected : GetParam().v1beta1()) {
    SCOPED_TRACE(expected.ShortDebugString());
    auto cel_value = ValueFrom(ExprValue(expected), kReg);
    EXPECT_TRUE(cel_value.owns_value());
    v1beta1::ExprValue actual;
    ValueTo(cel_value, &actual);
    EXPECT_TRUE(IsEquiv(expected, actual));
  }
}

TEST_P(ValueTest, RoundTrip_For) {
  for (const auto& expected : GetParam().v1beta1()) {
    SCOPED_TRACE(expected.ShortDebugString());
    auto cel_value = v1beta1::ValueFor(&expected, kReg);
    v1beta1::ExprValue actual;
    ValueTo(cel_value, &actual);
    EXPECT_TRUE(IsEquiv(expected, actual));
  }
}

INSTANTIATE_TEST_SUITE_P(
    UniqueValues, ValueTest,
    ::testing::ValuesIn(
        testutil::ReadTestData("unique_values").test_values().values()),
    testutil::TestDataParamName());

class UniqueValueTest
    : public ::testing::TestWithParam<std::pair<TestValue, TestValue>> {
 public:
};

TEST_P(UniqueValueTest, NotEqual) {
  for (const auto& lhs : GetParam().first.v1beta1()) {
    SCOPED_TRACE(lhs.ShortDebugString());
    auto lhs_value = ValueFor(&lhs, kReg);
    for (const auto& rhs : GetParam().second.v1beta1()) {
      SCOPED_TRACE(rhs.ShortDebugString());
      auto rhs_value = ValueFor(&rhs, kReg);
      EXPECT_NE(lhs_value, rhs_value);
    }
  }
}
INSTANTIATE_TEST_SUITE_P(
    All, UniqueValueTest,
    ::testing::ValuesIn(testutil::AllPairs(
        testutil::ReadTestData("unique_values").test_values())),
    testutil::TestDataParamName());

TEST(ConvertersTest, List) {
  auto value =
      ValueFrom(testutil::NewListValue(1, 2u, 3.0, "four").v1beta1(0), kReg);
  const auto& list = value.list_value();

  auto error = common::Value::FromError(expr::internal::OutOfRangeError(4, 4));
  EXPECT_EQ(4, list.size());
  EXPECT_EQ(common::Value::FromInt(1), list.Get(0));
  EXPECT_EQ(common::Value::FromUInt(2), list.Get(1));
  EXPECT_EQ(common::Value::FromDouble(3), list.Get(2));
  EXPECT_EQ(common::Value::ForString("four"), list.Get(3));
  EXPECT_EQ(error, list.Get(4));

  EXPECT_EQ(common::Value::FromBool(true),
            list.Contains(common::Value::FromInt(1)));
  EXPECT_EQ(common::Value::FromBool(true),
            list.Contains(common::Value::FromUInt(2)));
  EXPECT_EQ(common::Value::FromBool(true),
            list.Contains(common::Value::FromDouble(3)));
  EXPECT_EQ(common::Value::FromBool(true),
            list.Contains(common::Value::ForString("four")));
  EXPECT_EQ(error, list.Contains(error));

  EXPECT_EQ(common::Value::FromBool(false),
            list.Contains(common::Value::FromUInt(1)));
  EXPECT_EQ(common::Value::FromBool(false),
            list.Contains(common::Value::FromInt(2)));
  EXPECT_EQ(common::Value::FromBool(false),
            list.Contains(common::Value::FromInt(3)));
  EXPECT_EQ(common::Value::FromBool(false),
            list.Contains(common::Value::FromInt(4)));

  int i = 0;
  list.ForEach([&i, &list](const common::Value& elem) {
    EXPECT_EQ(list.Get(i++), elem);
    return internal::OkStatus();
  });
  EXPECT_EQ(i, 4);
}

TEST(ConvertersTest, Map) {
  auto value =
      ValueFrom(testutil::NewMapValue(1, 2u, 3.0, "four").v1beta1(0), kReg);
  const auto& map = value.map_value();

  auto error1 = common::Value::FromError(expr::internal::NoSuchKey("2u"));
  auto error2 = common::Value::FromError(expr::internal::NoSuchKey("\"four\""));
  EXPECT_EQ(2, map.size());
  EXPECT_EQ(common::Value::FromUInt(2), map.Get(common::Value::FromInt(1)));
  EXPECT_EQ(error1, map.Get(common::Value::FromUInt(2)));
  EXPECT_EQ(common::Value::ForString("four"),
            map.Get(common::Value::FromDouble(3)));
  EXPECT_EQ(error2, map.Get(common::Value::ForString("four")));

  int i = 0;
  map.ForEach([&i, &map](const common::Value& key, const common::Value& value) {
    i++;
    EXPECT_EQ(value, map.Get(key));
    return expr::internal::OkStatus();
  });
  EXPECT_EQ(i, 2);
}

TEST(ConvertersTest, BadValue) {
  v1beta1::ExprValue result;
  auto bad_value = common::Value::FromTime(absl::InfiniteFuture());
  auto status = ValueTo(bad_value, &result);
  auto expected = common::Value::FromError(
      expr::internal::InvalidArgumentError("time above max"));
  // Status returns the expected error code.
  EXPECT_EQ(common::Value::FromError(status), expected);
  // The result also encodes the error.
  EXPECT_EQ(expected, ValueFrom(result, kReg));
}

}  // namespace
}  // namespace v1beta1
}  // namespace expr
}  // namespace api
}  // namespace google
