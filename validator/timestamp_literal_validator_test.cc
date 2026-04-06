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

#include "validator/timestamp_literal_validator.h"

#include <memory>
#include <string>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "checker/validation_result.h"
#include "compiler/compiler.h"
#include "compiler/compiler_factory.h"
#include "compiler/standard_library.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "validator/validator.h"

namespace cel {
namespace {

using ::testing::HasSubstr;

absl::StatusOr<std::unique_ptr<Compiler>> StdLibCompiler() {
  auto builder =
      NewCompilerBuilder(internal::GetSharedTestingDescriptorPool()).value();
  builder->AddLibrary(StandardCompilerLibrary()).IgnoreError();
  return builder->Build();
}

class TimestampLiteralValidatorTest : public ::testing::Test {
 protected:
  TimestampLiteralValidatorTest() {
    validator_.AddValidation(TimestampLiteralValidator());
  }

  std::unique_ptr<Compiler> compiler_;
  Validator validator_;
};

TEST(TimestampLiteralValidatorTest, FormatsIssues) {
  Validator validator;
  validator.AddValidation(TimestampLiteralValidator());

  ASSERT_OK_AND_ASSIGN(auto compiler, StdLibCompiler());
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult result,
                       compiler->Compile("timestamp('invalid')"));

  validator.UpdateValidationResult(result);

  EXPECT_FALSE(result.IsValid());
  EXPECT_EQ(result.FormatError(),
            R"(ERROR: <input>:1:11: invalid timestamp literal
 | timestamp('invalid')
 | ..........^)");
}

TEST(TimestampLiteralValidatorTest, AccumulatesIssues) {
  Validator validator;
  validator.AddValidation(TimestampLiteralValidator());
  validator.AddValidation(DurationLiteralValidator());

  constexpr absl::string_view kExpression = R"cel(
    [ timestamp('invalid'),
      timestamp('9999-12-31T23:59:59Z'),
      timestamp('10000-01-01T00:00:00Z')
    ].all(t,
      t - timestamp(0) < duration('10000s') &&
      t - timestamp(0) > duration("invalid")
    ))cel";
  ASSERT_OK_AND_ASSIGN(auto compiler, StdLibCompiler());
  ASSERT_OK_AND_ASSIGN(cel::ValidationResult result,
                       compiler->Compile(kExpression));

  validator.UpdateValidationResult(result);

  EXPECT_FALSE(result.IsValid());
  EXPECT_THAT(result.FormatError(),
              AllOf(HasSubstr("2:17: invalid timestamp literal"),
                    HasSubstr("4:17: invalid timestamp literal"),
                    HasSubstr("7:35: invalid duration literal")));
}

struct TestCase {
  std::string expression;
  bool valid;
  std::string error_substr = "";
};

using TimestampLiteralValidatorParameterizedTest =
    testing::TestWithParam<TestCase>;

TEST_P(TimestampLiteralValidatorParameterizedTest, Validate) {
  const auto& test_case = GetParam();
  Validator validator;
  validator.AddValidation(TimestampLiteralValidator());
  validator.AddValidation(DurationLiteralValidator());

  ASSERT_OK_AND_ASSIGN(auto compiler, StdLibCompiler());
  ASSERT_OK_AND_ASSIGN(auto result, compiler->Compile(test_case.expression));
  validator.UpdateValidationResult(result);

  EXPECT_EQ(result.IsValid(), test_case.valid);
  if (!test_case.valid) {
    EXPECT_THAT(result.FormatError(), HasSubstr(test_case.error_substr));
  }
}

INSTANTIATE_TEST_SUITE_P(
    TimestampLiteralValidatorParameterizedTest,
    TimestampLiteralValidatorParameterizedTest,
    ::testing::Values(
        TestCase{"timestamp('2023-01-01T00:00:00Z')", true},
        TestCase{"timestamp('9999-12-31T23:59:59Z')", true},
        TestCase{"timestamp('invalid')", false, "invalid timestamp literal"},
        TestCase{"timestamp('10000-01-01T00:00:00Z')", false,
                 "invalid timestamp literal"},
        TestCase{"timestamp(0)", true},
        TestCase{"timestamp(-62135596801)", false,
                 "invalid timestamp literal: Timestamp \"0-12-31T23:59:59Z\" "
                 "below minimum allowed timestamp \"1-01-01T00:00:00Z\""},
        TestCase{"timestamp(253402300800)", false,
                 "invalid timestamp literal: Timestamp "
                 "\"10000-01-01T00:00:00Z\" above maximum allowed timestamp "
                 "\"9999-12-31T23:59:59.999999999Z\""},
        TestCase{"duration('1s')", true},
        TestCase{"duration('invalid')", false, "invalid duration literal"},
        TestCase{"duration('-1000000000000s')", false,
                 "below minimum allowed duration"},
        TestCase{"duration('1000000000000s')", false,
                 "above maximum allowed duration"}));

}  // namespace
}  // namespace cel
