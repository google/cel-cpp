// Copyright 2025 Google LLC.
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
#include "testing/testrunner/runner_lib.h"

#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/ast.h"
#include "common/ast_proto.h"
#include "common/internal/value_conversion.h"
#include "common/value.h"
#include "eval/public/activation.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_value.h"
#include "eval/public/transform_utility.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "runtime/activation.h"
#include "runtime/runtime.h"
#include "testing/testrunner/cel_test_context.h"
#include "cel/expr/conformance/test/suite.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/util/field_comparator.h"
#include "google/protobuf/util/message_differencer.h"

namespace cel::test {
namespace {

using ::cel::expr::conformance::test::InputValue;
using ::cel::expr::conformance::test::TestCase;
using ::cel::expr::conformance::test::TestOutput;
using ::cel::expr::CheckedExpr;
using ::google::api::expr::runtime::CelExpression;
using ::google::api::expr::runtime::CelValue;
using ::google::api::expr::runtime::ValueToCelValue;
using ValueProto = ::cel::expr::Value;
using ::google::api::expr::runtime::Activation;

absl::StatusOr<std::unique_ptr<cel::Program>> Plan(
    const CheckedExpr& checked_expr, const cel::Runtime* runtime) {
  std::unique_ptr<cel::Ast> ast;
  CEL_ASSIGN_OR_RETURN(ast, cel::CreateAstFromCheckedExpr(checked_expr));
  if (ast == nullptr) {
    return absl::InternalError("No expression provided for testing.");
  }
  return runtime->CreateProgram(std::move(ast));
}

const google::protobuf::DescriptorPool* GetDescriptorPool(const CelTestContext& context) {
  return context.cel_expression_builder() != nullptr
             ? google::protobuf::DescriptorPool::generated_pool()
             : context.runtime()->GetDescriptorPool();
}

google::protobuf::MessageFactory* GetMessageFactory(const CelTestContext& context) {
  return context.cel_expression_builder() != nullptr
             ? google::protobuf::MessageFactory::generated_factory()
             : context.runtime()->GetMessageFactory();
}

absl::StatusOr<cel::Value> EvalWithModernBindings(
    const CheckedExpr& checked_expr, const CelTestContext& context,
    const cel::Activation& activation, google::protobuf::Arena* arena) {
  CEL_ASSIGN_OR_RETURN(std::unique_ptr<cel::Program> program,
                       Plan(checked_expr, context.runtime()));
  return program->Evaluate(arena, activation);
}

absl::StatusOr<cel::Value> EvalWithLegacyBindings(
    const CheckedExpr& checked_expr, const CelTestContext& context,
    const Activation& activation, google::protobuf::Arena* arena) {
  const auto* builder = context.cel_expression_builder();

  CEL_ASSIGN_OR_RETURN(std::unique_ptr<CelExpression> sub_expression,
                       builder->CreateExpression(&checked_expr));

  CEL_ASSIGN_OR_RETURN(CelValue legacy_result,
                       sub_expression->Evaluate(activation, arena));

  ValueProto result_proto;
  CEL_RETURN_IF_ERROR(CelValueToValue(legacy_result, &result_proto));
  return FromExprValue(result_proto, GetDescriptorPool(context),
                       GetMessageFactory(context), arena);
}

absl::StatusOr<cel::Value> ResolveValue(const InputValue& input_value,
                                        const CelTestContext& context,
                                        google::protobuf::Arena* arena) {
  return FromExprValue(input_value.value(), GetDescriptorPool(context),
                       GetMessageFactory(context), arena);
}

absl::StatusOr<cel::Value> ResolveExpr(absl::string_view expr,
                                       const CelTestContext& context,
                                       google::protobuf::Arena* arena) {
  const auto* compiler = context.compiler();
  if (compiler == nullptr) {
    return absl::InvalidArgumentError(
        "Test case uses an expression but no compiler was provided.");
  }
  CEL_ASSIGN_OR_RETURN(auto validation_result, compiler->Compile(expr));
  if (!validation_result.IsValid()) {
    return absl::InternalError(validation_result.FormatError());
  }
  CheckedExpr checked_expr;
  CEL_RETURN_IF_ERROR(
      AstToCheckedExpr(*validation_result.GetAst(), &checked_expr));
  if (context.runtime() != nullptr) {
    cel::Activation empty_activation;
    return EvalWithModernBindings(checked_expr, context, empty_activation,
                                  arena);
  } else {
    Activation empty_activation;
    return EvalWithLegacyBindings(checked_expr, context, empty_activation,
                                  arena);
  }
}

absl::StatusOr<cel::Value> ResolveInputValue(const InputValue& input_value,
                                             const CelTestContext& context,
                                             google::protobuf::Arena* arena) {
  switch (input_value.kind_case()) {
    case InputValue::kValue: {
      return ResolveValue(input_value, context, arena);
    }
    case InputValue::kExpr: {
      return ResolveExpr(input_value.expr(), context, arena);
    }
    default:
      return absl::InvalidArgumentError("Unknown InputValue kind.");
  }
}

absl::StatusOr<cel::Activation> CreateModernActivationFromBindings(
    const TestCase& test_case, const CelTestContext& context,
    google::protobuf::Arena* arena) {
  cel::Activation activation;
  for (const auto& binding : test_case.input()) {
    CEL_ASSIGN_OR_RETURN(
        Value value,
        ResolveInputValue(/*input_value=*/binding.second, context, arena));
    activation.InsertOrAssignValue(/*name=*/binding.first, std::move(value));
  }
  return activation;
}

absl::StatusOr<Activation> CreateLegacyActivationFromBindings(
    const TestCase& test_case, const CelTestContext& context,
    google::protobuf::Arena* arena) {
  Activation activation;
  auto* message_factory = GetMessageFactory(context);
  auto* descriptor_pool = GetDescriptorPool(context);
  for (const auto& binding : test_case.input()) {
    CEL_ASSIGN_OR_RETURN(
        cel::Value resolved_cel_value,
        ResolveInputValue(/*input_value=*/binding.second, context, arena));
    CEL_ASSIGN_OR_RETURN(ValueProto value_proto,
                         ToExprValue(resolved_cel_value, descriptor_pool,
                                     message_factory, arena));
    CEL_ASSIGN_OR_RETURN(CelValue value, ValueToCelValue(value_proto, arena));
    activation.InsertValue(/*name=*/binding.first, value);
  }
  return activation;
}

bool IsEqual(const ValueProto& expected, const ValueProto& actual) {
  static auto* kFieldComparator = []() {
    auto* field_comparator = new google::protobuf::util::DefaultFieldComparator();
    field_comparator->set_treat_nan_as_equal(true);
    return field_comparator;
  }();
  static auto* kDifferencer = []() {
    auto* differencer = new google::protobuf::util::MessageDifferencer();
    differencer->set_message_field_comparison(
        google::protobuf::util::MessageDifferencer::EQUIVALENT);
    differencer->set_field_comparator(kFieldComparator);
    const auto* descriptor = cel::expr::MapValue::descriptor();
    const auto* entries_field = descriptor->FindFieldByName("entries");
    const auto* key_field =
        entries_field->message_type()->FindFieldByName("key");
    differencer->TreatAsMap(entries_field, key_field);
    return differencer;
  }();
  return kDifferencer->Compare(expected, actual);
}

MATCHER_P(MatchesValue, expected, "") { return IsEqual(arg, expected); }
}  // namespace

void TestRunner::AssertValue(const cel::Value& computed,
                             const TestOutput& output, google::protobuf::Arena* arena) {
  ValueProto expected_value_proto;
  const auto* descriptor_pool = GetDescriptorPool(*test_context_);
  auto* message_factory = GetMessageFactory(*test_context_);
  if (output.has_result_value()) {
    expected_value_proto = output.result_value();
  } else if (output.has_result_expr()) {
    InputValue input_value;
    input_value.set_expr(output.result_expr());
    ASSERT_OK_AND_ASSIGN(cel::Value resolved_cel_value,
                         ResolveInputValue(input_value, *test_context_, arena));
    ASSERT_OK_AND_ASSIGN(expected_value_proto,
                         ToExprValue(resolved_cel_value, descriptor_pool,
                                     message_factory, arena));
  }
  ValueProto computed_expr_value;
  ASSERT_OK_AND_ASSIGN(
      computed_expr_value,
      ToExprValue(computed, descriptor_pool, message_factory, arena));
  EXPECT_THAT(expected_value_proto, MatchesValue(computed_expr_value));
}

void TestRunner::Assert(const cel::Value& computed, const TestCase& test_case,
                        google::protobuf::Arena* arena) {
  TestOutput output = test_case.output();
  if (output.has_result_value() || output.has_result_expr()) {
    AssertValue(computed, output, arena);
  } else if (output.has_eval_error()) {
    ADD_FAILURE() << "Error assertion not implemented yet.";
  } else if (output.has_unknown()) {
    ADD_FAILURE() << "Unknown assertions not implemented yet.";
  } else {
    ADD_FAILURE() << "Unexpected  output kind.";
  }
}

absl::StatusOr<cel::Value> TestRunner::EvalWithRuntime(
    const TestCase& test_case, google::protobuf::Arena* arena) {
  CEL_ASSIGN_OR_RETURN(
      cel::Activation activation,
      CreateModernActivationFromBindings(test_case, *test_context_, arena));
  return EvalWithModernBindings(*test_context_->checked_expr(), *test_context_,
                                activation, arena);
}

absl::StatusOr<cel::Value> TestRunner::EvalWithCelExpressionBuilder(
    const TestCase& test_case, google::protobuf::Arena* arena) {
  CEL_ASSIGN_OR_RETURN(
      Activation activation,
      CreateLegacyActivationFromBindings(test_case, *test_context_, arena));
  return EvalWithLegacyBindings(*test_context_->checked_expr(), *test_context_,
                                activation, arena);
}

void TestRunner::RunTest(const TestCase& test_case) {
  // The arena has to be declared in RunTest because cel::Value returned by
  // EvalWithRuntime or EvalWithCelExpressionBuilder might contain pointers to
  // the arena. The arena has to be alive during the assertion.
  google::protobuf::Arena arena;
  const auto& checked_expr = test_context_->checked_expr();
  if (!checked_expr.has_value()) {
    ADD_FAILURE() << "CheckedExpr is required for evaluation.";
    return;
  }
  if (test_context_->runtime() != nullptr) {
    ASSERT_OK_AND_ASSIGN(cel::Value result, EvalWithRuntime(test_case, &arena));
    ASSERT_NO_FATAL_FAILURE(Assert(result, test_case, &arena));
  } else if (test_context_->cel_expression_builder() != nullptr) {
    ASSERT_OK_AND_ASSIGN(cel::Value result,
                         EvalWithCelExpressionBuilder(test_case, &arena));
    ASSERT_NO_FATAL_FAILURE(Assert(result, test_case, &arena));
  }
}
}  // namespace cel::test
