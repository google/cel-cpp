// Copyright 2023 Google LLC
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
#include "eval/compiler/flat_expr_builder_extensions.h"

#include <utility>

#include "absl/status/status.h"
#include "base/ast_internal/expr.h"
#include "eval/compiler/resolver.h"
#include "eval/eval/const_value_step.h"
#include "eval/eval/evaluator_core.h"
#include "internal/testing.h"
#include "runtime/function_registry.h"
#include "runtime/internal/issue_collector.h"
#include "runtime/runtime_issue.h"
#include "runtime/runtime_options.h"

namespace google::api::expr::runtime {
namespace {

using ::cel::RuntimeIssue;
using ::cel::ast_internal::Expr;
using ::cel::runtime_internal::IssueCollector;
using testing::ElementsAre;
using testing::IsEmpty;
using cel::internal::StatusIs;

class PlannerContextTest : public testing::Test {
 public:
  PlannerContextTest()
      : type_registry_(),
        function_registry_(),
        type_factory_(cel::MemoryManagerRef::ReferenceCounting()),
        type_manager_(type_factory_, type_registry_.GetComposedTypeProvider()),
        value_factory_(type_manager_),
        resolver_("", function_registry_, type_registry_, value_factory_,
                  type_registry_.resolveable_enums()),
        issue_collector_(RuntimeIssue::Severity::kError) {}

 protected:
  cel::TypeRegistry type_registry_;
  cel::FunctionRegistry function_registry_;
  cel::RuntimeOptions options_;
  cel::TypeFactory type_factory_;
  cel::TypeManager type_manager_;
  cel::ValueManager value_factory_;
  Resolver resolver_;
  IssueCollector issue_collector_;
};

MATCHER_P(UniquePtrHolds, ptr, "") {
  const auto& got = arg;
  return ptr == got.get();
}

// simulate a program of:
//    a
//   / \
//  b   c
absl::StatusOr<ExecutionPath> InitSimpleTree(
    const Expr& a, const Expr& b, const Expr& c,
    cel::ValueManager& value_factory, PlannerContext::ProgramTree& tree) {
  CEL_ASSIGN_OR_RETURN(auto a_step,
                       CreateConstValueStep(value_factory.GetNullValue(), -1));
  CEL_ASSIGN_OR_RETURN(auto b_step,
                       CreateConstValueStep(value_factory.GetNullValue(), -1));
  CEL_ASSIGN_OR_RETURN(auto c_step,
                       CreateConstValueStep(value_factory.GetNullValue(), -1));

  ExecutionPath path;
  path.push_back(std::move(b_step));
  path.push_back(std::move(c_step));
  path.push_back(std::move(a_step));

  PlannerContext::ProgramInfo& a_info = tree[&a];
  a_info.range_start = 0;
  a_info.range_len = 3;
  a_info.children = {&b, &c};

  PlannerContext::ProgramInfo& b_info = tree[&b];
  b_info.range_start = 0;
  b_info.range_len = 1;
  b_info.parent = &a;

  PlannerContext::ProgramInfo& c_info = tree[&c];
  c_info.range_start = 1;
  c_info.range_len = 1;
  c_info.parent = &a;

  return path;
}

TEST_F(PlannerContextTest, GetPlan) {
  Expr a;
  Expr b;
  Expr c;
  PlannerContext::ProgramTree tree;

  ASSERT_OK_AND_ASSIGN(ExecutionPath path,
                       InitSimpleTree(a, b, c, value_factory_, tree));

  const ExpressionStep* b_step_ptr = path[0].get();
  const ExpressionStep* c_step_ptr = path[1].get();
  const ExpressionStep* a_step_ptr = path[2].get();

  PlannerContext context(resolver_, options_, value_factory_, issue_collector_,
                         path, tree);

  EXPECT_THAT(context.GetSubplan(a), ElementsAre(UniquePtrHolds(b_step_ptr),
                                                 UniquePtrHolds(c_step_ptr),
                                                 UniquePtrHolds(a_step_ptr)));

  EXPECT_THAT(context.GetSubplan(b), ElementsAre(UniquePtrHolds(b_step_ptr)));

  EXPECT_THAT(context.GetSubplan(c), ElementsAre(UniquePtrHolds(c_step_ptr)));

  Expr d;
  EXPECT_THAT(context.GetSubplan(d), IsEmpty());
}

TEST_F(PlannerContextTest, ReplacePlan) {
  Expr a;
  Expr b;
  Expr c;
  PlannerContext::ProgramTree tree;

  ASSERT_OK_AND_ASSIGN(ExecutionPath path,
                       InitSimpleTree(a, b, c, value_factory_, tree));

  const ExpressionStep* b_step_ptr = path[0].get();
  const ExpressionStep* c_step_ptr = path[1].get();
  const ExpressionStep* a_step_ptr = path[2].get();

  PlannerContext context(resolver_, options_, value_factory_, issue_collector_,
                         path, tree);

  EXPECT_THAT(context.GetSubplan(a), ElementsAre(UniquePtrHolds(b_step_ptr),
                                                 UniquePtrHolds(c_step_ptr),
                                                 UniquePtrHolds(a_step_ptr)));

  ExecutionPath new_a;

  ASSERT_OK_AND_ASSIGN(auto new_a_step,
                       CreateConstValueStep(value_factory_.GetNullValue(), -1));
  const ExpressionStep* new_a_step_ptr = new_a_step.get();
  new_a.push_back(std::move(new_a_step));

  ASSERT_OK(context.ReplaceSubplan(a, std::move(new_a)));

  EXPECT_THAT(context.GetSubplan(a),
              ElementsAre(UniquePtrHolds(new_a_step_ptr)));
  EXPECT_THAT(context.GetSubplan(b), IsEmpty());
}

TEST_F(PlannerContextTest, ExtractPlan) {
  Expr a;
  Expr b;
  Expr c;
  PlannerContext::ProgramTree tree;

  ASSERT_OK_AND_ASSIGN(ExecutionPath path,
                       InitSimpleTree(a, b, c, value_factory_, tree));

  const ExpressionStep* b_step_ptr = path[0].get();
  const ExpressionStep* c_step_ptr = path[1].get();
  const ExpressionStep* a_step_ptr = path[2].get();

  PlannerContext context(resolver_, options_, value_factory_, issue_collector_,
                         path, tree);

  EXPECT_THAT(context.GetSubplan(a), ElementsAre(UniquePtrHolds(b_step_ptr),
                                                 UniquePtrHolds(c_step_ptr),
                                                 UniquePtrHolds(a_step_ptr)));

  ASSERT_OK_AND_ASSIGN(ExecutionPath extracted, context.ExtractSubplan(b));

  EXPECT_THAT(extracted, ElementsAre(UniquePtrHolds(b_step_ptr)));
  // Check that ownership was passed.
  EXPECT_NE(extracted[0], path[0]);
}

TEST_F(PlannerContextTest, ExtractPlanFailsOnUnfinishedNode) {
  Expr a;
  Expr b;
  Expr c;
  PlannerContext::ProgramTree tree;

  ASSERT_OK_AND_ASSIGN(ExecutionPath path,
                       InitSimpleTree(a, b, c, value_factory_, tree));

  // Mark a incomplete.
  tree[&a].range_len = -1;

  PlannerContext context(resolver_, options_, value_factory_, issue_collector_,
                         path, tree);

  EXPECT_THAT(context.ExtractSubplan(a), StatusIs(absl::StatusCode::kInternal));
}

TEST_F(PlannerContextTest, ExtractFailsOnReplacedNode) {
  Expr a;
  Expr b;
  Expr c;
  PlannerContext::ProgramTree tree;

  ASSERT_OK_AND_ASSIGN(ExecutionPath path,
                       InitSimpleTree(a, b, c, value_factory_, tree));

  PlannerContext context(resolver_, options_, value_factory_, issue_collector_,
                         path, tree);

  ASSERT_OK(context.ReplaceSubplan(a, {}));

  EXPECT_THAT(context.ExtractSubplan(b), StatusIs(absl::StatusCode::kInternal));
}

TEST_F(PlannerContextTest, ReplacePlanUpdatesParent) {
  Expr a;
  Expr b;
  Expr c;
  PlannerContext::ProgramTree tree;

  ASSERT_OK_AND_ASSIGN(ExecutionPath path,
                       InitSimpleTree(a, b, c, value_factory_, tree));

  const ExpressionStep* b_step_ptr = path[0].get();
  const ExpressionStep* c_step_ptr = path[1].get();
  const ExpressionStep* a_step_ptr = path[2].get();

  PlannerContext context(resolver_, options_, value_factory_, issue_collector_,
                         path, tree);

  EXPECT_THAT(context.GetSubplan(a), ElementsAre(UniquePtrHolds(b_step_ptr),
                                                 UniquePtrHolds(c_step_ptr),
                                                 UniquePtrHolds(a_step_ptr)));

  ASSERT_OK(context.ReplaceSubplan(c, {}));

  EXPECT_THAT(context.GetSubplan(a), ElementsAre(UniquePtrHolds(b_step_ptr),
                                                 UniquePtrHolds(a_step_ptr)));
  EXPECT_THAT(context.GetSubplan(c), IsEmpty());
}

TEST_F(PlannerContextTest, ReplacePlanUpdatesSibling) {
  Expr a;
  Expr b;
  Expr c;
  PlannerContext::ProgramTree tree;

  ASSERT_OK_AND_ASSIGN(ExecutionPath path,
                       InitSimpleTree(a, b, c, value_factory_, tree));

  const ExpressionStep* b_step_ptr = path[0].get();
  const ExpressionStep* c_step_ptr = path[1].get();
  const ExpressionStep* a_step_ptr = path[2].get();

  PlannerContext context(resolver_, options_, value_factory_, issue_collector_,
                         path, tree);

  EXPECT_THAT(context.GetSubplan(a), ElementsAre(UniquePtrHolds(b_step_ptr),
                                                 UniquePtrHolds(c_step_ptr),
                                                 UniquePtrHolds(a_step_ptr)));

  ExecutionPath new_b;

  ASSERT_OK_AND_ASSIGN(auto b1_step,
                       CreateConstValueStep(value_factory_.GetNullValue(), -1));
  const ExpressionStep* b1_step_ptr = b1_step.get();
  new_b.push_back(std::move(b1_step));
  ASSERT_OK_AND_ASSIGN(auto b2_step,
                       CreateConstValueStep(value_factory_.GetNullValue(), -1));
  const ExpressionStep* b2_step_ptr = b2_step.get();
  new_b.push_back(std::move(b2_step));

  ASSERT_OK(context.ReplaceSubplan(b, std::move(new_b)));

  EXPECT_THAT(context.GetSubplan(c), ElementsAre(UniquePtrHolds(c_step_ptr)));
  EXPECT_THAT(context.GetSubplan(b), ElementsAre(UniquePtrHolds(b1_step_ptr),
                                                 UniquePtrHolds(b2_step_ptr)));
  EXPECT_THAT(
      context.GetSubplan(a),
      ElementsAre(UniquePtrHolds(b1_step_ptr), UniquePtrHolds(b2_step_ptr),
                  UniquePtrHolds(c_step_ptr), UniquePtrHolds(a_step_ptr)));
}

TEST_F(PlannerContextTest, ReplacePlanFailsOnUpdatedNode) {
  Expr a;
  Expr b;
  Expr c;
  PlannerContext::ProgramTree tree;

  ASSERT_OK_AND_ASSIGN(ExecutionPath path,
                       InitSimpleTree(a, b, c, value_factory_, tree));

  const ExpressionStep* b_step_ptr = path[0].get();
  const ExpressionStep* c_step_ptr = path[1].get();
  const ExpressionStep* a_step_ptr = path[2].get();

  PlannerContext context(resolver_, options_, value_factory_, issue_collector_,
                         path, tree);

  EXPECT_THAT(context.GetSubplan(a), ElementsAre(UniquePtrHolds(b_step_ptr),
                                                 UniquePtrHolds(c_step_ptr),
                                                 UniquePtrHolds(a_step_ptr)));

  ASSERT_OK(context.ReplaceSubplan(a, {}));
  EXPECT_THAT(context.ReplaceSubplan(b, {}),
              StatusIs(absl::StatusCode::kInternal));
}

TEST_F(PlannerContextTest, ReplacePlanFailsOnUnfinishedNode) {
  Expr a;
  Expr b;
  Expr c;
  PlannerContext::ProgramTree tree;

  ASSERT_OK_AND_ASSIGN(ExecutionPath path,
                       InitSimpleTree(a, b, c, value_factory_, tree));

  tree[&a].range_len = -1;

  PlannerContext context(resolver_, options_, value_factory_, issue_collector_,
                         path, tree);

  EXPECT_THAT(context.GetSubplan(a), IsEmpty());

  EXPECT_THAT(context.ReplaceSubplan(a, {}),
              StatusIs(absl::StatusCode::kInternal));
}

}  // namespace
}  // namespace google::api::expr::runtime
