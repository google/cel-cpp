#include "eval/eval/attribute_utility.h"

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_value.h"
#include "eval/public/unknown_attribute_set.h"
#include "eval/public/unknown_set.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

using ::cel::extensions::ProtoMemoryManager;
using ::google::api::expr::v1alpha1::Expr;
using testing::Eq;
using testing::NotNull;
using testing::SizeIs;
using testing::UnorderedPointwise;

TEST(UnknownsUtilityTest, UnknownsUtilityCheckUnknowns) {
  google::protobuf::Arena arena;
  ProtoMemoryManager manager(&arena);
  std::vector<CelAttributePattern> unknown_patterns = {
      CelAttributePattern("unknown0", {CelAttributeQualifierPattern::Create(
                                          CelValue::CreateInt64(1))}),
      CelAttributePattern("unknown0", {CelAttributeQualifierPattern::Create(
                                          CelValue::CreateInt64(2))}),
      CelAttributePattern("unknown1", {}),
      CelAttributePattern("unknown2", {}),
  };

  std::vector<CelAttributePattern> missing_attribute_patterns;

  AttributeUtility utility(&unknown_patterns, &missing_attribute_patterns,
                           manager);
  // no match for void trail
  ASSERT_FALSE(utility.CheckForUnknown(AttributeTrail(), true));
  ASSERT_FALSE(utility.CheckForUnknown(AttributeTrail(), false));

  google::api::expr::v1alpha1::Expr unknown_expr0;
  unknown_expr0.mutable_ident_expr()->set_name("unknown0");

  AttributeTrail unknown_trail0(unknown_expr0, manager);

  { ASSERT_FALSE(utility.CheckForUnknown(unknown_trail0, false)); }

  { ASSERT_TRUE(utility.CheckForUnknown(unknown_trail0, true)); }

  {
    ASSERT_TRUE(utility.CheckForUnknown(
        unknown_trail0.Step(
            CelAttributeQualifier::Create(CelValue::CreateInt64(1)), manager),
        false));
  }

  {
    ASSERT_TRUE(utility.CheckForUnknown(
        unknown_trail0.Step(
            CelAttributeQualifier::Create(CelValue::CreateInt64(1)), manager),
        true));
  }
}

TEST(UnknownsUtilityTest, UnknownsUtilityMergeUnknownsFromValues) {
  google::protobuf::Arena arena;
  ProtoMemoryManager manager(&arena);

  google::api::expr::v1alpha1::Expr unknown_expr0;
  unknown_expr0.mutable_ident_expr()->set_name("unknown0");

  google::api::expr::v1alpha1::Expr unknown_expr1;
  unknown_expr1.mutable_ident_expr()->set_name("unknown1");

  google::api::expr::v1alpha1::Expr unknown_expr2;
  unknown_expr2.mutable_ident_expr()->set_name("unknown2");

  std::vector<CelAttributePattern> unknown_patterns;

  std::vector<CelAttributePattern> missing_attribute_patterns;

  CelAttribute attribute0(unknown_expr0, {});
  CelAttribute attribute1(unknown_expr1, {});
  CelAttribute attribute2(unknown_expr2, {});

  AttributeUtility utility(&unknown_patterns, &missing_attribute_patterns,
                           manager);

  UnknownSet unknown_set0(UnknownAttributeSet({attribute0}));
  UnknownSet unknown_set1(UnknownAttributeSet({attribute1}));
  UnknownSet unknown_set2(UnknownAttributeSet({attribute1, attribute2}));
  std::vector<CelValue> values = {
      CelValue::CreateUnknownSet(&unknown_set0),
      CelValue::CreateUnknownSet(&unknown_set1),
      CelValue::CreateBool(true),
      CelValue::CreateInt64(1),
  };

  const UnknownSet* unknown_set = utility.MergeUnknowns(values, nullptr);
  ASSERT_THAT(unknown_set, NotNull());
  ASSERT_THAT(unknown_set->unknown_attributes(),
              UnorderedPointwise(
                  Eq(), std::vector<CelAttribute>{attribute0, attribute1}));

  unknown_set = utility.MergeUnknowns(values, &unknown_set2);
  ASSERT_THAT(unknown_set, NotNull());
  ASSERT_THAT(
      unknown_set->unknown_attributes(),
      UnorderedPointwise(
          Eq(), std::vector<CelAttribute>{attribute0, attribute1, attribute2}));
}

TEST(UnknownsUtilityTest, UnknownsUtilityCheckForUnknownsFromAttributes) {
  google::protobuf::Arena arena;
  ProtoMemoryManager manager(&arena);

  std::vector<CelAttributePattern> unknown_patterns = {
      CelAttributePattern("unknown0",
                          {CelAttributeQualifierPattern::CreateWildcard()}),
  };

  std::vector<CelAttributePattern> missing_attribute_patterns;

  google::api::expr::v1alpha1::Expr unknown_expr0;
  unknown_expr0.mutable_ident_expr()->set_name("unknown0");

  google::api::expr::v1alpha1::Expr unknown_expr1;
  unknown_expr1.mutable_ident_expr()->set_name("unknown1");

  AttributeTrail trail0(unknown_expr0, manager);
  AttributeTrail trail1(unknown_expr1, manager);

  CelAttribute attribute1(unknown_expr1, {});
  UnknownSet unknown_set1(UnknownAttributeSet({attribute1}));

  AttributeUtility utility(&unknown_patterns, &missing_attribute_patterns,
                           manager);

  UnknownSet unknown_attr_set(utility.CheckForUnknowns(
      {
          AttributeTrail(),  // To make sure we handle empty trail gracefully.
          trail0.Step(CelAttributeQualifier::Create(CelValue::CreateInt64(1)),
                      manager),
          trail0.Step(CelAttributeQualifier::Create(CelValue::CreateInt64(2)),
                      manager),
      },
      false));

  UnknownSet unknown_set(unknown_set1, unknown_attr_set);

  ASSERT_THAT(unknown_set.unknown_attributes(), SizeIs(3));
}

TEST(UnknownsUtilityTest, UnknownsUtilityCheckForMissingAttributes) {
  google::protobuf::Arena arena;
  ProtoMemoryManager manager(&arena);

  std::vector<CelAttributePattern> unknown_patterns;

  std::vector<CelAttributePattern> missing_attribute_patterns;

  Expr expr;
  auto* select_expr = expr.mutable_select_expr();
  select_expr->set_field("ip");

  Expr* ident_expr = select_expr->mutable_operand();
  ident_expr->mutable_ident_expr()->set_name("destination");

  AttributeTrail trail(*ident_expr, manager);
  trail = trail.Step(
      CelAttributeQualifier::Create(CelValue::CreateStringView("ip")), manager);

  AttributeUtility utility0(&unknown_patterns, &missing_attribute_patterns,
                            manager);
  EXPECT_FALSE(utility0.CheckForMissingAttribute(trail));

  missing_attribute_patterns.push_back(CelAttributePattern(
      "destination", {CelAttributeQualifierPattern::Create(
                         CelValue::CreateStringView("ip"))}));

  AttributeUtility utility1(&unknown_patterns, &missing_attribute_patterns,
                            manager);
  EXPECT_TRUE(utility1.CheckForMissingAttribute(trail));
}

TEST(AttributeUtilityTest, CreateUnknownSet) {
  google::protobuf::Arena arena;
  ProtoMemoryManager manager(&arena);

  Expr expr;
  auto* select_expr = expr.mutable_select_expr();
  select_expr->set_field("ip");

  Expr* ident_expr = select_expr->mutable_operand();
  ident_expr->mutable_ident_expr()->set_name("destination");

  AttributeTrail trail(*ident_expr, manager);
  trail = trail.Step(
      CelAttributeQualifier::Create(CelValue::CreateStringView("ip")), manager);

  std::vector<CelAttributePattern> empty_patterns;
  AttributeUtility utility(&empty_patterns, &empty_patterns, manager);

  const UnknownSet* set = utility.CreateUnknownSet(trail.attribute());
  EXPECT_EQ(*set->unknown_attributes().begin()->AsString(), "destination.ip");
}

}  // namespace google::api::expr::runtime
