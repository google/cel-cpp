#include "eval/public/cel_attribute.h"

#include <string>

#include "google/protobuf/arena.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "internal/status_macros.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {
namespace {

using google::api::expr::v1alpha1::Expr;

using ::google::protobuf::Duration;
using ::google::protobuf::Timestamp;
using testing::Eq;
using testing::IsEmpty;
using testing::SizeIs;
using cel::internal::StatusIs;

class DummyMap : public CelMap {
 public:
  absl::optional<CelValue> operator[](CelValue value) const override {
    return CelValue::CreateNull();
  }
  absl::StatusOr<const CelList*> ListKeys() const override {
    return absl::UnimplementedError("CelMap::ListKeys is not implemented");
  }

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
  EXPECT_THAT(qualifier.GetUint64Key().value(), Eq(1UL));
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
  EXPECT_FALSE(
      qualifier.IsMatch(CelProtoWrapper::CreateDuration(&msg_duration)));

  Timestamp msg_timestamp;
  msg_timestamp.set_seconds(0);
  msg_timestamp.set_nanos(0);
  EXPECT_FALSE(
      qualifier.IsMatch(CelProtoWrapper::CreateTimestamp(&msg_timestamp)));

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

TEST(CreateCelAttributePattern, Basic) {
  const std::string kTest = "def";
  CelAttributePattern pattern = CreateCelAttributePattern(
      "abc", {kTest, static_cast<uint64_t>(1), static_cast<int64_t>(-1), false,
              CelAttributeQualifierPattern::CreateWildcard()});

  EXPECT_THAT(pattern.variable(), Eq("abc"));
  ASSERT_THAT(pattern.qualifier_path(), SizeIs(5));
  EXPECT_TRUE(
      pattern.qualifier_path()[0].IsMatch(CelValue::CreateStringView(kTest)));
  EXPECT_TRUE(pattern.qualifier_path()[1].IsMatch(CelValue::CreateUint64(1)));
  EXPECT_TRUE(pattern.qualifier_path()[2].IsMatch(CelValue::CreateInt64(-1)));
  EXPECT_TRUE(pattern.qualifier_path()[3].IsMatch(CelValue::CreateBool(false)));
  EXPECT_TRUE(pattern.qualifier_path()[4].IsWildcard());
}

TEST(CreateCelAttributePattern, EmptyPath) {
  CelAttributePattern pattern = CreateCelAttributePattern("abc");

  EXPECT_THAT(pattern.variable(), Eq("abc"));
  EXPECT_THAT(pattern.qualifier_path(), IsEmpty());
}

TEST(CreateCelAttributePattern, Wildcards) {
  const std::string kTest = "*";
  CelAttributePattern pattern = CreateCelAttributePattern(
      "abc", {kTest, "false", CelAttributeQualifierPattern::CreateWildcard()});

  EXPECT_THAT(pattern.variable(), Eq("abc"));
  ASSERT_THAT(pattern.qualifier_path(), SizeIs(3));
  EXPECT_TRUE(pattern.qualifier_path()[0].IsWildcard());
  EXPECT_FALSE(pattern.qualifier_path()[1].IsWildcard());
  EXPECT_TRUE(pattern.qualifier_path()[2].IsWildcard());
}

TEST(CelAttribute, AsStringBasic) {
  Expr expr;
  expr.mutable_ident_expr()->set_name("var");
  CelAttribute attr(
      expr,
      {
          CelAttributeQualifier::Create(CelValue::CreateStringView("qual1")),
          CelAttributeQualifier::Create(CelValue::CreateStringView("qual2")),
          CelAttributeQualifier::Create(CelValue::CreateStringView("qual3")),
      });

  ASSERT_OK_AND_ASSIGN(std::string string_format, attr.AsString());

  EXPECT_EQ(string_format, "var.qual1.qual2.qual3");
}

TEST(CelAttribute, AsStringInvalidRoot) {
  Expr expr;
  expr.mutable_const_expr()->set_int64_value(1);

  CelAttribute attr(
      expr,
      {
          CelAttributeQualifier::Create(CelValue::CreateStringView("qual1")),
          CelAttributeQualifier::Create(CelValue::CreateStringView("qual2")),
          CelAttributeQualifier::Create(CelValue::CreateStringView("qual3")),
      });

  EXPECT_EQ(attr.AsString().status().code(),
            absl::StatusCode::kInvalidArgument);
}

TEST(CelAttribute, InvalidQualifiers) {
  Expr expr;
  expr.mutable_ident_expr()->set_name("var");
  google::protobuf::Arena arena;

  CelAttribute attr1(expr, {
                               CelAttributeQualifier::Create(
                                   CelValue::CreateDuration(absl::Minutes(2))),
                           });
  CelAttribute attr2(expr,
                     {
                         CelAttributeQualifier::Create(
                             CelProtoWrapper::CreateMessage(&expr, &arena)),
                     });
  CelAttribute attr3(
      expr, {
                CelAttributeQualifier::Create(CelValue::CreateBool(false)),
            });

  // Implementation detail: Messages as attribute qualifiers are unsupported,
  // so the implementation treats them inequal to any other. This is included
  // for coverage.
  EXPECT_FALSE(attr1 == attr2);
  EXPECT_FALSE(attr2 == attr1);
  EXPECT_FALSE(attr2 == attr2);
  EXPECT_FALSE(attr1 == attr3);
  EXPECT_FALSE(attr3 == attr1);
  EXPECT_FALSE(attr2 == attr3);
  EXPECT_FALSE(attr3 == attr2);

  // If the attribute includes an unsupported qualifier, return invalid argument
  // error.
  EXPECT_THAT(attr1.AsString(), StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(attr2.AsString(), StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(CelAttribute, AsStringQualiferTypes) {
  Expr expr;
  expr.mutable_ident_expr()->set_name("var");
  CelAttribute attr(
      expr,
      {
          CelAttributeQualifier::Create(CelValue::CreateStringView("qual1")),
          CelAttributeQualifier::Create(CelValue::CreateUint64(1)),
          CelAttributeQualifier::Create(CelValue::CreateInt64(-1)),
          CelAttributeQualifier::Create(CelValue::CreateBool(false)),
      });

  ASSERT_OK_AND_ASSIGN(std::string string_format, attr.AsString());

  EXPECT_EQ(string_format, "var.qual1[1][-1][false]");
}

}  // namespace
}  // namespace google::api::expr::runtime
