#include "eval/eval/comprehension_step.h"

#include <cstddef>
#include <string>
#include <utility>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "google/protobuf/descriptor.h"
#include "absl/status/status.h"
#include "absl/strings/string_view.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/ident_step.h"
#include "eval/eval/test_type_registry.h"
#include "eval/public/activation.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "internal/status_macros.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {
namespace {

using ::cel::ast::internal::Expr;
using ::cel::ast::internal::Ident;
using ::google::protobuf::ListValue;
using ::google::protobuf::Struct;
using ::google::protobuf::Arena;
using testing::Eq;
using testing::SizeIs;

Ident CreateIdent(const std::string& var) {
  Ident expr;
  expr.set_name(var);
  return expr;
}

class ListKeysStepTest : public testing::Test {
 public:
  ListKeysStepTest() {}

  std::unique_ptr<CelExpressionFlatImpl> MakeExpression(
      ExecutionPath&& path, bool unknown_attributes = false) {
    return std::make_unique<CelExpressionFlatImpl>(
        &dummy_expr_, std::move(path), &TestTypeRegistry(), 0,
        std::set<std::string>(), unknown_attributes, unknown_attributes);
  }

 private:
  Expr dummy_expr_;
};

MATCHER_P(CelStringValue, val, "") {
  const CelValue& to_match = arg;
  absl::string_view value = val;
  return to_match.IsString() && to_match.StringOrDie().value() == value;
}

TEST_F(ListKeysStepTest, ListPassedThrough) {
  ExecutionPath path;
  Ident ident = CreateIdent("var");
  auto result = CreateIdentStep(ident, 0);
  ASSERT_OK(result);
  path.push_back(*std::move(result));
  result = CreateListKeysStep(1);
  ASSERT_OK(result);
  path.push_back(*std::move(result));

  auto expression = MakeExpression(std::move(path));

  Activation activation;
  Arena arena;
  ListValue value;
  value.add_values()->set_number_value(1.0);
  value.add_values()->set_number_value(2.0);
  value.add_values()->set_number_value(3.0);
  activation.InsertValue("var", CelProtoWrapper::CreateMessage(&value, &arena));

  auto eval_result = expression->Evaluate(activation, &arena);

  ASSERT_OK(eval_result);
  ASSERT_TRUE(eval_result->IsList());
  EXPECT_THAT(*eval_result->ListOrDie(), SizeIs(3));
}

TEST_F(ListKeysStepTest, MapToKeyList) {
  ExecutionPath path;
  Ident ident = CreateIdent("var");
  auto result = CreateIdentStep(ident, 0);
  ASSERT_OK(result);
  path.push_back(*std::move(result));
  result = CreateListKeysStep(1);
  ASSERT_OK(result);
  path.push_back(*std::move(result));

  auto expression = MakeExpression(std::move(path));

  Activation activation;
  Arena arena;
  Struct value;
  (*value.mutable_fields())["key1"].set_number_value(1.0);
  (*value.mutable_fields())["key2"].set_number_value(2.0);
  (*value.mutable_fields())["key3"].set_number_value(3.0);

  activation.InsertValue("var", CelProtoWrapper::CreateMessage(&value, &arena));

  auto eval_result = expression->Evaluate(activation, &arena);

  ASSERT_OK(eval_result);
  ASSERT_TRUE(eval_result->IsList());
  EXPECT_THAT(*eval_result->ListOrDie(), SizeIs(3));
  std::vector<CelValue> keys;
  keys.reserve(eval_result->ListOrDie()->size());
  for (int i = 0; i < eval_result->ListOrDie()->size(); i++) {
    keys.push_back(eval_result->ListOrDie()->operator[](i));
  }
  EXPECT_THAT(keys, testing::UnorderedElementsAre(CelStringValue("key1"),
                                                  CelStringValue("key2"),
                                                  CelStringValue("key3")));
}

TEST_F(ListKeysStepTest, MapPartiallyUnknown) {
  ExecutionPath path;
  Ident ident = CreateIdent("var");
  auto result = CreateIdentStep(ident, 0);
  ASSERT_OK(result);
  path.push_back(*std::move(result));
  result = CreateListKeysStep(1);
  ASSERT_OK(result);
  path.push_back(*std::move(result));

  auto expression =
      MakeExpression(std::move(path), /*unknown_attributes=*/true);

  Activation activation;
  Arena arena;
  Struct value;
  (*value.mutable_fields())["key1"].set_number_value(1.0);
  (*value.mutable_fields())["key2"].set_number_value(2.0);
  (*value.mutable_fields())["key3"].set_number_value(3.0);

  activation.InsertValue("var", CelProtoWrapper::CreateMessage(&value, &arena));
  activation.set_unknown_attribute_patterns({CelAttributePattern(
      "var",
      {CelAttributeQualifierPattern::Create(CelValue::CreateStringView("key2")),
       CelAttributeQualifierPattern::Create(CelValue::CreateStringView("foo")),
       CelAttributeQualifierPattern::CreateWildcard()})});

  auto eval_result = expression->Evaluate(activation, &arena);

  ASSERT_OK(eval_result);
  ASSERT_TRUE(eval_result->IsUnknownSet());
  const auto& attrs = eval_result->UnknownSetOrDie()->unknown_attributes();

  EXPECT_THAT(attrs, SizeIs(1));
  EXPECT_THAT(attrs.begin()->variable_name(), Eq("var"));
  EXPECT_THAT(attrs.begin()->qualifier_path(), SizeIs(0));
}

TEST_F(ListKeysStepTest, ErrorPassedThrough) {
  ExecutionPath path;
  Ident ident = CreateIdent("var");
  auto result = CreateIdentStep(ident, 0);
  ASSERT_OK(result);
  path.push_back(*std::move(result));
  result = CreateListKeysStep(1);
  ASSERT_OK(result);
  path.push_back(*std::move(result));

  auto expression = MakeExpression(std::move(path));

  Activation activation;
  Arena arena;

  // Var not in activation, turns into cel error at eval time.
  auto eval_result = expression->Evaluate(activation, &arena);

  ASSERT_OK(eval_result);
  ASSERT_TRUE(eval_result->IsError());
  EXPECT_THAT(eval_result->ErrorOrDie()->message(),
              testing::HasSubstr("\"var\""));
  EXPECT_EQ(eval_result->ErrorOrDie()->code(), absl::StatusCode::kUnknown);
}

TEST_F(ListKeysStepTest, UnknownSetPassedThrough) {
  ExecutionPath path;
  Ident ident = CreateIdent("var");
  auto result = CreateIdentStep(ident, 0);
  ASSERT_OK(result);
  path.push_back(*std::move(result));
  result = CreateListKeysStep(1);
  ASSERT_OK(result);
  path.push_back(*std::move(result));

  auto expression =
      MakeExpression(std::move(path), /*unknown_attributes=*/true);

  Activation activation;
  Arena arena;

  activation.set_unknown_attribute_patterns({CelAttributePattern("var", {})});

  auto eval_result = expression->Evaluate(activation, &arena);

  ASSERT_OK(eval_result);
  ASSERT_TRUE(eval_result->IsUnknownSet());
  EXPECT_THAT(eval_result->UnknownSetOrDie()->unknown_attributes(), SizeIs(1));
}

}  // namespace
}  // namespace google::api::expr::runtime
