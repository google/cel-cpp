#include "eval/eval/evaluator_stack.h"

#include "base/attribute.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "base/type_provider.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::TypeFactory;
using ::cel::TypeManager;
using ::cel::TypeProvider;
using ::cel::ValueFactory;
using ::cel::extensions::ProtoMemoryManager;

// Test Value Stack Push/Pop operation
TEST(EvaluatorStackTest, StackPushPop) {
  google::protobuf::Arena arena;
  ProtoMemoryManager manager(&arena);
  TypeFactory type_factory(manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);

  cel::Attribute attribute("name", {});
  EvaluatorStack stack(10);
  stack.Push(value_factory.CreateIntValue(1));
  stack.Push(value_factory.CreateIntValue(2), AttributeTrail());
  stack.Push(value_factory.CreateIntValue(3), AttributeTrail("name"));

  ASSERT_EQ(stack.Peek().As<cel::IntValue>()->NativeValue(), 3);
  ASSERT_FALSE(stack.PeekAttribute().empty());
  ASSERT_EQ(stack.PeekAttribute().attribute(), attribute);

  stack.Pop(1);

  ASSERT_EQ(stack.Peek().As<cel::IntValue>()->NativeValue(), 2);
  ASSERT_TRUE(stack.PeekAttribute().empty());

  stack.Pop(1);

  ASSERT_EQ(stack.Peek().As<cel::IntValue>()->NativeValue(), 1);
  ASSERT_TRUE(stack.PeekAttribute().empty());
}

// Test that inner stacks within value stack retain the equality of their sizes.
TEST(EvaluatorStackTest, StackBalanced) {
  google::protobuf::Arena arena;
  ProtoMemoryManager manager(&arena);
  TypeFactory type_factory(manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EvaluatorStack stack(10);
  ASSERT_EQ(stack.size(), stack.attribute_size());

  stack.Push(value_factory.CreateIntValue(1));
  ASSERT_EQ(stack.size(), stack.attribute_size());
  stack.Push(value_factory.CreateIntValue(2), AttributeTrail());
  stack.Push(value_factory.CreateIntValue(3), AttributeTrail());
  ASSERT_EQ(stack.size(), stack.attribute_size());

  stack.PopAndPush(value_factory.CreateIntValue(4), AttributeTrail());
  ASSERT_EQ(stack.size(), stack.attribute_size());
  stack.PopAndPush(value_factory.CreateIntValue(5));
  ASSERT_EQ(stack.size(), stack.attribute_size());

  stack.Pop(3);
  ASSERT_EQ(stack.size(), stack.attribute_size());
}

TEST(EvaluatorStackTest, Clear) {
  google::protobuf::Arena arena;
  ProtoMemoryManager manager(&arena);
  TypeFactory type_factory(manager);
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EvaluatorStack stack(10);
  ASSERT_EQ(stack.size(), stack.attribute_size());

  stack.Push(value_factory.CreateIntValue(1));
  stack.Push(value_factory.CreateIntValue(2), AttributeTrail());
  stack.Push(value_factory.CreateIntValue(3), AttributeTrail());
  ASSERT_EQ(stack.size(), 3);

  stack.Clear();
  ASSERT_EQ(stack.size(), 0);
  ASSERT_TRUE(stack.empty());
}

}  // namespace

}  // namespace google::api::expr::runtime
