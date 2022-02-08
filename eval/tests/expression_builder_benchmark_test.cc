/*
 * Copyright 2021 Google LLC
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      https://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/text_format.h"
#include "absl/base/attributes.h"
#include "absl/container/btree_map.h"
#include "absl/container/flat_hash_set.h"
#include "absl/container/node_hash_set.h"
#include "absl/strings/match.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/tests/request_context.pb.h"
#include "internal/benchmark.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "parser/parser.h"

namespace google::api::expr::runtime {

namespace {

using google::api::expr::v1alpha1::ParsedExpr;

void BM_RegisterBuiltins(benchmark::State& state) {
  for (auto _ : state) {
    auto builder = CreateCelExpressionBuilder();
    auto reg_status = RegisterBuiltinFunctions(builder->GetRegistry());
    ASSERT_OK(reg_status);
  }
}

BENCHMARK(BM_RegisterBuiltins);

void BM_SymbolicPolicy(benchmark::State& state) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse(R"cel(
   !(request.ip in ["10.0.1.4", "10.0.1.5", "10.0.1.6"]) &&
   ((request.path.startsWith("v1") && request.token in ["v1", "v2", "admin"]) ||
    (request.path.startsWith("v2") && request.token in ["v2", "admin"]) ||
    (request.path.startsWith("/admin") && request.token == "admin" &&
     request.ip in ["10.0.1.1",  "10.0.1.2", "10.0.1.3"])
   ))cel"));

  InterpreterOptions options;
  auto builder = CreateCelExpressionBuilder(options);
  auto reg_status = RegisterBuiltinFunctions(builder->GetRegistry());
  ASSERT_OK(reg_status);

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(
        auto expression,
        builder->CreateExpression(&expr.expr(), &expr.source_info()));
  }
}

BENCHMARK(BM_SymbolicPolicy);

void BM_NestedComprehension(benchmark::State& state) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse(R"(
    [4, 5, 6].all(x, [1, 2, 3].all(y, x > y) && [7, 8, 9].all(z, x < z))
  )"));

  InterpreterOptions options;
  auto builder = CreateCelExpressionBuilder(options);
  auto reg_status = RegisterBuiltinFunctions(builder->GetRegistry());
  ASSERT_OK(reg_status);

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(
        auto expression,
        builder->CreateExpression(&expr.expr(), &expr.source_info()));
  }
}

BENCHMARK(BM_NestedComprehension);

void BM_Comparisons(benchmark::State& state) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr expr, parser::Parse(R"(
    v11 < v12 && v12 < v13
      && v21 > v22 && v22 > v23
      && v31 == v32 && v32 == v33
      && v11 != v12 && v12 != v13
  )"));

  InterpreterOptions options;
  auto builder = CreateCelExpressionBuilder(options);
  auto reg_status = RegisterBuiltinFunctions(builder->GetRegistry());
  ASSERT_OK(reg_status);

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(
        auto expression,
        builder->CreateExpression(&expr.expr(), &expr.source_info()));
  }
}

BENCHMARK(BM_Comparisons);

}  // namespace
}  // namespace google::api::expr::runtime
