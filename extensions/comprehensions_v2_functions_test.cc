// Copyright 2024 Google LLC
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

#include "extensions/comprehensions_v2_functions.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "cel/expr/syntax.pb.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/source.h"
#include "common/value_testing.h"
#include "common/values/list_value_builder.h"
#include "common/values/map_value_builder.h"
#include "extensions/bindings_ext.h"
#include "extensions/comprehensions_v2_macros.h"
#include "extensions/protobuf/runtime_adapter.h"
#include "extensions/strings.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "parser/macro_registry.h"
#include "parser/options.h"
#include "parser/parser.h"
#include "parser/standard_macros.h"
#include "runtime/activation.h"
#include "runtime/optional_types.h"
#include "runtime/reference_resolver.h"
#include "runtime/runtime.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "google/protobuf/arena.h"

namespace cel::extensions {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::cel::test::BoolValueIs;
using ::cel::test::ErrorValueIs;
using ::google::api::expr::parser::EnrichedParse;
using ::testing::TestWithParam;

struct ComprehensionsV2FunctionsTestCase {
  std::string expression;
  absl::StatusCode expected_status_code = absl::StatusCode::kOk;
  std::string expected_error;
};

class ComprehensionsV2FunctionsTest
    : public TestWithParam<ComprehensionsV2FunctionsTestCase> {
 protected:
  bool enable_optimizations_ = false;

 public:
  void SetUp() override {
    RuntimeOptions options;
    options.enable_qualified_type_identifiers = true;
    options.enable_comprehension_list_append = enable_optimizations_;
    options.enable_comprehension_mutable_map = enable_optimizations_;
    ASSERT_OK_AND_ASSIGN(auto builder,
                         CreateStandardRuntimeBuilder(
                             internal::GetTestingDescriptorPool(), options));
    ASSERT_THAT(RegisterStringsFunctions(builder.function_registry(), options),
                IsOk());
    ASSERT_THAT(
        RegisterComprehensionsV2Functions(builder.function_registry(), options),
        IsOk());
    ASSERT_THAT(EnableOptionalTypes(builder), IsOk());
    ASSERT_THAT(
        EnableReferenceResolver(builder, ReferenceResolverEnabled::kAlways),
        IsOk());
    ASSERT_OK_AND_ASSIGN(runtime_, std::move(builder).Build());
  }

  absl::StatusOr<cel::expr::ParsedExpr> Parse(absl::string_view text) {
    CEL_ASSIGN_OR_RETURN(auto source, NewSource(text));

    ParserOptions options;
    options.enable_optional_syntax = true;

    MacroRegistry registry;
    CEL_RETURN_IF_ERROR(RegisterStandardMacros(registry, options));
    CEL_RETURN_IF_ERROR(RegisterComprehensionsV2Macros(registry, options));
    CEL_RETURN_IF_ERROR(RegisterBindingsMacros(registry, options));

    CEL_ASSIGN_OR_RETURN(auto result,
                         EnrichedParse(*source, registry, options));
    return result.parsed_expr();
  }

  void RunTest(const ComprehensionsV2FunctionsTestCase& test_case) {
    ASSERT_OK_AND_ASSIGN(auto ast, Parse(test_case.expression));
    ASSERT_OK_AND_ASSIGN(auto program,
                         ProtobufRuntimeAdapter::CreateProgram(*runtime_, ast));
    google::protobuf::Arena arena;
    Activation activation;
    if (test_case.expected_status_code == absl::StatusCode::kOk) {
      EXPECT_THAT(program->Evaluate(&arena, activation),
                  IsOkAndHolds(BoolValueIs(true)))
          << test_case.expression;
    } else {
      EXPECT_THAT(
          program->Evaluate(&arena, activation),
          IsOkAndHolds(ErrorValueIs(StatusIs(test_case.expected_status_code,
                                             test_case.expected_error))))
          << test_case.expression;
    }
  }

 protected:
  std::unique_ptr<const Runtime> runtime_;
};

class ComprehensionsV2FunctionsTestWithOptimizations
    : public ComprehensionsV2FunctionsTest {
 public:
  ComprehensionsV2FunctionsTestWithOptimizations()
      : ComprehensionsV2FunctionsTest() {
    enable_optimizations_ = true;
  }
};

TEST_P(ComprehensionsV2FunctionsTest, Basic) { RunTest(GetParam()); }

TEST_P(ComprehensionsV2FunctionsTestWithOptimizations, Optimized) {
  RunTest(GetParam());
}

std::vector<ComprehensionsV2FunctionsTestCase> GetTestCases() {
  return std::vector<ComprehensionsV2FunctionsTestCase>({
      // list.all()
      {.expression = "[1, 2, 3, 4].all(i, v, i < 5 && v > 0)"},
      {.expression = "[1, 2, 3, 4].all(i, v, i < v)"},
      {.expression = "[1, 2, 3, 4].all(i, v, i > v) == false"},
      {
          .expression =
              R"cel(cel.bind(listA, [1, 2, 3, 4], cel.bind(listB, [1, 2, 3, 4, 5], listA.all(i, v, listB[?i].hasValue() && listB[i] == v))))cel",
      },
      {
          .expression =
              R"cel(cel.bind(listA, [1, 2, 3, 4, 5, 6], cel.bind(listB, [1, 2, 3, 4, 5], listA.all(i, v, listB[?i].hasValue() && listB[i] == v))) == false)cel",
      },
      // list.exists()
      {
          .expression =
              R"cel(cel.bind(l, ['hello', 'world', 'hello!', 'worlds'], l.exists(i, v, v.startsWith('hello') && l[?(i+1)].optMap(next, next.endsWith('world')).orValue(false))))cel",
      },
      // list.existsOne()
      {
          .expression =
              R"cel(cel.bind(l, ['hello', 'world', 'hello!', 'worlds'], l.existsOne(i, v, v.startsWith('hello') && l[?(i+1)].optMap(next, next.endsWith('world')).orValue(false))))cel",
      },
      {
          .expression =
              R"cel(cel.bind(l, ['hello', 'goodbye', 'hello!', 'goodbye'], l.existsOne(i, v, v.startsWith('hello') && l[?(i+1)].optMap(next, next == "goodbye").orValue(false))) == false)cel",
      },
      // list.transformList()
      {
          .expression =
              R"cel(['Hello', 'world'].transformList(i, v, "[" + string(i) + "]" + v.lowerAscii()) == ["[0]hello", "[1]world"])cel",
      },
      {
          .expression =
              R"cel(['hello', 'world'].transformList(i, v, v.startsWith('greeting'), "[" + string(i) + "]" + v) == [])cel",
      },
      {
          .expression =
              R"cel([1, 2, 3].transformList(indexVar, valueVar, (indexVar * valueVar) + valueVar) == [1, 4, 9])cel",
      },
      {
          .expression =
              R"cel([1, 2, 3].transformList(indexVar, valueVar, indexVar % 2 == 0, (indexVar * valueVar) + valueVar) == [1, 9])cel",
      },
      // list.transformMap()
      {
          .expression =
              R"cel(['Hello', 'world'].transformMap(i, v, [v.lowerAscii()]) == {0: ['hello'], 1: ['world']})cel",
      },
      {
          .expression =
              R"cel([1, 2, 3].transformMap(indexVar, valueVar, (indexVar * valueVar) + valueVar) == {0: 1, 1: 4, 2: 9})cel",
      },
      {
          .expression =
              R"cel([1, 2, 3].transformMap(indexVar, valueVar, indexVar % 2 == 0, (indexVar * valueVar) + valueVar) == {0: 1, 2: 9})cel",
      },
      // map.all()
      {
          .expression =
              R"cel({'hello': 'world', 'hello!': 'world'}.all(k, v, k.startsWith('hello') && v == 'world'))cel",
      },
      {
          .expression =
              R"cel({'hello': 'world', 'hello!': 'worlds'}.all(k, v, k.startsWith('hello') && v.endsWith('world')) == false)cel",
      },
      // map.exists()
      {
          .expression =
              R"cel({'hello': 'world', 'hello!': 'worlds'}.exists(k, v, k.startsWith('hello') && v.endsWith('world')))cel",
      },
      // map.existsOne()
      {
          .expression =
              R"cel({'hello': 'world', 'hello!': 'worlds'}.existsOne(k, v, k.startsWith('hello') && v.endsWith('world')))cel",
      },
      {
          .expression =
              R"cel({'hello': 'world', 'hello!': 'wow, world'}.existsOne(k, v, k.startsWith('hello') && v.endsWith('world')) == false)cel",
      },
      // map.transformList()
      {
          .expression =
              R"cel({'Hello': 'world'}.transformList(k, v, k.lowerAscii() + "=" + v) == ["hello=world"])cel",
      },
      {
          .expression =
              R"cel({'hello': 'world'}.transformList(k, v, k.startsWith('greeting'), k + "=" + v) == [])cel",
      },
      {
          .expression =
              R"cel(cel.bind(m, {'farewell': 'goodbye', 'greeting': 'hello'}.transformList(k, _, k), m == ['farewell', 'greeting'] || m == ['greeting', 'farewell']))cel",
      },
      {
          .expression =
              R"cel(cel.bind(m, {'greeting': 'hello', 'farewell': 'goodbye'}.transformList(_, v, v), m == ['goodbye', 'hello'] || m == ['hello', 'goodbye']))cel",
      },
      // map.transformMap()
      {
          .expression =
              R"cel({'hello': 'world', 'goodbye': 'cruel world'}.transformMap(k, v, k + ", " + v + "!") == {'hello': 'hello, world!', 'goodbye': 'goodbye, cruel world!'})cel",
      },
      {
          .expression =
              R"cel({'hello': 'world', 'goodbye': 'cruel world'}.transformMap(k, v, v.startsWith('world'), k + ", " + v + "!") == {'hello': 'hello, world!'})cel",
      },
      // map.transformMapEntry
      {
          .expression =
              R"cel({'hello': 'world', 'greetings': 'tacocat'}.transformMapEntry(k, v, {v: k}) == {'world': 'hello', 'tacocat': 'greetings'})cel",
      },
      {
          .expression =
              R"cel({'hello': 'world', 'same': 'same'}.transformMapEntry(k, v, k != v, {v: k}) == {'world': 'hello'})cel",
      },
      {
          .expression =
              R"cel({'hello': 'world', 'greetings': 'tacocat'}.transformMapEntry(k, v, {}) == {})cel",
      },
      {
          .expression =
              R"cel({'a': 'same', 'c': 'same'}.transformMapEntry(k, v, {v: k}))cel",
          .expected_status_code = absl::StatusCode::kAlreadyExists,
          .expected_error = "duplicate key in map",
      },
      // list.transformMapEntry
      {
          .expression =
              R"cel(['one', 'two'].transformMapEntry(k, v, {k + 1: 'is ' + v}) == {1: 'is one', 2: 'is two'})cel",
      },
  });
};

INSTANTIATE_TEST_SUITE_P(ComprehensionsV2FunctionsTest,
                         ComprehensionsV2FunctionsTest,
                         ::testing::ValuesIn(GetTestCases()));

INSTANTIATE_TEST_SUITE_P(ComprehensionsV2FunctionsTest,
                         ComprehensionsV2FunctionsTestWithOptimizations,
                         ::testing::ValuesIn(GetTestCases()));

struct ComprehensionsV2FunctionsTestCaseMutableAccumulator {
  std::string expression;
  bool enable_optimizations;
  int max_recursion_depth = 0;
  bool expected_mutable_accumulator;
};

using ComprehensionsV2FunctionsTestMutableAccumulator =
    ::testing::TestWithParam<
        ComprehensionsV2FunctionsTestCaseMutableAccumulator>;

TEST_P(ComprehensionsV2FunctionsTestMutableAccumulator, MutableAccumulator) {
  ASSERT_OK_AND_ASSIGN(auto source, NewSource(GetParam().expression));

  RuntimeOptions options;
  options.enable_comprehension_list_append = GetParam().enable_optimizations;
  options.enable_comprehension_mutable_map = GetParam().enable_optimizations;
  options.max_recursion_depth = GetParam().max_recursion_depth;

  MacroRegistry registry;
  ASSERT_THAT(RegisterComprehensionsV2Macros(registry, ParserOptions()),
              IsOk());

  ASSERT_OK_AND_ASSIGN(auto parsed_expr,
                       EnrichedParse(*source, registry, ParserOptions()));

  ASSERT_OK_AND_ASSIGN(auto builder,
                       CreateStandardRuntimeBuilder(
                           internal::GetTestingDescriptorPool(), options));
  ASSERT_THAT(
      RegisterComprehensionsV2Functions(builder.function_registry(), options),
      IsOk());
  ASSERT_OK_AND_ASSIGN(auto runtime, std::move(builder).Build());

  ASSERT_OK_AND_ASSIGN(auto program, ProtobufRuntimeAdapter::CreateProgram(
                                         *runtime, parsed_expr.parsed_expr()));

  google::protobuf::Arena arena;
  Activation activation;

  ASSERT_OK_AND_ASSIGN(auto result, program->Evaluate(&arena, activation));
  bool is_mutable_accumulator = common_internal::IsMutableListValue(result) ||
                                common_internal::IsMutableMapValue(result);
  EXPECT_EQ(is_mutable_accumulator, GetParam().expected_mutable_accumulator);
}

INSTANTIATE_TEST_SUITE_P(
    ComprehensionsV2FunctionsTest,
    ComprehensionsV2FunctionsTestMutableAccumulator,
    ::testing::ValuesIn<ComprehensionsV2FunctionsTestCaseMutableAccumulator>({
        // list.transformList()
        {
            .expression = R"cel(['Hello', 'world'].transformList(i, v, i))cel",
            .enable_optimizations = false,
            .expected_mutable_accumulator = false,
        },
        {
            .expression = R"cel(['Hello', 'world'].transformList(i, v, i))cel",
            .enable_optimizations = true,
            .expected_mutable_accumulator = true,
        },
        // map.transformMap()
        {
            .expression = R"cel({'hello': 'world'}.transformMap(k, v, k))cel",
            .enable_optimizations = false,
            .expected_mutable_accumulator = false,
        },
        {
            .expression = R"cel({'hello': 'world'}.transformMap(k, v, k))cel",
            .enable_optimizations = true,
            .expected_mutable_accumulator = true,
        },
        {
            .expression = R"cel({'hello': 'world'}.transformMap(k, v, k))cel",
            .enable_optimizations = true,
            .max_recursion_depth = -1,
            .expected_mutable_accumulator = true,
        },
        // list.transformMap()
        {
            .expression = R"cel(['hello'].transformMap(k, v, k))cel",
            .enable_optimizations = false,
            .expected_mutable_accumulator = false,
        },
        {
            .expression = R"cel(['hello'].transformMap(k, v, k))cel",
            .enable_optimizations = true,
            .expected_mutable_accumulator = true,
        },
        {
            .expression = R"cel(['hello'].transformMap(k, v, k))cel",
            .enable_optimizations = true,
            .max_recursion_depth = -1,
            .expected_mutable_accumulator = true,
        },
        // map.transformMapEntry()
        {
            .expression =
                R"cel({'hello': 'world'}.transformMapEntry(k, v, {v: k}))cel",
            .enable_optimizations = false,
            .expected_mutable_accumulator = false,
        },
        {
            .expression =
                R"cel({'hello': 'world'}.transformMapEntry(k, v, {v: k}))cel",
            .enable_optimizations = true,
            .expected_mutable_accumulator = true,
        },
        {
            .expression =
                R"cel({'hello': 'world'}.transformMapEntry(k, v, {v: k}))cel",
            .enable_optimizations = true,
            .max_recursion_depth = -1,
            .expected_mutable_accumulator = true,
        },
        // list.transformMapEntry()
        {
            .expression = R"cel(['hello'].transformMapEntry(k, v, {v: k}))cel",
            .enable_optimizations = false,
            .expected_mutable_accumulator = false,
        },
        {
            .expression = R"cel(['hello'].transformMapEntry(k, v, {v: k}))cel",
            .enable_optimizations = true,
            .expected_mutable_accumulator = true,
        },
        {
            .expression = R"cel(['hello'].transformMapEntry(k, v, {v: k}))cel",
            .enable_optimizations = true,
            .max_recursion_depth = -1,
            .expected_mutable_accumulator = true,
        },
    }));

}  // namespace
}  // namespace cel::extensions
