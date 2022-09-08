#include "eval/eval/evaluator_core.h"

#include <string>
#include <utility>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/descriptor.h"
#include "eval/compiler/flat_expr_builder.h"
#include "eval/eval/attribute_trail.h"
#include "eval/eval/test_type_registry.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_value.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

using ::cel::extensions::ProtoMemoryManager;
using ::google::api::expr::v1alpha1::Expr;
using ::google::api::expr::runtime::RegisterBuiltinFunctions;
using testing::_;
using testing::Eq;

// Fake expression implementation
// Pushes int64_t(0) on top of value stack.
class FakeConstExpressionStep : public ExpressionStep {
 public:
  absl::Status Evaluate(ExecutionFrame* frame) const override {
    frame->value_stack().Push(CelValue::CreateInt64(0));
    return absl::OkStatus();
  }

  int64_t id() const override { return 0; }

  bool ComesFromAst() const override { return true; }
};

// Fake expression implementation
// Increments argument on top of the stack.
class FakeIncrementExpressionStep : public ExpressionStep {
 public:
  absl::Status Evaluate(ExecutionFrame* frame) const override {
    CelValue value = frame->value_stack().Peek();
    frame->value_stack().Pop(1);
    EXPECT_TRUE(value.IsInt64());
    int64_t val = value.Int64OrDie();
    frame->value_stack().Push(CelValue::CreateInt64(val + 1));
    return absl::OkStatus();
  }

  int64_t id() const override { return 0; }

  bool ComesFromAst() const override { return true; }
};

TEST(EvaluatorCoreTest, ExecutionFrameNext) {
  ExecutionPath path;
  auto const_step = absl::make_unique<const FakeConstExpressionStep>();
  auto incr_step1 = absl::make_unique<const FakeIncrementExpressionStep>();
  auto incr_step2 = absl::make_unique<const FakeIncrementExpressionStep>();

  path.push_back(std::move(const_step));
  path.push_back(std::move(incr_step1));
  path.push_back(std::move(incr_step2));

  auto dummy_expr = absl::make_unique<Expr>();

  Activation activation;
  CelExpressionFlatEvaluationState state(path.size(), {}, nullptr);
  ExecutionFrame frame(path, activation, &TestTypeRegistry(), 0, &state,
                       /*enable_unknowns=*/false,
                       /*enable_unknown_funcion_results=*/false,
                       /*enable_missing_attribute_errors=*/false,
                       /*enable_null_coercion=*/true,
                       /*enable_heterogeneous_numeric_lookups=*/true);

  EXPECT_THAT(frame.Next(), Eq(path[0].get()));
  EXPECT_THAT(frame.Next(), Eq(path[1].get()));
  EXPECT_THAT(frame.Next(), Eq(path[2].get()));
  EXPECT_THAT(frame.Next(), Eq(nullptr));
}

// Test the set, get, and clear functions for "IterVar" on ExecutionFrame
TEST(EvaluatorCoreTest, ExecutionFrameSetGetClearVar) {
  const std::string test_iter_var = "test_iter_var";
  const std::string test_accu_var = "test_accu_var";
  const int64_t test_value = 0xF00F00;

  Activation activation;
  google::protobuf::Arena arena;
  ProtoMemoryManager manager(&arena);
  ExecutionPath path;
  CelExpressionFlatEvaluationState state(path.size(), {test_iter_var}, nullptr);
  ExecutionFrame frame(path, activation, &TestTypeRegistry(), 0, &state,
                       /*enable_unknowns=*/false,
                       /*enable_unknown_funcion_results=*/false,
                       /*enable_missing_attribute_errors=*/false,
                       /*enable_null_coercion=*/true,
                       /*enable_heterogeneous_numeric_lookups=*/true);

  CelValue original = CelValue::CreateInt64(test_value);
  Expr ident;
  ident.mutable_ident_expr()->set_name("var");

  AttributeTrail original_trail =
      AttributeTrail(ident, manager)
          .Step(CelAttributeQualifier::Create(CelValue::CreateInt64(1)),
                manager);
  CelValue result;
  const AttributeTrail* trail;

  ASSERT_OK(frame.PushIterFrame(test_iter_var, test_accu_var));

  // Nothing is there yet
  ASSERT_FALSE(frame.GetIterVar(test_iter_var, &result));
  ASSERT_OK(frame.SetIterVar(original, original_trail));

  // Nothing is there yet
  ASSERT_FALSE(frame.GetIterVar(test_accu_var, &result));
  ASSERT_OK(frame.SetAccuVar(CelValue::CreateBool(true)));
  ASSERT_TRUE(frame.GetIterVar(test_accu_var, &result));
  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), true);

  // Make sure its now there
  ASSERT_TRUE(frame.GetIterVar(test_iter_var, &result));
  ASSERT_TRUE(frame.GetIterAttr(test_iter_var, &trail));

  int64_t result_value;
  ASSERT_TRUE(result.GetValue(&result_value));
  EXPECT_EQ(test_value, result_value);
  ASSERT_TRUE(trail->attribute().has_variable_name());
  ASSERT_EQ(trail->attribute().variable_name(), "var");

  // Test that it goes away properly
  ASSERT_OK(frame.ClearIterVar());
  ASSERT_FALSE(frame.GetIterVar(test_iter_var, &result));
  ASSERT_FALSE(frame.GetIterAttr(test_iter_var, &trail));

  ASSERT_OK(frame.PopIterFrame());

  // Access on empty stack ok, but no value.
  ASSERT_FALSE(frame.GetIterVar(test_iter_var, &result));

  // Pop empty stack
  ASSERT_FALSE(frame.PopIterFrame().ok());

  // Updates on empty stack not ok.
  ASSERT_FALSE(frame.SetIterVar(original).ok());
}

TEST(EvaluatorCoreTest, SimpleEvaluatorTest) {
  ExecutionPath path;
  auto const_step = absl::make_unique<FakeConstExpressionStep>();
  auto incr_step1 = absl::make_unique<FakeIncrementExpressionStep>();
  auto incr_step2 = absl::make_unique<FakeIncrementExpressionStep>();

  path.push_back(std::move(const_step));
  path.push_back(std::move(incr_step1));
  path.push_back(std::move(incr_step2));

  auto dummy_expr = absl::make_unique<cel::ast::internal::Expr>();

  CelExpressionFlatImpl impl(dummy_expr.get(), std::move(path),
                             &TestTypeRegistry(), 0, {});

  Activation activation;
  google::protobuf::Arena arena;

  auto status = impl.Evaluate(activation, &arena);
  EXPECT_OK(status);

  auto value = status.value();
  EXPECT_TRUE(value.IsInt64());
  EXPECT_THAT(value.Int64OrDie(), Eq(2));
}

class MockTraceCallback {
 public:
  MOCK_METHOD(void, Call,
              (int64_t expr_id, const CelValue& value, google::protobuf::Arena*));
};

TEST(EvaluatorCoreTest, TraceTest) {
  Expr expr;
  google::api::expr::v1alpha1::SourceInfo source_info;

  // 1 && [1,2,3].all(x, x > 0)

  expr.set_id(1);
  auto and_call = expr.mutable_call_expr();
  and_call->set_function("_&&_");

  auto true_expr = and_call->add_args();
  true_expr->set_id(2);
  true_expr->mutable_const_expr()->set_int64_value(1);

  auto comp_expr = and_call->add_args();
  comp_expr->set_id(3);
  auto comp = comp_expr->mutable_comprehension_expr();
  comp->set_iter_var("x");
  comp->set_accu_var("accu");

  auto list_expr = comp->mutable_iter_range();
  list_expr->set_id(4);
  auto el1_expr = list_expr->mutable_list_expr()->add_elements();
  el1_expr->set_id(11);
  el1_expr->mutable_const_expr()->set_int64_value(1);
  auto el2_expr = list_expr->mutable_list_expr()->add_elements();
  el2_expr->set_id(12);
  el2_expr->mutable_const_expr()->set_int64_value(2);
  auto el3_expr = list_expr->mutable_list_expr()->add_elements();
  el3_expr->set_id(13);
  el3_expr->mutable_const_expr()->set_int64_value(3);

  auto accu_init_expr = comp->mutable_accu_init();
  accu_init_expr->set_id(20);
  accu_init_expr->mutable_const_expr()->set_bool_value(true);

  auto loop_cond_expr = comp->mutable_loop_condition();
  loop_cond_expr->set_id(21);
  loop_cond_expr->mutable_const_expr()->set_bool_value(true);

  auto loop_step_expr = comp->mutable_loop_step();
  loop_step_expr->set_id(22);
  auto condition = loop_step_expr->mutable_call_expr();
  condition->set_function("_>_");

  auto iter_expr = condition->add_args();
  iter_expr->set_id(23);
  iter_expr->mutable_ident_expr()->set_name("x");

  auto zero_expr = condition->add_args();
  zero_expr->set_id(24);
  zero_expr->mutable_const_expr()->set_int64_value(0);

  auto result_expr = comp->mutable_result();
  result_expr->set_id(25);
  result_expr->mutable_const_expr()->set_bool_value(true);

  FlatExprBuilder builder;
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  builder.set_shortcircuiting(false);
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder.CreateExpression(&expr, &source_info));

  Activation activation;
  google::protobuf::Arena arena;

  MockTraceCallback callback;

  EXPECT_CALL(callback, Call(accu_init_expr->id(), _, &arena));
  EXPECT_CALL(callback, Call(el1_expr->id(), _, &arena));
  EXPECT_CALL(callback, Call(el2_expr->id(), _, &arena));
  EXPECT_CALL(callback, Call(el3_expr->id(), _, &arena));

  EXPECT_CALL(callback, Call(list_expr->id(), _, &arena));

  EXPECT_CALL(callback, Call(loop_cond_expr->id(), _, &arena)).Times(3);
  EXPECT_CALL(callback, Call(iter_expr->id(), _, &arena)).Times(3);
  EXPECT_CALL(callback, Call(zero_expr->id(), _, &arena)).Times(3);
  EXPECT_CALL(callback, Call(loop_step_expr->id(), _, &arena)).Times(3);

  EXPECT_CALL(callback, Call(result_expr->id(), _, &arena));
  EXPECT_CALL(callback, Call(comp_expr->id(), _, &arena));
  EXPECT_CALL(callback, Call(true_expr->id(), _, &arena));
  EXPECT_CALL(callback, Call(expr.id(), _, &arena));

  auto eval_status = cel_expr->Trace(
      activation, &arena,
      [&](int64_t expr_id, const CelValue& value, google::protobuf::Arena* arena) {
        callback.Call(expr_id, value, arena);
        return absl::OkStatus();
      });
  ASSERT_OK(eval_status);
}

}  // namespace google::api::expr::runtime
