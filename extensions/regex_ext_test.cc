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

#include "extensions/regex_ext.h"

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "extensions/protobuf/runtime_adapter.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "parser/parser.h"
#include "runtime/activation.h"
#include "runtime/optional_types.h"
#include "runtime/reference_resolver.h"
#include "runtime/runtime.h"
#include "runtime/runtime_builder.h"
#include "runtime/runtime_options.h"
#include "runtime/standard_runtime_builder_factory.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/extension_set.h"

namespace cel::extensions {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::cel::test::ErrorValueIs;
using ::google::api::expr::parser::Parse;
using test::BoolValueIs;
using test::OptionalValueIs;
using test::OptionalValueIsEmpty;
using test::StringValueIs;
using ::testing::HasSubstr;
using ::testing::TestWithParam;
using ::testing::ValuesIn;

enum class EvaluationType {
  kBoolTrue,
  kOptionalValue,
  kOptionalNone,
  kRuntimeError,
  kUnknownStaticError,
  kInvalidArgStaticError
};

struct RegexExtTestCase {
  EvaluationType evaluation_type;
  std::string expr;
  std::string expected_result = "";
};

class RegexExtTest : public TestWithParam<RegexExtTestCase> {
 public:
  void SetUp() override {
    RuntimeOptions options;
    options.enable_regex = true;
    options.enable_qualified_type_identifiers = true;

    ASSERT_OK_AND_ASSIGN(auto builder,
                         CreateStandardRuntimeBuilder(
                             internal::GetTestingDescriptorPool(), options));
    ASSERT_THAT(
        EnableReferenceResolver(builder, ReferenceResolverEnabled::kAlways),
        IsOk());
    ASSERT_THAT(EnableOptionalTypes(builder), IsOk());
    ASSERT_THAT(
        RegisterRegexExtensionFunctions(builder.function_registry(), options),
        IsOk());
    ASSERT_OK_AND_ASSIGN(runtime_, std::move(builder).Build());
  }

  absl::StatusOr<Value> TestEvaluate(const std::string& expr_string) {
    CEL_ASSIGN_OR_RETURN(auto parsed_expr, Parse(expr_string));
    CEL_ASSIGN_OR_RETURN(std::unique_ptr<cel::Program> program,
                         cel::extensions::ProtobufRuntimeAdapter::CreateProgram(
                             *runtime_, parsed_expr));
    Activation activation;
    return program->Evaluate(&arena_, activation);
  }

  google::protobuf::Arena arena_;
  std::unique_ptr<const Runtime> runtime_;
};

std::vector<RegexExtTestCase> regexTestCases() {
  return {
      // Tests for extract Function
      {EvaluationType::kOptionalValue,
       R"(regex.extract('hello world', 'hello (.*)'))", "world"},
      {EvaluationType::kOptionalValue,
       R"(regex.extract('item-A, item-B', r'item-(\w+)'))", "A"},
      {EvaluationType::kOptionalValue,
       R"(regex.extract('The color is red', r'The color is (\w+)'))", "red"},
      {EvaluationType::kOptionalValue,
       R"(regex.extract('The color is red', r'The color is \w+'))",
       "The color is red"},
      {EvaluationType::kOptionalValue, "regex.extract('brand', 'brand')",
       "brand"},
      {EvaluationType::kOptionalNone,
       "regex.extract('hello world', 'goodbye (.*)')"},
      {EvaluationType::kOptionalNone, "regex.extract('HELLO', 'hello')"},
      {EvaluationType::kOptionalNone, R"(regex.extract('', r'\w+'))"},

      // Tests for extractAll Function
      {EvaluationType::kBoolTrue,
       "regex.extractAll('id:123, id:456', 'assa') == []"},
      {EvaluationType::kBoolTrue,
       R"(regex.extractAll('id:123, id:456', r'id:\d+') == ['id:123','id:456'])"},
      {EvaluationType::kBoolTrue,
       R"(regex.extractAll('Files: f_1.txt, f_2.csv', r'f_(\d+)')==['1','2'])"},
      {EvaluationType::kBoolTrue,
       R"(regex.extractAll('testuser@', '(?P<username>.*)@') == ['testuser'])"},
      {EvaluationType::kBoolTrue,
       R"cel(regex.extractAll('t@gmail.com, a@y.com, 22@sdad.com',
          '(?P<username>.*)@') == ['t@gmail.com, a@y.com, 22'])cel"},
      {EvaluationType::kBoolTrue,
       R"cel(regex.extractAll('t@gmail.com, a@y.com, 22@sdad.com',
          r'(?P<username>\w+)@') == ['t','a', '22'])cel"},
      {EvaluationType::kBoolTrue,
       "regex.extractAll('banananana', '(ana)') == ['ana', 'ana']"},
      {EvaluationType::kBoolTrue,
       R"(regex.extractAll('item:a1, topic:b2',
          r'(?:item:|topic:)([a-z]\d)') == ['a1', 'b2'])"},
      {EvaluationType::kBoolTrue,
       R"(regex.extractAll('val=a, val=, val=c', 'val=([^,]*)')==['a','c'])"},
      {EvaluationType::kBoolTrue,
       "regex.extractAll('key=, key=, key=', 'key=([^,]*)') == []"},
      {EvaluationType::kBoolTrue,
       R"(regex.extractAll('a b c', r'(\S*)\s*') == ['a', 'b', 'c'])"},
      {EvaluationType::kBoolTrue,
       "regex.extractAll('abc', 'a|b*') == ['a','b']"},
      {EvaluationType::kBoolTrue,
       "regex.extractAll('abc', 'a|(b)|c*') == ['b']"},

      // Tests for replace Function
      {EvaluationType::kBoolTrue,
       "regex.replace('abc', '$', '_end') == 'abc_end'"},
      {EvaluationType::kBoolTrue,
       R"(regex.replace('a-b', r'\b', '|') == '|a|-|b|')"},
      {EvaluationType::kBoolTrue,
       R"(regex.replace('foo bar', '(fo)o (ba)r', r'\2 \1') == 'ba fo')"},
      {EvaluationType::kBoolTrue,
       R"(regex.replace('foo bar', 'foo', r'\\') == '\\ bar')"},
      {EvaluationType::kBoolTrue,
       "regex.replace('banana', 'ana', 'x') == 'bxna'"},
      {EvaluationType::kBoolTrue,
       R"(regex.replace('abc', 'b(.)', r'x\1') == 'axc')"},
      {EvaluationType::kBoolTrue,
       "regex.replace('hello world hello', 'hello', 'hi') == 'hi world hi'"},
      {EvaluationType::kBoolTrue,
       R"(regex.replace('ac', 'a(b)?c', r'[\1]') == '[]')"},
      {EvaluationType::kBoolTrue,
       "regex.replace('apple pie', 'p', 'X') == 'aXXle Xie'"},
      {EvaluationType::kBoolTrue,
       R"(regex.replace('remove all spaces', r'\s', '') ==
      'removeallspaces')"},
      {EvaluationType::kBoolTrue,
       R"(regex.replace('digit:99919291992', r'\d+', '3') == 'digit:3')"},
      {EvaluationType::kBoolTrue,
       R"cel(regex.replace('foo bar baz', r'\w+', r'(\0)') ==
      '(foo) (bar) (baz)')cel"},
      {EvaluationType::kBoolTrue, "regex.replace('', 'a', 'b') == ''"},
      {EvaluationType::kBoolTrue,
       R"cel(regex.replace('User: Alice, Age: 30',
      r'User: (?P<name>\w+), Age: (?P<age>\d+)',
      '${name} is ${age} years old') == '${name} is ${age} years old')cel"},
      {EvaluationType::kBoolTrue,
       R"cel(regex.replace('User: Alice, Age: 30',
      r'User: (?P<name>\w+), Age: (?P<age>\d+)', r'\1 is \2 years old') ==
      'Alice is 30 years old')cel"},
      {EvaluationType::kBoolTrue,
       "regex.replace('hello ☃', '☃', '❄') == 'hello ❄'"},
      {EvaluationType::kBoolTrue,
       R"(regex.replace('id=123', r'id=(?P<value>\d+)', r'value: \1') ==
      'value: 123')"},
      {EvaluationType::kBoolTrue,
       "regex.replace('banana', 'a', 'x') == 'bxnxnx'"},
      {EvaluationType::kBoolTrue,
       R"(regex.replace(regex.replace('%(foo) %(bar) %2', r'%\((\w+)\)',
      r'${\1}'),r'%(\d+)', r'$\1') == '${foo} ${bar} $2')"},
      {EvaluationType::kBoolTrue,
       R"(regex.replace('abc def', r'(abc)', r'\\1') == r'\1 def')"},
      {EvaluationType::kBoolTrue,
       R"(regex.replace('abc def', r'(abc)', r'\\2') == r'\2 def')"},
      {EvaluationType::kBoolTrue,
       R"(regex.replace('abc def', r'(abc)', r'\\{word}') == '\\{word} def')"},
      {EvaluationType::kBoolTrue,
       R"(regex.replace('abc def', r'(abc)', r'\\word') == '\\word def')"},
      {EvaluationType::kBoolTrue,
       "regex.replace('abc', '^', 'start_') == 'start_abc'"},

      // Tests for replace Function with count variable
      {EvaluationType::kBoolTrue,
       R"(regex.replace('foofoo', 'foo', 'bar',
       9223372036854775807) == 'barbar')"},
      {EvaluationType::kBoolTrue,
       "regex.replace('banana', 'a', 'x', 0) == 'banana'"},
      {EvaluationType::kBoolTrue,
       "regex.replace('banana', 'a', 'x', 1) == 'bxnana'"},
      {EvaluationType::kBoolTrue,
       "regex.replace('banana', 'a', 'x', 2) == 'bxnxna'"},
      {EvaluationType::kBoolTrue,
       "regex.replace('banana', 'a', 'x', 100) == 'bxnxnx'"},
      {EvaluationType::kBoolTrue,
       "regex.replace('banana', 'a', 'x', -1) == 'bxnxnx'"},
      {EvaluationType::kBoolTrue,
       "regex.replace('banana', 'a', 'x', -100) == 'bxnxnx'"},
      {EvaluationType::kBoolTrue,
       R"cel(regex.replace('cat-dog dog-cat cat-dog dog-cat', '(cat)-(dog)',
      r'\2-\1', 1) == 'dog-cat dog-cat cat-dog dog-cat')cel"},
      {EvaluationType::kBoolTrue,
       R"cel(regex.replace('cat-dog dog-cat cat-dog dog-cat', '(cat)-(dog)',
      r'\2-\1', 2) == 'dog-cat dog-cat dog-cat dog-cat')cel"},
      {EvaluationType::kBoolTrue,
       R"(regex.replace('a.b.c', r'\.', '-', 1) == 'a-b.c')"},
      {EvaluationType::kBoolTrue,
       R"(regex.replace('a.b.c', r'\.', '-', -1) == 'a-b-c')"},
      {EvaluationType::kBoolTrue,
       R"(regex.replace('123456789ABC',
       '(\\d)(\\d)(\\d)(\\d)(\\d)(\\d)(\\d)(\\d)(\\d)(\\w)(\\w)(\\w)','X', 1)
       == 'X')"},
      {EvaluationType::kBoolTrue,
       R"(regex.replace('123456789ABC',
       '(\\d)(\\d)(\\d)(\\d)(\\d)(\\d)(\\d)(\\d)(\\d)(\\w)(\\w)(\\w)',
       r'\1-\9-X', 1) == '1-9-X')"},

      // Static Errors
      {EvaluationType::kUnknownStaticError, "regex.replace('abc', '^', 1)",
       "No matching overloads found : regex.replace(string, string, int64)"},
      {EvaluationType::kUnknownStaticError, "regex.replace('abc', '^', '1','')",
       "No matching overloads found : regex.replace(string, string, string, "
       "string)"},
      {EvaluationType::kUnknownStaticError, "regex.extract('foo bar', 1)",
       "No matching overloads found : regex.extract(string, int64)"},
      {EvaluationType::kInvalidArgStaticError,
       "regex.extract('foo bar', 1, 'bar')",
       "No overload found in reference resolve step for extract"},
      {EvaluationType::kInvalidArgStaticError, "regex.extractAll()",
       "No overload found in reference resolve step for extractAll"},

      // Runtime Errors
      {EvaluationType::kRuntimeError, R"(regex.extract('foo', 'fo(o+)(abc'))",
       "given regex is invalid: missing ): fo(o+)(abc"},
      {EvaluationType::kRuntimeError, R"(regex.extractAll('foo bar', '[a-z'))",
       "given regex is invalid: missing ]: [a-z"},
      {EvaluationType::kRuntimeError,
       R"(regex.replace('foo bar', '[a-z', 'a'))",
       "given regex is invalid: missing ]: [a-z"},
      {EvaluationType::kRuntimeError,
       R"(regex.replace('foo bar', '[a-z', 'a', 1))",
       "given regex is invalid: missing ]: [a-z"},
      {EvaluationType::kRuntimeError,
       R"(regex.replace('id=123', r'id=(?P<value>\d+)', r'value: \values'))",
       R"(invalid replacement string: Rewrite schema error: '\' must be followed by a digit or '\'.)"},
      {EvaluationType::kRuntimeError, R"(regex.replace('test', '(t)', '\\2'))",
       "invalid replacement string: Rewrite schema requests 2 matches, but "
       "the regexp only has 1 parenthesized subexpressions"},
      {EvaluationType::kRuntimeError,
       R"(regex.replace('id=123', r'id=(?P<value>\d+)', '\\', 1))",
       R"(invalid replacement string: Rewrite schema error: '\' not allowed at end.)"},
      {EvaluationType::kRuntimeError,
       R"(regex.extract('phone: 415-5551212', r'phone: ((\d{3})-)?'))",
       R"(regular expression has more than one capturing group: phone: ((\d{3})-)?)"},
      {EvaluationType::kRuntimeError,
       R"(regex.extractAll('testuser@testdomain', '(.*)@([^.]*)'))",
       R"(regular expression has more than one capturing group: (.*)@([^.]*))"},
  };
}

TEST_P(RegexExtTest, RegexExtTests) {
  const RegexExtTestCase& test_case = GetParam();
  auto result = TestEvaluate(test_case.expr);

  switch (test_case.evaluation_type) {
    case EvaluationType::kRuntimeError:
      EXPECT_THAT(result, IsOkAndHolds(ErrorValueIs(
                              StatusIs(absl::StatusCode::kInvalidArgument,
                                       HasSubstr(test_case.expected_result)))))
          << "Expression: " << test_case.expr;
      break;
    case EvaluationType::kUnknownStaticError:
      EXPECT_THAT(result, IsOkAndHolds(ErrorValueIs(
                              StatusIs(absl::StatusCode::kUnknown,
                                       HasSubstr(test_case.expected_result)))))
          << "Expression: " << test_case.expr;
      break;
    case EvaluationType::kInvalidArgStaticError:
      EXPECT_THAT(result, StatusIs(absl::StatusCode::kInvalidArgument,
                                   HasSubstr(test_case.expected_result)))
          << "Expression: " << test_case.expr;
      break;
    case EvaluationType::kOptionalNone:
      EXPECT_THAT(result, IsOkAndHolds(OptionalValueIsEmpty()))
          << "Expression: " << test_case.expr;
      break;
    case EvaluationType::kOptionalValue:
      EXPECT_THAT(result, IsOkAndHolds(OptionalValueIs(
                              StringValueIs(test_case.expected_result))))
          << "Expression: " << test_case.expr;
      break;
    case EvaluationType::kBoolTrue:
      EXPECT_THAT(result, IsOkAndHolds(BoolValueIs(true)))
          << "Expression: " << test_case.expr;
      break;
  }
}

INSTANTIATE_TEST_SUITE_P(RegexExtTest, RegexExtTest,
                         ValuesIn(regexTestCases()));
}  // namespace
}  // namespace cel::extensions
