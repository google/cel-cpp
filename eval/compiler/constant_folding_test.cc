#include "eval/compiler/constant_folding.h"

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_function_registry.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/status_macros.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

namespace {

using ::google::api::expr::v1alpha1::Expr;

// Validate select is preserved as-is
TEST(ConstantFoldingTest, Select) {
  Expr expr;
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

  google::protobuf::Arena arena;
  CelFunctionRegistry registry;
  absl::flat_hash_map<std::string, CelValue> idents;
  Expr out;
  FoldConstants(expr, registry, &arena, idents, &out);
  google::protobuf::util::MessageDifferencer md;
  EXPECT_TRUE(md.Compare(out, expr)) << out.DebugString();
  EXPECT_TRUE(idents.empty());
}

// Validate struct message creation
TEST(ConstantFoldingTest, StructMessage) {
  Expr expr;
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

  google::protobuf::Arena arena;
  CelFunctionRegistry registry;

  absl::flat_hash_map<std::string, CelValue> idents;
  Expr out;
  FoldConstants(expr, registry, &arena, idents, &out);

  Expr expected;
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
  google::protobuf::util::MessageDifferencer md;
  EXPECT_TRUE(md.Compare(out, expected)) << out.DebugString();

  EXPECT_EQ(idents.size(), 2);
  EXPECT_TRUE(idents["$v0"].IsString());
  EXPECT_EQ(idents["$v0"].StringOrDie().value(), "value1");
  EXPECT_TRUE(idents["$v1"].IsInt64());
  EXPECT_EQ(idents["$v1"].Int64OrDie(), 12);
}

// Validate struct creation is not folded but recursed into
TEST(ConstantFoldingTest, StructComprehension) {
  Expr expr;
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

  google::protobuf::Arena arena;
  CelFunctionRegistry registry;

  absl::flat_hash_map<std::string, CelValue> idents;
  Expr out;
  FoldConstants(expr, registry, &arena, idents, &out);

  Expr expected;
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
  google::protobuf::util::MessageDifferencer md;
  EXPECT_TRUE(md.Compare(out, expected)) << out.DebugString();

  EXPECT_EQ(idents.size(), 3);
  EXPECT_TRUE(idents["$v0"].IsString());
  EXPECT_EQ(idents["$v0"].StringOrDie().value(), "y");
  EXPECT_TRUE(idents["$v1"].IsString());
  EXPECT_TRUE(idents["$v2"].IsString());
}

TEST(ConstantFoldingTest, ListComprehension) {
  Expr expr;
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

  google::protobuf::Arena arena;
  CelFunctionRegistry registry;

  absl::flat_hash_map<std::string, CelValue> idents;
  Expr out;
  FoldConstants(expr, registry, &arena, idents, &out);

  ASSERT_EQ(out.id(), 45);
  ASSERT_TRUE(out.has_ident_expr()) << out.DebugString();
  ASSERT_EQ(idents.size(), 1);
  auto value = idents[out.ident_expr().name()];
  ASSERT_TRUE(value.IsList());
  const auto& list = *value.ListOrDie();
  ASSERT_EQ(list.size(), 2);
  ASSERT_TRUE(list[0].IsInt64());
  ASSERT_EQ(list[0].Int64OrDie(), 1);
  ASSERT_TRUE(list[1].IsList());
  ASSERT_EQ(list[1].ListOrDie()->size(), 2);
}

// Validate that logic function application are not folded
TEST(ConstantFoldingTest, LogicApplication) {
  Expr expr;
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

  google::protobuf::Arena arena;
  CelFunctionRegistry registry;
  ASSERT_OK(RegisterBuiltinFunctions(&registry));

  absl::flat_hash_map<std::string, CelValue> idents;
  Expr out;
  FoldConstants(expr, registry, &arena, idents, &out);

  ASSERT_EQ(out.id(), 105);
  ASSERT_TRUE(out.has_call_expr()) << out.DebugString();
  ASSERT_EQ(idents.size(), 2);
}

TEST(ConstantFoldingTest, FunctionApplication) {
  Expr expr;
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

  google::protobuf::Arena arena;
  CelFunctionRegistry registry;
  ASSERT_OK(RegisterBuiltinFunctions(&registry));

  absl::flat_hash_map<std::string, CelValue> idents;
  Expr out;
  FoldConstants(expr, registry, &arena, idents, &out);

  ASSERT_EQ(out.id(), 15);
  ASSERT_TRUE(out.has_ident_expr()) << out.DebugString();
  ASSERT_EQ(idents.size(), 1);
  ASSERT_TRUE(idents[out.ident_expr().name()].IsList());

  const auto& list = *idents[out.ident_expr().name()].ListOrDie();
  ASSERT_EQ(list.size(), 2);
  ASSERT_EQ(list[0].Int64OrDie(), 1);
  ASSERT_EQ(list[1].Int64OrDie(), 2);
}

TEST(ConstantFoldingTest, FunctionApplicationWithReceiver) {
  Expr expr;
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

  google::protobuf::Arena arena;
  CelFunctionRegistry registry;
  ASSERT_OK(RegisterBuiltinFunctions(&registry));

  absl::flat_hash_map<std::string, CelValue> idents;
  Expr out;
  FoldConstants(expr, registry, &arena, idents, &out);

  ASSERT_EQ(out.id(), 10);
  ASSERT_TRUE(out.has_ident_expr()) << out.DebugString();
  ASSERT_EQ(idents.size(), 1);
  ASSERT_TRUE(idents[out.ident_expr().name()].IsInt64());
  ASSERT_EQ(idents[out.ident_expr().name()].Int64OrDie(), 2);
}

TEST(ConstantFoldingTest, FunctionApplicationNoOverload) {
  Expr expr;
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

  google::protobuf::Arena arena;
  CelFunctionRegistry registry;
  ASSERT_OK(RegisterBuiltinFunctions(&registry));

  absl::flat_hash_map<std::string, CelValue> idents;
  Expr out;
  FoldConstants(expr, registry, &arena, idents, &out);

  ASSERT_EQ(out.id(), 16);
  ASSERT_TRUE(out.has_ident_expr()) << out.DebugString();
  ASSERT_EQ(idents.size(), 1);
  ASSERT_TRUE(CheckNoMatchingOverloadError(idents[out.ident_expr().name()]));
}

// Validate that comprehension is recursed into
TEST(ConstantFoldingTest, MapComprehension) {
  Expr expr;
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

  google::protobuf::Arena arena;
  CelFunctionRegistry registry;

  absl::flat_hash_map<std::string, CelValue> idents;
  Expr out;
  FoldConstants(expr, registry, &arena, idents, &out);

  Expr expected;
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
  google::protobuf::util::MessageDifferencer md;
  EXPECT_TRUE(md.Compare(out, expected)) << out.DebugString();

  EXPECT_EQ(idents.size(), 6);
  EXPECT_TRUE(idents["$v0"].IsBool());
  EXPECT_TRUE(idents["$v1"].IsInt64());
  EXPECT_TRUE(idents["$v2"].IsString());
  EXPECT_TRUE(idents["$v3"].IsInt64());
  EXPECT_TRUE(idents["$v4"].IsString());
  EXPECT_TRUE(idents["$v5"].IsInt64());
}

}  // namespace

}  // namespace google::api::expr::runtime
