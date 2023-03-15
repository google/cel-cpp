#include "eval/compiler/constant_folding.h"

#include <string>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/text_format.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "base/value_factory.h"
#include "base/values/bool_value.h"
#include "base/values/error_value.h"
#include "base/values/int_value.h"
#include "base/values/list_value.h"
#include "base/values/string_value.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_function_registry.h"
#include "eval/testutil/test_message.pb.h"
#include "extensions/protobuf/ast_converters.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"
#include "internal/testing.h"

namespace cel::ast::internal {

namespace {

using ::cel::extensions::ProtoMemoryManager;
using ::cel::extensions::internal::ConvertProtoExprToNative;
using ::google::api::expr::runtime::CelFunctionRegistry;
using ::google::protobuf::Arena;

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
  FoldConstants(native_expr, registry, &arena, idents, out);
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
  FoldConstants(native_expr, registry, &arena, idents, out);

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
  FoldConstants(native_expr, registry, &arena, idents, out);

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
  FoldConstants(native_expr, registry, &arena, idents, out);

  ASSERT_EQ(out.id(), 45);
  ASSERT_TRUE(out.has_ident_expr());
  ASSERT_EQ(idents.size(), 1);
  auto value = idents[out.ident_expr().name()];
  ASSERT_TRUE(value->Is<ListValue>());
  const auto& list = value.As<ListValue>();
  ASSERT_EQ(list->size(), 2);
  ASSERT_OK_AND_ASSIGN(auto elem0, list->Get(value_factory_, 0));
  ASSERT_OK_AND_ASSIGN(auto elem1, list->Get(value_factory_, 1));
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
  FoldConstants(native_expr, registry, &arena, idents, out);

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
  FoldConstants(native_expr, registry, &arena, idents, out);

  ASSERT_EQ(out.id(), 15);
  ASSERT_TRUE(out.has_ident_expr());
  ASSERT_EQ(idents.size(), 1);
  ASSERT_TRUE(idents[out.ident_expr().name()]->Is<ListValue>());

  const auto& list = idents[out.ident_expr().name()].As<ListValue>();
  ASSERT_EQ(list->size(), 2);
  ASSERT_EQ(list->Get(value_factory_, 0).value().As<IntValue>()->value(), 1);
  ASSERT_EQ(list->Get(value_factory_, 1).value().As<IntValue>()->value(), 2);
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
  FoldConstants(native_expr, registry, &arena, idents, out);

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
  FoldConstants(native_expr, registry, &arena, idents, out);

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
  FoldConstants(native_expr, registry, &arena, idents, out);

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

}  // namespace

}  // namespace cel::ast::internal
