#include "eval/compiler/constant_folding.h"

#include <memory>
#include <string>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "base/ast_internal.h"
#include "base/internal/ast_impl.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "base/value_factory.h"
#include "base/values/bool_value.h"
#include "base/values/error_value.h"
#include "base/values/int_value.h"
#include "base/values/list_value.h"
#include "base/values/string_value.h"
#include "eval/compiler/flat_expr_builder_extensions.h"
#include "eval/compiler/resolver.h"
#include "eval/eval/const_value_step.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/expression_build_warning.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_function_registry.h"
#include "eval/public/cel_type_registry.h"
#include "extensions/protobuf/ast_converters.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "parser/parser.h"
#include "runtime/function_registry.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/text_format.h"

namespace cel::ast::internal {

namespace {

using ::cel::ast::internal::Constant;
using ::cel::ast::internal::ConstantKind;
using ::cel::extensions::ProtoMemoryManager;
using ::cel::extensions::internal::ConvertProtoExprToNative;
using ::google::api::expr::v1alpha1::ParsedExpr;
using ::google::api::expr::parser::Parse;
using ::google::api::expr::runtime::BuilderWarnings;
using ::google::api::expr::runtime::CelFunctionRegistry;
using ::google::api::expr::runtime::CelTypeRegistry;
using ::google::api::expr::runtime::CreateConstValueStep;
using ::google::api::expr::runtime::ExecutionPath;
using ::google::api::expr::runtime::PlannerContext;
using ::google::api::expr::runtime::ProgramOptimizer;
using ::google::api::expr::runtime::ProgramOptimizerFactory;
using ::google::api::expr::runtime::Resolver;
using ::google::protobuf::Arena;
using testing::SizeIs;
using cel::internal::StatusIs;

class ConstantFoldingTestWithValueFactory : public testing::Test {
 public:
  ConstantFoldingTestWithValueFactory()
      : memory_manager_(&arena_),
        type_factory_(memory_manager_),
        type_manager_(type_factory_, cel::TypeProvider::Builtin()),
        value_factory_(type_manager_) {}

 protected:
  Arena arena_;
  ProtoMemoryManager memory_manager_;
  TypeFactory type_factory_;
  TypeManager type_manager_;
  ValueFactory value_factory_;
};

// Validate select is preserved as-is
TEST(ConstantFoldingTest, Select) {
  google::api::expr::v1alpha1::Expr expr;
  // has(x.y)
  google::protobuf::TextFormat::ParseFromString(R"(
    id: 1
    select_expr {
      operand {
        id: 2
        ident_expr { name: "x" }
      }
      field: "y"
      test_only: true
    })",
                                      &expr);
  auto native_expr = ConvertProtoExprToNative(expr).value();

  google::protobuf::Arena arena;
  CelFunctionRegistry registry;
  absl::flat_hash_map<std::string, Handle<Value>> idents;
  Expr out;
  FoldConstants(native_expr, registry.InternalGetRegistry(), &arena, idents,
                out);
  EXPECT_EQ(out, native_expr);
  EXPECT_TRUE(idents.empty());
}

// Validate struct message creation
TEST(ConstantFoldingTest, StructMessage) {
  google::api::expr::v1alpha1::Expr expr;
  // {"field1": "y", "field2": "t"}
  google::protobuf::TextFormat::ParseFromString(
      R"pb(
        id: 5
        struct_expr {
          entries {
            id: 11
            field_key: "field1"
            value { const_expr { string_value: "value1" } }
          }
          entries {
            id: 7
            field_key: "field2"
            value { const_expr { int64_value: 12 } }
          }
          message_name: "MyProto"
        })pb",
      &expr);
  auto native_expr = ConvertProtoExprToNative(expr).value();

  google::protobuf::Arena arena;
  CelFunctionRegistry registry;

  absl::flat_hash_map<std::string, Handle<Value>> idents;
  Expr out;
  FoldConstants(native_expr, registry.InternalGetRegistry(), &arena, idents,
                out);

  google::api::expr::v1alpha1::Expr expected;
  google::protobuf::TextFormat::ParseFromString(R"(
    id: 5
    struct_expr {
      entries {
        id: 11
        field_key: "field1"
        value { ident_expr { name: "$v0" } }
      }
      entries {
        id: 7
        field_key: "field2"
        value { ident_expr { name: "$v1" } }
      }
      message_name: "MyProto"
    })",
                                      &expected);
  auto native_expected_expr = ConvertProtoExprToNative(expected).value();

  EXPECT_EQ(out, native_expected_expr);

  EXPECT_EQ(idents.size(), 2);
  EXPECT_TRUE(idents["$v0"]->Is<StringValue>());
  EXPECT_EQ(idents["$v0"].As<StringValue>()->ToString(), "value1");
  EXPECT_TRUE(idents["$v1"]->Is<IntValue>());
  EXPECT_EQ(idents["$v1"].As<IntValue>()->value(), 12);
}

// Validate struct creation is not folded but recursed into
TEST(ConstantFoldingTest, StructComprehension) {
  google::api::expr::v1alpha1::Expr expr;
  // {"x": "y", "z": "t"}
  google::protobuf::TextFormat::ParseFromString(R"(
    id: 5
    struct_expr {
      entries {
        id: 11
        field_key: "x"
        value { const_expr { string_value: "y" } }
      }
      entries {
        id: 7
        map_key { const_expr { string_value: "z" } }
        value { const_expr { string_value: "t" } }
      }
    })",
                                      &expr);
  auto native_expr = ConvertProtoExprToNative(expr).value();

  google::protobuf::Arena arena;
  CelFunctionRegistry registry;

  absl::flat_hash_map<std::string, Handle<Value>> idents;
  Expr out;
  FoldConstants(native_expr, registry.InternalGetRegistry(), &arena, idents,
                out);

  google::api::expr::v1alpha1::Expr expected;
  google::protobuf::TextFormat::ParseFromString(R"(
    id: 5
    struct_expr {
      entries {
        id: 11
        field_key: "x"
        value { ident_expr { name: "$v0" } }
      }
      entries {
        id: 7
        map_key { ident_expr { name: "$v1" } }
        value { ident_expr { name: "$v2" } }
      }
    })",
                                      &expected);
  auto native_expected_expr = ConvertProtoExprToNative(expected).value();

  EXPECT_EQ(out, native_expected_expr);

  EXPECT_EQ(idents.size(), 3);
  EXPECT_TRUE(idents["$v0"]->Is<StringValue>());
  EXPECT_EQ(idents["$v0"].As<StringValue>()->ToString(), "y");
  EXPECT_TRUE(idents["$v1"]->Is<StringValue>());
  EXPECT_TRUE(idents["$v2"]->Is<StringValue>());
}

TEST_F(ConstantFoldingTestWithValueFactory, ListComprehension) {
  google::api::expr::v1alpha1::Expr expr;
  // [1, [2, 3]]
  google::protobuf::TextFormat::ParseFromString(R"(
    id: 45
    list_expr {
      elements { const_expr { int64_value: 1 } }
      elements {
        list_expr {
          elements { const_expr { int64_value: 2 } }
          elements { const_expr { int64_value: 3 } }
        }
      }
    })",
                                      &expr);
  auto native_expr = ConvertProtoExprToNative(expr).value();

  google::protobuf::Arena arena;
  CelFunctionRegistry registry;

  absl::flat_hash_map<std::string, Handle<Value>> idents;
  Expr out;
  FoldConstants(native_expr, registry.InternalGetRegistry(), &arena, idents,
                out);

  ASSERT_EQ(out.id(), 45);
  ASSERT_TRUE(out.has_ident_expr());
  ASSERT_EQ(idents.size(), 1);
  auto value = idents[out.ident_expr().name()];
  ASSERT_TRUE(value->Is<ListValue>());
  const auto& list = value.As<ListValue>();
  ASSERT_EQ(list->size(), 2);
  ASSERT_OK_AND_ASSIGN(auto elem0,
                       list->Get(ListValue::GetContext(value_factory_), 0));
  ASSERT_OK_AND_ASSIGN(auto elem1,
                       list->Get(ListValue::GetContext(value_factory_), 1));
  ASSERT_TRUE(elem0->Is<IntValue>());
  ASSERT_EQ(elem0.As<IntValue>()->value(), 1);
  ASSERT_TRUE(elem1->Is<ListValue>());
  ASSERT_EQ(elem1.As<ListValue>()->size(), 2);
}

// Validate that logic function application are not folded
TEST(ConstantFoldingTest, LogicApplication) {
  google::api::expr::v1alpha1::Expr expr;
  // true && false
  google::protobuf::TextFormat::ParseFromString(R"(
    id: 105
    call_expr {
      function: "_&&_"
      args {
        const_expr { bool_value: true }
      }
      args {
        const_expr { bool_value: false }
      }
    })",
                                      &expr);
  auto native_expr = ConvertProtoExprToNative(expr).value();

  google::protobuf::Arena arena;
  CelFunctionRegistry registry;
  ASSERT_OK(RegisterBuiltinFunctions(&registry));

  absl::flat_hash_map<std::string, Handle<Value>> idents;
  Expr out;
  FoldConstants(native_expr, registry.InternalGetRegistry(), &arena, idents,
                out);

  ASSERT_EQ(out.id(), 105);
  ASSERT_TRUE(out.has_call_expr());
  ASSERT_EQ(idents.size(), 2);
}

TEST_F(ConstantFoldingTestWithValueFactory, FunctionApplication) {
  google::api::expr::v1alpha1::Expr expr;
  // [1] + [2]
  google::protobuf::TextFormat::ParseFromString(R"(
    id: 15
    call_expr {
      function: "_+_"
      args {
        list_expr {
          elements { const_expr { int64_value: 1 } }
        }
      }
      args {
        list_expr {
          elements { const_expr { int64_value: 2 } }
        }
      }
    })",
                                      &expr);
  auto native_expr = ConvertProtoExprToNative(expr).value();

  google::protobuf::Arena arena;
  CelFunctionRegistry registry;
  ASSERT_OK(RegisterBuiltinFunctions(&registry));

  absl::flat_hash_map<std::string, Handle<Value>> idents;
  Expr out;
  FoldConstants(native_expr, registry.InternalGetRegistry(), &arena, idents,
                out);

  ASSERT_EQ(out.id(), 15);
  ASSERT_TRUE(out.has_ident_expr());
  ASSERT_EQ(idents.size(), 1);
  ASSERT_TRUE(idents[out.ident_expr().name()]->Is<ListValue>());

  const auto& list = idents[out.ident_expr().name()].As<ListValue>();
  ASSERT_EQ(list->size(), 2);
  ASSERT_EQ(list->Get(ListValue::GetContext(value_factory_), 0)
                .value()
                .As<IntValue>()
                ->value(),
            1);
  ASSERT_EQ(list->Get(ListValue::GetContext(value_factory_), 1)
                .value()
                .As<IntValue>()
                ->value(),
            2);
}

TEST(ConstantFoldingTest, FunctionApplicationWithReceiver) {
  google::api::expr::v1alpha1::Expr expr;
  // [1, 1].size()
  google::protobuf::TextFormat::ParseFromString(R"(
    id: 10
    call_expr {
      function: "size"
      target {
        list_expr {
          elements { const_expr { int64_value: 1 } }
          elements { const_expr { int64_value: 1 } }
        }
    })",
                                      &expr);
  auto native_expr = ConvertProtoExprToNative(expr).value();

  google::protobuf::Arena arena;
  CelFunctionRegistry registry;
  ASSERT_OK(RegisterBuiltinFunctions(&registry));

  absl::flat_hash_map<std::string, Handle<Value>> idents;
  Expr out;
  FoldConstants(native_expr, registry.InternalGetRegistry(), &arena, idents,
                out);

  ASSERT_EQ(out.id(), 10);
  ASSERT_TRUE(out.has_ident_expr());
  ASSERT_EQ(idents.size(), 1);
  ASSERT_TRUE(idents[out.ident_expr().name()]->Is<IntValue>());
  ASSERT_EQ(idents[out.ident_expr().name()].As<IntValue>()->value(), 2);
}

TEST(ConstantFoldingTest, FunctionApplicationNoOverload) {
  google::api::expr::v1alpha1::Expr expr;
  // 1 + [2]
  google::protobuf::TextFormat::ParseFromString(R"(
    id: 16
    call_expr {
      function: "_+_"
      args {
        const_expr { int64_value: 1 }
      }
      args {
        list_expr {
          elements { const_expr { int64_value: 2 } }
        }
      }
    })",
                                      &expr);
  auto native_expr = ConvertProtoExprToNative(expr).value();

  google::protobuf::Arena arena;
  CelFunctionRegistry registry;
  ASSERT_OK(RegisterBuiltinFunctions(&registry));

  absl::flat_hash_map<std::string, Handle<Value>> idents;
  Expr out;
  FoldConstants(native_expr, registry.InternalGetRegistry(), &arena, idents,
                out);

  ASSERT_EQ(out.id(), 16);
  ASSERT_TRUE(out.has_ident_expr());
  ASSERT_EQ(idents.size(), 1);
  ASSERT_TRUE(idents[out.ident_expr().name()]->Is<ErrorValue>());
}

// Validate that comprehension is recursed into
TEST(ConstantFoldingTest, MapComprehension) {
  google::api::expr::v1alpha1::Expr expr;
  // {1: "", 2: ""}.all(x, x > 0)
  google::protobuf::TextFormat::ParseFromString(R"(
    id: 1
    comprehension_expr {
      iter_var: "k"
      accu_var: "accu"
      accu_init {
        id: 2
        const_expr { bool_value: true }
      }
      loop_condition {
        id: 3
        ident_expr { name: "accu" }
      }
      result {
        id: 4
        ident_expr { name: "accu" }
      }
      loop_step {
        id: 5
        call_expr {
          function: "_&&_"
          args {
            ident_expr { name: "accu" }
          }
          args {
            call_expr {
              function: "_>_"
              args { ident_expr { name: "k" } }
              args { const_expr { int64_value: 0 } }
            }
          }
        }
      }
      iter_range {
        id: 6
        struct_expr {
          entries {
            map_key { const_expr { int64_value: 1 } }
            value { const_expr { string_value: "" } }
          }
          entries {
            id: 7
            map_key { const_expr { int64_value: 2 } }
            value { const_expr { string_value: "" } }
          }
        }
      }
    })",
                                      &expr);
  auto native_expr = ConvertProtoExprToNative(expr).value();

  google::protobuf::Arena arena;
  CelFunctionRegistry registry;

  absl::flat_hash_map<std::string, Handle<Value>> idents;
  Expr out;
  FoldConstants(native_expr, registry.InternalGetRegistry(), &arena, idents,
                out);

  google::api::expr::v1alpha1::Expr expected;
  google::protobuf::TextFormat::ParseFromString(R"(
    id: 1
    comprehension_expr {
      iter_var: "k"
      accu_var: "accu"
      accu_init {
        id: 2
        ident_expr { name: "$v0" }
      }
      loop_condition {
        id: 3
        ident_expr { name: "accu" }
      }
      result {
        id: 4
        ident_expr { name: "accu" }
      }
      loop_step {
        id: 5
        call_expr {
          function: "_&&_"
          args {
            ident_expr { name: "accu" }
          }
          args {
            call_expr {
              function: "_>_"
              args { ident_expr { name: "k" } }
              args { ident_expr { name: "$v5" } }
            }
          }
        }
      }
      iter_range {
        id: 6
        struct_expr {
          entries {
            map_key { ident_expr { name: "$v1" } }
            value { ident_expr { name: "$v2" } }
          }
          entries {
            id: 7
            map_key { ident_expr { name: "$v3" } }
            value { ident_expr { name: "$v4" } }
          }
        }
      }
    })",
                                      &expected);
  auto native_expected_expr = ConvertProtoExprToNative(expected).value();

  EXPECT_EQ(out, native_expected_expr);

  EXPECT_EQ(idents.size(), 6);
  EXPECT_TRUE(idents["$v0"]->Is<BoolValue>());
  EXPECT_TRUE(idents["$v1"]->Is<IntValue>());
  EXPECT_TRUE(idents["$v2"]->Is<StringValue>());
  EXPECT_TRUE(idents["$v3"]->Is<IntValue>());
  EXPECT_TRUE(idents["$v4"]->Is<StringValue>());
  EXPECT_TRUE(idents["$v5"]->Is<IntValue>());
}

class UpdatedConstantFoldingTest : public testing::Test {
 public:
  UpdatedConstantFoldingTest()
      : resolver_("", function_registry_, &type_registry_) {}

 protected:
  cel::FunctionRegistry function_registry_;
  CelTypeRegistry type_registry_;
  cel::RuntimeOptions options_;
  BuilderWarnings builder_warnings_;
  Resolver resolver_;
};

absl::StatusOr<std::unique_ptr<cel::ast::Ast>> ParseFromCel(
    absl::string_view expression) {
  CEL_ASSIGN_OR_RETURN(ParsedExpr expr, Parse(expression));
  return cel::extensions::CreateAstFromParsedExpr(expr);
}

// While CEL doesn't provide execution order guarantees per se, short circuiting
// operators are treated specially to evaluate to user expectations.
//
// These behaviors aren't easily observable since the flat expression doesn't
// expose any details about the program after building, so a lot of setup is
// needed to simulate what the expression builder does.
TEST_F(UpdatedConstantFoldingTest, SkipsTernary) {
  // Arrange
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::ast::Ast> ast,
                       ParseFromCel("true ? true : false"));
  AstImpl& ast_impl = AstImpl::CastFromPublicAst(*ast);

  const Expr& call = ast_impl.root_expr();
  const Expr& condition = call.call_expr().args()[0];
  const Expr& true_branch = call.call_expr().args()[1];
  const Expr& false_branch = call.call_expr().args()[2];

  PlannerContext::ProgramTree tree;
  PlannerContext::ProgramInfo& call_info = tree[&call];
  call_info.range_start = 0;
  call_info.range_len = 4;
  call_info.children = {&condition, &true_branch, &false_branch};

  PlannerContext::ProgramInfo& condition_info = tree[&condition];
  condition_info.range_start = 0;
  condition_info.range_len = 1;
  condition_info.parent = &call;

  PlannerContext::ProgramInfo& true_branch_info = tree[&true_branch];
  true_branch_info.range_start = 1;
  true_branch_info.range_len = 1;
  true_branch_info.parent = &call;

  PlannerContext::ProgramInfo& false_branch_info = tree[&false_branch];
  false_branch_info.range_start = 2;
  false_branch_info.range_len = 1;
  false_branch_info.parent = &call;

  // Mock execution path that has placeholders for the non-shortcircuiting
  // version of ternary.
  ExecutionPath path;

  ASSERT_OK_AND_ASSIGN(path.emplace_back(),
                       CreateConstValueStep(Constant(ConstantKind(true)), -1));

  ASSERT_OK_AND_ASSIGN(path.emplace_back(),
                       CreateConstValueStep(Constant(ConstantKind(true)), -1));

  ASSERT_OK_AND_ASSIGN(path.emplace_back(),
                       CreateConstValueStep(Constant(ConstantKind(false)), -1));

  // Just a placeholder.
  ASSERT_OK_AND_ASSIGN(
      path.emplace_back(),
      CreateConstValueStep(Constant(NullValue::kNullValue), -1));

  PlannerContext context(resolver_, type_registry_, options_, builder_warnings_,
                         path, tree);

  google::protobuf::Arena arena;
  constexpr int kStackLimit = 1;
  ProgramOptimizerFactory constant_folder_factory =
      CreateConstantFoldingExtension(&arena, {kStackLimit});

  // Act
  // Issue the visitation calls.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ProgramOptimizer> constant_folder,
                       constant_folder_factory(context, ast_impl));
  ASSERT_OK(constant_folder->OnPreVisit(context, call));
  ASSERT_OK(constant_folder->OnPreVisit(context, condition));
  ASSERT_OK(constant_folder->OnPostVisit(context, condition));
  ASSERT_OK(constant_folder->OnPreVisit(context, true_branch));
  ASSERT_OK(constant_folder->OnPostVisit(context, true_branch));
  ASSERT_OK(constant_folder->OnPreVisit(context, false_branch));
  ASSERT_OK(constant_folder->OnPostVisit(context, false_branch));
  ASSERT_OK(constant_folder->OnPostVisit(context, call));

  // Assert
  // No changes attempted.
  EXPECT_THAT(path, SizeIs(4));
}

TEST_F(UpdatedConstantFoldingTest, SkipsOr) {
  // Arrange
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::ast::Ast> ast,
                       ParseFromCel("false || true"));
  AstImpl& ast_impl = AstImpl::CastFromPublicAst(*ast);

  const Expr& call = ast_impl.root_expr();
  const Expr& left_condition = call.call_expr().args()[0];
  const Expr& right_condition = call.call_expr().args()[1];

  PlannerContext::ProgramTree tree;
  PlannerContext::ProgramInfo& call_info = tree[&call];
  call_info.range_start = 0;
  call_info.range_len = 4;
  call_info.children = {&left_condition, &right_condition};

  PlannerContext::ProgramInfo& left_condition_info = tree[&left_condition];
  left_condition_info.range_start = 0;
  left_condition_info.range_len = 1;
  left_condition_info.parent = &call;

  PlannerContext::ProgramInfo& right_condition_info = tree[&right_condition];
  right_condition_info.range_start = 1;
  right_condition_info.range_len = 1;
  right_condition_info.parent = &call;

  // Mock execution path that has placeholders for the non-shortcircuiting
  // version of ternary.
  ExecutionPath path;

  ASSERT_OK_AND_ASSIGN(path.emplace_back(),
                       CreateConstValueStep(Constant(ConstantKind(false)), -1));

  ASSERT_OK_AND_ASSIGN(path.emplace_back(),
                       CreateConstValueStep(Constant(ConstantKind(true)), -1));

  // Just a placeholder.
  ASSERT_OK_AND_ASSIGN(
      path.emplace_back(),
      CreateConstValueStep(Constant(NullValue::kNullValue), -1));

  PlannerContext context(resolver_, type_registry_, options_, builder_warnings_,
                         path, tree);

  google::protobuf::Arena arena;
  constexpr int kStackLimit = 1;
  ProgramOptimizerFactory constant_folder_factory =
      CreateConstantFoldingExtension(&arena, {kStackLimit});

  // Act
  // Issue the visitation calls.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ProgramOptimizer> constant_folder,
                       constant_folder_factory(context, ast_impl));
  ASSERT_OK(constant_folder->OnPreVisit(context, call));
  ASSERT_OK(constant_folder->OnPreVisit(context, left_condition));
  ASSERT_OK(constant_folder->OnPostVisit(context, left_condition));
  ASSERT_OK(constant_folder->OnPreVisit(context, right_condition));
  ASSERT_OK(constant_folder->OnPostVisit(context, right_condition));
  ASSERT_OK(constant_folder->OnPostVisit(context, call));

  // Assert
  // No changes attempted.
  EXPECT_THAT(path, SizeIs(3));
}

TEST_F(UpdatedConstantFoldingTest, SkipsAnd) {
  // Arrange
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::ast::Ast> ast,
                       ParseFromCel("true && false"));
  AstImpl& ast_impl = AstImpl::CastFromPublicAst(*ast);

  const Expr& call = ast_impl.root_expr();
  const Expr& left_condition = call.call_expr().args()[0];
  const Expr& right_condition = call.call_expr().args()[1];

  PlannerContext::ProgramTree tree;
  PlannerContext::ProgramInfo& call_info = tree[&call];
  call_info.range_start = 0;
  call_info.range_len = 4;
  call_info.children = {&left_condition, &right_condition};

  PlannerContext::ProgramInfo& left_condition_info = tree[&left_condition];
  left_condition_info.range_start = 0;
  left_condition_info.range_len = 1;
  left_condition_info.parent = &call;

  PlannerContext::ProgramInfo& right_condition_info = tree[&right_condition];
  right_condition_info.range_start = 1;
  right_condition_info.range_len = 1;
  right_condition_info.parent = &call;

  // Mock execution path that has placeholders for the non-shortcircuiting
  // version of ternary.
  ExecutionPath path;

  ASSERT_OK_AND_ASSIGN(path.emplace_back(),
                       CreateConstValueStep(Constant(ConstantKind(true)), -1));

  ASSERT_OK_AND_ASSIGN(path.emplace_back(),
                       CreateConstValueStep(Constant(ConstantKind(false)), -1));

  // Just a placeholder.
  ASSERT_OK_AND_ASSIGN(
      path.emplace_back(),
      CreateConstValueStep(Constant(NullValue::kNullValue), -1));

  PlannerContext context(resolver_, type_registry_, options_, builder_warnings_,
                         path, tree);

  google::protobuf::Arena arena;
  constexpr int kStackLimit = 1;
  ProgramOptimizerFactory constant_folder_factory =
      CreateConstantFoldingExtension(&arena, {kStackLimit});

  // Act
  // Issue the visitation calls.
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ProgramOptimizer> constant_folder,
                       constant_folder_factory(context, ast_impl));
  ASSERT_OK(constant_folder->OnPreVisit(context, call));
  ASSERT_OK(constant_folder->OnPreVisit(context, left_condition));
  ASSERT_OK(constant_folder->OnPostVisit(context, left_condition));
  ASSERT_OK(constant_folder->OnPreVisit(context, right_condition));
  ASSERT_OK(constant_folder->OnPostVisit(context, right_condition));
  ASSERT_OK(constant_folder->OnPostVisit(context, call));

  // Assert
  // No changes attempted.
  EXPECT_THAT(path, SizeIs(3));
}

TEST_F(UpdatedConstantFoldingTest, ErrorsOnUnexpectedOrder) {
  // Arrange
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::ast::Ast> ast,
                       ParseFromCel("true && false"));
  AstImpl& ast_impl = AstImpl::CastFromPublicAst(*ast);

  const Expr& call = ast_impl.root_expr();
  const Expr& left_condition = call.call_expr().args()[0];
  const Expr& right_condition = call.call_expr().args()[1];

  PlannerContext::ProgramTree tree;
  PlannerContext::ProgramInfo& call_info = tree[&call];
  call_info.range_start = 0;
  call_info.range_len = 4;
  call_info.children = {&left_condition, &right_condition};

  PlannerContext::ProgramInfo& left_condition_info = tree[&left_condition];
  left_condition_info.range_start = 0;
  left_condition_info.range_len = 1;
  left_condition_info.parent = &call;

  PlannerContext::ProgramInfo& right_condition_info = tree[&right_condition];
  right_condition_info.range_start = 1;
  right_condition_info.range_len = 1;
  right_condition_info.parent = &call;

  // Mock execution path that has placeholders for the non-shortcircuiting
  // version of ternary.
  ExecutionPath path;

  ASSERT_OK_AND_ASSIGN(path.emplace_back(),
                       CreateConstValueStep(Constant(ConstantKind(true)), -1));

  ASSERT_OK_AND_ASSIGN(path.emplace_back(),
                       CreateConstValueStep(Constant(ConstantKind(false)), -1));

  // Just a placeholder.
  ASSERT_OK_AND_ASSIGN(
      path.emplace_back(),
      CreateConstValueStep(Constant(NullValue::kNullValue), -1));

  PlannerContext context(resolver_, type_registry_, options_, builder_warnings_,
                         path, tree);

  google::protobuf::Arena arena;
  constexpr int kStackLimit = 1;
  ProgramOptimizerFactory constant_folder_factory =
      CreateConstantFoldingExtension(&arena, {kStackLimit});

  // Act / Assert
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<ProgramOptimizer> constant_folder,
                       constant_folder_factory(context, ast_impl));
  EXPECT_THAT(constant_folder->OnPostVisit(context, left_condition),
              StatusIs(absl::StatusCode::kInternal));
}

}  // namespace

}  // namespace cel::ast::internal
