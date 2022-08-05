#include "eval/public/unknown_attribute_set.h"

#include <memory>
#include <string>

#include "eval/public/cel_attribute.h"
#include "eval/public/cel_value.h"
#include "internal/testing.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using testing::Eq;

using google::api::expr::v1alpha1::Expr;

TEST(UnknownAttributeSetTest, TestCreate) {
  Expr expr;
  expr.mutable_ident_expr()->set_name("root");

  const std::string kAttr1 = "a1";
  const std::string kAttr2 = "a2";
  const std::string kAttr3 = "a3";

  std::shared_ptr<CelAttribute> cel_attr = std::make_shared<CelAttribute>(
      expr, std::vector<CelAttributeQualifier>(
                {CelAttributeQualifier::Create(CelValue::CreateString(&kAttr1)),
                 CelAttributeQualifier::Create(CelValue::CreateInt64(1)),
                 CelAttributeQualifier::Create(CelValue::CreateUint64(2)),
                 CelAttributeQualifier::Create(CelValue::CreateBool(true))}));

  UnknownAttributeSet unknown_set({*cel_attr});
  EXPECT_THAT(unknown_set.size(), Eq(1));
  EXPECT_THAT(*(unknown_set.begin()), Eq(*cel_attr));
}

TEST(UnknownAttributeSetTest, TestMergeSets) {
  Expr expr;
  expr.mutable_ident_expr()->set_name("root");

  const std::string kAttr1 = "a1";
  const std::string kAttr2 = "a2";
  const std::string kAttr3 = "a3";

  CelAttribute cel_attr1(
      expr, std::vector<CelAttributeQualifier>(
                {CelAttributeQualifier::Create(CelValue::CreateString(&kAttr1)),
                 CelAttributeQualifier::Create(CelValue::CreateInt64(1)),
                 CelAttributeQualifier::Create(CelValue::CreateUint64(2)),
                 CelAttributeQualifier::Create(CelValue::CreateBool(true))}));

  CelAttribute cel_attr1_copy(
      expr, std::vector<CelAttributeQualifier>(
                {CelAttributeQualifier::Create(CelValue::CreateString(&kAttr1)),
                 CelAttributeQualifier::Create(CelValue::CreateInt64(1)),
                 CelAttributeQualifier::Create(CelValue::CreateUint64(2)),
                 CelAttributeQualifier::Create(CelValue::CreateBool(true))}));

  CelAttribute cel_attr2(
      expr, std::vector<CelAttributeQualifier>(
                {CelAttributeQualifier::Create(CelValue::CreateString(&kAttr1)),
                 CelAttributeQualifier::Create(CelValue::CreateInt64(2)),
                 CelAttributeQualifier::Create(CelValue::CreateUint64(2)),
                 CelAttributeQualifier::Create(CelValue::CreateBool(true))}));

  CelAttribute cel_attr3(
      expr, std::vector<CelAttributeQualifier>(
                {CelAttributeQualifier::Create(CelValue::CreateString(&kAttr1)),
                 CelAttributeQualifier::Create(CelValue::CreateInt64(2)),
                 CelAttributeQualifier::Create(CelValue::CreateUint64(2)),
                 CelAttributeQualifier::Create(CelValue::CreateBool(false))}));

  UnknownAttributeSet unknown_set1({cel_attr1, cel_attr2});
  UnknownAttributeSet unknown_set2({cel_attr1_copy, cel_attr3});

  UnknownAttributeSet unknown_set3 =
      UnknownAttributeSet::Merge(unknown_set1, unknown_set2);

  EXPECT_THAT(unknown_set3.size(), Eq(3));
  std::vector<CelAttribute> attrs1;
  for (const auto& attr_ptr : unknown_set3) {
    attrs1.push_back(attr_ptr);
  }

  std::vector<CelAttribute> attrs2 = {cel_attr1, cel_attr2, cel_attr3};

  EXPECT_THAT(attrs1, testing::UnorderedPointwise(Eq(), attrs2));
}

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
