#include "google/api/expr/v1beta1/value.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "testutil/test_data_io.h"
#include "testutil/test_data_util.h"
#include "testdata/test_data.pb.h"

namespace google {
namespace api {
namespace expr {
namespace testutil {

using testdata::TestValue;

/**
 * Test class for all pairs of values in "unique_values"
 */
class UniqueValuesTest
    : public ::testing::TestWithParam<std::pair<TestValue, TestValue>> {
 protected:
  UniqueValuesTest() { v1beta1::InitValueDifferencer(&v1beta1_differ_); }

  bool Equivalent(const v1beta1::ExprValue& lhs,
                  const v1beta1::ExprValue& rhs) {
    return v1beta1_differ_.Compare(lhs, rhs);
  }

  void ExpectEquivalent(bool expected, const TestValue& lhs,
                        const TestValue& rhs) {
    for (const auto& lhs_value : lhs.v1beta1()) {
      SCOPED_TRACE(lhs_value.ShortDebugString());
      for (const auto& rhs_value : rhs.v1beta1()) {
        SCOPED_TRACE(rhs_value.ShortDebugString());
        EXPECT_EQ(expected, Equivalent(lhs_value, rhs_value));
      }
    }
  }

 private:
  google::protobuf::util::MessageDifferencer v1beta1_differ_;
};

/**
 * Tests that the values in "unqiue_values" are not equivalent to each other.
 */
TEST_P(UniqueValuesTest, NotEquivalent) {
  ExpectEquivalent(false, GetParam().first, GetParam().second);
}

INSTANTIATE_TEST_SUITE_P(
    UniqueValues, UniqueValuesTest,
    ::testing::ValuesIn(AllPairs(ReadTestData("unique_values").test_values())),
    TestDataParamName());

/**
 * Test class for TestValue invariants.
 */
class TestValueTest : public ::testing::TestWithParam<TestValue> {
 protected:
  TestValueTest() { v1beta1::InitValueDifferencer(&v1beta1_differ_); }

  ::testing::AssertionResult Equivalent(const v1beta1::ExprValue& lhs,
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

/**
 * Tests that all values within a given TestValue are equivalent to each other.
 */
TEST_P(TestValueTest, Equivalent) {
  const auto& first = GetParam().v1beta1(0);
  for (const auto& value : GetParam().v1beta1()) {
    SCOPED_TRACE(value.ShortDebugString());
    EXPECT_TRUE(Equivalent(first, value));
  }
}

INSTANTIATE_TEST_SUITE_P(
    UniqueValues, TestValueTest,
    ::testing::ValuesIn(ReadTestData("unique_values").test_values().values()),
    TestDataParamName());

}  // namespace testutil
}  // namespace expr
}  // namespace api
}  // namespace google
