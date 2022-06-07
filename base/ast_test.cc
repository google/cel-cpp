// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "base/ast.h"

#include <memory>
#include <utility>

#include "absl/memory/memory.h"
#include "absl/types/variant.h"
#include "internal/testing.h"

namespace cel {
namespace ast {
namespace internal {
namespace {
TEST(AstTest, ExprConstructionConstant) {
  Expr expr(1, true);
  ASSERT_TRUE(absl::holds_alternative<Constant>(expr.expr_kind()));
  const auto& constant = absl::get<Constant>(expr.expr_kind());
  ASSERT_TRUE(absl::holds_alternative<bool>(constant));
  ASSERT_TRUE(absl::get<bool>(constant));
}

TEST(AstTest, ExprConstructionIdent) {
  Expr expr(1, Ident("var"));
  ASSERT_TRUE(absl::holds_alternative<Ident>(expr.expr_kind()));
  ASSERT_EQ(absl::get<Ident>(expr.expr_kind()).name(), "var");
}

TEST(AstTest, ExprConstructionSelect) {
  Expr expr(1, Select(std::make_unique<Expr>(2, Ident("var")), "field"));
  ASSERT_TRUE(absl::holds_alternative<Select>(expr.expr_kind()));
  const auto& select = absl::get<Select>(expr.expr_kind());
  ASSERT_TRUE(absl::holds_alternative<Ident>(select.operand()->expr_kind()));
  ASSERT_EQ(absl::get<Ident>(select.operand()->expr_kind()).name(), "var");
  ASSERT_EQ(select.field(), "field");
}

TEST(AstTest, SelectMutableOperand) {
  Select select;
  select.mutable_operand().set_expr_kind(Ident("var"));
  ASSERT_TRUE(absl::holds_alternative<Ident>(select.operand()->expr_kind()));
  ASSERT_EQ(absl::get<Ident>(select.operand()->expr_kind()).name(), "var");
}

TEST(AstTest, ExprConstructionCall) {
  Expr expr(1, Call(std::make_unique<Expr>(2, Ident("var")), "function", {}));
  ASSERT_TRUE(absl::holds_alternative<Call>(expr.expr_kind()));
  const auto& call = absl::get<Call>(expr.expr_kind());
  ASSERT_TRUE(absl::holds_alternative<Ident>(call.target()->expr_kind()));
  ASSERT_EQ(absl::get<Ident>(call.target()->expr_kind()).name(), "var");
  ASSERT_EQ(call.function(), "function");
  ASSERT_TRUE(call.args().empty());
}

TEST(AstTest, CallMutableTarget) {
  Call call;
  call.mutable_target().set_expr_kind(Ident("var"));
  ASSERT_TRUE(absl::holds_alternative<Ident>(call.target()->expr_kind()));
  ASSERT_EQ(absl::get<Ident>(call.target()->expr_kind()).name(), "var");
}

TEST(AstTest, ExprConstructionCreateList) {
  CreateList create_list;
  create_list.mutable_elements().emplace_back(Expr(2, Ident("var1")));
  create_list.mutable_elements().emplace_back(Expr(3, Ident("var2")));
  create_list.mutable_elements().emplace_back(Expr(4, Ident("var3")));
  Expr expr(1, std::move(create_list));
  ASSERT_TRUE(absl::holds_alternative<CreateList>(expr.expr_kind()));
  const auto& elements = absl::get<CreateList>(expr.expr_kind()).elements();
  ASSERT_EQ(absl::get<Ident>(elements[0].expr_kind()).name(), "var1");
  ASSERT_EQ(absl::get<Ident>(elements[1].expr_kind()).name(), "var2");
  ASSERT_EQ(absl::get<Ident>(elements[2].expr_kind()).name(), "var3");
}

TEST(AstTest, ExprConstructionCreateStruct) {
  CreateStruct create_struct;
  create_struct.set_message_name("name");
  create_struct.mutable_entries().emplace_back(CreateStruct::Entry(
      1, "key1", std::make_unique<Expr>(2, Ident("value1"))));
  create_struct.mutable_entries().emplace_back(CreateStruct::Entry(
      3, "key2", std::make_unique<Expr>(4, Ident("value2"))));
  create_struct.mutable_entries().emplace_back(
      CreateStruct::Entry(5, std::make_unique<Expr>(6, Ident("key3")),
                          std::make_unique<Expr>(6, Ident("value3"))));
  Expr expr(1, std::move(create_struct));
  ASSERT_TRUE(absl::holds_alternative<CreateStruct>(expr.expr_kind()));
  const auto& entries = absl::get<CreateStruct>(expr.expr_kind()).entries();
  ASSERT_EQ(absl::get<std::string>(entries[0].key_kind()), "key1");
  ASSERT_EQ(absl::get<Ident>(entries[0].value()->expr_kind()).name(), "value1");
  ASSERT_EQ(absl::get<std::string>(entries[1].key_kind()), "key2");
  ASSERT_EQ(absl::get<Ident>(entries[1].value()->expr_kind()).name(), "value2");
  ASSERT_EQ(
      absl::get<Ident>(
          absl::get<std::unique_ptr<Expr>>(entries[2].key_kind())->expr_kind())
          .name(),
      "key3");
  ASSERT_EQ(absl::get<Ident>(entries[2].value()->expr_kind()).name(), "value3");
}

TEST(AstTest, CreateStructEntryMutableValue) {
  CreateStruct::Entry entry;
  entry.mutable_value().set_expr_kind(Ident("var"));
  ASSERT_TRUE(absl::holds_alternative<Ident>(entry.value()->expr_kind()));
  ASSERT_EQ(absl::get<Ident>(entry.value()->expr_kind()).name(), "var");
}

TEST(AstTest, ExprConstructionComprehension) {
  Comprehension comprehension;
  comprehension.set_iter_var("iter_var");
  comprehension.set_iter_range(std::make_unique<Expr>(1, Ident("range")));
  comprehension.set_accu_var("accu_var");
  comprehension.set_accu_init(std::make_unique<Expr>(2, Ident("init")));
  comprehension.set_loop_condition(std::make_unique<Expr>(3, Ident("cond")));
  comprehension.set_loop_step(std::make_unique<Expr>(4, Ident("step")));
  comprehension.set_result(std::make_unique<Expr>(5, Ident("result")));
  Expr expr(6, std::move(comprehension));
  ASSERT_TRUE(absl::holds_alternative<Comprehension>(expr.expr_kind()));
  auto& created_expr = absl::get<Comprehension>(expr.expr_kind());
  ASSERT_EQ(created_expr.iter_var(), "iter_var");
  ASSERT_EQ(absl::get<Ident>(created_expr.iter_range()->expr_kind()).name(),
            "range");
  ASSERT_EQ(created_expr.accu_var(), "accu_var");
  ASSERT_EQ(absl::get<Ident>(created_expr.accu_init()->expr_kind()).name(),
            "init");
  ASSERT_EQ(absl::get<Ident>(created_expr.loop_condition()->expr_kind()).name(),
            "cond");
  ASSERT_EQ(absl::get<Ident>(created_expr.loop_step()->expr_kind()).name(),
            "step");
  ASSERT_EQ(absl::get<Ident>(created_expr.result()->expr_kind()).name(),
            "result");
}

TEST(AstTest, ComprehensionMutableConstruction) {
  Comprehension comprehension;
  comprehension.mutable_iter_range().set_expr_kind(Ident("var"));
  ASSERT_TRUE(
      absl::holds_alternative<Ident>(comprehension.iter_range()->expr_kind()));
  ASSERT_EQ(absl::get<Ident>(comprehension.iter_range()->expr_kind()).name(),
            "var");
  comprehension.mutable_accu_init().set_expr_kind(Ident("var"));
  ASSERT_TRUE(
      absl::holds_alternative<Ident>(comprehension.accu_init()->expr_kind()));
  ASSERT_EQ(absl::get<Ident>(comprehension.accu_init()->expr_kind()).name(),
            "var");
  comprehension.mutable_loop_condition().set_expr_kind(Ident("var"));
  ASSERT_TRUE(absl::holds_alternative<Ident>(
      comprehension.loop_condition()->expr_kind()));
  ASSERT_EQ(
      absl::get<Ident>(comprehension.loop_condition()->expr_kind()).name(),
      "var");
  comprehension.mutable_loop_step().set_expr_kind(Ident("var"));
  ASSERT_TRUE(
      absl::holds_alternative<Ident>(comprehension.loop_step()->expr_kind()));
  ASSERT_EQ(absl::get<Ident>(comprehension.loop_step()->expr_kind()).name(),
            "var");
  comprehension.mutable_result().set_expr_kind(Ident("var"));
  ASSERT_TRUE(
      absl::holds_alternative<Ident>(comprehension.result()->expr_kind()));
  ASSERT_EQ(absl::get<Ident>(comprehension.result()->expr_kind()).name(),
            "var");
}

TEST(AstTest, ExprMoveTest) {
  Expr expr(1, Ident("var"));
  ASSERT_TRUE(absl::holds_alternative<Ident>(expr.expr_kind()));
  ASSERT_EQ(absl::get<Ident>(expr.expr_kind()).name(), "var");
  Expr new_expr = std::move(expr);
  ASSERT_TRUE(absl::holds_alternative<Ident>(new_expr.expr_kind()));
  ASSERT_EQ(absl::get<Ident>(new_expr.expr_kind()).name(), "var");
}

TEST(AstTest, ParsedExpr) {
  ParsedExpr parsed_expr;
  parsed_expr.set_expr(Expr(1, Ident("name")));
  auto& source_info = parsed_expr.mutable_source_info();
  source_info.set_syntax_version("syntax_version");
  source_info.set_location("location");
  source_info.set_line_offsets({1, 2, 3});
  source_info.set_positions({{1, 1}, {2, 2}});
  ASSERT_TRUE(absl::holds_alternative<Ident>(parsed_expr.expr().expr_kind()));
  ASSERT_EQ(absl::get<Ident>(parsed_expr.expr().expr_kind()).name(), "name");
  ASSERT_EQ(parsed_expr.source_info().syntax_version(), "syntax_version");
  ASSERT_EQ(parsed_expr.source_info().location(), "location");
  EXPECT_THAT(parsed_expr.source_info().line_offsets(),
              testing::UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(
      parsed_expr.source_info().positions(),
      testing::UnorderedElementsAre(testing::Pair(1, 1), testing::Pair(2, 2)));
}

TEST(AstTest, ListTypeMutableConstruction) {
  ListType type;
  type.mutable_elem_type() = Type(PrimitiveType::kBool);
  EXPECT_EQ(absl::get<PrimitiveType>(type.elem_type()->type_kind()),
            PrimitiveType::kBool);
}

TEST(AstTest, MapTypeMutableConstruction) {
  MapType type;
  type.mutable_key_type() = Type(PrimitiveType::kBool);
  type.mutable_value_type() = Type(PrimitiveType::kBool);
  EXPECT_EQ(absl::get<PrimitiveType>(type.key_type()->type_kind()),
            PrimitiveType::kBool);
  EXPECT_EQ(absl::get<PrimitiveType>(type.value_type()->type_kind()),
            PrimitiveType::kBool);
}

TEST(AstTest, FunctionTypeMutableConstruction) {
  FunctionType type;
  type.mutable_result_type() = Type(PrimitiveType::kBool);
  EXPECT_EQ(absl::get<PrimitiveType>(type.result_type()->type_kind()),
            PrimitiveType::kBool);
}

TEST(AstTest, CheckedExpr) {
  CheckedExpr checked_expr;
  checked_expr.set_expr(Expr(1, Ident("name")));
  auto& source_info = checked_expr.mutable_source_info();
  source_info.set_syntax_version("syntax_version");
  source_info.set_location("location");
  source_info.set_line_offsets({1, 2, 3});
  source_info.set_positions({{1, 1}, {2, 2}});
  checked_expr.set_expr_version("expr_version");
  checked_expr.mutable_type_map().insert(
      {1, Type(PrimitiveType(PrimitiveType::kBool))});
  ASSERT_TRUE(absl::holds_alternative<Ident>(checked_expr.expr().expr_kind()));
  ASSERT_EQ(absl::get<Ident>(checked_expr.expr().expr_kind()).name(), "name");
  ASSERT_EQ(checked_expr.source_info().syntax_version(), "syntax_version");
  ASSERT_EQ(checked_expr.source_info().location(), "location");
  EXPECT_THAT(checked_expr.source_info().line_offsets(),
              testing::UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(
      checked_expr.source_info().positions(),
      testing::UnorderedElementsAre(testing::Pair(1, 1), testing::Pair(2, 2)));
  EXPECT_EQ(checked_expr.expr_version(), "expr_version");
}

}  // namespace
}  // namespace internal
}  // namespace ast
}  // namespace cel
