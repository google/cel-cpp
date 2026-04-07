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

#include "validator/validator.h"

#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "checker/type_check_issue.h"
#include "common/ast.h"
#include "common/expr.h"
#include "common/source.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::testing::ElementsAre;
using ::testing::Eq;
using ::testing::Property;

TEST(ValidatorTest, AddValidationAndValidate) {
  Validator validator;
  validator.AddValidation(Validation([](ValidationContext& context) {
    context.ReportError("error 1");
    return false;
  }));
  validator.AddValidation(Validation([](ValidationContext& context) {
    context.ReportWarning("warning 1");
    return true;
  }));

  Ast ast;
  auto output = validator.Validate(ast);

  EXPECT_FALSE(output.valid);
  EXPECT_THAT(output.issues,
              ElementsAre(Property(&TypeCheckIssue::message, Eq("error 1")),
                          Property(&TypeCheckIssue::message, Eq("warning 1"))));
  EXPECT_EQ(output.issues[0].severity(), TypeCheckIssue::Severity::kError);
  EXPECT_EQ(output.issues[1].severity(), TypeCheckIssue::Severity::kWarning);
}

TEST(ValidatorTest, ReportAt) {
  Validator validator;
  validator.AddValidation(Validation([](ValidationContext& context) {
    context.ReportErrorAt(1, "error at 1");
    context.ReportWarningAt(2, "warning at 2");
    return false;
  }));

  Expr expr;
  expr.set_id(1);
  SourceInfo source_info;
  source_info.mutable_positions()[1] = 10;
  source_info.mutable_positions()[2] = 20;
  source_info.set_line_offsets({15, 25});

  Ast ast(std::move(expr), std::move(source_info));
  auto output = validator.Validate(ast);

  EXPECT_FALSE(output.valid);
  ASSERT_EQ(output.issues.size(), 2);

  EXPECT_EQ(output.issues[0].location().line, 1);
  EXPECT_EQ(output.issues[0].location().column, 10);

  EXPECT_EQ(output.issues[1].location().line, 2);
  EXPECT_EQ(output.issues[1].location().column, 5);
}

}  // namespace
}  // namespace cel
