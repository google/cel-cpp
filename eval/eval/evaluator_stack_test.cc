#include "eval/eval/evaluator_stack.h"

#include "base/value.h"
#include "eval/internal/interop.h"
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
  EvaluatorStack stack(10, manager);
  stack.Push(CelValue::CreateInt64(1));
  stack.Push(CelValue::CreateInt64(2), AttributeTrail());
  stack.Push(CelValue::CreateInt64(3), AttributeTrail(expr, manager));

  ASSERT_EQ(stack.Peek().As<cel::IntValue>()->value(), 3);
  ASSERT_FALSE(stack.PeekAttribute().empty());
  ASSERT_EQ(stack.PeekAttribute().attribute(), attribute);

  stack.Pop(1);

  ASSERT_EQ(stack.Peek().As<cel::IntValue>()->value(), 2);
  ASSERT_TRUE(stack.PeekAttribute().empty());

  stack.Pop(1);

  ASSERT_EQ(stack.Peek().As<cel::IntValue>()->value(), 1);
  ASSERT_TRUE(stack.PeekAttribute().empty());
}

// Test that inner stacks within value stack retain the equality of their sizes.
TEST(EvaluatorStackTest, StackBalanced) {
  google::protobuf::Arena arena;
  ProtoMemoryManager manager(&arena);
  EvaluatorStack stack(10, manager);
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
  google::protobuf::Arena arena;
  ProtoMemoryManager manager(&arena);
  EvaluatorStack stack(10, manager);
  ASSERT_EQ(stack.size(), stack.attribute_size());

  stack.Push(CelValue::CreateInt64(1));
  stack.Push(CelValue::CreateInt64(2), AttributeTrail());
  stack.Push(CelValue::CreateInt64(3), AttributeTrail());
  ASSERT_EQ(stack.size(), 3);

  stack.Clear();
  ASSERT_EQ(stack.size(), 0);
  ASSERT_TRUE(stack.empty());
}

}  // namespace

}  // namespace google::api::expr::runtime
