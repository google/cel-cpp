/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "eval/compiler/flat_expr_builder.h"

#include <fstream>
#include <functional>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/field_mask.pb.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "absl/status/status.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "eval/eval/expression_build_warning.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_function_adapter.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/structs/cel_proto_descriptor_pool_builder.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/public/structs/protobuf_descriptor_type_provider.h"
#include "eval/public/testing/matchers.h"
#include "eval/public/unknown_attribute_set.h"
#include "eval/public/unknown_set.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "parser/parser.h"

namespace google::api::expr::runtime {

namespace {

using ::google::api::expr::v1alpha1::CheckedExpr;
using ::google::api::expr::v1alpha1::Expr;
using ::google::api::expr::v1alpha1::ParsedExpr;
using ::google::api::expr::v1alpha1::SourceInfo;
using testing::Eq;
using testing::HasSubstr;
using cel::internal::StatusIs;

inline constexpr absl::string_view kSimpleTestMessageDescriptorSetFile =
    "eval/testutil/"
    "simple_test_message_proto-descriptor-set.proto.bin";

template <class MessageClass>
absl::Status ReadBinaryProtoFromDisk(absl::string_view file_name,
                                     MessageClass& message) {
  std::ifstream file;
  file.open(std::string(file_name), std::fstream::in);
  if (!file.is_open()) {
    return absl::NotFoundError(absl::StrFormat("Failed to open file '%s': %s",
                                               file_name, strerror(errno)));
  }

  if (!message.ParseFromIstream(&file)) {
    return absl::InvalidArgumentError(
        absl::StrFormat("Failed to parse proto of type '%s' from file '%s'",
                        message.GetTypeName(), file_name));
  }

  return absl::OkStatus();
}

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
  EXPECT_THAT(
      builder.CreateExpression(&expr, &source_info).status(),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr("Illegal type provided for "
                    "google::api::expr::v1alpha1::Expr::CreateStruct::Entry::key_kind")));

  // Set the entry key, but not the value.
  entry->mutable_map_key()->mutable_const_expr()->set_bool_value(true);
  EXPECT_THAT(
      builder.CreateExpression(&expr, &source_info).status(),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr(
              "google::api::expr::v1alpha1::Expr::CreateStruct::Entry missing value")));
}

TEST(FlatExprBuilderTest, MessageFieldValueUnset) {
  Expr expr;
  SourceInfo source_info;
  FlatExprBuilder builder;
  builder.GetTypeRegistry()->RegisterTypeProvider(
      std::make_unique<ProtobufDescriptorProvider>(
          google::protobuf::DescriptorPool::generated_pool(),
          google::protobuf::MessageFactory::generated_factory()));

  // Don't set either the field or the value for the message creation step.
  auto* create_message = expr.mutable_struct_expr();
  create_message->set_message_name("google.protobuf.Value");
  auto* entry = create_message->add_entries();
  EXPECT_THAT(
      builder.CreateExpression(&expr, &source_info).status(),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr("Illegal type provided for "
                    "google::api::expr::v1alpha1::Expr::CreateStruct::Entry::key_kind")));

  // Set the entry field, but not the value.
  entry->set_field_key("bool_value");
  EXPECT_THAT(
      builder.CreateExpression(&expr, &source_info).status(),
      StatusIs(
          absl::StatusCode::kInvalidArgument,
          HasSubstr(
              "google::api::expr::v1alpha1::Expr::CreateStruct::Entry missing value")));
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
              Eq("No matching overloads found : concat(string, string)"));

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

TEST(FlatExprBuilderTest, ParsedNamespacedFunctionSupport) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("ext.XOr(a, b)"));
  FlatExprBuilder builder;
  builder.set_enable_qualified_identifier_rewrites(true);
  using FunctionAdapterT = FunctionAdapter<bool, bool, bool>;

  ASSERT_OK(FunctionAdapterT::CreateAndRegister(
      "ext.XOr", /*receiver_style=*/false,
      [](google::protobuf::Arena*, bool a, bool b) { return a != b; },
      builder.GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder.CreateExpression(
                                          &expr.expr(), &expr.source_info()));

  google::protobuf::Arena arena;
  Activation act1;
  act1.InsertValue("a", CelValue::CreateBool(false));
  act1.InsertValue("b", CelValue::CreateBool(true));

  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(act1, &arena));
  EXPECT_THAT(result, test::IsCelBool(true));

  Activation act2;
  act2.InsertValue("a", CelValue::CreateBool(true));
  act2.InsertValue("b", CelValue::CreateBool(true));

  ASSERT_OK_AND_ASSIGN(result, cel_expr->Evaluate(act2, &arena));
  EXPECT_THAT(result, test::IsCelBool(false));
}

TEST(FlatExprBuilderTest, ParsedNamespacedFunctionSupportWithContainer) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("XOr(a, b)"));
  FlatExprBuilder builder;
  builder.set_enable_qualified_identifier_rewrites(true);
  builder.set_container("ext");
  using FunctionAdapterT = FunctionAdapter<bool, bool, bool>;

  ASSERT_OK(FunctionAdapterT::CreateAndRegister(
      "ext.XOr", /*receiver_style=*/false,
      [](google::protobuf::Arena*, bool a, bool b) { return a != b; },
      builder.GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder.CreateExpression(
                                          &expr.expr(), &expr.source_info()));
  google::protobuf::Arena arena;
  Activation act1;
  act1.InsertValue("a", CelValue::CreateBool(false));
  act1.InsertValue("b", CelValue::CreateBool(true));

  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(act1, &arena));
  EXPECT_THAT(result, test::IsCelBool(true));

  Activation act2;
  act2.InsertValue("a", CelValue::CreateBool(true));
  act2.InsertValue("b", CelValue::CreateBool(true));

  ASSERT_OK_AND_ASSIGN(result, cel_expr->Evaluate(act2, &arena));
  EXPECT_THAT(result, test::IsCelBool(false));
}

TEST(FlatExprBuilderTest, ParsedNamespacedFunctionResolutionOrder) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("c.d.Get()"));
  FlatExprBuilder builder;
  builder.set_enable_qualified_identifier_rewrites(true);
  builder.set_container("a.b");
  using FunctionAdapterT = FunctionAdapter<bool>;

  ASSERT_OK(FunctionAdapterT::CreateAndRegister(
      "a.b.c.d.Get", /*receiver_style=*/false,
      [](google::protobuf::Arena*) { return true; }, builder.GetRegistry()));
  ASSERT_OK(FunctionAdapterT::CreateAndRegister(
      "c.d.Get", /*receiver_style=*/false, [](google::protobuf::Arena*) { return false; },
      builder.GetRegistry()));
  ASSERT_OK((FunctionAdapter<bool, bool>::CreateAndRegister(
      "Get",
      /*receiver_style=*/true, [](google::protobuf::Arena*, bool) { return false; },
      builder.GetRegistry())));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder.CreateExpression(
                                          &expr.expr(), &expr.source_info()));
  google::protobuf::Arena arena;
  Activation act1;
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(act1, &arena));
  EXPECT_THAT(result, test::IsCelBool(true));
}

TEST(FlatExprBuilderTest,
     ParsedNamespacedFunctionResolutionOrderParentContainer) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("c.d.Get()"));
  FlatExprBuilder builder;
  builder.set_enable_qualified_identifier_rewrites(true);
  builder.set_container("a.b");
  using FunctionAdapterT = FunctionAdapter<bool>;

  ASSERT_OK(FunctionAdapterT::CreateAndRegister(
      "a.c.d.Get", /*receiver_style=*/false,
      [](google::protobuf::Arena*) { return true; }, builder.GetRegistry()));
  ASSERT_OK(FunctionAdapterT::CreateAndRegister(
      "c.d.Get", /*receiver_style=*/false, [](google::protobuf::Arena*) { return false; },
      builder.GetRegistry()));
  ASSERT_OK((FunctionAdapter<bool, bool>::CreateAndRegister(
      "Get",
      /*receiver_style=*/true, [](google::protobuf::Arena*, bool) { return false; },
      builder.GetRegistry())));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder.CreateExpression(
                                          &expr.expr(), &expr.source_info()));
  google::protobuf::Arena arena;
  Activation act1;
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(act1, &arena));
  EXPECT_THAT(result, test::IsCelBool(true));
}

TEST(FlatExprBuilderTest,
     ParsedNamespacedFunctionResolutionOrderExplicitGlobal) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse(".c.d.Get()"));
  FlatExprBuilder builder;
  builder.set_enable_qualified_identifier_rewrites(true);
  builder.set_container("a.b");
  using FunctionAdapterT = FunctionAdapter<bool>;

  ASSERT_OK(FunctionAdapterT::CreateAndRegister(
      "a.c.d.Get", /*receiver_style=*/false,
      [](google::protobuf::Arena*) { return false; }, builder.GetRegistry()));
  ASSERT_OK(FunctionAdapterT::CreateAndRegister(
      "c.d.Get", /*receiver_style=*/false, [](google::protobuf::Arena*) { return true; },
      builder.GetRegistry()));
  ASSERT_OK((FunctionAdapter<bool, bool>::CreateAndRegister(
      "Get",
      /*receiver_style=*/true, [](google::protobuf::Arena*, bool) { return false; },
      builder.GetRegistry())));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder.CreateExpression(
                                          &expr.expr(), &expr.source_info()));
  google::protobuf::Arena arena;
  Activation act1;
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(act1, &arena));
  EXPECT_THAT(result, test::IsCelBool(true));
}

TEST(FlatExprBuilderTest, ParsedNamespacedFunctionResolutionOrderReceiverCall) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("e.Get()"));
  FlatExprBuilder builder;
  builder.set_enable_qualified_identifier_rewrites(true);
  builder.set_container("a.b");
  using FunctionAdapterT = FunctionAdapter<bool>;

  ASSERT_OK(FunctionAdapterT::CreateAndRegister(
      "a.c.d.Get", /*receiver_style=*/false,
      [](google::protobuf::Arena*) { return false; }, builder.GetRegistry()));
  ASSERT_OK(FunctionAdapterT::CreateAndRegister(
      "c.d.Get", /*receiver_style=*/false, [](google::protobuf::Arena*) { return false; },
      builder.GetRegistry()));
  ASSERT_OK((FunctionAdapter<bool, bool>::CreateAndRegister(
      "Get",
      /*receiver_style=*/true, [](google::protobuf::Arena*, bool) { return true; },
      builder.GetRegistry())));
  ASSERT_OK_AND_ASSIGN(auto cel_expr, builder.CreateExpression(
                                          &expr.expr(), &expr.source_info()));
  google::protobuf::Arena arena;
  Activation act1;
  act1.InsertValue("e", CelValue::CreateBool(false));
  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(act1, &arena));
  EXPECT_THAT(result, test::IsCelBool(true));
}

TEST(FlatExprBuilderTest, ParsedNamespacedFunctionSupportDisabled) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse("ext.XOr(a, b)"));
  FlatExprBuilder builder;
  builder.set_fail_on_warnings(false);
  std::vector<absl::Status> build_warnings;
  builder.set_container("ext");
  using FunctionAdapterT = FunctionAdapter<bool, bool, bool>;

  ASSERT_OK(FunctionAdapterT::CreateAndRegister(
      "ext.XOr", /*receiver_style=*/false,
      [](google::protobuf::Arena*, bool a, bool b) { return a != b; },
      builder.GetRegistry()));
  ASSERT_OK_AND_ASSIGN(
      auto cel_expr, builder.CreateExpression(&expr.expr(), &expr.source_info(),
                                              &build_warnings));
  google::protobuf::Arena arena;
  Activation act1;
  act1.InsertValue("a", CelValue::CreateBool(false));
  act1.InsertValue("b", CelValue::CreateBool(true));

  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr->Evaluate(act1, &arena));
  EXPECT_THAT(result, test::IsCelError(StatusIs(absl::StatusCode::kUnknown,
                                                HasSubstr("ext"))));
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
              Eq("No matching overloads found : <iter_range>"));
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

    UnknownSet unknown_selector(UnknownAttributeSet({selector_attr}));
    UnknownSet unknown_value1(UnknownAttributeSet({value1_attr}));
    UnknownSet unknown_value2(UnknownAttributeSet({value2_attr}));
    CelValue result;
    ASSERT_OK(RunTernaryExpression(
        CelValue::CreateUnknownSet(&unknown_selector),
        CelValue::CreateUnknownSet(&unknown_value1),
        CelValue::CreateUnknownSet(&unknown_value2), &arena, &result));
    ASSERT_TRUE(result.IsUnknownSet());
    const UnknownSet* result_set = result.UnknownSetOrDie();
    EXPECT_THAT(result_set->unknown_attributes().size(), Eq(1));
    EXPECT_THAT(result_set->unknown_attributes().begin()->variable_name(),
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

TEST(FlatExprBuilderTest, NullUnboxingEnabled) {
  TestMessage message;
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr,
                       parser::Parse("message.int32_wrapper_value"));
  FlatExprBuilder builder;
  builder.set_enable_wrapper_type_null_unboxing(true);
  ASSERT_OK_AND_ASSIGN(auto expression,
                       builder.CreateExpression(&parsed_expr.expr(),
                                                &parsed_expr.source_info()));

  Activation activation;
  google::protobuf::Arena arena;
  activation.InsertValue("message",
                         CelProtoWrapper::CreateMessage(&message, &arena));
  ASSERT_OK_AND_ASSIGN(CelValue result,
                       expression->Evaluate(activation, &arena));

  EXPECT_TRUE(result.IsNull());
}

TEST(FlatExprBuilderTest, NullUnboxingDisabled) {
  TestMessage message;
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr,
                       parser::Parse("message.int32_wrapper_value"));
  FlatExprBuilder builder;
  builder.set_enable_wrapper_type_null_unboxing(false);
  ASSERT_OK_AND_ASSIGN(auto expression,
                       builder.CreateExpression(&parsed_expr.expr(),
                                                &parsed_expr.source_info()));

  Activation activation;
  google::protobuf::Arena arena;
  activation.InsertValue("message",
                         CelProtoWrapper::CreateMessage(&message, &arena));
  ASSERT_OK_AND_ASSIGN(CelValue result,
                       expression->Evaluate(activation, &arena));

  EXPECT_THAT(result, test::IsCelInt64(0));
}

TEST(FlatExprBuilderTest, HeterogeneousEqualityEnabled) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr,
                       parser::Parse("{1: 2, 2u: 3}[1.0]"));
  FlatExprBuilder builder;
  builder.set_enable_heterogeneous_equality(true);
  ASSERT_OK_AND_ASSIGN(auto expression,
                       builder.CreateExpression(&parsed_expr.expr(),
                                                &parsed_expr.source_info()));

  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result,
                       expression->Evaluate(activation, &arena));

  EXPECT_THAT(result, test::IsCelInt64(2));
}

TEST(FlatExprBuilderTest, HeterogeneousEqualityDisabled) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr,
                       parser::Parse("{1: 2, 2u: 3}[1.0]"));
  FlatExprBuilder builder;
  builder.set_enable_heterogeneous_equality(false);
  ASSERT_OK_AND_ASSIGN(auto expression,
                       builder.CreateExpression(&parsed_expr.expr(),
                                                &parsed_expr.source_info()));

  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result,
                       expression->Evaluate(activation, &arena));

  EXPECT_THAT(result,
              test::IsCelError(StatusIs(absl::StatusCode::kInvalidArgument,
                                        HasSubstr("Invalid map key type"))));
}

TEST(FlatExprBuilderTest, CustomDescriptorPoolForCreateStruct) {
  ASSERT_OK_AND_ASSIGN(
      ParsedExpr parsed_expr,
      parser::Parse("google.api.expr.runtime.SimpleTestMessage{}"));

  // This time, the message is unknown. We only have the proto as data, we did
  // not link the generated message, so it's not included in the generated pool.
  FlatExprBuilder builder;
  builder.GetTypeRegistry()->RegisterTypeProvider(
      std::make_unique<ProtobufDescriptorProvider>(
          google::protobuf::DescriptorPool::generated_pool(),
          google::protobuf::MessageFactory::generated_factory()));

  EXPECT_THAT(
      builder.CreateExpression(&parsed_expr.expr(), &parsed_expr.source_info()),
      StatusIs(absl::StatusCode::kInvalidArgument));

  // Now we create a custom DescriptorPool to which we add SimpleTestMessage
  google::protobuf::DescriptorPool desc_pool;
  google::protobuf::FileDescriptorSet filedesc_set;

  ASSERT_OK(ReadBinaryProtoFromDisk(kSimpleTestMessageDescriptorSetFile,
                                    filedesc_set));
  ASSERT_EQ(filedesc_set.file_size(), 1);
  desc_pool.BuildFile(filedesc_set.file(0));

  google::protobuf::DynamicMessageFactory message_factory(&desc_pool);

  // This time, the message is *known*. We are using a custom descriptor pool
  // that has been primed with the relevant message.
  FlatExprBuilder builder2;
  builder2.GetTypeRegistry()->RegisterTypeProvider(
      std::make_unique<ProtobufDescriptorProvider>(&desc_pool,
                                                   &message_factory));

  ASSERT_OK_AND_ASSIGN(auto expression,
                       builder2.CreateExpression(&parsed_expr.expr(),
                                                 &parsed_expr.source_info()));

  Activation activation;
  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result,
                       expression->Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsMessage());
  EXPECT_EQ(result.MessageOrDie()->GetTypeName(),
            "google.api.expr.runtime.SimpleTestMessage");
}

TEST(FlatExprBuilderTest, CustomDescriptorPoolForSelect) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr,
                       parser::Parse("message.int64_value"));

  google::protobuf::DescriptorPool desc_pool;
  google::protobuf::FileDescriptorSet filedesc_set;

  ASSERT_OK(ReadBinaryProtoFromDisk(kSimpleTestMessageDescriptorSetFile,
                                    filedesc_set));
  ASSERT_EQ(filedesc_set.file_size(), 1);
  desc_pool.BuildFile(filedesc_set.file(0));

  google::protobuf::DynamicMessageFactory message_factory(&desc_pool);

  const google::protobuf::Descriptor* desc = desc_pool.FindMessageTypeByName(
      "google.api.expr.runtime.SimpleTestMessage");
  const google::protobuf::Message* message_prototype = message_factory.GetPrototype(desc);
  google::protobuf::Message* message = message_prototype->New();
  const google::protobuf::Reflection* refl = message->GetReflection();
  const google::protobuf::FieldDescriptor* field = desc->FindFieldByName("int64_value");
  refl->SetInt64(message, field, 123);

  // The since this is access only, the evaluator will work with message duck
  // typing.
  FlatExprBuilder builder;
  ASSERT_OK_AND_ASSIGN(auto expression,
                       builder.CreateExpression(&parsed_expr.expr(),
                                                &parsed_expr.source_info()));
  Activation activation;
  google::protobuf::Arena arena;
  activation.InsertValue("message",
                         CelProtoWrapper::CreateMessage(message, &arena));
  ASSERT_OK_AND_ASSIGN(CelValue result,
                       expression->Evaluate(activation, &arena));
  EXPECT_THAT(result, test::IsCelInt64(123));

  delete message;
}

std::pair<google::protobuf::Message*, const google::protobuf::Reflection*> CreateTestMessage(
    const google::protobuf::DescriptorPool& descriptor_pool,
    google::protobuf::MessageFactory& message_factory, absl::string_view name) {
  const google::protobuf::Descriptor* desc = descriptor_pool.FindMessageTypeByName(std::string(name));
  const google::protobuf::Message* message_prototype = message_factory.GetPrototype(desc);
  google::protobuf::Message* message = message_prototype->New();
  const google::protobuf::Reflection* refl = message->GetReflection();
  return std::make_pair(message, refl);
}

struct CustomDescriptorPoolTestParam final {
  using SetterFunction =
      std::function<void(google::protobuf::Message*, const google::protobuf::Reflection*,
                         const google::protobuf::FieldDescriptor*)>;
  std::string message_type;
  std::string field_name;
  SetterFunction setter;
  test::CelValueMatcher matcher;
};

class CustomDescriptorPoolTest
    : public ::testing::TestWithParam<CustomDescriptorPoolTestParam> {};

// This test in particular checks for conversion errors in cel_proto_wrapper.cc.
TEST_P(CustomDescriptorPoolTest, TestType) {
  const CustomDescriptorPoolTestParam& p = GetParam();

  google::protobuf::DescriptorPool descriptor_pool;
  google::protobuf::Arena arena;

  // Setup descriptor pool and builder
  ASSERT_OK(AddStandardMessageTypesToDescriptorPool(descriptor_pool));
  google::protobuf::DynamicMessageFactory message_factory(&descriptor_pool);
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, parser::Parse("m"));
  FlatExprBuilder builder;
  builder.GetTypeRegistry()->RegisterTypeProvider(
      std::make_unique<ProtobufDescriptorProvider>(&descriptor_pool,
                                                   &message_factory));
  ASSERT_OK(RegisterBuiltinFunctions(builder.GetRegistry()));

  // Create test subject, invoke custom setter for message
  auto [message, reflection] =
      CreateTestMessage(descriptor_pool, message_factory, p.message_type);
  const google::protobuf::FieldDescriptor* field =
      message->GetDescriptor()->FindFieldByName(p.field_name);

  p.setter(message, reflection, field);
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<CelExpression> expression,
                       builder.CreateExpression(&parsed_expr.expr(),
                                                &parsed_expr.source_info()));

  // Evaluate expression, verify expectation with custom matcher
  Activation activation;
  activation.InsertValue("m", CelProtoWrapper::CreateMessage(message, &arena));
  ASSERT_OK_AND_ASSIGN(CelValue result,
                       expression->Evaluate(activation, &arena));
  EXPECT_THAT(result, p.matcher);

  delete message;
}

INSTANTIATE_TEST_SUITE_P(
    ValueTypes, CustomDescriptorPoolTest,
    ::testing::ValuesIn(std::vector<CustomDescriptorPoolTestParam>{
        {"google.protobuf.Duration", "seconds",
         [](google::protobuf::Message* message, const google::protobuf::Reflection* reflection,
            const google::protobuf::FieldDescriptor* field) {
           reflection->SetInt64(message, field, 10);
         },
         test::IsCelDuration(absl::Seconds(10))},
        {"google.protobuf.DoubleValue", "value",
         [](google::protobuf::Message* message, const google::protobuf::Reflection* reflection,
            const google::protobuf::FieldDescriptor* field) {
           reflection->SetDouble(message, field, 1.2);
         },
         test::IsCelDouble(1.2)},
        {"google.protobuf.Int64Value", "value",
         [](google::protobuf::Message* message, const google::protobuf::Reflection* reflection,
            const google::protobuf::FieldDescriptor* field) {
           reflection->SetInt64(message, field, -23);
         },
         test::IsCelInt64(-23)},
        {"google.protobuf.UInt64Value", "value",
         [](google::protobuf::Message* message, const google::protobuf::Reflection* reflection,
            const google::protobuf::FieldDescriptor* field) {
           reflection->SetUInt64(message, field, 42);
         },
         test::IsCelUint64(42)},
        {"google.protobuf.BoolValue", "value",
         [](google::protobuf::Message* message, const google::protobuf::Reflection* reflection,
            const google::protobuf::FieldDescriptor* field) {
           reflection->SetBool(message, field, true);
         },
         test::IsCelBool(true)},
        {"google.protobuf.StringValue", "value",
         [](google::protobuf::Message* message, const google::protobuf::Reflection* reflection,
            const google::protobuf::FieldDescriptor* field) {
           reflection->SetString(message, field, "foo");
         },
         test::IsCelString("foo")},
        {"google.protobuf.BytesValue", "value",
         [](google::protobuf::Message* message, const google::protobuf::Reflection* reflection,
            const google::protobuf::FieldDescriptor* field) {
           reflection->SetString(message, field, "bar");
         },
         test::IsCelBytes("bar")},
        {"google.protobuf.Timestamp", "seconds",
         [](google::protobuf::Message* message, const google::protobuf::Reflection* reflection,
            const google::protobuf::FieldDescriptor* field) {
           reflection->SetInt64(message, field, 20);
         },
         test::IsCelTimestamp(absl::FromUnixSeconds(20))}}));

}  // namespace

}  // namespace google::api::expr::runtime
