#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/field_mask.pb.h"
#include "google/protobuf/text_format.h"
#include "absl/status/status.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "eval/compiler/flat_expr_builder.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/testing/matchers.h"
#include "eval/public/unknown_attribute_set.h"
#include "eval/public/unknown_set.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "parser/parser.h"

namespace google::api::expr::runtime {

namespace {

using google::api::expr::v1alpha1::CheckedExpr;
using testing::HasSubstr;
using cel::internal::StatusIs;

TEST(FlatExprBuilderComprehensionsTest, NestedComp) {
  FlatExprBuilder builder;
  builder.set_enable_comprehension_list_append(true);

  ASSERT_OK_AND_ASSIGN(auto parsed_expr,
                       parser::Parse("[1, 2].filter(x, [3, 4].all(y, x < y))"));
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder.CreateExpression(&parsed_expr.expr(),
                                                &parsed_expr.source_info()));

  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsList());
  EXPECT_THAT(*result.ListOrDie(), testing::SizeIs(2));
}

TEST(FlatExprBuilderComprehensionsTest, MapComp) {
  FlatExprBuilder builder;
  builder.set_enable_comprehension_list_append(true);

  ASSERT_OK_AND_ASSIGN(auto parsed_expr, parser::Parse("[1, 2].map(x, x * 2)"));
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder.CreateExpression(&parsed_expr.expr(),
                                                &parsed_expr.source_info()));

  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsList());
  EXPECT_THAT(*result.ListOrDie(), testing::SizeIs(2));
  EXPECT_THAT((*result.ListOrDie())[0],
              test::EqualsCelValue(CelValue::CreateInt64(2)));
  EXPECT_THAT((*result.ListOrDie())[1],
              test::EqualsCelValue(CelValue::CreateInt64(4)));
}

TEST(FlatExprBuilderComprehensionsTest, InvalidComprehensionWithRewrite) {
  CheckedExpr expr;
  // The rewrite step which occurs when an identifier gets a more qualified name
  // from the reference map has the potential to make invalid comprehensions
  // appear valid, by populating missing fields with default values.
  // var.<macro>(x, <missing>)
  google::protobuf::TextFormat::ParseFromString(R"pb(
                                        reference_map {
                                          key: 1
                                          value { name: "qualified.var" }
                                        }
                                        expr {
                                          comprehension_expr {
                                            iter_var: "x"
                                            iter_range {
                                              id: 1
                                              ident_expr { name: "var" }
                                            }
                                            accu_var: "y"
                                            accu_init {
                                              id: 1
                                              const_expr { bool_value: true }
                                            }
                                          }
                                        })pb",
                                      &expr);

  FlatExprBuilder builder;
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  EXPECT_THAT(builder.CreateExpression(&expr).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid comprehension")));
}

}  // namespace

}  // namespace google::api::expr::runtime
