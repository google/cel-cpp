#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/text_format.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_value.h"
#include "eval/testutil/test_message.pb.h"
#include "base/status_macros.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using ::google::protobuf::Arena;
using google::protobuf::TextFormat;

using google::api::expr::v1alpha1::Expr;
using google::api::expr::v1alpha1::SourceInfo;

// Simple end-to-end test, which also serves as usage example.
TEST(EndToEndTest, SimpleOnePlusOne) {
  // AST CEL equivalent of "1+var"
  constexpr char kExpr0[] = R"(
    call_expr: <
      function: "_+_"
      args: <
        ident_expr: <
          name: "var"
        >
      >
      args: <
        const_expr: <
          int64_value: 1
        >
      >
    >
  )";

  Expr expr;
  SourceInfo source_info;
  TextFormat::ParseFromString(kExpr0, &expr);

  // Obtain CEL Expression builder.
  std::unique_ptr<CelExpressionBuilder> builder = CreateCelExpressionBuilder();

  // Builtin registration.
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  // Create CelExpression from AST (Expr object).
  auto cel_expression_status = builder->CreateExpression(&expr, &source_info);

  ASSERT_OK(cel_expression_status);

  auto cel_expression = std::move(cel_expression_status.value());

  Activation activation;

  // Bind value to "var" parameter.
  activation.InsertValue("var", CelValue::CreateInt64(1));

  Arena arena;

  // Run evaluation.
  auto eval_status = cel_expression->Evaluate(activation, &arena);

  ASSERT_OK(eval_status);

  CelValue result = eval_status.value();

  ASSERT_TRUE(result.IsInt64());
  EXPECT_EQ(result.Int64OrDie(), 2);
}

// Simple end-to-end test, which also serves as usage example.
TEST(EndToEndTest, EmptyStringCompare) {
  // AST CEL equivalent of "var.string_value == """
  constexpr char kExpr0[] = R"(
    call_expr: <
      function: "_&&_"
      args: <
        call_expr: <
          function: "_==_"
          args: <
            select_expr: <
              operand: <
                ident_expr: <
                  name: "var"
                >
              >
              field: "string_value"
            >
          >
          args: <
            const_expr: <
              string_value: ""
            >
          >
        >
      >
      args: <
        call_expr: <
          function: "_==_"
          args: <
            select_expr: <
              operand: <
                ident_expr: <
                  name: "var"
                >
              >
              field: "int64_value"
            >
          >
          args: <
            const_expr: <
              int64_value: 0
            >
          >
        >
      >
    >
  )";

  Expr expr;
  SourceInfo source_info;
  TextFormat::ParseFromString(kExpr0, &expr);

  // Obtain CEL Expression builder.
  std::unique_ptr<CelExpressionBuilder> builder = CreateCelExpressionBuilder();

  // Builtin registration.
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  // Create CelExpression from AST (Expr object).
  auto cel_expression_status = builder->CreateExpression(&expr, &source_info);

  ASSERT_OK(cel_expression_status);

  auto cel_expression = std::move(cel_expression_status.value());

  Activation activation;

  // Bind value to "var" parameter.
  constexpr char kData[] = R"(
    string_value: ""
    int64_value: 0
  )";
  TestMessage data;
  TextFormat::ParseFromString(kData, &data);
  Arena arena;
  activation.InsertValue("var", CelValue::CreateMessage(&data, &arena));


  // Run evaluation.
  auto eval_status = cel_expression->Evaluate(activation, &arena);

  ASSERT_OK(eval_status);

  CelValue result = eval_status.value();

  ASSERT_TRUE(result.IsBool());
  EXPECT_TRUE(result.BoolOrDie());
}

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
