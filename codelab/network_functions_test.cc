// Copyright 2025 Google LLC
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

#include "codelab/network_functions.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/decl.h"
#include "common/minimal_descriptor_pool.h"
#include "common/type.h"
#include "common/value.h"
#include "compiler/compiler.h"
#include "compiler/compiler_factory.h"
#include "compiler/standard_library.h"
#include "internal/benchmark.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "runtime/activation.h"
#include "runtime/constant_folding.h"
#include "runtime/runtime.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "google/protobuf/arena.h"

namespace cel_codelab {
namespace {

using ::absl_testing::IsOk;
using ::cel::Activation;
using ::cel::Compiler;
using ::cel::Program;
using ::cel::Runtime;
using ::cel::RuntimeOptions;
using ::cel::StringValue;
using ::testing::HasSubstr;

struct TestCase {
  std::string name;
  std::string expr;
  std::string type_check_err_substr;
};

class NetworkFunctionsCheckerTest : public testing::TestWithParam<TestCase> {};

TEST_P(NetworkFunctionsCheckerTest, DeclarationsTest) {
  const TestCase& test_case = GetParam();

  ASSERT_OK_AND_ASSIGN(
      auto compiler_builder,
      cel::NewCompilerBuilder(cel::GetMinimalDescriptorPool()));
  ASSERT_THAT(compiler_builder->AddLibrary(cel::StandardCompilerLibrary()),
              IsOk());
  ASSERT_THAT(compiler_builder->AddLibrary(NetworkFunctionsCompilerLibrary()),
              IsOk());
  ASSERT_OK_AND_ASSIGN(auto compiler, compiler_builder->Build());

  ASSERT_OK_AND_ASSIGN(auto result, compiler->Compile(test_case.expr));

  if (!test_case.type_check_err_substr.empty()) {
    EXPECT_THAT(result.FormatError(),
                HasSubstr(test_case.type_check_err_substr));
    return;
  }

  EXPECT_TRUE(result.IsValid()) << result.FormatError();
}

INSTANTIATE_TEST_SUITE_P(
    NetworkFunctionsCheckerTests, NetworkFunctionsCheckerTest,
    testing::ValuesIn<TestCase>({
        {"type_identifier_addr", "net.Address != type(1)"},
        {"type_identifier_addr_2", "net.Address != list"},
        {"type_identifier_addr_matcher", "net.AddressMatcher != type(1)"},
        {"parse_address", "net.parseAddress('1.2.3.4')"},
        {"parse_address_or_zero", "net.parseAddressOrZero('1.2.3.4')"},
        {"parse_address_no_match", "net.parseAddress(1.0)",
         "no matching overload for 'net.parseAddress'"},
        {"address_zero", "net.addressZeroValue"},
        {"equals", "net.parseAddress('1.2.3.4') != net.addressZeroValue"},
        {"address_matcher_parse",
         "net.parseAddressMatcher('8.8.8.0-8.8.8.255')"},
        {"address_matcher_parse_invalid",
         "net.parseAddressMatcher('8.8.8.0-8.8.4.255')"},
        {"address_matcher_contains",
         "net.parseAddressMatcher('8.8.8.0-8.8.8.255').containsAddress(net."
         "parseAddress('8.8.8.1'))"},
        {"address_matcher_contains_string",
         "net.parseAddressMatcher('8.8.8.0-8.8.8.255').containsAddress('8.8.8."
         "1')"},
    }),
    [](const testing::TestParamInfo<NetworkFunctionsCheckerTest::ParamType>&
           info) { return info.param.name; });

struct RuntimeTestCase {
  std::string name;
  std::string expr;
  std::string runtime_err_substr;
  bool expected_value = true;
};

class NetworkFunctionsRuntimeTest
    : public testing::TestWithParam<RuntimeTestCase> {};

TEST_P(NetworkFunctionsRuntimeTest, EvaluationTest) {
  const RuntimeTestCase& test_case = GetParam();

  ASSERT_OK_AND_ASSIGN(
      auto compiler_builder,
      cel::NewCompilerBuilder(cel::GetMinimalDescriptorPool()));
  ASSERT_THAT(compiler_builder->AddLibrary(cel::StandardCompilerLibrary()),
              IsOk());
  ASSERT_THAT(compiler_builder->AddLibrary(NetworkFunctionsCompilerLibrary()),
              IsOk());
  ASSERT_OK_AND_ASSIGN(auto compiler, compiler_builder->Build());

  ASSERT_OK_AND_ASSIGN(auto result, compiler->Compile(test_case.expr));

  ASSERT_OK_AND_ASSIGN(auto ast, result.ReleaseAst());
  RuntimeOptions runtime_options;
  runtime_options.enable_qualified_type_identifiers = true;
  ASSERT_OK_AND_ASSIGN(auto runtime_builder,
                       CreateStandardRuntimeBuilder(
                           cel::GetMinimalDescriptorPool(), runtime_options));
  ASSERT_THAT(
      RegisterNetworkTypes(runtime_builder.type_registry(), runtime_options),
      IsOk());
  ASSERT_THAT(RegisterNetworkFunctions(runtime_builder.function_registry(),
                                       runtime_options),
              IsOk());

  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(runtime_builder).Build());

  ASSERT_OK_AND_ASSIGN(auto program, runtime->CreateProgram(std::move(ast)));

  google::protobuf::Arena arena;
  Activation activation;
  ASSERT_OK_AND_ASSIGN(auto eval_result, program->Evaluate(&arena, activation));

  if (!test_case.runtime_err_substr.empty()) {
    if (!eval_result.IsError()) {
      FAIL() << "Expected error, but got: " << eval_result.DebugString();
    }
    EXPECT_THAT(eval_result.GetError().ToStatus().message(),
                HasSubstr(test_case.runtime_err_substr));
    return;
  }

  if (test_case.expected_value) {
    EXPECT_TRUE(eval_result.IsBool() && eval_result.GetBool())
        << eval_result.DebugString();
  }
}

INSTANTIATE_TEST_SUITE_P(
    NetworkFunctionsRuntimeTests, NetworkFunctionsRuntimeTest,
    testing::ValuesIn<RuntimeTestCase>(
        {{"type_identifier_addr", "net.Address != type(1)"},
         {"type_identifier_addr_2", "net.Address != list"},
         {"type_identifier_addr_matcher", "net.AddressMatcher != type(1)"},
         {"parse_address",
          "net.parseAddress('1.2.3.4') == net.parseAddress('1.2.3.4')"},
         {"parse_address_2",
          "net.parseAddress('1.2.3.4') != net.parseAddress('2.3.4.5')"},
         {"parse_address_invalid",
          "net.parseAddress('256.2.3.4') != net.parseAddress('1.2.3.4')",
          "invalid address"},
         {"parse_address_or_zero",
          "net.parseAddressOrZero('256.2.3.4') != "
          "net.parseAddressOrZero('1.2.3.4')"},
         {"parse_address_matcher",
          "net.parseAddressMatcher('8.8.8.0-8.8.8.255') != "
          "net.parseAddressMatcher('8.8.8.0-8.8.8.127')"},
         {"address_matcher_matches",
          "net.parseAddressMatcher('8.8.8.0-8.8.8.255').containsAddress(net."
          "parseAddress('8.8.8.1'))"}}),
    [](const testing::TestParamInfo<NetworkFunctionsRuntimeTest::ParamType>&
           info) { return info.param.name; });

class BenchmarkState {
 public:
  static absl::StatusOr<BenchmarkState> Create(bool optimize) {
    CEL_ASSIGN_OR_RETURN(
        auto compiler_builder,
        cel::NewCompilerBuilder(cel::GetMinimalDescriptorPool()));
    CEL_RETURN_IF_ERROR(
        compiler_builder->AddLibrary(cel::StandardCompilerLibrary()));
    CEL_RETURN_IF_ERROR(
        compiler_builder->AddLibrary(NetworkFunctionsCompilerLibrary()));
    compiler_builder->GetCheckerBuilder()
        .AddVariable(MakeVariableDecl("ip", cel::StringType()))
        .IgnoreError();

    CEL_ASSIGN_OR_RETURN(auto compiler, compiler_builder->Build());

    RuntimeOptions runtime_options;
    CEL_ASSIGN_OR_RETURN(auto runtime_builder,
                         CreateStandardRuntimeBuilder(
                             cel::GetMinimalDescriptorPool(), runtime_options));
    CEL_RETURN_IF_ERROR(
        RegisterNetworkTypes(runtime_builder.type_registry(), runtime_options));
    CEL_RETURN_IF_ERROR(RegisterNetworkFunctions(
        runtime_builder.function_registry(), runtime_options));

    if (optimize) {
      CEL_RETURN_IF_ERROR(
          cel::extensions::EnableConstantFolding(runtime_builder));
    }
    CEL_ASSIGN_OR_RETURN(auto runtime, std::move(runtime_builder).Build());
    return BenchmarkState(std::move(compiler), std::move(runtime));
  }

  absl::StatusOr<std::unique_ptr<Program>> MakeProgram(absl::string_view expr) {
    CEL_ASSIGN_OR_RETURN(auto result, compiler_->Compile(expr));
    if (!result.IsValid()) {
      return absl::InvalidArgumentError(result.FormatError());
    }
    CEL_ASSIGN_OR_RETURN(auto ast, result.ReleaseAst());
    return runtime_->CreateProgram(std::move(ast));
  }

 private:
  BenchmarkState(std::unique_ptr<Compiler> c, std::unique_ptr<const Runtime> r)
      : compiler_(std::move(c)), runtime_(std::move(r)) {}

  std::unique_ptr<Compiler> compiler_;
  std::unique_ptr<const Runtime> runtime_;
  std::unique_ptr<google::protobuf::Arena> constants_;
};

void BM_ParseAddress(benchmark::State& state) {
  bool optimize = state.range(0);
  auto runner = BenchmarkState::Create(optimize);

  ABSL_CHECK_OK(runner.status());

  auto program = runner->MakeProgram("net.parseAddress('1.2.3.4')");
  ABSL_CHECK_OK(program.status());

  google::protobuf::Arena arena;
  Activation activation;
  for (auto s : state) {
    auto result = (*program)->Evaluate(&arena, activation);
    ABSL_CHECK_OK(result.status());
  }
}

void BM_ParseAddressVar(benchmark::State& state) {
  bool optimize = state.range(0);
  auto runner = BenchmarkState::Create(optimize);

  ABSL_CHECK_OK(runner.status());

  auto program = runner->MakeProgram("net.parseAddress(ip)");
  ABSL_CHECK_OK(program.status());

  google::protobuf::Arena arena;
  Activation activation;
  activation.InsertOrAssignValue("ip", StringValue::From("8.8.8.8", &arena));
  for (auto s : state) {
    auto result = (*program)->Evaluate(&arena, activation);
    ABSL_CHECK_OK(result.status());
  }
}

void BM_ParseAddressMatcher(benchmark::State& state) {
  bool optimize = state.range(0);
  auto runner = BenchmarkState::Create(optimize);

  ABSL_CHECK_OK(runner.status());

  auto program =
      runner->MakeProgram("net.parseAddressMatcher('8.8.8.0-8.8.8.255')");
  ABSL_CHECK_OK(program.status());

  google::protobuf::Arena arena;
  Activation activation;
  for (auto s : state) {
    auto result = (*program)->Evaluate(&arena, activation);
    ABSL_CHECK_OK(result.status());
  }
}

void BM_ParseAddressMatcherMatches(benchmark::State& state) {
  bool optimize = state.range(0);
  auto runner = BenchmarkState::Create(optimize);

  ABSL_CHECK_OK(runner.status());

  auto program = runner->MakeProgram(
      "net.parseAddressMatcher('8.8.8.0-8.8.8.255').containsAddress(net."
      "parseAddress('8.8.8.1'))");
  ABSL_CHECK_OK(program.status());

  google::protobuf::Arena arena;
  Activation activation;
  for (auto s : state) {
    auto result = (*program)->Evaluate(&arena, activation);
    ABSL_CHECK_OK(result.status());
  }
}

void BM_ParseAddressMatcherMatchesVar(benchmark::State& state) {
  bool optimize = state.range(0);
  auto runner = BenchmarkState::Create(optimize);

  ABSL_CHECK_OK(runner.status());

  auto program = runner->MakeProgram(
      "net.parseAddressMatcher('8.8.0.0-8.8.255.255').containsAddress(net."
      "parseAddress(ip))");
  ABSL_CHECK_OK(program.status());

  google::protobuf::Arena arena;
  Activation activation;
  activation.InsertOrAssignValue("ip", StringValue::From("8.8.4.4", &arena));
  for (auto s : state) {
    auto result = (*program)->Evaluate(&arena, activation);
    ABSL_CHECK_OK(result.status());
  }
}

BENCHMARK(BM_ParseAddress)->Arg(0)->Arg(1);
BENCHMARK(BM_ParseAddressVar)->Arg(0)->Arg(1);
BENCHMARK(BM_ParseAddressMatcher)->Arg(0)->Arg(1);
BENCHMARK(BM_ParseAddressMatcherMatches)->Arg(0)->Arg(1);
BENCHMARK(BM_ParseAddressMatcherMatchesVar)->Arg(0)->Arg(1);

}  // namespace
}  // namespace cel_codelab
