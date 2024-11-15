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

#include "compiler/compiler_factory.h"

#include <memory>
#include <utility>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "checker/optional.h"
#include "checker/standard_library.h"
#include "checker/type_check_issue.h"
#include "checker/validation_result.h"
#include "common/decl.h"
#include "common/type.h"
#include "compiler/compiler.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "parser/macro.h"
#include "parser/parser_interface.h"
#include "testutil/baseline_tests.h"
#include "google/protobuf/descriptor.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::cel::test::FormatBaselineAst;
using ::testing::Contains;
using ::testing::HasSubstr;
using ::testing::Property;

TEST(CompilerFactoryTest, Works) {
  ASSERT_OK_AND_ASSIGN(
      auto builder,
      NewCompilerBuilder(cel::internal::GetSharedTestingDescriptorPool()));

  ASSERT_THAT(builder->AddLibrary(StandardCheckerLibrary()), IsOk());

  ASSERT_OK_AND_ASSIGN(auto compiler, std::move(*builder).Build());

  ASSERT_OK_AND_ASSIGN(
      ValidationResult result,
      compiler->Compile("['a', 'b', 'c'].exists(x, x in ['c', 'd', 'e']) && 10 "
                        "< (5 % 3 * 2 + 1 - 2)"));

  ASSERT_TRUE(result.IsValid());

  EXPECT_EQ(FormatBaselineAst(*result.GetAst()),
            R"(_&&_(
  __comprehension__(
    // Variable
    x,
    // Target
    [
      "a"~string,
      "b"~string,
      "c"~string
    ]~list(string),
    // Accumulator
    __result__,
    // Init
    false~bool,
    // LoopCondition
    @not_strictly_false(
      !_(
        __result__~bool^__result__
      )~bool^logical_not
    )~bool^not_strictly_false,
    // LoopStep
    _||_(
      __result__~bool^__result__,
      @in(
        x~string^x,
        [
          "c"~string,
          "d"~string,
          "e"~string
        ]~list(string)
      )~bool^in_list
    )~bool^logical_or,
    // Result
    __result__~bool^__result__)~bool,
  _<_(
    10~int,
    _-_(
      _+_(
        _*_(
          _%_(
            5~int,
            3~int
          )~int^modulo_int64,
          2~int
        )~int^multiply_int64,
        1~int
      )~int^add_int64,
      2~int
    )~int^subtract_int64
  )~bool^less_int64
)~bool^logical_and)");
}

TEST(CompilerFactoryTest, ParserLibrary) {
  ASSERT_OK_AND_ASSIGN(
      auto builder,
      NewCompilerBuilder(cel::internal::GetSharedTestingDescriptorPool()));

  ASSERT_THAT(
      builder->AddLibrary({"test",
                           [](ParserBuilder& builder) -> absl::Status {
                             builder.GetOptions().disable_standard_macros =
                                 true;
                             return builder.AddMacro(cel::HasMacro());
                           }}),
      IsOk());

  ASSERT_THAT(builder->GetCheckerBuilder().AddVariable(
                  MakeVariableDecl("a", MapType())),
              IsOk());

  ASSERT_OK_AND_ASSIGN(auto compiler, std::move(*builder).Build());

  ASSERT_THAT(compiler->Compile("has(a.b)"), IsOk());

  ASSERT_OK_AND_ASSIGN(ValidationResult result,
                       compiler->Compile("[].map(x, x)"));

  EXPECT_FALSE(result.IsValid());
  EXPECT_THAT(result.GetIssues(),
              Contains(Property(&TypeCheckIssue::message,
                                HasSubstr("undeclared reference to 'map'"))))
      << result.GetIssues()[2].message();
}

TEST(CompilerFactoryTest, ParserOptions) {
  ASSERT_OK_AND_ASSIGN(
      auto builder,
      NewCompilerBuilder(cel::internal::GetSharedTestingDescriptorPool()));

  builder->GetParserBuilder().GetOptions().enable_optional_syntax = true;
  ASSERT_THAT(builder->AddLibrary(OptionalCheckerLibrary()), IsOk());

  ASSERT_THAT(builder->GetCheckerBuilder().AddVariable(
                  MakeVariableDecl("a", MapType())),
              IsOk());

  ASSERT_OK_AND_ASSIGN(auto compiler, std::move(*builder).Build());

  ASSERT_THAT(compiler->Compile("a.?b.orValue('foo')"), IsOk());
}

TEST(CompilerFactoryTest, FailsIfLibraryAddedTwice) {
  ASSERT_OK_AND_ASSIGN(
      auto builder,
      NewCompilerBuilder(cel::internal::GetSharedTestingDescriptorPool()));

  ASSERT_THAT(builder->AddLibrary(StandardCheckerLibrary()), IsOk());
  ASSERT_THAT(builder->AddLibrary(StandardCheckerLibrary()),
              StatusIs(absl::StatusCode::kAlreadyExists,
                       HasSubstr("library already exists: stdlib")));
}

TEST(CompilerFactoryTest, FailsIfNullDescriptorPool) {
  std::shared_ptr<const google::protobuf::DescriptorPool> pool =
      internal::GetSharedTestingDescriptorPool();
  pool.reset();
  ASSERT_THAT(
      NewCompilerBuilder(std::move(pool)),
      absl_testing::StatusIs(absl::StatusCode::kInvalidArgument,
                             HasSubstr("descriptor_pool must not be null")));
}

}  // namespace
}  // namespace cel