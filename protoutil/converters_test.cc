#include "protoutil/converters.h"

#include <utility>

#include "google/protobuf/any.pb.h"
#include "google/type/money.pb.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "common/value.h"
#include "protoutil/type_registry.h"
#include "testutil/test_data_io.h"
#include "testutil/test_data_util.h"
#include "testdata/test_data.pb.h"
#include "testdata/test_value.pb.h"

namespace google {
namespace api {
namespace expr {
namespace protoutil {

using testdata::TestValue;

namespace {

const TypeRegistry* kReg = []() {
  TypeRegistry* registry = new TypeRegistry;
  RegisterConvertersWith(registry);
  return registry;
}();

TEST(ConverterTest, ForProto) {
  google::type::Money money;
  money.set_nanos(1);

  common::Value value = kReg->ValueFor(&money);
  EXPECT_EQ(value.kind(), common::Value::Kind::kObject);
  EXPECT_FALSE(value.is_inline());
  EXPECT_TRUE(value.is_value());
  EXPECT_FALSE(value.owns_value());
  EXPECT_EQ(value.object_value().GetMember("nanos").int_value(), 1);
  EXPECT_EQ(value.object_value().object_type().full_name(),
            "google.type.Money");
}

TEST(ConverterTest, FromProto) {
  auto money = absl::make_unique<google::type::Money>();
  money->set_nanos(1);

  common::Value value = kReg->ValueFrom(std::move(money));
  EXPECT_EQ(value.kind(), common::Value::Kind::kObject);
  EXPECT_FALSE(value.is_inline());
  EXPECT_TRUE(value.is_value());
  EXPECT_TRUE(value.owns_value());
  EXPECT_EQ(value.object_value().GetMember("nanos").int_value(), 1);
  EXPECT_EQ(value.object_value().object_type().full_name(),
            "google.type.Money");
}

TEST(ConverterTest, FromProto_Any) {
  google::type::Money money;
  money.set_nanos(1);

  google::protobuf::Any any;
  any.PackFrom(money);

  common::Value value = kReg->ValueFrom(any);
  EXPECT_EQ(value.kind(), common::Value::Kind::kObject);
  EXPECT_FALSE(value.is_inline());
  EXPECT_TRUE(value.is_value());
  EXPECT_TRUE(value.owns_value());
  EXPECT_EQ(value.object_value().GetMember("nanos").int_value(), 1);
  EXPECT_EQ(value.object_value().object_type().full_name(),
            "google.type.Money");
}

TEST(ConverterTest, FromProto_AnyPtr) {
  google::type::Money money;
  money.set_nanos(1);

  google::protobuf::Any any;
  any.PackFrom(money);

  common::Value value = kReg->ValueFor(&any);
  EXPECT_EQ(value.kind(), common::Value::Kind::kObject);
  EXPECT_FALSE(value.is_inline());
  EXPECT_TRUE(value.is_value());
  EXPECT_TRUE(value.owns_value());
  EXPECT_EQ(value.object_value().GetMember("nanos").int_value(), 1);
  EXPECT_EQ(value.object_value().object_type().full_name(),
            "google.type.Money");
}

class ValueTest : public ::testing::TestWithParam<TestValue> {
 public:
  ValueTest() { v1beta1::InitValueDifferencer(&v1beta1_differ_); }

 protected:
 private:
  google::protobuf::util::MessageDifferencer v1beta1_differ_;
};

TEST_P(ValueTest, SelfEqual) {
  for (const auto& lhs : GetParam().proto()) {
    SCOPED_TRACE(lhs.ShortDebugString());
    auto lhs_value =
        kReg->ValueFor(&lhs).object_value().GetMember(lhs.value_field_name());
    SCOPED_TRACE(lhs_value);
    for (const auto& rhs : GetParam().proto()) {
      SCOPED_TRACE(rhs.ShortDebugString());
      auto rhs_value =
          kReg->ValueFor(&rhs).object_value().GetMember(rhs.value_field_name());
      SCOPED_TRACE(rhs_value);
      EXPECT_EQ(lhs_value, rhs_value);
    }
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
  for (const auto& lhs : GetParam().first.proto()) {
    SCOPED_TRACE(lhs.ShortDebugString());
    auto lhs_value =
        kReg->ValueFor(&lhs).object_value().GetMember(lhs.value_field_name());
    SCOPED_TRACE(lhs_value);
    for (const auto& rhs : GetParam().second.proto()) {
      SCOPED_TRACE(rhs.ShortDebugString());
      auto rhs_value =
          kReg->ValueFor(&rhs).object_value().GetMember(rhs.value_field_name());
      SCOPED_TRACE(rhs_value);
      EXPECT_NE(lhs_value, rhs_value);
    }
  }
}

INSTANTIATE_TEST_SUITE_P(
    All, UniqueValueTest,
    ::testing::ValuesIn(testutil::AllPairs(
        testutil::ReadTestData("unique_values").test_values())),
    testutil::TestDataParamName());

}  // namespace
}  // namespace protoutil
}  // namespace expr
}  // namespace api
}  // namespace google
