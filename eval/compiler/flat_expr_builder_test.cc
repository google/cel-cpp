#include "eval/compiler/flat_expr_builder.h"

#include <memory>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/field_mask.pb.h"
#include "google/protobuf/text_format.h"
#include "absl/status/status.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_function_adapter.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/public/unknown_attribute_set.h"
#include "eval/public/unknown_set.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/status_macros.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {

namespace {

using google::api::expr::v1alpha1::CheckedExpr;
using google::api::expr::v1alpha1::Expr;
using google::api::expr::v1alpha1::SourceInfo;

using google::protobuf::FieldMask;
using testing::Eq;
using testing::HasSubstr;
using testing::Not;
using cel::internal::IsOk;
using cel::internal::StatusIs;

class ConcatFunction : public CelFunction {
 public:
  explicit ConcatFunction() : CelFunction(CreateDescriptor()) {}

  static CelFunctionDescriptor CreateDescriptor() {
    return CelFunctionDescriptor{
        "concat", false, {CelValue::Type::kString, CelValue::Type::kString}};
  }

  absl::Status Evaluate(absl::Span<const CelValue> args, CelValue* result,
                        google::protobuf::Arena* arena) const override {
    if (args.size() != 2) {
      return absl::InvalidArgumentError("Bad arguments number");
    }

    std::string concat = std::string(args[0].StringOrDie().value()) +
                         std::string(args[1].StringOrDie().value());

    auto* concatenated =
        google::protobuf::Arena::Create<std::string>(arena, std::move(concat));

    *result = CelValue::CreateString(concatenated);

    return absl::OkStatus();
  }
};

class RecorderFunction : public CelFunction {
 public:
  explicit RecorderFunction(const std::string& name, int* count)
      : CelFunction(CelFunctionDescriptor{name, false, {}}), count_(count) {}

  absl::Status Evaluate(absl::Span<const CelValue> args, CelValue* result,
                        google::protobuf::Arena* arena) const override {
    if (!args.empty()) {
      return absl::Status(absl::StatusCode::kInvalidArgument,
                          "Bad arguments number");
    }
    (*count_)++;
    *result = CelValue::CreateBool(true);
    return absl::OkStatus();
  }

  int* count_;
};

TEST(FlatExprBuilderTest, SimpleEndToEnd) {
  Expr expr;
  SourceInfo source_info;
  auto call_expr = expr.mutable_call_expr();
  call_expr->set_function("concat");

  auto arg1 = call_expr->add_args();
  arg1->mutable_const_expr()->set_string_value("prefix");

  auto arg2 = call_expr->add_args();
  arg2->mutable_ident_expr()->set_name("value");

  FlatExprBuilder builder;

  ASSERT_OK(
      builder.GetRegistry()->Register(absl::make_unique<ConcatFunction>()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder.CreateExpression(&expr, &source_info));

  std::string variable = "test";

  Activation activation;
  activation.InsertValue("value", CelValue::CreateString(&variable));

  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsString());
  EXPECT_THAT(result.StringOrDie().value(), Eq("prefixtest"));
}

TEST(FlatExprBuilderTest, ExprUnset) {
  Expr expr;
  SourceInfo source_info;
  FlatExprBuilder builder;
  EXPECT_THAT(builder.CreateExpression(&expr, &source_info).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid empty expression")));
}

TEST(FlatExprBuilderTest, ConstValueUnset) {
  Expr expr;
  SourceInfo source_info;
  FlatExprBuilder builder;
  // Create an empty constant expression to ensure that it triggers an error.
  expr.mutable_const_expr();

  EXPECT_THAT(builder.CreateExpression(&expr, &source_info).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Unsupported constant type")));
}

TEST(FlatExprBuilderTest, MapKeyValueUnset) {
  Expr expr;
  SourceInfo source_info;
  FlatExprBuilder builder;

  // Don't set either the key or the value for the map creation step.
  auto* entry = expr.mutable_struct_expr()->add_entries();
  EXPECT_THAT(builder.CreateExpression(&expr, &source_info).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Map entry missing key")));

  // Set the entry key, but not the value.
  entry->mutable_map_key()->mutable_const_expr()->set_bool_value(true);
  EXPECT_THAT(builder.CreateExpression(&expr, &source_info).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Map entry missing value")));
}

TEST(FlatExprBuilderTest, MessageFieldValueUnset) {
  Expr expr;
  SourceInfo source_info;
  FlatExprBuilder builder;

  // Don't set either the field or the value for the message creation step.
  auto* create_message = expr.mutable_struct_expr();
  create_message->set_message_name("google.protobuf.Value");
  auto* entry = create_message->add_entries();
  EXPECT_THAT(builder.CreateExpression(&expr, &source_info).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Message entry missing field name")));

  // Set the entry field, but not the value.
  entry->set_field_key("bool_value");
  EXPECT_THAT(builder.CreateExpression(&expr, &source_info).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Message entry missing value")));
}

TEST(FlatExprBuilderTest, BinaryCallTooManyArguments) {
  Expr expr;
  SourceInfo source_info;
  FlatExprBuilder builder;

  auto* call = expr.mutable_call_expr();
  call->set_function(builtin::kAnd);
  call->mutable_target()->mutable_const_expr()->set_string_value("random");
  call->add_args()->mutable_const_expr()->set_bool_value(false);
  call->add_args()->mutable_const_expr()->set_bool_value(true);

  EXPECT_THAT(builder.CreateExpression(&expr, &source_info).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid argument count")));
}

TEST(FlatExprBuilderTest, TernaryCallTooManyArguments) {
  Expr expr;
  SourceInfo source_info;
  FlatExprBuilder builder;

  auto* call = expr.mutable_call_expr();
  call->set_function(builtin::kTernary);
  call->mutable_target()->mutable_const_expr()->set_string_value("random");
  call->add_args()->mutable_const_expr()->set_bool_value(false);
  call->add_args()->mutable_const_expr()->set_int64_value(1);
  call->add_args()->mutable_const_expr()->set_int64_value(2);

  EXPECT_THAT(builder.CreateExpression(&expr, &source_info).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid argument count")));

  // Disable short-circuiting to ensure that a different visitor is used.
  builder.set_shortcircuiting(false);
  EXPECT_THAT(builder.CreateExpression(&expr, &source_info).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid argument count")));
}

TEST(FlatExprBuilderTest, DelayedFunctionResolutionErrors) {
  Expr expr;
  SourceInfo source_info;
  auto call_expr = expr.mutable_call_expr();
  call_expr->set_function("concat");

  auto arg1 = call_expr->add_args();
  arg1->mutable_const_expr()->set_string_value("prefix");

  auto arg2 = call_expr->add_args();
  arg2->mutable_ident_expr()->set_name("value");

  FlatExprBuilder builder;
  builder.set_fail_on_warnings(false);
  std::vector<absl::Status> warnings;

  // Concat function not registered.

  ASSERT_OK_AND_ASSIGN(
      auto cel_expr, builder.CreateExpression(&expr, &source_info, &warnings));

  std::string variable = "test";
  Activation activation;
  activation.InsertValue("value", CelValue::CreateString(&variable));

  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsError());
  EXPECT_THAT(result.ErrorOrDie()->message(),
              Eq("No matching overloads found"));

  ASSERT_THAT(warnings, testing::SizeIs(1));
  EXPECT_EQ(warnings[0].code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(std::string(warnings[0].message()),
              testing::HasSubstr("No overloads provided"));
}

TEST(FlatExprBuilderTest, Shortcircuiting) {
  Expr expr;
  SourceInfo source_info;
  auto call_expr = expr.mutable_call_expr();
  call_expr->set_function("_||_");

  auto arg1 = call_expr->add_args();
  arg1->mutable_call_expr()->set_function("recorder1");

  auto arg2 = call_expr->add_args();
  arg2->mutable_call_expr()->set_function("recorder2");

  FlatExprBuilder builder;
  auto builtin = RegisterBuiltinFunctions(builder.GetRegistry());

  int count1 = 0;
  int count2 = 0;

  ASSERT_OK(builder.GetRegistry()->Register(
      absl::make_unique<RecorderFunction>("recorder1", &count1)));
  ASSERT_OK(builder.GetRegistry()->Register(
      absl::make_unique<RecorderFunction>("recorder2", &count2)));

  // Shortcircuiting on.
  ASSERT_OK_AND_ASSIGN(auto cel_expr_on,
                       builder.CreateExpression(&expr, &source_info));
  Activation activation;
  google::protobuf::Arena arena;
  auto eval_on = cel_expr_on->Evaluate(activation, &arena);
  ASSERT_OK(eval_on);

  EXPECT_THAT(count1, Eq(1));
  EXPECT_THAT(count2, Eq(0));

  // Shortcircuiting off.
  builder.set_shortcircuiting(false);
  ASSERT_OK_AND_ASSIGN(auto cel_expr_off,
                       builder.CreateExpression(&expr, &source_info));
  count1 = 0;
  count2 = 0;

  ASSERT_OK(cel_expr_off->Evaluate(activation, &arena));
  EXPECT_THAT(count1, Eq(1));
  EXPECT_THAT(count2, Eq(1));
}

TEST(FlatExprBuilderTest, ShortcircuitingComprehension) {
  Expr expr;
  SourceInfo source_info;
  auto comprehension_expr = expr.mutable_comprehension_expr();
  comprehension_expr->set_iter_var("x");
  auto list_expr =
      comprehension_expr->mutable_iter_range()->mutable_list_expr();
  list_expr->add_elements()->mutable_const_expr()->set_int64_value(1);
  list_expr->add_elements()->mutable_const_expr()->set_int64_value(2);
  list_expr->add_elements()->mutable_const_expr()->set_int64_value(3);
  comprehension_expr->set_accu_var("accu");
  comprehension_expr->mutable_accu_init()->mutable_const_expr()->set_bool_value(
      false);
  comprehension_expr->mutable_loop_condition()
      ->mutable_const_expr()
      ->set_bool_value(false);
  comprehension_expr->mutable_loop_step()->mutable_call_expr()->set_function(
      "loop_step");
  comprehension_expr->mutable_result()->mutable_const_expr()->set_bool_value(
      false);

  FlatExprBuilder builder;
  auto builtin = RegisterBuiltinFunctions(builder.GetRegistry());

  int count = 0;
  ASSERT_OK(builder.GetRegistry()->Register(
      absl::make_unique<RecorderFunction>("loop_step", &count)));

  // Shortcircuiting on.
  ASSERT_OK_AND_ASSIGN(auto cel_expr_on,
                       builder.CreateExpression(&expr, &source_info));
  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK(cel_expr_on->Evaluate(activation, &arena));
  EXPECT_THAT(count, Eq(0));

  // Shortcircuiting off.
  builder.set_shortcircuiting(false);
  ASSERT_OK_AND_ASSIGN(auto cel_expr_off,
                       builder.CreateExpression(&expr, &source_info));
  count = 0;
  ASSERT_OK(cel_expr_off->Evaluate(activation, &arena));
  EXPECT_THAT(count, Eq(3));
}

TEST(FlatExprBuilderTest, IdentExprUnsetName) {
  Expr expr;
  SourceInfo source_info;
  // An empty ident without the name set should error.
  google::protobuf::TextFormat::ParseFromString(R"(ident_expr {})", &expr);

  FlatExprBuilder builder;
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  EXPECT_THAT(builder.CreateExpression(&expr, &source_info).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("'name' must not be empty")));
}

TEST(FlatExprBuilderTest, SelectExprUnsetField) {
  Expr expr;
  SourceInfo source_info;
  // An empty ident without the name set should error.
  google::protobuf::TextFormat::ParseFromString(R"(select_expr{
    operand{ ident_expr {name: 'var'} }
    })",
                                      &expr);

  FlatExprBuilder builder;
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  EXPECT_THAT(builder.CreateExpression(&expr, &source_info).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("'field' must not be empty")));
}

TEST(FlatExprBuilderTest, ComprehensionExprUnsetAccuVar) {
  Expr expr;
  SourceInfo source_info;
  // An empty ident without the name set should error.
  google::protobuf::TextFormat::ParseFromString(R"(comprehension_expr{})", &expr);
  FlatExprBuilder builder;
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  EXPECT_THAT(builder.CreateExpression(&expr, &source_info).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("'accu_var' must not be empty")));
}

TEST(FlatExprBuilderTest, ComprehensionExprUnsetIterVar) {
  Expr expr;
  SourceInfo source_info;
  // An empty ident without the name set should error.
  google::protobuf::TextFormat::ParseFromString(R"(
      comprehension_expr{accu_var: "a"}
    )",
                                      &expr);
  FlatExprBuilder builder;
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  EXPECT_THAT(builder.CreateExpression(&expr, &source_info).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("'iter_var' must not be empty")));
}

TEST(FlatExprBuilderTest, ComprehensionExprUnsetAccuInit) {
  Expr expr;
  SourceInfo source_info;
  // An empty ident without the name set should error.
  google::protobuf::TextFormat::ParseFromString(R"(
    comprehension_expr{
      accu_var: "a"
      iter_var: "b"}
    )",
                                      &expr);
  FlatExprBuilder builder;
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  EXPECT_THAT(builder.CreateExpression(&expr, &source_info).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("'accu_init' must be set")));
}

TEST(FlatExprBuilderTest, ComprehensionExprUnsetLoopCondition) {
  Expr expr;
  SourceInfo source_info;
  // An empty ident without the name set should error.
  google::protobuf::TextFormat::ParseFromString(R"(
    comprehension_expr{
      accu_var: 'a'
      iter_var: 'b'
      accu_init {
        const_expr {bool_value: true}
      }}
    )",
                                      &expr);
  FlatExprBuilder builder;
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  EXPECT_THAT(builder.CreateExpression(&expr, &source_info).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("'loop_condition' must be set")));
}

TEST(FlatExprBuilderTest, ComprehensionExprUnsetLoopStep) {
  Expr expr;
  SourceInfo source_info;
  // An empty ident without the name set should error.
  google::protobuf::TextFormat::ParseFromString(R"(
    comprehension_expr{
      accu_var: 'a'
      iter_var: 'b'
      accu_init {
        const_expr {bool_value: true}
      }
      loop_condition {
        const_expr {bool_value: true}
      }}
    )",
                                      &expr);
  FlatExprBuilder builder;
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  EXPECT_THAT(builder.CreateExpression(&expr, &source_info).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("'loop_step' must be set")));
}

TEST(FlatExprBuilderTest, ComprehensionExprUnsetResult) {
  Expr expr;
  SourceInfo source_info;
  // An empty ident without the name set should error.
  google::protobuf::TextFormat::ParseFromString(R"(
    comprehension_expr{
      accu_var: 'a'
      iter_var: 'b'
      accu_init {
        const_expr {bool_value: true}
      }
      loop_condition {
        const_expr {bool_value: true}
      }
      loop_step {
        const_expr {bool_value: false}
      }}
    )",
                                      &expr);
  FlatExprBuilder builder;
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  EXPECT_THAT(builder.CreateExpression(&expr, &source_info).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("'result' must be set")));
}

TEST(FlatExprBuilderTest, MapComprehension) {
  Expr expr;
  SourceInfo source_info;
  // {1: "", 2: ""}.all(x, x > 0)
  google::protobuf::TextFormat::ParseFromString(R"(
    comprehension_expr {
      iter_var: "k"
      accu_var: "accu"
      accu_init {
        const_expr { bool_value: true }
      }
      loop_condition { ident_expr { name: "accu" } }
      result { ident_expr { name: "accu" } }
      loop_step {
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
        struct_expr {
          entries {
            map_key { const_expr { int64_value: 1 } }
            value { const_expr { string_value: "" } }
          }
          entries {
            map_key { const_expr { int64_value: 2 } }
            value { const_expr { string_value: "" } }
          }
        }
      }
    })",
                                      &expr);

  FlatExprBuilder builder;
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder.CreateExpression(&expr, &source_info));

  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsBool());
  EXPECT_TRUE(result.BoolOrDie());
}

TEST(FlatExprBuilderTest, InvalidContainer) {
  Expr expr;
  SourceInfo source_info;
  // foo && bar
  google::protobuf::TextFormat::ParseFromString(R"(
    call_expr {
      function: "_&&_"
      args {
        ident_expr {
          name: "foo"
        }
      }
      args {
        ident_expr {
          name: "bar"
        }
      }
    })",
                                      &expr);

  FlatExprBuilder builder;
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));

  builder.set_container(".bad");
  EXPECT_THAT(builder.CreateExpression(&expr, &source_info).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("container: '.bad'")));

  builder.set_container("bad.");
  EXPECT_THAT(builder.CreateExpression(&expr, &source_info).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("container: 'bad.'")));
}

TEST(FlatExprBuilderTest, BasicCheckedExprSupport) {
  CheckedExpr expr;
  // foo && bar
  google::protobuf::TextFormat::ParseFromString(R"(
    expr {
      id: 1
      call_expr {
        function: "_&&_"
        args {
          id: 2
          ident_expr {
            name: "foo"
          }
        }
        args {
          id: 3
          ident_expr {
            name: "bar"
          }
        }
      }
    })",
                                      &expr);

  FlatExprBuilder builder;
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder.CreateExpression(&expr));

  Activation activation;
  activation.InsertValue("foo", CelValue::CreateBool(true));
  activation.InsertValue("bar", CelValue::CreateBool(true));
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsBool());
  EXPECT_TRUE(result.BoolOrDie());
}

TEST(FlatExprBuilderTest, CheckedExprWithReferenceMap) {
  CheckedExpr expr;
  // `foo.var1` && `bar.var2`
  google::protobuf::TextFormat::ParseFromString(R"(
    reference_map {
      key: 2
      value {
        name: "foo.var1"
      }
    }
    reference_map {
      key: 4
      value {
        name: "bar.var2"
      }
    }
    expr {
      id: 1
      call_expr {
        function: "_&&_"
        args {
          id: 2
          select_expr {
            field: "var1"
            operand {
              id: 3
              ident_expr {
                name: "foo"
              }
            }
          }
        }
        args {
          id: 4
          select_expr {
            field: "var2"
            operand {
              ident_expr {
                name: "bar"
              }
            }
          }
        }
      }
    })",
                                      &expr);

  FlatExprBuilder builder;
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder.CreateExpression(&expr));

  Activation activation;
  activation.InsertValue("foo.var1", CelValue::CreateBool(true));
  activation.InsertValue("bar.var2", CelValue::CreateBool(true));
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsBool());
  EXPECT_TRUE(result.BoolOrDie());
}

TEST(FlatExprBuilderTest, CheckedExprWithReferenceMapFunction) {
  CheckedExpr expr;
  // ext.and(var1, bar.var2)
  google::protobuf::TextFormat::ParseFromString(R"(
    reference_map {
      key: 1
      value {
        overload_id: "com.foo.ext.and"
      }
    }
    reference_map {
      key: 3
      value {
        name: "com.foo.var1"
      }
    }
    reference_map {
      key: 4
      value {
        name: "bar.var2"
      }
    }
    expr {
      id: 1
      call_expr {
        function: "and"
        target {
          id: 2
          ident_expr {
            name: "ext"
          }
        }
        args {
          id: 3
          ident_expr {
            name: "var1"
          }
        }
        args {
          id: 4
          select_expr {
            field: "var2"
            operand {
              id: 5
              ident_expr {
                name: "bar"
              }
            }
          }
        }
      }
    })",
                                      &expr);

  FlatExprBuilder builder;
  builder.set_container("com.foo");
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  ASSERT_OK((FunctionAdapter<bool, bool, bool>::CreateAndRegister(
      "com.foo.ext.and", false,
      [](google::protobuf::Arena*, bool lhs, bool rhs) { return lhs && rhs; },
      builder.GetRegistry())));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder.CreateExpression(&expr));

  Activation activation;
  activation.InsertValue("com.foo.var1", CelValue::CreateBool(true));
  activation.InsertValue("bar.var2", CelValue::CreateBool(true));
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsBool());
  EXPECT_TRUE(result.BoolOrDie());
}

TEST(FlatExprBuilderTest, CheckedExprActivationMissesReferences) {
  CheckedExpr expr;
  // <foo.var1> && <bar>.<var2>
  google::protobuf::TextFormat::ParseFromString(R"(
    reference_map {
      key: 2
      value {
        name: "foo.var1"
      }
    }
    reference_map {
      key: 5
      value {
        name: "bar"
      }
    }
    expr {
      id: 1
      call_expr {
        function: "_&&_"
        args {
          id: 2
          select_expr {
            field: "var1"
            operand {
              id: 3
              ident_expr {
                name: "foo"
              }
            }
          }
        }
        args {
          id: 4
          select_expr {
            field: "var2"
            operand {
              id: 5
              ident_expr {
                name: "bar"
              }
            }
          }
        }
      }
    })",
                                      &expr);

  FlatExprBuilder builder;
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder.CreateExpression(&expr));

  Activation activation;
  activation.InsertValue("foo.var1", CelValue::CreateBool(true));
  // Activation tries to bind a namespaced variable but the reference map refers
  // to the container 'bar'.
  activation.InsertValue("bar.var2", CelValue::CreateBool(true));
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsError());
  EXPECT_THAT(*(result.ErrorOrDie()),
              StatusIs(absl::StatusCode::kUnknown,
                       HasSubstr("No value with name \"bar\" found")));

  // Re-run with the expected interpretation of `bar`.`var2`
  std::vector<std::pair<CelValue, CelValue>> map_pairs{
      {CelValue::CreateStringView("var2"), CelValue::CreateBool(false)}};

  std::unique_ptr<CelMap> map_value =
      *CreateContainerBackedMap(absl::MakeSpan(map_pairs));
  activation.InsertValue("bar", CelValue::CreateMap(map_value.get()));
  ASSERT_OK_AND_ASSIGN(result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsBool());
  EXPECT_FALSE(result.BoolOrDie());
}

TEST(FlatExprBuilderTest, CheckedExprWithReferenceMapAndConstantFolding) {
  CheckedExpr expr;
  // {`var1`: 'hello'}
  google::protobuf::TextFormat::ParseFromString(R"(
    reference_map {
      key: 3
      value {
        name: "var1"
        value {
          int64_value: 1
        }
      }
    }
    expr {
      id: 1
      struct_expr {
        entries {
          id: 2
          map_key {
            id: 3
            ident_expr {
              name: "var1"
            }
          }
          value {
            id: 4
            const_expr {
              string_value: "hello"
            }
          }
        }
      }
    })",
                                      &expr);

  FlatExprBuilder builder;
  google::protobuf::Arena arena;
  builder.set_constant_folding(true, &arena);
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder.CreateExpression(&expr));

  Activation activation;
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsMap());
  auto m = result.MapOrDie();
  auto v = (*m)[CelValue::CreateInt64(1L)];
  EXPECT_THAT(v->StringOrDie().value(), Eq("hello"));
}

TEST(FlatExprBuilderTest, ComprehensionWorksForError) {
  Expr expr;
  SourceInfo source_info;
  // {}[0].all(x, x) should evaluate OK but return an error value
  google::protobuf::TextFormat::ParseFromString(R"(
    id: 4
    comprehension_expr {
      iter_var: "x"
      iter_range {
        id: 2
        call_expr {
          function: "_[_]"
          args {
            id: 1
            struct_expr {
            }
          }
          args {
            id: 3
            const_expr {
              int64_value: 0
            }
          }
        }
      }
      accu_var: "__result__"
      accu_init {
        id: 7
        const_expr {
          bool_value: true
        }
      }
      loop_condition {
        id: 8
        call_expr {
          function: "__not_strictly_false__"
          args {
            id: 9
            ident_expr {
              name: "__result__"
            }
          }
        }
      }
      loop_step {
        id: 10
        call_expr {
          function: "_&&_"
          args {
            id: 11
            ident_expr {
              name: "__result__"
            }
          }
          args {
            id: 6
            ident_expr {
              name: "x"
            }
          }
        }
      }
      result {
        id: 12
        ident_expr {
          name: "__result__"
        }
      }
    })",
                                      &expr);

  FlatExprBuilder builder;
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder.CreateExpression(&expr, &source_info));

  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsError());
}

TEST(FlatExprBuilderTest, ComprehensionWorksForNonContainer) {
  Expr expr;
  SourceInfo source_info;
  // 0.all(x, x) should evaluate OK but return an error value.
  google::protobuf::TextFormat::ParseFromString(R"(
    id: 4
    comprehension_expr {
      iter_var: "x"
      iter_range {
        id: 2
        const_expr {
          int64_value: 0
        }
      }
      accu_var: "__result__"
      accu_init {
        id: 7
        const_expr {
          bool_value: true
        }
      }
      loop_condition {
        id: 8
        call_expr {
          function: "__not_strictly_false__"
          args {
            id: 9
            ident_expr {
              name: "__result__"
            }
          }
        }
      }
      loop_step {
        id: 10
        call_expr {
          function: "_&&_"
          args {
            id: 11
            ident_expr {
              name: "__result__"
            }
          }
          args {
            id: 6
            ident_expr {
              name: "x"
            }
          }
        }
      }
      result {
        id: 12
        ident_expr {
          name: "__result__"
        }
      }
    })",
                                      &expr);

  FlatExprBuilder builder;
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder.CreateExpression(&expr, &source_info));

  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsError());
  EXPECT_THAT(result.ErrorOrDie()->message(),
              Eq("No matching overloads found <iter_range>"));
}

TEST(FlatExprBuilderTest, ComprehensionBudget) {
  Expr expr;
  SourceInfo source_info;
  // [1, 2].all(x, x > 0)
  google::protobuf::TextFormat::ParseFromString(R"(
    comprehension_expr {
      iter_var: "k"
      accu_var: "accu"
      accu_init {
        const_expr { bool_value: true }
      }
      loop_condition { ident_expr { name: "accu" } }
      result { ident_expr { name: "accu" } }
      loop_step {
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
        list_expr {
          { const_expr { int64_value: 1 } }
          { const_expr { int64_value: 2 } }
        }
      }
    })",
                                      &expr);

  FlatExprBuilder builder;
  builder.set_comprehension_max_iterations(1);
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder.CreateExpression(&expr, &source_info));

  Activation activation;
  google::protobuf::Arena arena;
  EXPECT_THAT(cel_expr->Evaluate(activation, &arena).status(),
              StatusIs(absl::StatusCode::kInternal,
                       HasSubstr("Iteration budget exceeded")));
}

TEST(FlatExprBuilderTest, UnknownSupportTest) {
  TestMessage message;

  Expr expr;
  SourceInfo source_info;
  auto select_expr = expr.mutable_select_expr();
  select_expr->set_field("int32_value");

  auto operand1 = select_expr->mutable_operand();
  auto select_expr1 = operand1->mutable_select_expr();

  select_expr1->set_field("message_value");
  auto operand2 = select_expr1->mutable_operand();

  operand2->mutable_ident_expr()->set_name("message");

  FlatExprBuilder builder;
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder.CreateExpression(&expr, &source_info));

  message.mutable_message_value()->set_int32_value(1);

  google::protobuf::Arena arena;
  Activation activation;
  activation.InsertValue("message",
                         CelProtoWrapper::CreateMessage(&message, &arena));

  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsInt64());
  EXPECT_THAT(result.Int64OrDie(), Eq(1));

  FieldMask mask;
  mask.add_paths("message.message_value.int32_value");
  activation.set_unknown_paths(mask);
  ASSERT_OK_AND_ASSIGN(result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsError());
  ASSERT_TRUE(IsUnknownValueError(result));
  EXPECT_THAT(GetUnknownPathsSetOrDie(result),
              Eq(std::set<std::string>({"message.message_value.int32_value"})));

  mask.clear_paths();
  mask.add_paths("message.message_value");
  activation.set_unknown_paths(mask);
  ASSERT_OK_AND_ASSIGN(result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsError());
  ASSERT_TRUE(IsUnknownValueError(result));
  EXPECT_THAT(GetUnknownPathsSetOrDie(result),
              Eq(std::set<std::string>({"message.message_value"})));
}

TEST(FlatExprBuilderTest, SimpleEnumTest) {
  TestMessage message;
  Expr expr;
  SourceInfo source_info;
  constexpr char enum_name[] =
      "google.api.expr.runtime.TestMessage.TestEnum.TEST_ENUM_1";

  std::vector<std::string> enum_name_parts = absl::StrSplit(enum_name, '.');
  Expr* cur_expr = &expr;

  for (int i = enum_name_parts.size() - 1; i > 0; i--) {
    auto select_expr = cur_expr->mutable_select_expr();
    select_expr->set_field(enum_name_parts[i]);
    cur_expr = select_expr->mutable_operand();
  }

  cur_expr->mutable_ident_expr()->set_name(enum_name_parts[0]);

  FlatExprBuilder builder;
  builder.GetTypeRegistry()->Register(TestMessage::TestEnum_descriptor());
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder.CreateExpression(&expr, &source_info));

  google::protobuf::Arena arena;
  Activation activation;
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsInt64());
  EXPECT_THAT(result.Int64OrDie(), Eq(TestMessage::TEST_ENUM_1));
}

TEST(FlatExprBuilderTest, SimpleEnumIdentTest) {
  TestMessage message;
  Expr expr;
  SourceInfo source_info;
  constexpr char enum_name[] =
      "google.api.expr.runtime.TestMessage.TestEnum.TEST_ENUM_1";

  Expr* cur_expr = &expr;
  cur_expr->mutable_ident_expr()->set_name(enum_name);

  FlatExprBuilder builder;
  builder.GetTypeRegistry()->Register(TestMessage::TestEnum_descriptor());
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder.CreateExpression(&expr, &source_info));

  google::protobuf::Arena arena;
  Activation activation;
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsInt64());
  EXPECT_THAT(result.Int64OrDie(), Eq(TestMessage::TEST_ENUM_1));
}

TEST(FlatExprBuilderTest, ContainerStringFormat) {
  Expr expr;
  SourceInfo source_info;
  expr.mutable_ident_expr()->set_name("ident");

  FlatExprBuilder builder;
  builder.set_container("");
  ASSERT_OK(builder.CreateExpression(&expr, &source_info));

  builder.set_container("random.namespace");
  ASSERT_OK(builder.CreateExpression(&expr, &source_info));

  // Leading '.'
  builder.set_container(".random.namespace");
  EXPECT_THAT(builder.CreateExpression(&expr, &source_info).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid expression container")));

  // Trailing '.'
  builder.set_container("random.namespace.");
  EXPECT_THAT(builder.CreateExpression(&expr, &source_info).status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("Invalid expression container")));
}

void EvalExpressionWithEnum(absl::string_view enum_name,
                            absl::string_view container, CelValue* result) {
  TestMessage message;

  Expr expr;
  SourceInfo source_info;

  std::vector<std::string> enum_name_parts = absl::StrSplit(enum_name, '.');
  Expr* cur_expr = &expr;

  for (int i = enum_name_parts.size() - 1; i > 0; i--) {
    auto select_expr = cur_expr->mutable_select_expr();
    select_expr->set_field(enum_name_parts[i]);
    cur_expr = select_expr->mutable_operand();
  }

  cur_expr->mutable_ident_expr()->set_name(enum_name_parts[0]);

  FlatExprBuilder builder;
  builder.GetTypeRegistry()->Register(TestMessage::TestEnum_descriptor());
  builder.GetTypeRegistry()->Register(TestEnum_descriptor());
  builder.set_container(std::string(container));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder.CreateExpression(&expr, &source_info));

  google::protobuf::Arena arena;
  Activation activation;
  auto eval = cel_expr->Evaluate(activation, &arena);
  ASSERT_OK(eval);
  *result = eval.value();
}

TEST(FlatExprBuilderTest, ShortEnumResolution) {
  CelValue result;
  // Test resolution of "<EnumName>.<EnumValue>".
  ASSERT_NO_FATAL_FAILURE(EvalExpressionWithEnum(
      "TestEnum.TEST_ENUM_1", "google.api.expr.runtime.TestMessage", &result));
  ASSERT_TRUE(result.IsInt64());
  EXPECT_THAT(result.Int64OrDie(), Eq(TestMessage::TEST_ENUM_1));
}

TEST(FlatExprBuilderTest, FullEnumNameWithContainerResolution) {
  CelValue result;
  // Fully qualified name should work.
  ASSERT_NO_FATAL_FAILURE(EvalExpressionWithEnum(
      "google.api.expr.runtime.TestMessage.TestEnum.TEST_ENUM_1",
      "very.random.Namespace", &result));
  ASSERT_TRUE(result.IsInt64());
  EXPECT_THAT(result.Int64OrDie(), Eq(TestMessage::TEST_ENUM_1));
}

TEST(FlatExprBuilderTest, SameShortNameEnumResolution) {
  CelValue result;

  // This precondition validates that
  // TestMessage::TestEnum::TEST_ENUM1 and TestEnum::TEST_ENUM1 are compiled and
  // linked in and their values are different.
  ASSERT_TRUE(static_cast<int>(TestEnum::TEST_ENUM_1) !=
              static_cast<int>(TestMessage::TEST_ENUM_1));
  ASSERT_NO_FATAL_FAILURE(EvalExpressionWithEnum(
      "TestEnum.TEST_ENUM_1", "google.api.expr.runtime.TestMessage", &result));
  ASSERT_TRUE(result.IsInt64());
  EXPECT_THAT(result.Int64OrDie(), Eq(TestMessage::TEST_ENUM_1));

  // TEST_ENUM3 is present in google.api.expr.runtime.TestEnum, is absent in
  // google.api.expr.runtime.TestMessage.TestEnum.
  ASSERT_NO_FATAL_FAILURE(EvalExpressionWithEnum(
      "TestEnum.TEST_ENUM_3", "google.api.expr.runtime.TestMessage", &result));
  ASSERT_TRUE(result.IsInt64());
  EXPECT_THAT(result.Int64OrDie(), Eq(TestEnum::TEST_ENUM_3));

  ASSERT_NO_FATAL_FAILURE(EvalExpressionWithEnum(
      "TestEnum.TEST_ENUM_1", "google.api.expr.runtime", &result));
  ASSERT_TRUE(result.IsInt64());
  EXPECT_THAT(result.Int64OrDie(), Eq(TestEnum::TEST_ENUM_1));
}

TEST(FlatExprBuilderTest, PartialQualifiedEnumResolution) {
  CelValue result;
  ASSERT_NO_FATAL_FAILURE(EvalExpressionWithEnum(
      "runtime.TestMessage.TestEnum.TEST_ENUM_1", "google.api.expr", &result));

  ASSERT_TRUE(result.IsInt64());
  EXPECT_THAT(result.Int64OrDie(), Eq(TestMessage::TEST_ENUM_1));
}

TEST(FlatExprBuilderTest, MapFieldPresence) {
  Expr expr;
  SourceInfo source_info;
  google::protobuf::TextFormat::ParseFromString(R"(
    id: 1,
    select_expr{
      operand {
        id: 2
        ident_expr{ name: "msg" }
      }
      field: "string_int32_map"
      test_only: true
    })",
                                      &expr);

  FlatExprBuilder builder;
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder.CreateExpression(&expr, &source_info));

  google::protobuf::Arena arena;
  {
    TestMessage message;
    auto strMap = message.mutable_string_int32_map();
    strMap->insert({"key", 1});
    Activation activation;
    activation.InsertValue("msg",
                           CelProtoWrapper::CreateMessage(&message, &arena));
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsBool());
    ASSERT_TRUE(result.BoolOrDie());
  }
  {
    TestMessage message;
    Activation activation;
    activation.InsertValue("msg",
                           CelProtoWrapper::CreateMessage(&message, &arena));
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsBool());
    ASSERT_FALSE(result.BoolOrDie());
  }
}

TEST(FlatExprBuilderTest, RepeatedFieldPresence) {
  Expr expr;
  SourceInfo source_info;
  google::protobuf::TextFormat::ParseFromString(R"(
    id: 1,
    select_expr{
      operand {
        id: 2
        ident_expr{ name: "msg" }
      }
      field: "int32_list"
      test_only: true
    })",
                                      &expr);

  FlatExprBuilder builder;
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder.CreateExpression(&expr, &source_info));

  google::protobuf::Arena arena;
  {
    TestMessage message;
    message.add_int32_list(1);
    Activation activation;
    activation.InsertValue("msg",
                           CelProtoWrapper::CreateMessage(&message, &arena));
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsBool());
    ASSERT_TRUE(result.BoolOrDie());
  }
  {
    TestMessage message;
    Activation activation;
    activation.InsertValue("msg",
                           CelProtoWrapper::CreateMessage(&message, &arena));
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsBool());
    ASSERT_FALSE(result.BoolOrDie());
  }
}

absl::Status RunTernaryExpression(CelValue selector, CelValue value1,
                                  CelValue value2, google::protobuf::Arena* arena,
                                  CelValue* result) {
  Expr expr;
  SourceInfo source_info;
  auto call_expr = expr.mutable_call_expr();
  call_expr->set_function(builtin::kTernary);

  auto arg0 = call_expr->add_args();
  arg0->mutable_ident_expr()->set_name("selector");
  auto arg1 = call_expr->add_args();
  arg1->mutable_ident_expr()->set_name("value1");
  auto arg2 = call_expr->add_args();
  arg2->mutable_ident_expr()->set_name("value2");

  FlatExprBuilder builder;
  CEL_ASSIGN_OR_RETURN(auto cel_expr,
                       builder.CreateExpression(&expr, &source_info));

  std::string variable = "test";

  Activation activation;
  activation.InsertValue("selector", selector);
  activation.InsertValue("value1", value1);
  activation.InsertValue("value2", value2);

  CEL_ASSIGN_OR_RETURN(auto eval, cel_expr->Evaluate(activation, arena));
  *result = eval;
  return absl::OkStatus();
}

TEST(FlatExprBuilderTest, Ternary) {
  Expr expr;
  SourceInfo source_info;
  auto call_expr = expr.mutable_call_expr();
  call_expr->set_function(builtin::kTernary);

  auto arg0 = call_expr->add_args();
  arg0->mutable_ident_expr()->set_name("selector");
  auto arg1 = call_expr->add_args();
  arg1->mutable_ident_expr()->set_name("value1");
  auto arg2 = call_expr->add_args();
  arg2->mutable_ident_expr()->set_name("value1");

  FlatExprBuilder builder;
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder.CreateExpression(&expr, &source_info));

  google::protobuf::Arena arena;

  // On True, value 1
  {
    CelValue result;
    ASSERT_OK(RunTernaryExpression(CelValue::CreateBool(true),
                                   CelValue::CreateInt64(1),
                                   CelValue::CreateInt64(2), &arena, &result));
    ASSERT_TRUE(result.IsInt64());
    EXPECT_THAT(result.Int64OrDie(), Eq(1));

    // Unknown handling
    UnknownSet unknown_set;
    ASSERT_OK(RunTernaryExpression(CelValue::CreateBool(true),
                                   CelValue::CreateUnknownSet(&unknown_set),
                                   CelValue::CreateInt64(2), &arena, &result));
    ASSERT_TRUE(result.IsUnknownSet());

    ASSERT_OK(RunTernaryExpression(
        CelValue::CreateBool(true), CelValue::CreateInt64(1),
        CelValue::CreateUnknownSet(&unknown_set), &arena, &result));
    ASSERT_TRUE(result.IsInt64());
    EXPECT_THAT(result.Int64OrDie(), Eq(1));
  }

  // On False, value 2
  {
    CelValue result;
    ASSERT_OK(RunTernaryExpression(CelValue::CreateBool(false),
                                   CelValue::CreateInt64(1),
                                   CelValue::CreateInt64(2), &arena, &result));
    ASSERT_TRUE(result.IsInt64());
    EXPECT_THAT(result.Int64OrDie(), Eq(2));

    // Unknown handling
    UnknownSet unknown_set;
    ASSERT_OK(RunTernaryExpression(CelValue::CreateBool(false),
                                   CelValue::CreateUnknownSet(&unknown_set),
                                   CelValue::CreateInt64(2), &arena, &result));
    ASSERT_TRUE(result.IsInt64());
    EXPECT_THAT(result.Int64OrDie(), Eq(2));

    ASSERT_OK(RunTernaryExpression(
        CelValue::CreateBool(false), CelValue::CreateInt64(1),
        CelValue::CreateUnknownSet(&unknown_set), &arena, &result));
    ASSERT_TRUE(result.IsUnknownSet());
  }
  // On Error, surface error
  {
    CelValue result;
    ASSERT_OK(RunTernaryExpression(CreateErrorValue(&arena, "error"),
                                   CelValue::CreateInt64(1),
                                   CelValue::CreateInt64(2), &arena, &result));
    ASSERT_TRUE(result.IsError());
  }
  // On Unknown, surface Unknown
  {
    UnknownSet unknown_set;
    CelValue result;
    ASSERT_OK(RunTernaryExpression(CelValue::CreateUnknownSet(&unknown_set),
                                   CelValue::CreateInt64(1),
                                   CelValue::CreateInt64(2), &arena, &result));
    ASSERT_TRUE(result.IsUnknownSet());
    EXPECT_THAT(&unknown_set, Eq(result.UnknownSetOrDie()));
  }
  // We should not merge unknowns
  {
    Expr selector;
    selector.mutable_ident_expr()->set_name("selector");
    CelAttribute selector_attr(selector, {});

    Expr value1;
    value1.mutable_ident_expr()->set_name("value1");
    CelAttribute value1_attr(value1, {});

    Expr value2;
    value2.mutable_ident_expr()->set_name("value2");
    CelAttribute value2_attr(value2, {});

    UnknownSet unknown_selector(UnknownAttributeSet({&selector_attr}));
    UnknownSet unknown_value1(UnknownAttributeSet({&value1_attr}));
    UnknownSet unknown_value2(UnknownAttributeSet({&value2_attr}));
    CelValue result;
    ASSERT_OK(RunTernaryExpression(
        CelValue::CreateUnknownSet(&unknown_selector),
        CelValue::CreateUnknownSet(&unknown_value1),
        CelValue::CreateUnknownSet(&unknown_value2), &arena, &result));
    ASSERT_TRUE(result.IsUnknownSet());
    const UnknownSet* result_set = result.UnknownSetOrDie();
    EXPECT_THAT(result_set->unknown_attributes().attributes().size(), Eq(1));
    EXPECT_THAT(result_set->unknown_attributes()
                    .attributes()[0]
                    ->variable()
                    .ident_expr()
                    .name(),
                Eq("selector"));
  }
}

TEST(FlatExprBuilderTest, EmptyCallList) {
  std::vector<std::string> operators = {"_&&_", "_||_", "_?_:_"};
  for (const auto& op : operators) {
    Expr expr;
    SourceInfo source_info;
    auto call_expr = expr.mutable_call_expr();
    call_expr->set_function(op);
    FlatExprBuilder builder;
    ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));
    auto build = builder.CreateExpression(&expr, &source_info);
    ASSERT_FALSE(build.ok());
  }
}

}  // namespace

}  // namespace google::api::expr::runtime
