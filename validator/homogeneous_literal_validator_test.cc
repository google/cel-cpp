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

#include "validator/homogeneous_literal_validator.h"

#include <memory>
#include <string>

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "checker/validation_result.h"
#include "common/decl.h"
#include "common/type.h"
#include "compiler/compiler.h"
#include "compiler/compiler_factory.h"
#include "compiler/optional.h"
#include "compiler/standard_library.h"
#include "extensions/strings.h"
#include "internal/status_macros.h"
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
  builder->AddLibrary(OptionalCompilerLibrary()).IgnoreError();
  builder->AddLibrary(extensions::StringsCompilerLibrary()).IgnoreError();
  cel::Type message_type = cel::Type::Message(
      builder->GetCheckerBuilder().descriptor_pool()->FindMessageTypeByName(
          "cel.expr.conformance.proto3.TestAllTypes"));
  CEL_RETURN_IF_ERROR(builder->GetCheckerBuilder().AddVariable(
      MakeVariableDecl("msg", message_type)));
  return builder->Build();
}

struct TestCase {
  std::string expression;
  bool valid;
  std::string error_substr = "";
};

using HomogeneousLiteralValidatorTest = testing::TestWithParam<TestCase>;

TEST_P(HomogeneousLiteralValidatorTest, Validate) {
  const auto& test_case = GetParam();
  Validator validator;
  validator.AddValidation(HomogeneousLiteralValidator());

  ASSERT_OK_AND_ASSIGN(auto compiler, StdLibCompiler());
  ASSERT_OK_AND_ASSIGN(auto result, compiler->Compile(test_case.expression));
  validator.UpdateValidationResult(result);

  EXPECT_EQ(result.IsValid(), test_case.valid);
  if (!test_case.valid) {
    EXPECT_THAT(result.FormatError(), HasSubstr(test_case.error_substr));
  }
}

INSTANTIATE_TEST_SUITE_P(
    HomogeneousLiteralValidatorTest, HomogeneousLiteralValidatorTest,
    testing::Values(
        // Lists
        TestCase{"[1, 2, 3]", true}, TestCase{"['a', 'b', 'c']", true},
        TestCase{"[1, 'a']", false, "expected type 'int' but found 'string'"},
        TestCase{"[1, 2, 'a']", false,
                 "expected type 'int' but found 'string'"},
        TestCase{"[[1], [2]]", true},
        TestCase{"[[1], ['a']]", false,
                 "expected type 'list(int)' but found 'list(string)'"},

        // Dyn casts
        TestCase{"[dyn(1), dyn('a')]", true, ""},
        TestCase{"[dyn(1), 2]", false, "expected type 'dyn' but found 'int'"},

        // Maps
        TestCase{"{1: 'a', 2: 'b'}", true}, TestCase{"{'a': 1, 'b': 2}", true},
        TestCase{"{1: 'a', 'b': 2}", false,
                 "expected type 'int' but found 'string'"},
        TestCase{"{1: 'a', 2: 3}", false,
                 "expected type 'string' but found 'int'"},

        // Optionals
        TestCase{"[optional.of(1), optional.of(2)]", true},
        TestCase{"[optional.of(1), optional.of('b')]", false,
                 "expected type 'optional_type(int)' but found "
                 "'optional_type(string)'"},

        TestCase{"[?optional.of(1), ?optional.of(2)]", true},
        TestCase{"[?optional.of(1), ?optional.of('a')]", false,
                 "expected type 'int' but found 'string'"},
        TestCase{"{?1: optional.of('a'), ?2: optional.none()}", true},
        TestCase{"{?1: optional.of('a'), ?2: optional.of(1)}", false,
                 "expected type 'string' but found 'int'"},

        // Exempted Functions
        TestCase{"'%v %v'.format([1, 'a'])", true},

        // Mixed Primitives and Wrappers
        TestCase{"[1, msg.single_int64_wrapper]", true},
        TestCase{"[msg.single_int64_wrapper, 1]", true},
        TestCase{"['foo', msg.single_string_wrapper]", true},
        TestCase{"[msg.single_string_wrapper, 'foo']", true},
        TestCase{"{1: msg.single_int64_wrapper, 2: 3}", true},
        TestCase{"{1: 2, 2: msg.single_int64_wrapper}", true},
        TestCase{"[[1], [msg.single_int64_wrapper]]", true},
        TestCase{"[optional.of(1), optional.of(msg.single_int64_wrapper)]",
                 true},
        TestCase{"[1, msg.single_string_wrapper]", false,
                 "expected type 'int' but found 'wrapper(string)'"},
        TestCase{"[msg.single_int64_wrapper, 'foo']", false,
                 "expected type 'wrapper(int)' but found 'string'"},
        TestCase{"[msg.single_int64_wrapper, msg.single_string_wrapper]", false,
                 "expected type 'wrapper(int)' but found 'wrapper(string)'"},

        // Nested
        TestCase{"[1, [2, 'a']]", false,
                 "expected type 'int' but found 'string'"},
        TestCase{"[[1, 2], [3, 4]]", true, ""},
        TestCase{"[{1: 2}, {'foo': 3}]", false,
                 "expected type 'map(int, int)' but found 'map(string, int)'"},
        TestCase{"[{1: 2}, {3: 'foo'}]", false,
                 "expected type 'map(int, int)' but found 'map(int, string)'"},
        TestCase{"[{1: 2}, {3: 4}]", true, ""}));

}  // namespace
}  // namespace cel
