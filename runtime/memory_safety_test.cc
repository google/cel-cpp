// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//       https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Tests for memory safety using the CEL Evaluator.
#include <memory>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/memory/memory.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "checker/validation_result.h"
#include "common/decl.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "compiler/compiler.h"
#include "compiler/compiler_factory.h"
#include "compiler/optional.h"
#include "compiler/standard_library.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "runtime/activation.h"
#include "runtime/constant_folding.h"
#include "runtime/function_adapter.h"
#include "runtime/reference_resolver.h"
#include "runtime/regex_precompilation.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "cel/expr/conformance/proto3/test_all_types.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"
#include "google/protobuf/util/message_differencer.h"

namespace cel {
namespace {

using ::absl_testing::IsOkAndHolds;
using ::cel::expr::conformance::proto3::NestedTestAllTypes;
using ::cel::expr::conformance::proto3::TestAllTypes;
using ::cel::test::ValueMatcher;

struct TestCase {
  std::string name;
  std::string expression;
  absl::flat_hash_map<absl::string_view, Value> activation;
  test::ValueMatcher expected_matcher;
  bool reference_resolver_enabled = false;
};

enum Options { kDefault, kExhaustive, kFoldConstants };

using ParamType = std::tuple<TestCase, Options>;

absl::StatusOr<std::unique_ptr<Compiler>> CreateCompiler() {
  google::protobuf::LinkMessageReflection<cel::expr::conformance::proto3::TestAllTypes>();
  google::protobuf::LinkMessageReflection<
      cel::expr::conformance::proto3::NestedTestAllTypes>();

  CEL_ASSIGN_OR_RETURN(
      std::unique_ptr<CompilerBuilder> b,
      NewCompilerBuilder(google::protobuf::DescriptorPool::generated_pool()));
  CEL_RETURN_IF_ERROR(b->AddLibrary(StandardCompilerLibrary()));
  CEL_RETURN_IF_ERROR(b->AddLibrary(OptionalCompilerLibrary()));
  b->GetCheckerBuilder().set_container("cel.expr.conformance.proto3");
  auto& cb = b->GetCheckerBuilder();
  CEL_RETURN_IF_ERROR(cb.AddVariable(MakeVariableDecl("bool_var", BoolType())));
  CEL_RETURN_IF_ERROR(
      cb.AddVariable(MakeVariableDecl("string_var", StringType())));
  CEL_RETURN_IF_ERROR(
      cb.AddVariable(MakeVariableDecl("condition", BoolType())));

  CEL_RETURN_IF_ERROR(cb.AddFunction(
      MakeFunctionDecl("IsPrivate", MakeOverloadDecl("IsPrivate_string",
                                                     BoolType(), StringType()))
          .value()));
  CEL_RETURN_IF_ERROR(cb.AddFunction(
      MakeFunctionDecl(
          "net.IsPrivate",
          MakeOverloadDecl("net_IsPrivate_string", BoolType(), StringType()))
          .value()));

  return b->Build();
}

const Compiler& GetCompiler() {
  static const Compiler* compiler = []() {
    auto compiler = CreateCompiler();
    ABSL_QCHECK_OK(compiler.status());
    return compiler->release();
  }();
  return *compiler;
}

std::string TestCaseName(const testing::TestParamInfo<ParamType>& param_info) {
  const ParamType& param = param_info.param;
  absl::string_view opt;
  switch (std::get<1>(param)) {
    case Options::kDefault:
      opt = "default";
      break;
    case Options::kExhaustive:
      opt = "exhaustive";
      break;
    case Options::kFoldConstants:
      opt = "opt";
      break;
  }

  return absl::StrCat(std::get<0>(param).name, "_", opt);
}

bool IsPrivateIpv4Impl(const StringValue& addr) {
  // Implementation for demonstration, this is simple but incomplete and
  // brittle.
  std::string buf;
  return absl::StartsWith(addr.ToStringView(&buf), "192.168.") ||
         absl::StartsWith(addr.ToStringView(&buf), "10.");
}

absl::StatusOr<std::unique_ptr<Runtime>> ConfigureRuntimeImpl(
    bool resolve_references, Options evaluation_options) {
  RuntimeOptions options;
  switch (evaluation_options) {
    case Options::kDefault:
      options.short_circuiting = true;
      break;
    case Options::kExhaustive:
      options.short_circuiting = false;
      break;
    case Options::kFoldConstants:
      options.enable_comprehension_list_append = true;
      options.short_circuiting = true;
      break;
  }
  options.enable_qualified_type_identifiers = resolve_references;
  options.container = "cel.expr.conformance.proto3";
  CEL_ASSIGN_OR_RETURN(cel::RuntimeBuilder runtime_builder,
                       CreateStandardRuntimeBuilder(
                           google::protobuf::DescriptorPool::generated_pool(), options));
  if (resolve_references) {
    CEL_RETURN_IF_ERROR(EnableReferenceResolver(
        runtime_builder, ReferenceResolverEnabled::kAlways));
  }
  if (evaluation_options == Options::kFoldConstants) {
    CEL_RETURN_IF_ERROR(extensions::EnableConstantFolding(runtime_builder));
    CEL_RETURN_IF_ERROR(extensions::EnableRegexPrecompilation(runtime_builder));
  }

  auto s = UnaryFunctionAdapter<bool, const StringValue&>::Register(
      "IsPrivate", false, &IsPrivateIpv4Impl,
      runtime_builder.function_registry());
  CEL_RETURN_IF_ERROR(s);
  s.Update(UnaryFunctionAdapter<bool, const StringValue&>::Register(
      "net.IsPrivate", false, &IsPrivateIpv4Impl,
      runtime_builder.function_registry()));
  CEL_RETURN_IF_ERROR(s);

  return std::move(runtime_builder).Build();
}

class EvaluatorMemorySafetyTest : public testing::TestWithParam<ParamType> {
 public:
  EvaluatorMemorySafetyTest() = default;

 protected:
  const TestCase& GetTestCase() { return std::get<0>(GetParam()); }

  absl::StatusOr<std::unique_ptr<Runtime>> ConfigureRuntime() {
    return ConfigureRuntimeImpl(GetTestCase().reference_resolver_enabled,
                                std::get<1>(GetParam()));
  }
};

TEST_P(EvaluatorMemorySafetyTest, Basic) {
  const auto& test_case = GetTestCase();

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Runtime> runtime, ConfigureRuntime());

  ASSERT_OK_AND_ASSIGN(ValidationResult validation,
                       GetCompiler().Compile(test_case.expression));

  ASSERT_TRUE(validation.IsValid()) << validation.FormatError();
  ASSERT_OK_AND_ASSIGN(auto ast, validation.ReleaseAst());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       runtime->CreateProgram(std::move(ast)));

  Activation activation;
  for (const auto& [key, value] : test_case.activation) {
    activation.InsertOrAssignValue(key, value);
  }
  google::protobuf::Arena arena;
  absl::StatusOr<Value> got = program->Evaluate(&arena, activation);

  EXPECT_THAT(got, IsOkAndHolds(test_case.expected_matcher));
}

TEST_P(EvaluatorMemorySafetyTest, ProgramSafeAfterRuntimeDestroyed) {
  const auto& test_case = GetTestCase();

  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Runtime> runtime, ConfigureRuntime());

  ASSERT_OK_AND_ASSIGN(ValidationResult validation,
                       GetCompiler().Compile(test_case.expression));

  ASSERT_TRUE(validation.IsValid()) << validation.FormatError();
  ASSERT_OK_AND_ASSIGN(auto ast, validation.ReleaseAst());
  ASSERT_OK_AND_ASSIGN(std::unique_ptr<Program> program,
                       runtime->CreateProgram(std::move(ast)));

  Activation activation;
  for (const auto& [key, value] : test_case.activation) {
    activation.InsertOrAssignValue(key, value);
  }
  runtime.reset();
  google::protobuf::Arena arena;
  absl::StatusOr<Value> got = program->Evaluate(&arena, activation);
  EXPECT_THAT(got, IsOkAndHolds(test_case.expected_matcher));
}

// Helper for making an eternal string value without looking like a memory leak.
Value MakeStringValue(absl::string_view str) {
  static absl::NoDestructor<google::protobuf::Arena> kArena;
  return StringValue::Wrap(str, kArena.get());
}

INSTANTIATE_TEST_SUITE_P(
    Expression, EvaluatorMemorySafetyTest,
    testing::Combine(
        testing::ValuesIn(std::vector<TestCase>{
            {
                "bool",
                "(true && false) || bool_var || string_var == 'test_str'",
                {{"bool_var", BoolValue(false)},
                 {"string_var", MakeStringValue("test_str")}},
                test::BoolValueIs(true),
            },
            {
                "const_str",
                "condition ? 'left_hand_string' : 'right_hand_string'",
                {{"condition", BoolValue(false)}},
                test::StringValueIs("right_hand_string"),
            },
            {
                "long_const_string",
                "condition ? 'left_hand_string' : "
                "'long_right_hand_string_0123456789'",
                {{"condition", BoolValue(false)}},
                test::StringValueIs("long_right_hand_string_0123456789"),
            },
            {
                "computed_string",
                "(condition ? 'a.b' : 'b.c') + '.d.e.f'",
                {{"condition", BoolValue(false)}},
                test::StringValueIs("b.c.d.e.f"),
            },
            {
                "regex",
                R"('192.168.128.64'.matches(r'^192\.168\.[0-2]?[0-9]?[0-9]\.[0-2]?[0-9]?[0-9]') )",
                {},
                test::BoolValueIs(true),
            },
            {
                "list_create",
                "[1, 2, 3, 4, 5, 6][3] == 4",
                {},
                test::BoolValueIs(true),
            },
            {
                "list_create_strings",
                "['1', '2', '3', '4', '5', '6'][2] == '3'",
                {},
                test::BoolValueIs(true),
            },
            {
                "map_create",
                "{'1': 'one', '2': 'two'}['2']",
                {},
                test::StringValueIs("two"),
            },
            {
                "struct_create",
                R"cel(
                  NestedTestAllTypes{
                    child: NestedTestAllTypes{
                      payload: TestAllTypes{
                        repeated_int32: [1, 2, 3]
                      }
                    },
                    payload: TestAllTypes{
                      repeated_string: ["foo", "bar", "baz"]
                    }
                  })cel",
                {},
                test::StructValueIs(testing::Truly([](const StructValue& v)
                                                       -> bool {
                  if (!v.IsParsedMessage()) {
                    return false;
                  }
                  auto& msg = v.GetParsedMessage();
                  auto cmp = absl::WrapUnique(msg->New());
                  google::protobuf::TextFormat::ParseFromString(
                      R"pb(
                        child { payload { repeated_int32: [ 1, 2, 3 ] } }
                        payload { repeated_string: [ "foo", "bar", "baz" ] }
                      )pb",
                      cmp.get());
                  return google::protobuf::util::MessageDifferencer::Equals(*msg, *cmp);
                })),
            },
            {"extension_function",
             "IsPrivate('8.8.8.8')",
             {},
             test::BoolValueIs(false),
             /*enable_reference_resolver=*/false},
            {"namespaced_function",
             "net.IsPrivate('192.168.0.1')",
             {},
             test::BoolValueIs(true),
             /*enable_reference_resolver=*/true},
            {
                "comprehension",
                "['abc', 'def', 'ghi', 'jkl'].exists(el, el == 'mno')",
                {},
                test::BoolValueIs(false),
            },
            {
                "comprehension_complex",
                "['a' + 'b' + 'c', 'd' + 'ef', 'g' + 'hi', 'j' + 'kl']"
                ".exists(el, el.startsWith('g'))",
                {},
                test::BoolValueIs(true),
            }}),
        testing::Values(Options::kDefault, Options::kExhaustive,
                        Options::kFoldConstants)),
    &TestCaseName);

}  // namespace
}  // namespace cel
