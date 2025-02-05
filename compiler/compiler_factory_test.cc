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
#include "checker/checker_options.h"
#include "checker/optional.h"
#include "checker/standard_library.h"
#include "checker/type_check_issue.h"
#include "checker/type_checker_builder.h"
#include "checker/validation_result.h"
#include "common/decl.h"
#include "common/type.h"
#include "compiler/compiler.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "parser/macro.h"
#include "parser/parser_interface.h"
#include "testutil/baseline_tests.h"
#include "google/protobuf/arena.h"
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
  builder->GetParserBuilder().GetOptions().enable_hidden_accumulator_var = true;
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
    @result,
    // Init
    false~bool,
    // LoopCondition
    @not_strictly_false(
      !_(
        @result~bool^@result
      )~bool^logical_not
    )~bool^not_strictly_false,
    // LoopStep
    _||_(
      @result~bool^@result,
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
    @result~bool^@result)~bool,
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

TEST(CompilerFactoryTest, AnnotationSupport) {
  CompilerOptions options;
  options.parser_options.enable_annotations = true;
  options.parser_options.enable_hidden_accumulator_var = true;
  options.checker_options.annotation_support = CheckerAnnotationSupport::kCheck;
  ASSERT_OK_AND_ASSIGN(
      auto builder,
      NewCompilerBuilder(cel::internal::GetSharedTestingDescriptorPool(),
                         options));

  absl::Status s;
  s.Update(builder->AddLibrary(StandardCheckerLibrary()));
  s.Update(builder->AddLibrary(
      CompilerLibrary("test", [](TypeCheckerBuilder& builder) -> absl::Status {
        absl::Status s;
        AnnotationDecl decl;
        decl.set_name("Describe");
        decl.set_expected_type(StringType());
        s.Update(builder.AddAnnotation(std::move(decl)));
        s.Update(builder.AddVariable(MakeVariableDecl("foo", MapType())));
        s.Update(builder.AddVariable(MakeVariableDecl("bar", StringType())));

        return s;
      })));

  ASSERT_THAT(s, IsOk());
  ASSERT_OK_AND_ASSIGN(auto compiler, std::move(*builder).Build());

  ASSERT_OK_AND_ASSIGN(ValidationResult result, compiler->Compile(R"cel(
      cel.annotate(
        ['a', 'b', 'c'] in foo,
        cel.Annotation{
          name: "Describe",
          value: "foo " + (cel.annotated_value ? "contains" : "does not contain") +
                 " something interesting"
        }) ||
      cel.annotate(
         ['d', 'e', 'f'].exists(x, x.endsWith(bar)),
         cel.Annotation{
           name: "Describe",
           value: "bar " +
             (cel.annotated_value ? "is" : "is not" ) +
             "an interesting suffix"
         })
      )cel"));

  ASSERT_TRUE(result.IsValid()) << result.FormatError();

  EXPECT_EQ(FormatBaselineAst(*result.GetAst()),
            R"(cel.@annotated(
  _||_(
    @in(
      [
        "a"~string,
        "b"~string,
        "c"~string
      ]~list(string),
      foo~map(dyn, dyn)^foo
    )~bool^in_map,
    __comprehension__(
      // Variable
      x,
      // Target
      [
        "d"~string,
        "e"~string,
        "f"~string
      ]~list(string),
      // Accumulator
      @result,
      // Init
      false~bool,
      // LoopCondition
      @not_strictly_false(
        !_(
          @result~bool^@result
        )~bool^logical_not
      )~bool^not_strictly_false,
      // LoopStep
      _||_(
        @result~bool^@result,
        x~string^x.endsWith(
          bar~string^bar
        )~bool^ends_with_string
      )~bool^logical_or,
      // Result
      @result~bool^@result)~bool
  )~bool^logical_or,
  {
    7:[
      cel.Annotation{
        name:"Describe",
        value:_+_(
          _+_(
            "foo "~string,
            _?_:_(
              cel.annotated_value~bool^cel.annotated_value,
              "contains"~string,
              "does not contain"~string
            )~string^conditional
          )~string^add_string,
          " something interesting"~string
        )~string^add_string
      }
    ],
    40:[
      cel.Annotation{
        name:"Describe",
        value:_+_(
          _+_(
            "bar "~string,
            _?_:_(
              cel.annotated_value~bool^cel.annotated_value,
              "is"~string,
              "is not"~string
            )~string^conditional
          )~string^add_string,
          "an interesting suffix"~string
        )~string^add_string
      }
    ]
  }
))");
}

TEST(CompilerFactoryTest, AnnotationScopingRules) {
  CompilerOptions options;
  options.parser_options.enable_annotations = true;
  options.parser_options.enable_hidden_accumulator_var = true;
  options.checker_options.annotation_support = CheckerAnnotationSupport::kCheck;
  ASSERT_OK_AND_ASSIGN(
      auto builder,
      NewCompilerBuilder(cel::internal::GetSharedTestingDescriptorPool(),
                         options));
  google::protobuf::Arena arena;
  Type map_list_string =
      MapType(&arena, StringType(), ListType(&arena, StringType()));
  absl::Status s;
  s.Update(builder->AddLibrary(StandardCheckerLibrary()));
  s.Update(builder->AddLibrary(
      CompilerLibrary("test", [=](TypeCheckerBuilder& builder) -> absl::Status {
        absl::Status s;
        AnnotationDecl decl;
        decl.set_name("Describe");
        decl.set_expected_type(StringType());
        s.Update(builder.AddAnnotation(std::move(decl)));
        s.Update(builder.AddVariable(
            MakeVariableDecl("memberships", map_list_string)));
        s.Update(builder.AddVariable(MakeVariableDecl("user", StringType())));

        return s;
      })));

  ASSERT_THAT(s, IsOk());
  ASSERT_OK_AND_ASSIGN(auto compiler, std::move(*builder).Build());

  ASSERT_OK_AND_ASSIGN(ValidationResult result, compiler->Compile(R"cel(
      cel.annotate(
        ['g1', 'g2', 'g3'].all(g,
          cel.annotate(
            g in memberships[user],
            cel.Annotation{
              name: "Describe",
              value: "user '" + user + "' " +
                     (cel.annotated_value ? "is" : "is not") +
                     " a member of " + g
            }
          )
        ),
        cel.Annotation{
          name: "Describe",
          value:
            "user '" + user + "' " +
            (cel.annotated_value ? "is" : "is not") +
            " a member of all required groups"
        }
      )
      )cel"));

  ASSERT_TRUE(result.IsValid()) << result.FormatError();

  std::string adorned_ast = FormatBaselineAst(*result.GetAst());
  EXPECT_EQ(adorned_ast,
            R"(cel.@annotated(
  __comprehension__(
    // Variable
    g,
    // Target
    [
      "g1"~string,
      "g2"~string,
      "g3"~string
    ]~list(string),
    // Accumulator
    @result,
    // Init
    true~bool,
    // LoopCondition
    @not_strictly_false(
      @result~bool^@result
    )~bool^not_strictly_false,
    // LoopStep
    _&&_(
      @result~bool^@result,
      @in(
        g~string^g,
        _[_](
          memberships~map(string, list(string))^memberships,
          user~string^user
        )~list(string)^index_map
      )~bool^in_list
    )~bool^logical_and,
    // Result
    @result~bool^@result)~bool,
  {
    12:[
      cel.Annotation{
        name:"Describe",
        value:_+_(
          _+_(
            _+_(
              _+_(
                _+_(
                  "user '"~string,
                  user~string^user
                )~string^add_string,
                "' "~string
              )~string^add_string,
              _?_:_(
                cel.annotated_value~bool^cel.annotated_value,
                "is"~string,
                "is not"~string
              )~string^conditional
            )~string^add_string,
            " a member of "~string
          )~string^add_string,
          g~string^g
        )~string^add_string
      }
    ],
    41:[
      cel.Annotation{
        name:"Describe",
        value:_+_(
          _+_(
            _+_(
              _+_(
                "user '"~string,
                user~string^user
              )~string^add_string,
              "' "~string
            )~string^add_string,
            _?_:_(
              cel.annotated_value~bool^cel.annotated_value,
              "is"~string,
              "is not"~string
            )~string^conditional
          )~string^add_string,
          " a member of all required groups"~string
        )~string^add_string
      }
    ]
  }
))");
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
