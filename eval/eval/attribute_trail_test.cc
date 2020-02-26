#include "eval/eval/attribute_trail.h"

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

// Attribute Trail behavior
TEST(AttributeTrailTest, AttributeTrailEmptyStep) {
  google::protobuf::Arena arena;
  std::string step = "step";
  CelValue step_value = CelValue::CreateString(&step);
  AttributeTrail trail;
  ASSERT_TRUE(trail.Step(&step, &arena).empty());
  ASSERT_TRUE(
      trail.Step(CelAttributeQualifier::Create(step_value), &arena).empty());
}

TEST(AttributeTrailTest, AttributeTrailStep) {
  google::protobuf::Arena arena;
  std::string step = "step";
  CelValue step_value = CelValue::CreateString(&step);
  google::api::expr::v1alpha1::Expr root;
  root.mutable_ident_expr()->set_name("ident");
  AttributeTrail trail = AttributeTrail(root, &arena).Step(&step, &arena);

  ASSERT_TRUE(trail.attribute() != nullptr);
  ASSERT_EQ(*trail.attribute(),
            CelAttribute(root, {CelAttributeQualifier::Create(step_value)}));
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
