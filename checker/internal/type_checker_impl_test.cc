// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "checker/internal/type_checker_impl.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "absl/base/no_destructor.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_set.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "base/ast_internal/ast_impl.h"
#include "base/ast_internal/expr.h"
#include "checker/internal/test_ast_helpers.h"
#include "checker/internal/type_check_env.h"
#include "checker/type_check_issue.h"
#include "checker/validation_result.h"
#include "common/ast.h"
#include "common/decl.h"
#include "common/type.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"
#include "google/protobuf/arena.h"

namespace cel {
namespace checker_internal {

namespace {

using ::absl_testing::IsOk;
using ::cel::ast_internal::AstImpl;
using ::cel::ast_internal::Reference;
using ::google::api::expr::test::v1::proto3::TestAllTypes;
using ::testing::_;
using ::testing::Contains;
using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::Pair;

using AstType = cel::ast_internal::Type;

using AstType = ast_internal::Type;
using Severity = TypeCheckIssue::Severity;

std::string SevString(Severity severity) {
  switch (severity) {
    case Severity::kDeprecated:
      return "Deprecated";
    case Severity::kError:
      return "Error";
    case Severity::kWarning:
      return "Warning";
    case Severity::kInformation:
      return "Information";
  }
}

}  // namespace
}  // namespace checker_internal

template <typename Sink>
void AbslStringify(Sink& sink, const TypeCheckIssue& issue) {
  absl::Format(&sink, "TypeCheckIssue(%s): %s",
               checker_internal::SevString(issue.severity()), issue.message());
}

namespace checker_internal {
namespace {

absl::Nonnull<google::protobuf::Arena*> TestTypeArena() {
  static absl::NoDestructor<google::protobuf::Arena> kArena;
  return &(*kArena);
}

FunctionDecl MakeIdentFunction() {
  auto decl = MakeFunctionDecl(
      "identity",
      MakeOverloadDecl("identity", TypeParamType("A"), TypeParamType("A")));
  ABSL_CHECK_OK(decl.status());
  return decl.value();
}

MATCHER_P2(IsIssueWithSubstring, severity, substring, "") {
  const TypeCheckIssue& issue = arg;
  if (issue.severity() == severity &&
      absl::StrContains(issue.message(), substring)) {
    return true;
  }

  *result_listener << "expected: " << SevString(severity) << " " << substring
                   << "\nactual: " << SevString(issue.severity()) << " "
                   << issue.message();

  return false;
}

MATCHER_P(IsVariableReference, var_name, "") {
  const Reference& reference = arg;
  if (reference.name() == var_name) {
    return true;
  }
  *result_listener << "expected: " << var_name
                   << "\nactual: " << reference.name();

  return false;
}

MATCHER_P2(IsFunctionReference, fn_name, overloads, "") {
  const Reference& reference = arg;
  if (reference.name() != fn_name) {
    *result_listener << "expected: " << fn_name
                     << "\nactual: " << reference.name();
  }

  absl::flat_hash_set<std::string> got_overload_set(
      reference.overload_id().begin(), reference.overload_id().end());
  absl::flat_hash_set<std::string> want_overload_set(overloads.begin(),
                                                     overloads.end());

  if (got_overload_set != want_overload_set) {
    *result_listener << "expected overload_ids: "
                     << absl::StrJoin(want_overload_set, ",")
                     << "\nactual: " << absl::StrJoin(got_overload_set, ",");
  }

  return reference.name() == fn_name && got_overload_set == want_overload_set;
}

class TypeCheckerImplTest : public ::testing::Test {
 public:
  TypeCheckerImplTest() = default;

  absl::Status RegisterMinimalBuiltins(TypeCheckEnv& env) {
    Type list_of_a = ListType(&arena_, TypeParamType("A"));

    FunctionDecl add_op;

    add_op.set_name("_+_");
    CEL_RETURN_IF_ERROR(add_op.AddOverload(
        MakeOverloadDecl("add_int_int", IntType(), IntType(), IntType())));
    CEL_RETURN_IF_ERROR(add_op.AddOverload(
        MakeOverloadDecl("add_uint_uint", UintType(), UintType(), UintType())));
    CEL_RETURN_IF_ERROR(add_op.AddOverload(MakeOverloadDecl(
        "add_double_double", DoubleType(), DoubleType(), DoubleType())));

    CEL_RETURN_IF_ERROR(add_op.AddOverload(
        MakeOverloadDecl("add_list", list_of_a, list_of_a, list_of_a)));

    FunctionDecl not_op;
    not_op.set_name("!_");
    CEL_RETURN_IF_ERROR(not_op.AddOverload(
        MakeOverloadDecl("logical_not",
                         /*return_type=*/BoolType{}, BoolType{})));
    FunctionDecl not_strictly_false;
    not_strictly_false.set_name("@not_strictly_false");
    CEL_RETURN_IF_ERROR(not_strictly_false.AddOverload(
        MakeOverloadDecl("not_strictly_false",
                         /*return_type=*/BoolType{}, DynType{})));
    FunctionDecl mult_op;
    mult_op.set_name("_*_");
    CEL_RETURN_IF_ERROR(mult_op.AddOverload(
        MakeOverloadDecl("mult_int_int",
                         /*return_type=*/IntType(), IntType(), IntType())));
    FunctionDecl or_op;
    or_op.set_name("_||_");
    CEL_RETURN_IF_ERROR(or_op.AddOverload(
        MakeOverloadDecl("logical_or",
                         /*return_type=*/BoolType{}, BoolType{}, BoolType{})));

    FunctionDecl and_op;
    and_op.set_name("_&&_");
    CEL_RETURN_IF_ERROR(and_op.AddOverload(
        MakeOverloadDecl("logical_and",
                         /*return_type=*/BoolType{}, BoolType{}, BoolType{})));

    FunctionDecl lt_op;
    lt_op.set_name("_<_");
    CEL_RETURN_IF_ERROR(lt_op.AddOverload(
        MakeOverloadDecl("lt_int_int",
                         /*return_type=*/BoolType{}, IntType(), IntType())));

    FunctionDecl gt_op;
    gt_op.set_name("_>_");
    CEL_RETURN_IF_ERROR(gt_op.AddOverload(
        MakeOverloadDecl("gt_int_int",
                         /*return_type=*/BoolType{}, IntType(), IntType())));

    FunctionDecl eq_op;
    eq_op.set_name("_==_");
    CEL_RETURN_IF_ERROR(eq_op.AddOverload(MakeOverloadDecl(
        "equals",
        /*return_type=*/BoolType{}, TypeParamType("A"), TypeParamType("A"))));

    FunctionDecl ternary_op;
    ternary_op.set_name("_?_:_");
    CEL_RETURN_IF_ERROR(eq_op.AddOverload(
        MakeOverloadDecl("conditional",
                         /*return_type=*/
                         TypeParamType("A"), BoolType{}, TypeParamType("A"),
                         TypeParamType("A"))));

    FunctionDecl to_int;
    to_int.set_name("int");
    CEL_RETURN_IF_ERROR(to_int.AddOverload(
        MakeOverloadDecl("to_int",
                         /*return_type=*/IntType(), DynType())));

    FunctionDecl to_dyn;
    to_dyn.set_name("dyn");
    CEL_RETURN_IF_ERROR(to_dyn.AddOverload(
        MakeOverloadDecl("to_dyn",
                         /*return_type=*/DynType(), TypeParamType("A"))));

    env.InsertFunctionIfAbsent(std::move(not_op));
    env.InsertFunctionIfAbsent(std::move(not_strictly_false));
    env.InsertFunctionIfAbsent(std::move(add_op));
    env.InsertFunctionIfAbsent(std::move(mult_op));
    env.InsertFunctionIfAbsent(std::move(or_op));
    env.InsertFunctionIfAbsent(std::move(and_op));
    env.InsertFunctionIfAbsent(std::move(lt_op));
    env.InsertFunctionIfAbsent(std::move(gt_op));
    env.InsertFunctionIfAbsent(std::move(to_int));
    env.InsertFunctionIfAbsent(std::move(eq_op));
    env.InsertFunctionIfAbsent(std::move(ternary_op));
    env.InsertFunctionIfAbsent(std::move(to_dyn));

    return absl::OkStatus();
  }

 private:
  google::protobuf::Arena arena_;
};

TEST_F(TypeCheckerImplTest, SmokeTest) {
  TypeCheckEnv env;

  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("1 + 2"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
}

TEST_F(TypeCheckerImplTest, SimpleIdentsResolved) {
  TypeCheckEnv env;

  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());

  env.InsertVariableIfAbsent(MakeVariableDecl("x", IntType()));
  env.InsertVariableIfAbsent(MakeVariableDecl("y", IntType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("x + y"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
}

TEST_F(TypeCheckerImplTest, ReportMissingIdentDecl) {
  TypeCheckEnv env;

  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());

  env.InsertVariableIfAbsent(MakeVariableDecl("x", IntType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("x + y"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_FALSE(result.IsValid());

  EXPECT_THAT(result.GetIssues(),
              ElementsAre(IsIssueWithSubstring(Severity::kError,
                                               "undeclared reference to 'y'")));
}

TEST_F(TypeCheckerImplTest, QualifiedIdentsResolved) {
  TypeCheckEnv env;

  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());

  env.InsertVariableIfAbsent(MakeVariableDecl("x.y", IntType()));
  env.InsertVariableIfAbsent(MakeVariableDecl("x.z", IntType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("x.y + x.z"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
}

TEST_F(TypeCheckerImplTest, ReportMissingQualifiedIdentDecl) {
  TypeCheckEnv env;

  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());

  env.InsertVariableIfAbsent(MakeVariableDecl("x", IntType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("y.x"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_FALSE(result.IsValid());

  EXPECT_THAT(result.GetIssues(),
              ElementsAre(IsIssueWithSubstring(
                  Severity::kError, "undeclared reference to 'y.x'")));
}

TEST_F(TypeCheckerImplTest, ResolveMostQualfiedIdent) {
  TypeCheckEnv env;

  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());

  env.InsertVariableIfAbsent(MakeVariableDecl("x", IntType()));
  env.InsertVariableIfAbsent(MakeVariableDecl("x.y", MapType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("x.y.z"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());
  auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_THAT(ast_impl.reference_map(),
              Contains(Pair(_, IsVariableReference("x.y"))));
}

TEST_F(TypeCheckerImplTest, MemberFunctionCallResolved) {
  TypeCheckEnv env;

  env.InsertVariableIfAbsent(MakeVariableDecl("x", IntType()));

  env.InsertVariableIfAbsent(MakeVariableDecl("y", IntType()));
  FunctionDecl foo;
  foo.set_name("foo");
  ASSERT_THAT(foo.AddOverload(MakeMemberOverloadDecl("int_foo_int",
                                                     /*return_type=*/IntType(),
                                                     IntType(), IntType())),
              IsOk());
  env.InsertFunctionIfAbsent(std::move(foo));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("x.foo(y)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
}

TEST_F(TypeCheckerImplTest, MemberFunctionCallNotDeclared) {
  TypeCheckEnv env;

  env.InsertVariableIfAbsent(MakeVariableDecl("x", IntType()));
  env.InsertVariableIfAbsent(MakeVariableDecl("y", IntType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("x.foo(y)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_FALSE(result.IsValid());

  EXPECT_THAT(result.GetIssues(),
              ElementsAre(IsIssueWithSubstring(
                  Severity::kError, "undeclared reference to 'foo'")));
}

TEST_F(TypeCheckerImplTest, FunctionShapeMismatch) {
  TypeCheckEnv env;
  // foo(int, int) -> int
  ASSERT_OK_AND_ASSIGN(
      auto foo,
      MakeFunctionDecl("foo", MakeOverloadDecl("foo_int_int", IntType(),
                                               IntType(), IntType())));
  env.InsertFunctionIfAbsent(foo);
  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("foo(1, 2, 3)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_FALSE(result.IsValid());

  EXPECT_THAT(result.GetIssues(),
              ElementsAre(IsIssueWithSubstring(
                  Severity::kError, "undeclared reference to 'foo'")));
}

TEST_F(TypeCheckerImplTest, NamespaceFunctionCallResolved) {
  TypeCheckEnv env;
  // Variables
  env.InsertVariableIfAbsent(MakeVariableDecl("x", IntType()));
  env.InsertVariableIfAbsent(MakeVariableDecl("y", IntType()));

  // add x.foo as a namespaced function.
  FunctionDecl foo;
  foo.set_name("x.foo");
  ASSERT_THAT(
      foo.AddOverload(MakeOverloadDecl("x_foo_int",
                                       /*return_type=*/IntType(), IntType())),
      IsOk());
  env.InsertFunctionIfAbsent(std::move(foo));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("x.foo(y)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());
  EXPECT_THAT(result.GetIssues(), IsEmpty());

  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());
  auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_TRUE(ast_impl.root_expr().has_call_expr())
      << absl::StrCat("kind: ", ast_impl.root_expr().kind().index());
  EXPECT_EQ(ast_impl.root_expr().call_expr().function(), "x.foo");
  EXPECT_FALSE(ast_impl.root_expr().call_expr().has_target());
}

TEST_F(TypeCheckerImplTest, MixedListTypeToDyn) {
  TypeCheckEnv env;

  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("[1, 'a']"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
  auto& ast_impl = AstImpl::CastFromPublicAst(*result.GetAst());
  EXPECT_TRUE(ast_impl.type_map().at(1).list_type().elem_type().has_dyn());
}

TEST_F(TypeCheckerImplTest, FreeListTypeToDyn) {
  TypeCheckEnv env;

  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("[]"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
  auto& ast_impl = AstImpl::CastFromPublicAst(*result.GetAst());
  EXPECT_TRUE(ast_impl.type_map().at(1).list_type().elem_type().has_dyn());
}

TEST_F(TypeCheckerImplTest, FreeMapTypeToDyn) {
  TypeCheckEnv env;

  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("{}"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
  auto& ast_impl = AstImpl::CastFromPublicAst(*result.GetAst());
  EXPECT_TRUE(ast_impl.type_map().at(1).map_type().key_type().has_dyn());
  EXPECT_TRUE(ast_impl.type_map().at(1).map_type().value_type().has_dyn());
}

TEST_F(TypeCheckerImplTest, MapTypeWithMixedKeys) {
  TypeCheckEnv env;

  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("{'a': 1, 2: 3}"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
  auto& ast_impl = AstImpl::CastFromPublicAst(*result.GetAst());
  EXPECT_TRUE(ast_impl.type_map().at(1).map_type().key_type().has_dyn());
  EXPECT_EQ(ast_impl.type_map().at(1).map_type().value_type().primitive(),
            ast_internal::PrimitiveType::kInt64);
}

TEST_F(TypeCheckerImplTest, MapTypeUnsupportedKeyWarns) {
  TypeCheckEnv env;

  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("{{}: 'a'}"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(),
              ElementsAre(IsIssueWithSubstring(Severity::kWarning,
                                               "unsupported map key type:")));
}

TEST_F(TypeCheckerImplTest, MapTypeWithMixedValues) {
  TypeCheckEnv env;

  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("{'a': 1, 'b': '2'}"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
  auto& ast_impl = AstImpl::CastFromPublicAst(*result.GetAst());
  EXPECT_EQ(ast_impl.type_map().at(1).map_type().key_type().primitive(),
            ast_internal::PrimitiveType::kString);
  EXPECT_TRUE(ast_impl.type_map().at(1).map_type().value_type().has_dyn());
}

TEST_F(TypeCheckerImplTest, ComprehensionVariablesResolved) {
  TypeCheckEnv env;

  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast,
                       MakeTestParsedAst("[1, 2, 3].exists(x, x * x > 10)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
}

TEST_F(TypeCheckerImplTest, MapComprehensionVariablesResolved) {
  TypeCheckEnv env;

  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast,
                       MakeTestParsedAst("{1: 3, 2: 4}.exists(x, x == 2)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
}

TEST_F(TypeCheckerImplTest, NestedComprehensions) {
  TypeCheckEnv env;

  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(
      auto ast,
      MakeTestParsedAst("[1, 2].all(x, ['1', '2'].exists(y, int(y) == x))"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
}

TEST_F(TypeCheckerImplTest, ComprehensionVarsFollowNamespacePriorityRules) {
  TypeCheckEnv env;
  env.set_container("com");
  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());

  // Namespace resolution still applies, compre var doesn't shadow com.x
  env.InsertVariableIfAbsent(MakeVariableDecl("com.x", IntType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast,
                       MakeTestParsedAst("['1', '2'].all(x, x == 2)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());
  auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_THAT(ast_impl.reference_map(),
              Contains(Pair(_, IsVariableReference("com.x"))));
}

TEST_F(TypeCheckerImplTest, ComprehensionVarsFollowQualifiedIdentPriority) {
  TypeCheckEnv env;
  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());

  // Namespace resolution still applies, compre var doesn't shadow x.y
  env.InsertVariableIfAbsent(MakeVariableDecl("x.y", IntType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast,
                       MakeTestParsedAst("[{'y': '2'}].all(x, x.y == 2)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());
  auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_THAT(ast_impl.reference_map(),
              Contains(Pair(_, IsVariableReference("x.y"))));
}

struct PrimitiveLiteralsTestCase {
  std::string expr;
  ast_internal::PrimitiveType expected_type;
};

class PrimitiveLiteralsTest
    : public testing::TestWithParam<PrimitiveLiteralsTestCase> {};

TEST_P(PrimitiveLiteralsTest, LiteralsTypeInferred) {
  TypeCheckEnv env;
  const PrimitiveLiteralsTestCase& test_case = GetParam();
  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst(test_case.expr));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_TRUE(result.IsValid());
  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());
  auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_EQ(ast_impl.type_map()[1].primitive(), test_case.expected_type);
}

INSTANTIATE_TEST_SUITE_P(
    PrimitiveLiteralsTests, PrimitiveLiteralsTest,
    ::testing::Values(
        PrimitiveLiteralsTestCase{
            .expr = "1",
            .expected_type = ast_internal::PrimitiveType::kInt64,
        },
        PrimitiveLiteralsTestCase{
            .expr = "1.0",
            .expected_type = ast_internal::PrimitiveType::kDouble,
        },
        PrimitiveLiteralsTestCase{
            .expr = "1u",
            .expected_type = ast_internal::PrimitiveType::kUint64,
        },
        PrimitiveLiteralsTestCase{
            .expr = "'string'",
            .expected_type = ast_internal::PrimitiveType::kString,
        },
        PrimitiveLiteralsTestCase{
            .expr = "b'bytes'",
            .expected_type = ast_internal::PrimitiveType::kBytes,
        },
        PrimitiveLiteralsTestCase{
            .expr = "false",
            .expected_type = ast_internal::PrimitiveType::kBool,
        }));
struct AstTypeConversionTestCase {
  Type decl_type;
  ast_internal::Type expected_type;
};

class AstTypeConversionTest
    : public testing::TestWithParam<AstTypeConversionTestCase> {};

TEST_P(AstTypeConversionTest, TypeConversion) {
  TypeCheckEnv env;
  ASSERT_TRUE(
      env.InsertVariableIfAbsent(MakeVariableDecl("x", GetParam().decl_type)));
  const AstTypeConversionTestCase& test_case = GetParam();
  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("x"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_TRUE(result.IsValid());
  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());
  auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_EQ(ast_impl.type_map()[1], test_case.expected_type)
      << GetParam().decl_type.DebugString();
}

INSTANTIATE_TEST_SUITE_P(
    Primitives, AstTypeConversionTest,
    ::testing::Values(
        AstTypeConversionTestCase{
            .decl_type = NullType(),
            .expected_type = AstType(ast_internal::NullValue()),
        },
        AstTypeConversionTestCase{
            .decl_type = DynType(),
            .expected_type = AstType(ast_internal::DynamicType()),
        },
        AstTypeConversionTestCase{
            .decl_type = BoolType(),
            .expected_type = AstType(ast_internal::PrimitiveType::kBool),
        },
        AstTypeConversionTestCase{
            .decl_type = IntType(),
            .expected_type = AstType(ast_internal::PrimitiveType::kInt64),
        },
        AstTypeConversionTestCase{
            .decl_type = UintType(),
            .expected_type = AstType(ast_internal::PrimitiveType::kUint64),
        },
        AstTypeConversionTestCase{
            .decl_type = DoubleType(),
            .expected_type = AstType(ast_internal::PrimitiveType::kDouble),
        },
        AstTypeConversionTestCase{
            .decl_type = StringType(),
            .expected_type = AstType(ast_internal::PrimitiveType::kString),
        },
        AstTypeConversionTestCase{
            .decl_type = BytesType(),
            .expected_type = AstType(ast_internal::PrimitiveType::kBytes),
        },
        AstTypeConversionTestCase{
            .decl_type = TimestampType(),
            .expected_type = AstType(ast_internal::WellKnownType::kTimestamp),
        },
        AstTypeConversionTestCase{
            .decl_type = DurationType(),
            .expected_type = AstType(ast_internal::WellKnownType::kDuration),
        }));

INSTANTIATE_TEST_SUITE_P(
    Wrappers, AstTypeConversionTest,
    ::testing::Values(
        AstTypeConversionTestCase{
            .decl_type = IntWrapperType(),
            .expected_type = AstType(ast_internal::PrimitiveTypeWrapper(
                ast_internal::PrimitiveType::kInt64)),
        },
        AstTypeConversionTestCase{
            .decl_type = UintWrapperType(),
            .expected_type = AstType(ast_internal::PrimitiveTypeWrapper(
                ast_internal::PrimitiveType::kUint64)),
        },
        AstTypeConversionTestCase{
            .decl_type = DoubleWrapperType(),
            .expected_type = AstType(ast_internal::PrimitiveTypeWrapper(
                ast_internal::PrimitiveType::kDouble)),
        },
        AstTypeConversionTestCase{
            .decl_type = BoolWrapperType(),
            .expected_type = AstType(ast_internal::PrimitiveTypeWrapper(
                ast_internal::PrimitiveType::kBool)),
        },
        AstTypeConversionTestCase{
            .decl_type = StringWrapperType(),
            .expected_type = AstType(ast_internal::PrimitiveTypeWrapper(
                ast_internal::PrimitiveType::kString)),
        },
        AstTypeConversionTestCase{
            .decl_type = BytesWrapperType(),
            .expected_type = AstType(ast_internal::PrimitiveTypeWrapper(
                ast_internal::PrimitiveType::kBytes)),
        }));

INSTANTIATE_TEST_SUITE_P(
    ComplexTypes, AstTypeConversionTest,
    ::testing::Values(
        AstTypeConversionTestCase{
            .decl_type = ListType(TestTypeArena(), IntType()),
            .expected_type =
                AstType(ast_internal::ListType(std::make_unique<AstType>(
                    ast_internal::PrimitiveType::kInt64))),
        },
        AstTypeConversionTestCase{
            .decl_type = MapType(TestTypeArena(), IntType(), IntType()),
            .expected_type = AstType(ast_internal::MapType(
                std::make_unique<AstType>(ast_internal::PrimitiveType::kInt64),
                std::make_unique<AstType>(
                    ast_internal::PrimitiveType::kInt64))),
        },
        AstTypeConversionTestCase{
            .decl_type = TypeType(TestTypeArena(), IntType()),
            .expected_type = AstType(
                std::make_unique<AstType>(ast_internal::PrimitiveType::kInt64)),
        },
        AstTypeConversionTestCase{
            .decl_type = OpaqueType(TestTypeArena(), "tuple",
                                    {IntType(), IntType()}),
            .expected_type = AstType(ast_internal::AbstractType(
                "tuple", {AstType(ast_internal::PrimitiveType::kInt64),
                          AstType(ast_internal::PrimitiveType::kInt64)})),
        },
        AstTypeConversionTestCase{
            .decl_type = StructType(MessageType(TestAllTypes::descriptor())),
            .expected_type = AstType(ast_internal::MessageType(
                "google.api.expr.test.v1.proto3.TestAllTypes"))}));

TEST_F(TypeCheckerImplTest, NullLiteral) {
  TypeCheckEnv env;
  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("null"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  ASSERT_TRUE(result.IsValid());
  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());
  auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_TRUE(ast_impl.type_map()[1].has_null());
}

TEST_F(TypeCheckerImplTest, ComprehensionUnsupportedRange) {
  TypeCheckEnv env;
  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());

  env.InsertVariableIfAbsent(MakeVariableDecl("y", IntType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("'abc'.all(x, y == 2)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_FALSE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), Contains(IsIssueWithSubstring(
                                      Severity::kError,
                                      "expression of type 'string' cannot be "
                                      "the range of a comprehension")));
}

TEST_F(TypeCheckerImplTest, ComprehensionDynRange) {
  TypeCheckEnv env;
  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());

  env.InsertVariableIfAbsent(MakeVariableDecl("range", DynType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("range.all(x, x == 2)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());
}

TEST_F(TypeCheckerImplTest, BasicOvlResolution) {
  TypeCheckEnv env;
  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());

  env.InsertVariableIfAbsent(MakeVariableDecl("x", DoubleType()));
  env.InsertVariableIfAbsent(MakeVariableDecl("y", DoubleType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("x + y"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());

  // Assumes parser numbering: + should always be id 2.
  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());
  auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_THAT(ast_impl.reference_map()[2],
              IsFunctionReference(
                  "_+_", std::vector<std::string>{"add_double_double"}));
}

TEST_F(TypeCheckerImplTest, OvlResolutionMultipleOverloads) {
  TypeCheckEnv env;
  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());

  env.InsertVariableIfAbsent(MakeVariableDecl("x", DoubleType()));
  env.InsertVariableIfAbsent(MakeVariableDecl("y", DoubleType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("dyn(x) + dyn(y)"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());

  // Assumes parser numbering: + should always be id 3.
  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());
  auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_THAT(ast_impl.reference_map()[3],
              IsFunctionReference("_+_", std::vector<std::string>{
                                             "add_double_double", "add_int_int",
                                             "add_list", "add_uint_uint"}));
}

TEST_F(TypeCheckerImplTest, BasicFunctionResultTypeResolution) {
  TypeCheckEnv env;
  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());

  env.InsertVariableIfAbsent(MakeVariableDecl("x", DoubleType()));
  env.InsertVariableIfAbsent(MakeVariableDecl("y", DoubleType()));
  env.InsertVariableIfAbsent(MakeVariableDecl("z", DoubleType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("x + y + z"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());

  EXPECT_THAT(result.GetIssues(), IsEmpty());

  // Assumes parser numbering: + should always be id 2 and 4.
  ASSERT_OK_AND_ASSIGN(auto checked_ast, result.ReleaseAst());
  auto& ast_impl = AstImpl::CastFromPublicAst(*checked_ast);
  EXPECT_THAT(ast_impl.reference_map()[2],
              IsFunctionReference(
                  "_+_", std::vector<std::string>{"add_double_double"}));
  EXPECT_THAT(ast_impl.reference_map()[4],
              IsFunctionReference(
                  "_+_", std::vector<std::string>{"add_double_double"}));
  int64_t root_id = ast_impl.root_expr().id();
  EXPECT_EQ(ast_impl.type_map()[root_id].primitive(),
            ast_internal::PrimitiveType::kDouble);
}

TEST_F(TypeCheckerImplTest, BasicOvlResolutionNoMatch) {
  TypeCheckEnv env;
  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());

  env.InsertVariableIfAbsent(MakeVariableDecl("x", IntType()));
  env.InsertVariableIfAbsent(MakeVariableDecl("y", StringType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("x + y"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_FALSE(result.IsValid());

  EXPECT_THAT(result.GetIssues(),
              Contains(IsIssueWithSubstring(Severity::kError,
                                            "no matching overload for '_+_'"
                                            " applied to (int, string)")));
}

TEST_F(TypeCheckerImplTest, ParmeterizedOvlResolutionMatch) {
  TypeCheckEnv env;
  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());

  env.InsertVariableIfAbsent(MakeVariableDecl("x", IntType()));
  env.InsertVariableIfAbsent(MakeVariableDecl("y", StringType()));

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast, MakeTestParsedAst("([x] + []) == [x]"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid());
}

TEST_F(TypeCheckerImplTest, AliasedTypeVarSameType) {
  TypeCheckEnv env;
  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());

  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast,
                       MakeTestParsedAst("[].exists(x, x == 10 || x == '10')"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_FALSE(result.IsValid());
  EXPECT_THAT(
      result.GetIssues(),
      ElementsAre(IsIssueWithSubstring(
          Severity::kError, "no matching overload for '_==_' applied to")));
}

TEST_F(TypeCheckerImplTest, TypeVarRange) {
  TypeCheckEnv env;
  ASSERT_THAT(RegisterMinimalBuiltins(env), IsOk());
  env.InsertFunctionIfAbsent(MakeIdentFunction());
  TypeCheckerImpl impl(std::move(env));
  ASSERT_OK_AND_ASSIGN(auto ast,
                       MakeTestParsedAst("identity([]).exists(x, x == 10 )"));
  ASSERT_OK_AND_ASSIGN(ValidationResult result, impl.Check(std::move(ast)));

  EXPECT_TRUE(result.IsValid()) << absl::StrJoin(result.GetIssues(), "\n");
}

}  // namespace
}  // namespace checker_internal
}  // namespace cel
