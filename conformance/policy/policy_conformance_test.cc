// Copyright 2026 Google LLC
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

#include <iostream>
#include <memory>
#include <string>
#include <utility>
#include <vector>
// NOLINTNEXTLINE(build/c++17) for OSS compatibility
#include <filesystem>

#include "cel/expr/eval.pb.h"
#include "absl/flags/flag.h"
#include "absl/log/absl_check.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "common/ast.h"
#include "common/internal/value_conversion.h"
#include "common/source.h"
#include "common/value.h"
#include "compiler/compiler.h"
#include "env/config.h"
#include "env/env.h"
#include "env/env_runtime.h"
#include "env/env_std_extensions.h"
#include "env/env_yaml.h"
#include "env/runtime_std_extensions.h"
#include "extensions/protobuf/bind_proto_to_activation.h"
#include "extensions/protobuf/enum_adapter.h"
#include "internal/runfiles.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "policy/cel_policy.h"
#include "policy/cel_policy_parse_result.h"
#include "policy/cel_policy_validation_result.h"
#include "policy/compiler.h"
#include "policy/test_util.h"
#include "policy/yaml_policy_parser.h"
#include "runtime/activation.h"
#include "runtime/function_adapter.h"
#include "runtime/runtime.h"
#include "cel/expr/conformance/test/suite.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/message.h"
#include "google/protobuf/text_format.h"

// Use a specific file to handle bazel runfiles resolution correctly. We find
// parent directory named 'testdata' to use as the root of the test cases.
ABSL_FLAG(std::string, testdata_example, "",
          "Path to a specific example file.");
ABSL_FLAG(std::vector<std::string>, skip_tests, {},
          "Comma-separated list of tests to skip.");

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::cel::expr::conformance::test::TestSuite;
using ::cel::internal::GetSharedTestingDescriptorPool;
using ::testing::HasSubstr;

// Implementations for extension functions referenced in conformance tests.
cel::Value LocationCode(const cel::StringValue& ip,
                        const google::protobuf::DescriptorPool* pool,
                        google::protobuf::MessageFactory* factory, google::protobuf::Arena* arena) {
  std::string ip_str = ip.ToString();
  if (ip_str == "10.0.0.1") return cel::StringValue(arena, "us");
  if (ip_str == "10.0.0.2") return cel::StringValue(arena, "de");
  return cel::StringValue(arena, "ir");
}

// TODO(uncreated-issue/92): This should be migrated to use the testrunner utility
// after adding support for reading the yaml specification for envs/tests.
class InputEvaluator {
 public:
  static absl::StatusOr<std::unique_ptr<InputEvaluator>> Create(
      const std::shared_ptr<const google::protobuf::DescriptorPool>& pool) {
    cel::Env env;
    env.SetDescriptorPool(pool);
    cel::RegisterStandardExtensions(env);

    cel::EnvRuntime env_runtime;
    env_runtime.SetDescriptorPool(pool);
    cel::RegisterStandardExtensions(env_runtime);
    env_runtime.mutable_runtime_options().enable_qualified_type_identifiers =
        true;

    // Enable default extensions (optional, bindings)
    cel::Config config;
    CEL_RETURN_IF_ERROR(config.AddExtensionConfig(
        "optional", cel::Config::ExtensionConfig::kLatest));
    CEL_RETURN_IF_ERROR(config.AddExtensionConfig(
        "bindings", cel::Config::ExtensionConfig::kLatest));
    env.SetConfig(config);
    env_runtime.SetConfig(config);

    auto compiler_builder_or = env.NewCompilerBuilder();
    CEL_ASSIGN_OR_RETURN(auto compiler_builder, std::move(compiler_builder_or));
    compiler_builder->GetParserBuilder().GetOptions().enable_optional_syntax =
        true;
    CEL_ASSIGN_OR_RETURN(auto compiler, compiler_builder->Build());

    auto runtime_builder_or = env_runtime.CreateRuntimeBuilder();
    CEL_ASSIGN_OR_RETURN(auto runtime_builder, std::move(runtime_builder_or));

    // Register conformance enums
    for (const auto& enum_name :
         {"cel.expr.conformance.proto2.GlobalEnum",
          "cel.expr.conformance.proto3.GlobalEnum",
          "cel.expr.conformance.proto2.TestAllTypes.NestedEnum",
          "cel.expr.conformance.proto3.TestAllTypes.NestedEnum"}) {
      auto* enum_desc = pool->FindEnumTypeByName(enum_name);
      if (enum_desc != nullptr) {
        CEL_RETURN_IF_ERROR(cel::extensions::RegisterProtobufEnum(
            runtime_builder.type_registry(), enum_desc));
      }
    }

    CEL_ASSIGN_OR_RETURN(auto runtime, std::move(runtime_builder).Build());

    return absl::WrapUnique(
        new InputEvaluator(std::move(compiler), std::move(runtime)));
  }

  absl::StatusOr<cel::Value> Evaluate(
      absl::string_view expr_str, google::protobuf::Arena* arena,
      google::protobuf::MessageFactory* message_factory) const {
    CEL_ASSIGN_OR_RETURN(auto validation_result, compiler_->Compile(expr_str));
    if (!validation_result.IsValid()) {
      return absl::InvalidArgumentError(
          absl::StrCat("Failed to compile input expr: ", expr_str));
    }
    CEL_ASSIGN_OR_RETURN(auto ast, validation_result.ReleaseAst());
    CEL_ASSIGN_OR_RETURN(
        auto program,
        runtime_->CreateProgram(std::make_unique<cel::Ast>(std::move(*ast))));
    cel::Activation activation;
    EvaluateOptions options;
    options.message_factory = message_factory;
    return program->Evaluate(arena, activation, options);
  }

 private:
  InputEvaluator(std::unique_ptr<cel::Compiler> compiler,
                 std::unique_ptr<cel::Runtime> runtime)
      : compiler_(std::move(compiler)), runtime_(std::move(runtime)) {}

  std::unique_ptr<cel::Compiler> compiler_;
  std::unique_ptr<cel::Runtime> runtime_;
};

absl::StatusOr<cel::Value> EvaluateInputValue(
    const cel::expr::conformance::test::InputValue& input_val,
    const InputEvaluator& evaluator,
    const google::protobuf::DescriptorPool* descriptor_pool,
    google::protobuf::MessageFactory* message_factory, google::protobuf::Arena* arena) {
  if (input_val.has_expr()) {
    return evaluator.Evaluate(input_val.expr(), arena, message_factory);
  }
  if (input_val.has_value()) {
    return cel::test::FromExprValue(input_val.value(), descriptor_pool,
                                    message_factory, arena);
  }
  return absl::InvalidArgumentError("Empty InputValue");
}

class CelValueMatcherImpl
    : public testing::MatcherInterface<const cel::Value&> {
 public:
  CelValueMatcherImpl(cel::Value expected_val,
                      const google::protobuf::DescriptorPool* pool,
                      google::protobuf::MessageFactory* message_factory,
                      google::protobuf::Arena* arena)
      : expected_val_(std::move(expected_val)),
        pool_(pool),
        message_factory_(message_factory),
        arena_(arena) {}

  bool MatchAndExplain(const cel::Value& actual_val,
                       testing::MatchResultListener* listener) const override {
    cel::Value actual = actual_val;
    if (actual.IsOptional() && !expected_val_.IsOptional()) {
      auto opt_val = actual.AsOptional();
      if (opt_val->HasValue()) {
        actual = opt_val->Value();
      }
    }
    cel::Value eq_result;
    auto eq_status = actual.Equal(expected_val_, pool_, message_factory_,
                                  arena_, &eq_result);
    if (!eq_status.ok()) {
      *listener << "equality check failed with status: " << eq_status;
      return false;
    }
    if (!eq_result.IsTrue()) {
      *listener << "expected: " << expected_val_.DebugString()
                << "\nactual: " << actual.DebugString();
      return false;
    }
    return true;
  }

  void DescribeTo(std::ostream* os) const override {
    *os << "is equal to " << expected_val_.DebugString();
  }

  void DescribeNegationTo(std::ostream* os) const override {
    *os << "is not equal to " << expected_val_.DebugString();
  }

 private:
  cel::Value expected_val_;
  const google::protobuf::DescriptorPool* pool_;
  google::protobuf::MessageFactory* message_factory_;
  google::protobuf::Arena* arena_;
};

absl::StatusOr<testing::Matcher<cel::Value>> MakeExpectedValueMatcher(
    const cel::expr::conformance::test::TestOutput& output,
    const InputEvaluator& input_evaluator, const google::protobuf::DescriptorPool* pool,
    google::protobuf::MessageFactory* message_factory, google::protobuf::Arena* arena) {
  cel::Value expected_val;
  if (output.has_result_expr()) {
    CEL_ASSIGN_OR_RETURN(
        expected_val,
        input_evaluator.Evaluate(output.result_expr(), arena, message_factory));
  } else if (output.has_result_value()) {
    CEL_ASSIGN_OR_RETURN(expected_val,
                         cel::test::FromExprValue(output.result_value(), pool,
                                                  message_factory, arena));
  } else {
    return absl::InvalidArgumentError("Unsupported output kind");
  }
  return testing::Matcher<cel::Value>(
      new CelValueMatcherImpl(expected_val, pool, message_factory, arena));
}

bool ShouldRunTest(absl::string_view test_name,
                   const std::vector<std::string>& skip_tests) {
  for (const std::string& skip : skip_tests) {
    if (absl::StartsWith(test_name, skip)) {
      return false;
    }
  }
  return true;
}

absl::Status PopulateActivation(
    const cel::expr::conformance::test::TestCase& test,
    const InputEvaluator& input_evaluator,
    const google::protobuf::DescriptorPool* descriptor_pool,
    google::protobuf::MessageFactory* message_factory,
    absl::string_view context_msg_type_name, google::protobuf::Arena* arena,
    Activation& activation) {
  if (!test.has_input_context()) {
    for (const auto& [var_name, input_val] : test.input()) {
      CEL_ASSIGN_OR_RETURN(
          auto val,
          EvaluateInputValue(input_val, input_evaluator, descriptor_pool,
                             message_factory, arena));
      activation.InsertOrAssignValue(var_name, std::move(val));
    }
    return absl::OkStatus();
  }

  const auto& input_context = test.input_context();
  const google::protobuf::Message* context_message = nullptr;

  if (input_context.has_context_message()) {
    const google::protobuf::Any& any_msg = input_context.context_message();
    const google::protobuf::Descriptor* msg_descriptor =
        descriptor_pool->FindMessageTypeByName(context_msg_type_name);
    if (msg_descriptor == nullptr) {
      return absl::NotFoundError(absl::StrCat(
          "Failed to find message descriptor for: ", context_msg_type_name));
    }
    const google::protobuf::Message* prototype =
        message_factory->GetPrototype(msg_descriptor);
    if (prototype == nullptr) {
      return absl::NotFoundError(
          absl::StrCat("Failed to get prototype for: ", context_msg_type_name));
    }
    auto* buf = prototype->New(arena);
    if (!any_msg.UnpackTo(buf)) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Failed to unpack context message to ", context_msg_type_name));
    }
    context_message = buf;
  } else if (input_context.has_context_expr() &&
             !context_msg_type_name.empty()) {
    CEL_ASSIGN_OR_RETURN(cel::Value evaluated_val,
                         input_evaluator.Evaluate(input_context.context_expr(),
                                                  arena, message_factory));

    if (!evaluated_val.IsParsedMessage()) {
      return absl::InvalidArgumentError(
          absl::StrCat("Context expression did not evaluate to a message: ",
                       input_context.context_expr()));
    }
    if (evaluated_val.GetParsedMessage().GetDescriptor()->full_name() !=
        context_msg_type_name) {
      return absl::InvalidArgumentError(absl::StrCat(
          "Context expression evaluated to a message of type ",
          evaluated_val.GetParsedMessage().GetDescriptor()->full_name(),
          " which does not match the expected type ", context_msg_type_name));
    }
    context_message = static_cast<const google::protobuf::Message*>(
        evaluated_val.GetParsedMessage().operator->());
  }
  if (context_message == nullptr) {
    return absl::InvalidArgumentError(
        "Failed to resolve context message for test case");
  }

  return cel::extensions::BindProtoToActivation(
      *context_message,
      cel::extensions::BindProtoUnsetFieldBehavior::kBindDefaultValue,
      descriptor_pool, message_factory, arena, &activation);
}

class PolicyTestSuiteRunner {
 public:
  PolicyTestSuiteRunner(std::string suite_name,
                        std::unique_ptr<cel::Compiler> compiler,
                        std::unique_ptr<cel::Runtime> runtime,
                        std::shared_ptr<CelPolicySource> policy_source,
                        CelPolicyValidationResult compile_result,
                        std::shared_ptr<const google::protobuf::DescriptorPool> pool,
                        std::shared_ptr<google::protobuf::MessageFactory> message_factory,
                        std::shared_ptr<InputEvaluator> input_evaluator,
                        std::string context_msg_type_name,
                        bool expect_compile_fail = false)
      : suite_name_(std::move(suite_name)),
        compiler_(std::move(compiler)),
        runtime_(std::move(runtime)),
        policy_source_(std::move(policy_source)),
        compile_result_(std::move(compile_result)),
        pool_(std::move(pool)),
        message_factory_(std::move(message_factory)),
        input_evaluator_(std::move(input_evaluator)),
        context_msg_type_name_(std::move(context_msg_type_name)),
        expect_compile_fail_(expect_compile_fail) {}

  void RunTest(const cel::expr::conformance::test::TestCase& test,
               absl::string_view full_test_name) {
    const auto& output = test.output();

    if (expect_compile_fail_) {
      ASSERT_FALSE(compile_result_.IsValid())
          << "Expected compilation to fail in " << full_test_name;
      ASSERT_TRUE(output.has_eval_error())
          << "Expected eval_error to be present in compile error test "
          << full_test_name;
      std::string err_msg = compile_result_.FormatIssues();
      for (const auto& expected_err : output.eval_error().errors()) {
        EXPECT_THAT(err_msg, HasSubstr(expected_err.message()))
            << "Did not find expected compile time error";
      }
      return;
    }

    // Compilation should have succeeded for evaluation tests
    ASSERT_TRUE(compile_result_.IsValid())
        << "Compilation has validation errors in " << full_test_name << ": "
        << compile_result_.FormatIssues();

    ASSERT_OK_AND_ASSIGN(std::unique_ptr<cel::Program> program,
                         runtime_->CreateProgram(std::make_unique<cel::Ast>(
                             *compile_result_.GetAst())));

    // Parse Inputs and evaluate them
    google::protobuf::Arena arena;
    Activation activation;
    ASSERT_THAT(PopulateActivation(test, *input_evaluator_, pool_.get(),
                                   message_factory_.get(),
                                   context_msg_type_name_, &arena, activation),
                IsOk());

    // Evaluate Policy
    auto eval_result_or = program->Evaluate(&arena, activation);
    ASSERT_THAT(eval_result_or.status(), IsOk())
        << "Evaluation failed in " << full_test_name;
    cel::Value actual_val = *eval_result_or;

    ASSERT_OK_AND_ASSIGN(
        auto matcher,
        MakeExpectedValueMatcher(output, *input_evaluator_, pool_.get(),
                                 message_factory_.get(), &arena));

    // Apply matcher to the output of evaluation
    EXPECT_THAT(actual_val, matcher) << "Test failed: " << full_test_name;
  }

 private:
  std::string suite_name_;
  std::unique_ptr<cel::Compiler> compiler_;
  std::unique_ptr<cel::Runtime> runtime_;
  std::shared_ptr<CelPolicySource> policy_source_;
  CelPolicyValidationResult compile_result_;
  std::shared_ptr<const google::protobuf::DescriptorPool> pool_;
  std::shared_ptr<google::protobuf::MessageFactory> message_factory_;
  std::shared_ptr<InputEvaluator> input_evaluator_;
  std::string context_msg_type_name_;
  bool expect_compile_fail_;
};

class CelPolicyTest : public testing::Test {
 public:
  explicit CelPolicyTest(std::shared_ptr<PolicyTestSuiteRunner> runner,
                         cel::expr::conformance::test::TestCase test_case,
                         std::string full_test_name, bool skip)
      : runner_(std::move(runner)),
        test_case_(std::move(test_case)),
        full_test_name_(std::move(full_test_name)),
        skip_(skip) {}

  void TestBody() override {
    if (skip_) {
      GTEST_SKIP() << "Skipping test: " << full_test_name_;
    }
    EXPECT_NO_FATAL_FAILURE(runner_->RunTest(test_case_, full_test_name_));
  }

 private:
  std::shared_ptr<PolicyTestSuiteRunner> runner_;
  cel::expr::conformance::test::TestCase test_case_;
  std::string full_test_name_;
  bool skip_;
};


absl::Status RegisterTestSuite(
    const std::filesystem::path& dir_path, const std::string& suite_name,
    const std::shared_ptr<InputEvaluator>& input_evaluator,
    const std::shared_ptr<const google::protobuf::DescriptorPool>& pool,
    const std::shared_ptr<google::protobuf::MessageFactory>& message_factory,
    const std::vector<std::string>& skip_tests) {
  // Check if the entire suite should be skipped (prefix match)
  for (const auto& skip : skip_tests) {
    if (suite_name == skip ||
        absl::StartsWith(suite_name, absl::StrCat(skip, "/"))) {
      std::cout << "[ SKIPPED SUITE ] " << suite_name << std::endl;
      return absl::OkStatus();
    }
  }

  std::filesystem::path policy_path = dir_path / "policy.yaml";
  std::filesystem::path tests_path = dir_path / "tests.yaml";
  bool is_yaml = true;
  if (!std::filesystem::exists(tests_path)) {
    tests_path = dir_path / "tests.textproto";
    is_yaml = false;
  }
  std::filesystem::path config_path = dir_path / "config.yaml";

  if (!std::filesystem::exists(policy_path) ||
      !std::filesystem::exists(tests_path)) {
    // Not a valid test suite, assume it's a directory we don't care about.
    return absl::OkStatus();
  }

  // Parse Environment Config
  cel::Config config;
  if (std::filesystem::exists(config_path)) {
    std::string config_content;
    CEL_RETURN_IF_ERROR(
        cel::internal::GetFileContents(config_path.string(), &config_content));
    CEL_ASSIGN_OR_RETURN(config, cel::EnvConfigFromYaml(config_content));
  }

  // Enable default extensions (optional, bindings) in the config
  CEL_RETURN_IF_ERROR(config.AddExtensionConfig(
      "optional", cel::Config::ExtensionConfig::kLatest));
  CEL_RETURN_IF_ERROR(config.AddExtensionConfig(
      "bindings", cel::Config::ExtensionConfig::kLatest));

  // Set up compiler & runtime environments
  cel::Env env;
  env.SetDescriptorPool(pool);
  cel::RegisterStandardExtensions(env);
  env.SetConfig(config);

  cel::EnvRuntime env_runtime;
  env_runtime.SetDescriptorPool(pool);
  cel::RegisterStandardExtensions(env_runtime);
  env_runtime.SetConfig(config);
  env_runtime.mutable_runtime_options().enable_qualified_type_identifiers =
      true;

  CEL_ASSIGN_OR_RETURN(auto compiler_builder, env.NewCompilerBuilder());
  compiler_builder->GetParserBuilder().GetOptions().enable_optional_syntax =
      true;

  CEL_ASSIGN_OR_RETURN(auto compiler, compiler_builder->Build());

  CEL_ASSIGN_OR_RETURN(auto runtime_builder,
                       env_runtime.CreateRuntimeBuilder());

  // Register conformance enums
  for (const auto& enum_name :
       {"cel.expr.conformance.proto2.GlobalEnum",
        "cel.expr.conformance.proto3.GlobalEnum",
        "cel.expr.conformance.proto2.TestAllTypes.NestedEnum",
        "cel.expr.conformance.proto3.TestAllTypes.NestedEnum"}) {
    auto* enum_desc = pool->FindEnumTypeByName(enum_name);
    if (enum_desc != nullptr) {
      CEL_RETURN_IF_ERROR(cel::extensions::RegisterProtobufEnum(
          runtime_builder.type_registry(), enum_desc));
    }
  }

  // Register locationCode in runtime
  CEL_RETURN_IF_ERROR(
      (cel::UnaryFunctionAdapter<cel::Value, const cel::StringValue&>::
           RegisterGlobalOverload("locationCode", LocationCode,
                                  runtime_builder.function_registry())));

  CEL_ASSIGN_OR_RETURN(auto runtime, std::move(runtime_builder).Build());

  // Parse Policy
  std::string policy_content;
  CEL_RETURN_IF_ERROR(
      cel::internal::GetFileContents(policy_path.string(), &policy_content));
  CEL_ASSIGN_OR_RETURN(auto source,
                       cel::NewSource(policy_content, "policy.yaml"));
  auto policy_source = std::make_shared<CelPolicySource>(std::move(source));
  CEL_ASSIGN_OR_RETURN(CelPolicyParseResult parse_result,
                       cel::ParseYamlCelPolicy(policy_source));
  if (!parse_result.IsValid()) {
    return absl::InvalidArgumentError(
        absl::StrCat("Failed to parse policy.yaml in ", suite_name,
                     "\nIssues:\n", parse_result.FormattedIssues()));
  }
  const CelPolicy* policy = parse_result.GetPolicy();

  // Compile Policy (unexpected non-ok status represents a bug)
  CEL_ASSIGN_OR_RETURN(CelPolicyValidationResult compile_result,
                       CompilePolicy(*compiler, *policy));

  std::string tests_content;
  CEL_RETURN_IF_ERROR(
      cel::internal::GetFileContents(tests_path.string(), &tests_content));
  TestSuite test_suite;
  if (is_yaml) {
    CEL_ASSIGN_OR_RETURN(test_suite,
                         cel::test::ParsePolicyTestSuiteYaml(tests_content));
  } else {
    if (!google::protobuf::TextFormat::ParseFromString(tests_content, &test_suite)) {
      return absl::InvalidArgumentError(
          absl::StrCat("Failed to parse text proto in ", tests_path.string()));
    }
  }

  auto runner = std::make_shared<PolicyTestSuiteRunner>(
      suite_name, std::move(compiler), std::move(runtime),
      std::move(policy_source), std::move(compile_result), pool,
      message_factory, input_evaluator, config.GetContextType(),
      /*expect_compile_fail=*/absl::StrContains(suite_name, "compile_errors"));

  for (const auto& section : test_suite.sections()) {
    std::string section_name = section.name();
    for (const auto& test : section.tests()) {
      std::string test_name = test.name();
      std::string full_test_name =
          absl::StrCat(suite_name, "/", section_name, "/", test_name);

      bool skip = !ShouldRunTest(full_test_name, skip_tests);

      testing::RegisterTest(
          suite_name.c_str(),
          absl::StrCat(section_name, "/", test_name).c_str(), nullptr,
          test_name.c_str(), __FILE__, __LINE__,
          [runner, test, full_test_name, skip]() -> CelPolicyTest* {
            return new CelPolicyTest(runner, test, full_test_name, skip);
          });
    }
  }
  return absl::OkStatus();
}

void RegisterAllTests() {
  // cel::google3-end
  std::string testdata_example_flag = absl::GetFlag(FLAGS_testdata_example);
  std::vector<std::string> skip_tests = absl::GetFlag(FLAGS_skip_tests);

  std::string abs_testdata_example =
      cel::internal::ResolveRunfilesPath(testdata_example_flag);
  ABSL_CHECK(!abs_testdata_example.empty())
      << "Could not find testdata directory: " << testdata_example_flag;

  std::shared_ptr<const google::protobuf::DescriptorPool> pool =
      GetSharedTestingDescriptorPool();
  auto message_factory =
      std::make_shared<google::protobuf::DynamicMessageFactory>(pool.get());
  message_factory->SetDelegateToGeneratedFactory(true);
  auto evaluator_or = InputEvaluator::Create(pool);
  ABSL_CHECK_OK(evaluator_or.status()) << "Failed to create input evaluator";
  std::shared_ptr<InputEvaluator> evaluator = std::move(evaluator_or.value());

  std::filesystem::path testdata_path(abs_testdata_example);
  ABSL_CHECK(std::filesystem::exists(testdata_path))
      << "Testdata path does not exist: " << testdata_path;
  // walk up to find 'testdata' parent. A work around to portably
  // get the expected directory from bazel.
  while (!absl::EndsWith(testdata_path.string(), "testdata")) {
    testdata_path = testdata_path.parent_path();
    ABSL_CHECK(testdata_path.string().size() > sizeof("testdata"))
        << "could not resolve testdata directory";
  }

  for (const auto& entry :
       std::filesystem::recursive_directory_iterator(testdata_path)) {
    if (!entry.is_directory()) {
      continue;
    }
    std::filesystem::path dir_path = entry.path();
    // Check if this directory has policy.yaml and tests.yaml (or
    // tests.textproto)
    if (std::filesystem::exists(dir_path / "policy.yaml") &&
        (std::filesystem::exists(dir_path / "tests.yaml") ||
         std::filesystem::exists(dir_path / "tests.textproto"))) {
      std::string suite_name = absl::StrReplaceAll(
          std::filesystem::relative(dir_path, testdata_path).string(),
          {{"\\", "/"}});

      ABSL_CHECK_OK(RegisterTestSuite(dir_path, suite_name, evaluator, pool,
                                      message_factory, skip_tests));
    }
  }
}

}  // namespace
}  // namespace cel

int main(int argc, char** argv) {
  testing::InitGoogleTest(&argc, argv);

  cel::RegisterAllTests();
  return RUN_ALL_TESTS();
}
