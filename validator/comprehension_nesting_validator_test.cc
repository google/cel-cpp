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

#include "validator/comprehension_nesting_validator.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/status/statusor.h"
#include "compiler/compiler.h"
#include "compiler/compiler_factory.h"
#include "compiler/standard_library.h"
#include "extensions/bindings_ext.h"
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
  CEL_RETURN_IF_ERROR(builder->AddLibrary(StandardCompilerLibrary()));
  CEL_RETURN_IF_ERROR(
      builder->AddLibrary(cel::extensions::BindingsCompilerLibrary()));
  return builder->Build();
}

struct TestCase {
  std::string expression;
  int limit;
  bool valid;
  std::string error_substr = "";
};

using ComprehensionNestingValidatorTest = testing::TestWithParam<TestCase>;

TEST_P(ComprehensionNestingValidatorTest, Validate) {
  const auto& test_case = GetParam();
  Validator validator;
  validator.AddValidation(ComprehensionNestingLimitValidator(test_case.limit));

  ASSERT_OK_AND_ASSIGN(auto compiler, StdLibCompiler());
  auto result_or = compiler->Compile(test_case.expression);
  if (!result_or.ok()) {
    GTEST_SKIP() << "Expression failed to compile: " << test_case.expression
                 << " " << result_or.status().message();
  }
  auto result = std::move(result_or).value();

  validator.UpdateValidationResult(result);

  EXPECT_EQ(result.IsValid(), test_case.valid)
      << "Expression: " << test_case.expression
      << " Limit: " << test_case.limit;
  if (!test_case.valid) {
    EXPECT_THAT(result.FormatError(), HasSubstr(test_case.error_substr));
  }
}

INSTANTIATE_TEST_SUITE_P(
    ComprehensionNestingValidatorTest, ComprehensionNestingValidatorTest,
    testing::Values(
        TestCase{"[1, 2].all(x, x > 0)", 1, true},
        TestCase{"[1, 2].all(x, [1, 2].all(y, x > y))", 1, false,
                 "comprehension nesting level of 2 exceeds limit of 1"},
        TestCase{"[1, 2].all(x, [1, 2].all(y, x > y))", 2, true},
        // Empty range comprehension (does not count)
        TestCase{"[].all(x, [1, 2].all(y, y > 0))", 1, true},
        TestCase{"cel.bind(x, [1, 2].all(y, y > 0), [1, 2].all(z, z > 0))", 1,
                 true},
        // Nested empty range comprehensions
        TestCase{"[].all(x, [].all(y, true))", 0, true},
        // Deeply nested mixed
        TestCase{"[1].all(x, [].all(y, [2].all(z, true)))", 1, false,
                 "comprehension nesting level of 2 exceeds limit of 1"},
        TestCase{"[1].all(x, [].all(y, [2].all(z, true)))", 2, true}));

}  // namespace
}  // namespace cel
