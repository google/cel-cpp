#include "eval/eval/attribute_utility.h"

#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "eval/internal/interop.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_value.h"
#include "eval/public/unknown_attribute_set.h"
#include "eval/public/unknown_set.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

using ::cel::extensions::ProtoMemoryManager;
using ::cel::interop_internal::CreateBoolValue;
using ::cel::interop_internal::CreateIntValue;
using ::cel::interop_internal::CreateUnknownValueFromView;
using ::google::api::expr::v1alpha1::Expr;
using testing::Eq;
using testing::NotNull;
using testing::SizeIs;
using testing::UnorderedPointwise;

TEST(UnknownsUtilityTest, UnknownsUtilityCheckUnknowns) {
  google::protobuf::Arena arena;
  ProtoMemoryManager manager(&arena);
  std::vector<CelAttributePattern> unknown_patterns = {
      CelAttributePattern("unknown0", {CreateCelAttributeQualifierPattern(
                                          CelValue::CreateInt64(1))}),
      CelAttributePattern("unknown0", {CreateCelAttributeQualifierPattern(
                                          CelValue::CreateInt64(2))}),
      CelAttributePattern("unknown1", {}),
      CelAttributePattern("unknown2", {}),
  };

  std::vector<CelAttributePattern> missing_attribute_patterns;

  AttributeUtility utility(unknown_patterns, missing_attribute_patterns,
                           manager);
  // no match for void trail
  ASSERT_FALSE(utility.CheckForUnknown(AttributeTrail(), true));
  ASSERT_FALSE(utility.CheckForUnknown(AttributeTrail(), false));

  AttributeTrail unknown_trail0("unknown0");

  { ASSERT_FALSE(utility.CheckForUnknown(unknown_trail0, false)); }

  { ASSERT_TRUE(utility.CheckForUnknown(unknown_trail0, true)); }

  {
    ASSERT_TRUE(utility.CheckForUnknown(
        unknown_trail0.Step(
            CreateCelAttributeQualifier(CelValue::CreateInt64(1))),
        false));
  }

  {
    ASSERT_TRUE(utility.CheckForUnknown(
        unknown_trail0.Step(
            CreateCelAttributeQualifier(CelValue::CreateInt64(1))),
        true));
  }
}

TEST(UnknownsUtilityTest, UnknownsUtilityMergeUnknownsFromValues) {
  google::protobuf::Arena arena;
  ProtoMemoryManager manager(&arena);

  std::vector<CelAttributePattern> unknown_patterns;

  std::vector<CelAttributePattern> missing_attribute_patterns;

  CelAttribute attribute0("unknown0", {});
  CelAttribute attribute1("unknown1", {});
  CelAttribute attribute2("unknown2", {});

  AttributeUtility utility(unknown_patterns, missing_attribute_patterns,
                           manager);

  UnknownSet unknown_set0(UnknownAttributeSet({attribute0}));
  UnknownSet unknown_set1(UnknownAttributeSet({attribute1}));
  UnknownSet unknown_set2(UnknownAttributeSet({attribute1, attribute2}));
  std::vector<cel::Handle<cel::Value>> values = {
      CreateUnknownValueFromView(&unknown_set0),
      CreateUnknownValueFromView(&unknown_set1),
      CreateBoolValue(true),
      CreateIntValue(1),
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

  AttributeTrail trail0("unknown0");
  AttributeTrail trail1("unknown1");

  CelAttribute attribute1("unknown1", {});
  UnknownSet unknown_set1(UnknownAttributeSet({attribute1}));

  AttributeUtility utility(unknown_patterns, missing_attribute_patterns,
                           manager);

  UnknownSet unknown_attr_set(utility.CheckForUnknowns(
      {
          AttributeTrail(),  // To make sure we handle empty trail gracefully.
          trail0.Step(CreateCelAttributeQualifier(CelValue::CreateInt64(1))),
          trail0.Step(CreateCelAttributeQualifier(CelValue::CreateInt64(2))),
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

  AttributeTrail trail("destination");
  trail =
      trail.Step(CreateCelAttributeQualifier(CelValue::CreateStringView("ip")));

  AttributeUtility utility0(unknown_patterns, missing_attribute_patterns,
                            manager);
  EXPECT_FALSE(utility0.CheckForMissingAttribute(trail));

  missing_attribute_patterns.push_back(CelAttributePattern(
      "destination",
      {CreateCelAttributeQualifierPattern(CelValue::CreateStringView("ip"))}));

  AttributeUtility utility1(unknown_patterns, missing_attribute_patterns,
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

  AttributeTrail trail("destination");
  trail =
      trail.Step(CreateCelAttributeQualifier(CelValue::CreateStringView("ip")));

  std::vector<CelAttributePattern> empty_patterns;
  AttributeUtility utility(empty_patterns, empty_patterns, manager);

  const UnknownSet* set = utility.CreateUnknownSet(trail.attribute());
  EXPECT_EQ(*set->unknown_attributes().begin()->AsString(), "destination.ip");
}

}  // namespace google::api::expr::runtime
