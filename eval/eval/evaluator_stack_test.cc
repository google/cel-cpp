#include "eval/eval/evaluator_stack.h"

#include "extensions/protobuf/memory_manager.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::extensions::ProtoMemoryManager;

// Test Value Stack Push/Pop operation
TEST(EvaluatorStackTest, StackPushPop) {
  google::protobuf::Arena arena;
  ProtoMemoryManager manager(&arena);
  google::api::expr::v1alpha1::Expr expr;
  expr.mutable_ident_expr()->set_name("name");
  CelAttribute attribute(expr, {});
  EvaluatorStack stack(10);
  stack.Push(CelValue::CreateInt64(1));
  stack.Push(CelValue::CreateInt64(2), AttributeTrail());
  stack.Push(CelValue::CreateInt64(3), AttributeTrail(expr, manager));

  ASSERT_EQ(stack.Peek().Int64OrDie(), 3);
  ASSERT_FALSE(stack.PeekAttribute().empty());
  ASSERT_EQ(stack.PeekAttribute().attribute(), attribute);

  stack.Pop(1);

  ASSERT_EQ(stack.Peek().Int64OrDie(), 2);
  ASSERT_TRUE(stack.PeekAttribute().empty());

  stack.Pop(1);

  ASSERT_EQ(stack.Peek().Int64OrDie(), 1);
  ASSERT_TRUE(stack.PeekAttribute().empty());
}

// Test that inner stacks within value stack retain the equality of their sizes.
TEST(EvaluatorStackTest, StackBalanced) {
  EvaluatorStack stack(10);
  ASSERT_EQ(stack.size(), stack.attribute_size());

  stack.Push(CelValue::CreateInt64(1));
  ASSERT_EQ(stack.size(), stack.attribute_size());
  stack.Push(CelValue::CreateInt64(2), AttributeTrail());
  stack.Push(CelValue::CreateInt64(3), AttributeTrail());
  ASSERT_EQ(stack.size(), stack.attribute_size());

  stack.PopAndPush(CelValue::CreateInt64(4), AttributeTrail());
  ASSERT_EQ(stack.size(), stack.attribute_size());
  stack.PopAndPush(CelValue::CreateInt64(5));
  ASSERT_EQ(stack.size(), stack.attribute_size());

  stack.Pop(3);
  ASSERT_EQ(stack.size(), stack.attribute_size());
}

TEST(EvaluatorStackTest, Clear) {
  EvaluatorStack stack(10);
  ASSERT_EQ(stack.size(), stack.attribute_size());

  stack.Push(CelValue::CreateInt64(1));
  stack.Push(CelValue::CreateInt64(2), AttributeTrail());
  stack.Push(CelValue::CreateInt64(3), AttributeTrail());
  ASSERT_EQ(stack.size(), 3);

  stack.Clear();
  ASSERT_EQ(stack.size(), 0);
  ASSERT_TRUE(stack.empty());
}

TEST(EvaluatorStackTest, CoerceNulls) {
  EvaluatorStack stack(10);
  stack.Push(CelValue::CreateNull());
  stack.Push(CelValue::CreateInt64(0));

  absl::Span<const CelValue> stack_vars = stack.GetSpan(2);

  EXPECT_TRUE(stack_vars.at(0).IsNull());
  EXPECT_FALSE(stack_vars.at(0).IsMessage());
  EXPECT_TRUE(stack_vars.at(1).IsInt64());

  stack.CoerceNullValues(2);
  stack_vars = stack.GetSpan(2);

  EXPECT_TRUE(stack_vars.at(0).IsNull());
  EXPECT_TRUE(stack_vars.at(0).IsMessage());
  EXPECT_TRUE(stack_vars.at(1).IsInt64());
}

}  // namespace

}  // namespace google::api::expr::runtime
