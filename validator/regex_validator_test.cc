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

#include "validator/regex_validator.h"

#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "common/decl.h"
#include "common/type.h"
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
  CEL_ASSIGN_OR_RETURN(
      auto builder,
      NewCompilerBuilder(internal::GetSharedTestingDescriptorPool()));
  builder->AddLibrary(StandardCompilerLibrary()).IgnoreError();
  CEL_RETURN_IF_ERROR(builder->GetCheckerBuilder().AddVariable(
      MakeVariableDecl("p", StringType())));
  return builder->Build();
}

struct TestCase {
  std::string expression;
  bool valid;
  std::string error_substr = "";
};

using MatchesValidatorTest = testing::TestWithParam<TestCase>;

TEST_P(MatchesValidatorTest, Validate) {
  const auto& test_case = GetParam();
  Validator validator;
  validator.AddValidation(MatchesValidator());

  ASSERT_OK_AND_ASSIGN(auto compiler, StdLibCompiler());
  ASSERT_OK_AND_ASSIGN(auto result, compiler->Compile(test_case.expression));

  validator.UpdateValidationResult(result);

  EXPECT_EQ(result.IsValid(), test_case.valid)
      << "Expression: " << test_case.expression;
  if (!test_case.valid) {
    EXPECT_THAT(result.FormatError(), HasSubstr(test_case.error_substr));
  }
}

INSTANTIATE_TEST_SUITE_P(
    MatchesValidatorTest, MatchesValidatorTest,
    testing::Values(
        // Member calls
        TestCase{"'hello'.matches('h.*')", true},
        TestCase{"'hello'.matches('h[')", false, "invalid regular expression"},
        TestCase{"'hello'.matches('h(a|b)')", true},
        TestCase{"'hello'.matches('h(a|b')", false,
                 "invalid regular expression"},
        // Global calls
        TestCase{"matches('hello', 'h.*')", true},
        TestCase{"matches('hello', 'h[')", false, "invalid regular expression"},
        // Non-literal patterns (should not report regex errors)
        TestCase{"'hello'.matches(p)", true},
        TestCase{"'hello'.matches('h' + 'ello')", true},
        TestCase{"'hello'.matches(dyn(1))", true},

        // Empty pattern
        TestCase{"'hello'.matches('')", true}));

}  // namespace
}  // namespace cel
