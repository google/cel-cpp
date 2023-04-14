#include "eval/eval/ident_step.h"

#include <memory>
#include <string>
#include <utility>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/descriptor.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/test_type_registry.h"
#include "eval/public/activation.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "runtime/runtime_options.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::ast::internal::Expr;
using ::google::protobuf::Arena;
using testing::Eq;

TEST(IdentStepTest, TestIdentStep) {
  Expr expr;
  auto& ident_expr = expr.mutable_ident_expr();
  ident_expr.set_name("name0");

  ASSERT_OK_AND_ASSIGN(auto step, CreateIdentStep(ident_expr, expr.id()));

  ExecutionPath path;
  path.push_back(std::move(step));

  CelExpressionFlatImpl impl(std::move(path), &TestTypeRegistry(),
                             cel::RuntimeOptions{});

  Activation activation;
  Arena arena;
  std::string value("test");

  activation.InsertValue("name0", CelValue::CreateString(&value));
  auto status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  CelValue result = status0.value();

  ASSERT_TRUE(result.IsString());
  EXPECT_THAT(result.StringOrDie().value(), Eq("test"));
}

TEST(IdentStepTest, TestIdentStepNameNotFound) {
  Expr expr;
  auto& ident_expr = expr.mutable_ident_expr();
  ident_expr.set_name("name0");

  ASSERT_OK_AND_ASSIGN(auto step, CreateIdentStep(ident_expr, expr.id()));

  ExecutionPath path;
  path.push_back(std::move(step));

  CelExpressionFlatImpl impl(std::move(path), &TestTypeRegistry(),
                             cel::RuntimeOptions{});

  Activation activation;
  Arena arena;
  std::string value("test");

  auto status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  CelValue result = status0.value();
  ASSERT_TRUE(result.IsError());
}

TEST(IdentStepTest, DisableMissingAttributeErrorsOK) {
  Expr expr;
  auto& ident_expr = expr.mutable_ident_expr();
  ident_expr.set_name("name0");

  ASSERT_OK_AND_ASSIGN(auto step, CreateIdentStep(ident_expr, expr.id()));

  ExecutionPath path;
  path.push_back(std::move(step));
  cel::RuntimeOptions options;
  options.unknown_processing = cel::UnknownProcessingOptions::kDisabled;
  CelExpressionFlatImpl impl(std::move(path), &TestTypeRegistry(), options);

  Activation activation;
  Arena arena;
  std::string value("test");

  activation.InsertValue("name0", CelValue::CreateString(&value));
  auto status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  CelValue result = status0.value();

  ASSERT_TRUE(result.IsString());
  EXPECT_THAT(result.StringOrDie().value(), Eq("test"));

  const CelAttributePattern pattern("name0", {});
  activation.set_missing_attribute_patterns({pattern});

  status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  EXPECT_THAT(status0->StringOrDie().value(), Eq("test"));
}

TEST(IdentStepTest, TestIdentStepMissingAttributeErrors) {
  Expr expr;
  auto& ident_expr = expr.mutable_ident_expr();
  ident_expr.set_name("name0");

  ASSERT_OK_AND_ASSIGN(auto step, CreateIdentStep(ident_expr, expr.id()));

  ExecutionPath path;
  path.push_back(std::move(step));

  cel::RuntimeOptions options;
  options.unknown_processing = cel::UnknownProcessingOptions::kDisabled;
  options.enable_missing_attribute_errors = true;

  CelExpressionFlatImpl impl(std::move(path), &TestTypeRegistry(), options);

  Activation activation;
  Arena arena;
  std::string value("test");

  activation.InsertValue("name0", CelValue::CreateString(&value));
  auto status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  CelValue result = status0.value();

  ASSERT_TRUE(result.IsString());
  EXPECT_THAT(result.StringOrDie().value(), Eq("test"));

  CelAttributePattern pattern("name0", {});
  activation.set_missing_attribute_patterns({pattern});

  status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  EXPECT_EQ(status0->ErrorOrDie()->code(), absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(status0->ErrorOrDie()->message(), "MissingAttributeError: name0");
}

TEST(IdentStepTest, TestIdentStepUnknownAttribute) {
  Expr expr;
  auto& ident_expr = expr.mutable_ident_expr();
  ident_expr.set_name("name0");

  ASSERT_OK_AND_ASSIGN(auto step, CreateIdentStep(ident_expr, expr.id()));

  ExecutionPath path;
  path.push_back(std::move(step));

  // Expression with unknowns enabled.
  cel::RuntimeOptions options;
  options.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;
  CelExpressionFlatImpl impl(std::move(path), &TestTypeRegistry(), options);

  Activation activation;
  Arena arena;
  std::string value("test");

  activation.InsertValue("name0", CelValue::CreateString(&value));
  std::vector<CelAttributePattern> unknown_patterns;
  unknown_patterns.push_back(CelAttributePattern("name_bad", {}));

  activation.set_unknown_attribute_patterns(unknown_patterns);
  auto status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  CelValue result = status0.value();

  ASSERT_TRUE(result.IsString());
  EXPECT_THAT(result.StringOrDie().value(), Eq("test"));

  unknown_patterns.push_back(CelAttributePattern("name0", {}));

  activation.set_unknown_attribute_patterns(unknown_patterns);
  status0 = impl.Evaluate(activation, &arena);
  ASSERT_OK(status0);

  result = status0.value();

  ASSERT_TRUE(result.IsUnknownSet());
}

}  // namespace

}  // namespace google::api::expr::runtime
