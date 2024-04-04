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

#include "eval/eval/create_map_step.h"

#include <string>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "base/ast_internal/expr.h"
#include "base/type_provider.h"
#include "eval/eval/cel_expression_flat_impl.h"
#include "eval/eval/evaluator_core.h"
#include "eval/eval/ident_step.h"
#include "eval/public/activation.h"
#include "eval/public/cel_value.h"
#include "eval/public/unknown_set.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::TypeProvider;
using ::cel::ast_internal::Expr;
using ::google::protobuf::Arena;


// Helper method. Creates simple pipeline containing CreateStruct step that
// builds Map and runs it.
absl::StatusOr<CelValue> RunCreateMapExpression(
    const std::vector<std::pair<CelValue, CelValue>>& values,
    google::protobuf::Arena* arena, bool enable_unknowns) {
  ExecutionPath path;
  Activation activation;

  Expr expr0;
  Expr expr1;

  std::vector<Expr> exprs;
  exprs.reserve(values.size() * 2);
  int index = 0;

  auto& create_struct = expr1.mutable_struct_expr();
  for (const auto& item : values) {
    std::string key_name = absl::StrCat("key", index);
    std::string value_name = absl::StrCat("value", index);

    auto& key_expr = exprs.emplace_back();
    auto& key_ident = key_expr.mutable_ident_expr();
    key_ident.set_name(key_name);
    CEL_ASSIGN_OR_RETURN(auto step_key,
                         CreateIdentStep(key_ident, exprs.back().id()));

    auto& value_expr = exprs.emplace_back();
    auto& value_ident = value_expr.mutable_ident_expr();
    value_ident.set_name(value_name);
    CEL_ASSIGN_OR_RETURN(auto step_value,
                         CreateIdentStep(value_ident, exprs.back().id()));

    path.push_back(std::move(step_key));
    path.push_back(std::move(step_value));

    activation.InsertValue(key_name, item.first);
    activation.InsertValue(value_name, item.second);

    create_struct.mutable_entries().emplace_back();
    index++;
  }

  CEL_ASSIGN_OR_RETURN(auto step1,
                       CreateCreateStructStepForMap(create_struct, expr1.id()));
  path.push_back(std::move(step1));

  cel::RuntimeOptions options;
  if (enable_unknowns) {
    options.unknown_processing = cel::UnknownProcessingOptions::kAttributeOnly;
  }

  CelExpressionFlatImpl cel_expr(
      FlatExpression(std::move(path), /*comprehension_slot_count=*/0,
                     TypeProvider::Builtin(), options));
  return cel_expr.Evaluate(activation, arena);
}

class CreateMapStepTest : public testing::TestWithParam<bool> {};

// Test that Empty Map is created successfully.
TEST_P(CreateMapStepTest, TestCreateEmptyMap) {
  Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunCreateMapExpression({}, &arena, GetParam()));
  ASSERT_TRUE(result.IsMap());

  const CelMap* cel_map = result.MapOrDie();
  ASSERT_EQ(cel_map->size(), 0);
}

// Test message creation if unknown argument is passed
TEST(CreateMapStepTest, TestMapCreateWithUnknown) {
  Arena arena;
  UnknownSet unknown_set;
  std::vector<std::pair<CelValue, CelValue>> entries;

  std::vector<std::string> kKeys = {"test2", "test1"};

  entries.push_back(
      {CelValue::CreateString(&kKeys[0]), CelValue::CreateInt64(2)});
  entries.push_back({CelValue::CreateString(&kKeys[1]),
                     CelValue::CreateUnknownSet(&unknown_set)});

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunCreateMapExpression(entries, &arena, true));
  ASSERT_TRUE(result.IsUnknownSet());
}

// Test that String Map is created successfully.
TEST_P(CreateMapStepTest, TestCreateStringMap) {
  Arena arena;

  std::vector<std::pair<CelValue, CelValue>> entries;

  std::vector<std::string> kKeys = {"test2", "test1"};

  entries.push_back(
      {CelValue::CreateString(&kKeys[0]), CelValue::CreateInt64(2)});
  entries.push_back(
      {CelValue::CreateString(&kKeys[1]), CelValue::CreateInt64(1)});

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunCreateMapExpression(entries, &arena, GetParam()));
  ASSERT_TRUE(result.IsMap());

  const CelMap* cel_map = result.MapOrDie();
  ASSERT_EQ(cel_map->size(), 2);

  auto lookup0 = cel_map->Get(&arena, CelValue::CreateString(&kKeys[0]));
  ASSERT_TRUE(lookup0.has_value());
  ASSERT_TRUE(lookup0->IsInt64()) << lookup0->DebugString();
  EXPECT_EQ(lookup0->Int64OrDie(), 2);

  auto lookup1 = cel_map->Get(&arena, CelValue::CreateString(&kKeys[1]));
  ASSERT_TRUE(lookup1.has_value());
  ASSERT_TRUE(lookup1->IsInt64());
  EXPECT_EQ(lookup1->Int64OrDie(), 1);
}

INSTANTIATE_TEST_SUITE_P(CreateMapStep, CreateMapStepTest, testing::Bool());

}  // namespace

}  // namespace google::api::expr::runtime
