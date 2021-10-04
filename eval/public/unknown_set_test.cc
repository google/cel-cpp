#include "eval/public/unknown_set.h"

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/arena.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/unknown_attribute_set.h"
#include "eval/public/unknown_function_result_set.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {
namespace {

using ::google::protobuf::Arena;
using testing::IsEmpty;
using testing::UnorderedElementsAre;

UnknownFunctionResultSet MakeFunctionResult(Arena* arena, int64_t id) {
  CelFunctionDescriptor desc("OneInt", false, {CelValue::Type::kInt64});
  std::vector<CelValue> call_args{CelValue::CreateInt64(id)};
  const auto* function_result = Arena::Create<UnknownFunctionResult>(
      arena, desc, /*expr_id=*/0, call_args);
  return UnknownFunctionResultSet(function_result);
}

UnknownAttributeSet MakeAttribute(Arena* arena, int64_t id) {
  google::api::expr::v1alpha1::Expr expr;
  expr.mutable_ident_expr()->set_name("x");

  std::vector<CelAttributeQualifier> attr_trail{
      CelAttributeQualifier::Create(CelValue::CreateInt64(id))};

  const auto* attr = Arena::Create<CelAttribute>(arena, expr, attr_trail);
  return UnknownAttributeSet({attr});
}

MATCHER_P(UnknownAttributeIs, id, "") {
  const CelAttribute* attr = arg;
  if (attr->qualifier_path().size() != 1) {
    return false;
  }
  auto maybe_qualifier = attr->qualifier_path()[0].GetInt64Key();
  if (!maybe_qualifier.has_value()) {
    return false;
  }
  return maybe_qualifier.value() == id;
}

MATCHER_P(UnknownFunctionResultIs, id, "") {
  const UnknownFunctionResult* result = arg;
  if (result->arguments().size() != 1) {
    return false;
  }
  if (!result->arguments()[0].IsInt64()) {
    return false;
  }
  return result->arguments()[0].Int64OrDie() == id;
}

TEST(UnknownSet, AttributesMerge) {
  Arena arena;
  UnknownSet a(MakeAttribute(&arena, 1));
  UnknownSet b(MakeAttribute(&arena, 2));
  UnknownSet c(MakeAttribute(&arena, 2));
  UnknownSet d(a, b);
  UnknownSet e(c, d);

  EXPECT_THAT(
      d.unknown_attributes().attributes(),
      UnorderedElementsAre(UnknownAttributeIs(1), UnknownAttributeIs(2)));
  EXPECT_THAT(
      e.unknown_attributes().attributes(),
      UnorderedElementsAre(UnknownAttributeIs(1), UnknownAttributeIs(2)));
}

TEST(UnknownSet, FunctionsMerge) {
  Arena arena;

  UnknownSet a(MakeFunctionResult(&arena, 1));
  UnknownSet b(MakeFunctionResult(&arena, 2));
  UnknownSet c(MakeFunctionResult(&arena, 2));
  UnknownSet d(a, b);
  UnknownSet e(c, d);

  EXPECT_THAT(d.unknown_function_results().unknown_function_results(),
              UnorderedElementsAre(UnknownFunctionResultIs(1),
                                   UnknownFunctionResultIs(2)));
  EXPECT_THAT(e.unknown_function_results().unknown_function_results(),
              UnorderedElementsAre(UnknownFunctionResultIs(1),
                                   UnknownFunctionResultIs(2)));
}

TEST(UnknownSet, DefaultEmpty) {
  UnknownSet empty_set;
  EXPECT_THAT(empty_set.unknown_attributes().attributes(), IsEmpty());
  EXPECT_THAT(empty_set.unknown_function_results().unknown_function_results(),
              IsEmpty());
}

TEST(UnknownSet, MixedMerges) {
  Arena arena;

  UnknownSet a(MakeAttribute(&arena, 1), MakeFunctionResult(&arena, 1));
  UnknownSet b(MakeFunctionResult(&arena, 2));
  UnknownSet c(MakeAttribute(&arena, 2));
  UnknownSet d(a, b);
  UnknownSet e(c, d);

  EXPECT_THAT(d.unknown_attributes().attributes(),
              UnorderedElementsAre(UnknownAttributeIs(1)));
  EXPECT_THAT(d.unknown_function_results().unknown_function_results(),
              UnorderedElementsAre(UnknownFunctionResultIs(1),
                                   UnknownFunctionResultIs(2)));
  EXPECT_THAT(
      e.unknown_attributes().attributes(),
      UnorderedElementsAre(UnknownAttributeIs(1), UnknownAttributeIs(2)));
  EXPECT_THAT(e.unknown_function_results().unknown_function_results(),
              UnorderedElementsAre(UnknownFunctionResultIs(1),
                                   UnknownFunctionResultIs(2)));
}

}  // namespace
}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
