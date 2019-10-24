#include "eval/public/cel_attribute.h"

#include "google/protobuf/arena.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/strings/string_view.h"
#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

using ::google::protobuf::Duration;
using ::google::protobuf::Timestamp;

using testing::Eq;

namespace {

class DummyMap : public CelMap {
 public:
  absl::optional<CelValue> operator[](CelValue value) const override {
    return CelValue::CreateNull();
  }
  const CelList* ListKeys() const override { return nullptr; }

  int size() const override { return 0; }
};

class DummyList : public CelList {
 public:
  int size() const override { return 0; }

  CelValue operator[](int index) const override {
    return CelValue::CreateNull();
  }
};

TEST(CelAttributeQualifierTest, TestBoolAccess) {
  auto qualifier = CelAttributeQualifier::Create(CelValue::CreateBool(true));

  EXPECT_FALSE(qualifier.GetStringKey().has_value());
  EXPECT_FALSE(qualifier.GetInt64Key().has_value());
  EXPECT_FALSE(qualifier.GetUint64Key().has_value());
  EXPECT_TRUE(qualifier.GetBoolKey().has_value());
  EXPECT_THAT(qualifier.GetBoolKey().value(), Eq(true));
}

TEST(CelAttributeQualifierTest, TestInt64Access) {
  auto qualifier = CelAttributeQualifier::Create(CelValue::CreateInt64(1));

  EXPECT_FALSE(qualifier.GetBoolKey().has_value());
  EXPECT_FALSE(qualifier.GetStringKey().has_value());
  EXPECT_FALSE(qualifier.GetUint64Key().has_value());

  EXPECT_TRUE(qualifier.GetInt64Key().has_value());
  EXPECT_THAT(qualifier.GetInt64Key().value(), Eq(1));
}

TEST(CelAttributeQualifierTest, TestUint64Access) {
  auto qualifier = CelAttributeQualifier::Create(CelValue::CreateUint64(1));

  EXPECT_FALSE(qualifier.GetBoolKey().has_value());
  EXPECT_FALSE(qualifier.GetStringKey().has_value());
  EXPECT_FALSE(qualifier.GetInt64Key().has_value());

  EXPECT_TRUE(qualifier.GetUint64Key().has_value());
  EXPECT_THAT(qualifier.GetUint64Key().value(), Eq(1));
}

TEST(CelAttributeQualifierTest, TestStringAccess) {
  const std::string test = "test";
  auto qualifier = CelAttributeQualifier::Create(CelValue::CreateString(&test));

  EXPECT_FALSE(qualifier.GetBoolKey().has_value());
  EXPECT_FALSE(qualifier.GetInt64Key().has_value());
  EXPECT_FALSE(qualifier.GetUint64Key().has_value());

  EXPECT_TRUE(qualifier.GetStringKey().has_value());
  EXPECT_THAT(qualifier.GetStringKey().value(), Eq("test"));
}

void TestAllInequalities(const CelAttributeQualifier& qualifier) {
  EXPECT_FALSE(qualifier ==
               CelAttributeQualifier::Create(CelValue::CreateBool(false)));
  EXPECT_FALSE(qualifier ==
               CelAttributeQualifier::Create(CelValue::CreateInt64(0)));
  EXPECT_FALSE(qualifier ==
               CelAttributeQualifier::Create(CelValue::CreateUint64(0)));
  const std::string test = "Those are not the droids you are looking for.";
  EXPECT_FALSE(qualifier ==
               CelAttributeQualifier::Create(CelValue::CreateString(&test)));
}

TEST(CelAttributeQualifierTest, TestBoolComparison) {
  auto qualifier = CelAttributeQualifier::Create(CelValue::CreateBool(true));
  TestAllInequalities(qualifier);
  EXPECT_TRUE(qualifier ==
              CelAttributeQualifier::Create(CelValue::CreateBool(true)));
}

TEST(CelAttributeQualifierTest, TestInt64Comparison) {
  auto qualifier = CelAttributeQualifier::Create(CelValue::CreateInt64(true));
  TestAllInequalities(qualifier);
  EXPECT_TRUE(qualifier ==
              CelAttributeQualifier::Create(CelValue::CreateInt64(true)));
}

TEST(CelAttributeQualifierTest, TestUint64Comparison) {
  auto qualifier = CelAttributeQualifier::Create(CelValue::CreateUint64(true));
  TestAllInequalities(qualifier);
  EXPECT_TRUE(qualifier ==
              CelAttributeQualifier::Create(CelValue::CreateUint64(true)));
}

TEST(CelAttributeQualifierTest, TestStringComparison) {
  const std::string kTest = "test";
  auto qualifier =
      CelAttributeQualifier::Create(CelValue::CreateString(&kTest));
  TestAllInequalities(qualifier);
  EXPECT_TRUE(qualifier ==
              CelAttributeQualifier::Create(CelValue::CreateString(&kTest)));
}

void TestAllCelValueMismatches(const CelAttributeQualifierPattern& qualifier) {
  EXPECT_FALSE(qualifier.IsMatch(CelValue::CreateNull()));
  EXPECT_FALSE(qualifier.IsMatch(CelValue::CreateBool(false)));
  EXPECT_FALSE(qualifier.IsMatch(CelValue::CreateInt64(0)));
  EXPECT_FALSE(qualifier.IsMatch(CelValue::CreateUint64(0)));
  EXPECT_FALSE(qualifier.IsMatch(CelValue::CreateDouble(0.)));

  const std::string kStr = "Those are not the droids you are looking for.";
  EXPECT_FALSE(qualifier.IsMatch(CelValue::CreateString(&kStr)));
  EXPECT_FALSE(qualifier.IsMatch(CelValue::CreateBytes(&kStr)));

  Duration msg_duration;
  msg_duration.set_seconds(0);
  msg_duration.set_nanos(0);
  EXPECT_FALSE(qualifier.IsMatch(CelValue::CreateDuration(&msg_duration)));

  Timestamp msg_timestamp;
  msg_timestamp.set_seconds(0);
  msg_timestamp.set_nanos(0);
  EXPECT_FALSE(qualifier.IsMatch(CelValue::CreateTimestamp(&msg_timestamp)));

  DummyList dummy_list;
  EXPECT_FALSE(qualifier.IsMatch(CelValue::CreateList(&dummy_list)));

  DummyMap dummy_map;
  EXPECT_FALSE(qualifier.IsMatch(CelValue::CreateMap(&dummy_map)));

  google::protobuf::Arena arena;
  EXPECT_FALSE(qualifier.IsMatch(CreateErrorValue(&arena, kStr)));
}

void TestAllQualifierMismatches(const CelAttributeQualifierPattern& qualifier) {
  const std::string test = "Those are not the droids you are looking for.";
  EXPECT_FALSE(qualifier.IsMatch(
      CelAttributeQualifier::Create(CelValue::CreateBool(false))));
  EXPECT_FALSE(qualifier.IsMatch(
      CelAttributeQualifier::Create(CelValue::CreateInt64(0))));
  EXPECT_FALSE(qualifier.IsMatch(
      CelAttributeQualifier::Create(CelValue::CreateUint64(0))));
  EXPECT_FALSE(qualifier.IsMatch(
      CelAttributeQualifier::Create(CelValue::CreateString(&test))));
}

TEST(CelAttributeQualifierPatternTest, TestCelValueBoolMatch) {
  auto qualifier =
      CelAttributeQualifierPattern::Create(CelValue::CreateBool(true));

  TestAllCelValueMismatches(qualifier);

  CelValue value_match = CelValue::CreateBool(true);

  EXPECT_TRUE(qualifier.IsMatch(value_match));
}

TEST(CelAttributeQualifierPatternTest, TestCelValueInt64Match) {
  auto qualifier =
      CelAttributeQualifierPattern::Create(CelValue::CreateInt64(1));

  TestAllCelValueMismatches(qualifier);

  CelValue value_match = CelValue::CreateInt64(1);

  EXPECT_TRUE(qualifier.IsMatch(value_match));
}

TEST(CelAttributeQualifierPatternTest, TestCelValueUint64Match) {
  auto qualifier =
      CelAttributeQualifierPattern::Create(CelValue::CreateUint64(1));

  TestAllCelValueMismatches(qualifier);

  CelValue value_match = CelValue::CreateUint64(1);

  EXPECT_TRUE(qualifier.IsMatch(value_match));
}

TEST(CelAttributeQualifierPatternTest, TestCelValueStringMatch) {
  std::string kTest = "test";
  auto qualifier =
      CelAttributeQualifierPattern::Create(CelValue::CreateString(&kTest));

  TestAllCelValueMismatches(qualifier);

  CelValue value_match = CelValue::CreateString(&kTest);

  EXPECT_TRUE(qualifier.IsMatch(value_match));
}

TEST(CelAttributeQualifierPatternTest, TestQualifierBoolMatch) {
  auto qualifier =
      CelAttributeQualifierPattern::Create(CelValue::CreateBool(true));

  TestAllQualifierMismatches(qualifier);

  EXPECT_TRUE(qualifier.IsMatch(
      CelAttributeQualifier::Create(CelValue::CreateBool(true))));
}

TEST(CelAttributeQualifierPatternTest, TestQualifierInt64Match) {
  auto qualifier =
      CelAttributeQualifierPattern::Create(CelValue::CreateInt64(1));

  TestAllQualifierMismatches(qualifier);
  EXPECT_TRUE(qualifier.IsMatch(
      CelAttributeQualifier::Create(CelValue::CreateInt64(1))));
}

TEST(CelAttributeQualifierPatternTest, TestQualifierUint64Match) {
  auto qualifier =
      CelAttributeQualifierPattern::Create(CelValue::CreateUint64(1));

  TestAllQualifierMismatches(qualifier);
  EXPECT_TRUE(qualifier.IsMatch(
      CelAttributeQualifier::Create(CelValue::CreateUint64(1))));
}

TEST(CelAttributeQualifierPatternTest, TestQualifierStringMatch) {
  const std::string test = "test";
  auto qualifier =
      CelAttributeQualifierPattern::Create(CelValue::CreateString(&test));

  TestAllQualifierMismatches(qualifier);

  EXPECT_TRUE(qualifier.IsMatch(
      CelAttributeQualifier::Create(CelValue::CreateString(&test))));
}

TEST(CelAttributeQualifierPatternTest, TestQualifierWildcardMatch) {
  auto qualifier = CelAttributeQualifierPattern::CreateWildcard();
  EXPECT_TRUE(qualifier.IsMatch(
      CelAttributeQualifier::Create(CelValue::CreateBool(false))));
  EXPECT_TRUE(qualifier.IsMatch(
      CelAttributeQualifier::Create(CelValue::CreateBool(true))));
  EXPECT_TRUE(qualifier.IsMatch(
      CelAttributeQualifier::Create(CelValue::CreateInt64(0))));
  EXPECT_TRUE(qualifier.IsMatch(
      CelAttributeQualifier::Create(CelValue::CreateInt64(1))));
  EXPECT_TRUE(qualifier.IsMatch(
      CelAttributeQualifier::Create(CelValue::CreateUint64(0))));
  EXPECT_TRUE(qualifier.IsMatch(
      CelAttributeQualifier::Create(CelValue::CreateUint64(1))));

  const std::string kTest1 = "test1";
  const std::string kTest2 = "test2";

  EXPECT_TRUE(qualifier.IsMatch(
      CelAttributeQualifier::Create(CelValue::CreateString(&kTest1))));
  EXPECT_TRUE(qualifier.IsMatch(
      CelAttributeQualifier::Create(CelValue::CreateString(&kTest2))));
}

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
