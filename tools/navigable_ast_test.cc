// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "tools/navigable_ast.h"

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "base/builtins.h"
#include "internal/testing.h"
#include "parser/parser.h"

namespace cel {
namespace {

using ::google::api::expr::v1alpha1::Expr;
using ::google::api::expr::parser::Parse;
using testing::ElementsAre;
using testing::IsEmpty;
using testing::SizeIs;

TEST(NavigableAst, Basic) {
  Expr const_node;
  const_node.set_id(1);
  const_node.mutable_const_expr()->set_int64_value(42);

  NavigableAst ast = NavigableAst::Build(const_node);
  EXPECT_TRUE(ast.IdsAreUnique());

  const AstNode& root = ast.Root();

  EXPECT_EQ(root.expr(), &const_node);
  EXPECT_THAT(root.children(), IsEmpty());
  EXPECT_TRUE(root.parent() == nullptr);
  EXPECT_EQ(root.child_index(), -1);
  EXPECT_EQ(root.node_kind(), NodeKind::kConstant);
  EXPECT_EQ(root.parent_relation(), ChildKind::kUnspecified);
}

TEST(NavigableAst, FindById) {
  Expr const_node;
  const_node.set_id(1);
  const_node.mutable_const_expr()->set_int64_value(42);

  NavigableAst ast = NavigableAst::Build(const_node);

  const AstNode& root = ast.Root();

  EXPECT_EQ(ast.FindId(const_node.id()), &root);
  EXPECT_EQ(ast.FindId(-1), nullptr);
}

MATCHER_P(AstNodeWrapping, expr, "") {
  const AstNode* ptr = arg;
  return ptr != nullptr && ptr->expr() == expr;
}

TEST(NavigableAst, ToleratesNonUnique) {
  Expr call_node;
  call_node.set_id(1);
  call_node.mutable_call_expr()->set_function(cel::builtin::kNot);
  Expr* const_node = call_node.mutable_call_expr()->add_args();
  const_node->mutable_const_expr()->set_bool_value(false);
  const_node->set_id(1);

  NavigableAst ast = NavigableAst::Build(call_node);

  const AstNode& root = ast.Root();

  EXPECT_EQ(ast.FindId(1), &root);
  EXPECT_EQ(ast.FindExpr(&call_node), &root);
  EXPECT_FALSE(ast.IdsAreUnique());
  EXPECT_THAT(ast.FindExpr(const_node), AstNodeWrapping(const_node));
}

TEST(NavigableAst, FindByExprPtr) {
  Expr const_node;
  const_node.set_id(1);
  const_node.mutable_const_expr()->set_int64_value(42);

  NavigableAst ast = NavigableAst::Build(const_node);

  const AstNode& root = ast.Root();

  EXPECT_EQ(ast.FindExpr(&const_node), &root);
  EXPECT_EQ(ast.FindExpr(&Expr::default_instance()), nullptr);
}

TEST(NavigableAst, Children) {
  ASSERT_OK_AND_ASSIGN(auto parsed_expr, Parse("1 + 2"));

  NavigableAst ast = NavigableAst::Build(parsed_expr.expr());
  const AstNode& root = ast.Root();

  EXPECT_EQ(root.expr(), &parsed_expr.expr());
  EXPECT_THAT(root.children(), SizeIs(2));
  EXPECT_TRUE(root.parent() == nullptr);
  EXPECT_EQ(root.child_index(), -1);
  EXPECT_EQ(root.parent_relation(), ChildKind::kUnspecified);
  EXPECT_EQ(root.node_kind(), NodeKind::kCall);

  EXPECT_THAT(
      root.children(),
      ElementsAre(AstNodeWrapping(&parsed_expr.expr().call_expr().args(0)),
                  AstNodeWrapping(&parsed_expr.expr().call_expr().args(1))));

  ASSERT_THAT(root.children(), SizeIs(2));
  const auto* child1 = root.children()[0];
  EXPECT_EQ(child1->child_index(), 0);
  EXPECT_EQ(child1->parent(), &root);
  EXPECT_EQ(child1->parent_relation(), ChildKind::kCallArg);
  EXPECT_EQ(child1->node_kind(), NodeKind::kConstant);
  EXPECT_THAT(child1->children(), IsEmpty());

  const auto* child2 = root.children()[1];
  EXPECT_EQ(child2->child_index(), 1);
}

TEST(NavigableAst, UnspecifiedExpr) {
  Expr expr;
  expr.set_id(1);
  NavigableAst ast = NavigableAst::Build(expr);
  const AstNode& root = ast.Root();

  EXPECT_EQ(root.expr(), &expr);
  EXPECT_THAT(root.children(), SizeIs(0));
  EXPECT_TRUE(root.parent() == nullptr);
  EXPECT_EQ(root.child_index(), -1);
  EXPECT_EQ(root.node_kind(), NodeKind::kUnspecified);
}

TEST(NavigableAst, ParentRelationSelect) {
  ASSERT_OK_AND_ASSIGN(auto parsed_expr, Parse("a.b"));

  NavigableAst ast = NavigableAst::Build(parsed_expr.expr());
  const AstNode& root = ast.Root();

  ASSERT_THAT(root.children(), SizeIs(1));
  const auto* child = root.children()[0];

  EXPECT_EQ(child->parent_relation(), ChildKind::kSelectOperand);
  EXPECT_EQ(child->node_kind(), NodeKind::kIdent);
}

TEST(NavigableAst, ParentRelationCallReceiver) {
  ASSERT_OK_AND_ASSIGN(auto parsed_expr, Parse("a.b()"));

  NavigableAst ast = NavigableAst::Build(parsed_expr.expr());
  const AstNode& root = ast.Root();

  ASSERT_THAT(root.children(), SizeIs(1));
  const auto* child = root.children()[0];

  EXPECT_EQ(child->parent_relation(), ChildKind::kCallReceiver);
  EXPECT_EQ(child->node_kind(), NodeKind::kIdent);
}

TEST(NavigableAst, ParentRelationCreateStruct) {
  ASSERT_OK_AND_ASSIGN(auto parsed_expr,
                       Parse("com.example.Type{field: '123'}"));

  NavigableAst ast = NavigableAst::Build(parsed_expr.expr());
  const AstNode& root = ast.Root();

  EXPECT_EQ(root.node_kind(), NodeKind::kStruct);
  ASSERT_THAT(root.children(), SizeIs(1));
  const auto* child = root.children()[0];

  EXPECT_EQ(child->parent_relation(), ChildKind::kStructValue);
  EXPECT_EQ(child->node_kind(), NodeKind::kConstant);
}

TEST(NavigableAst, ParentRelationCreateMap) {
  ASSERT_OK_AND_ASSIGN(auto parsed_expr, Parse("{'a': 123}"));

  NavigableAst ast = NavigableAst::Build(parsed_expr.expr());
  const AstNode& root = ast.Root();

  EXPECT_EQ(root.node_kind(), NodeKind::kMap);
  ASSERT_THAT(root.children(), SizeIs(2));
  const auto* key = root.children()[0];
  const auto* value = root.children()[1];

  EXPECT_EQ(key->parent_relation(), ChildKind::kMapKey);
  EXPECT_EQ(key->node_kind(), NodeKind::kConstant);

  EXPECT_EQ(value->parent_relation(), ChildKind::kMapValue);
  EXPECT_EQ(value->node_kind(), NodeKind::kConstant);
}

TEST(NavigableAst, ParentRelationCreateList) {
  ASSERT_OK_AND_ASSIGN(auto parsed_expr, Parse("[123]"));

  NavigableAst ast = NavigableAst::Build(parsed_expr.expr());
  const AstNode& root = ast.Root();

  EXPECT_EQ(root.node_kind(), NodeKind::kList);
  ASSERT_THAT(root.children(), SizeIs(1));
  const auto* child = root.children()[0];

  EXPECT_EQ(child->parent_relation(), ChildKind::kListElem);
  EXPECT_EQ(child->node_kind(), NodeKind::kConstant);
}

TEST(NavigableAst, ParentRelationComprehension) {
  ASSERT_OK_AND_ASSIGN(auto parsed_expr, Parse("[1].all(x, x < 2)"));

  NavigableAst ast = NavigableAst::Build(parsed_expr.expr());
  const AstNode& root = ast.Root();

  EXPECT_EQ(root.node_kind(), NodeKind::kComprehension);
  ASSERT_THAT(root.children(), SizeIs(5));
  const auto* range = root.children()[0];
  const auto* init = root.children()[1];
  const auto* condition = root.children()[2];
  const auto* step = root.children()[3];
  const auto* finish = root.children()[4];

  EXPECT_EQ(range->parent_relation(), ChildKind::kCompehensionRange);
  EXPECT_EQ(init->parent_relation(), ChildKind::kCompehensionInit);
  EXPECT_EQ(condition->parent_relation(), ChildKind::kComprehensionCondition);
  EXPECT_EQ(step->parent_relation(), ChildKind::kComprehensionLoopStep);
  EXPECT_EQ(finish->parent_relation(), ChildKind::kComprensionResult);
}

}  // namespace
}  // namespace cel
