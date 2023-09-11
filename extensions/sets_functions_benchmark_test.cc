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

#include <string>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_options.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "extensions/sets_functions.h"
#include "internal/benchmark.h"
#include "internal/testing.h"
#include "parser/parser.h"
#include "runtime/runtime_options.h"
#include "google/protobuf/arena.h"

namespace cel::extensions {
namespace {

using ::google::api::expr::v1alpha1::ParsedExpr;
using ::google::api::expr::parser::Parse;
using ::google::api::expr::runtime::Activation;
using ::google::api::expr::runtime::CelValue;
using ::google::api::expr::runtime::ContainerBackedListImpl;
using ::google::api::expr::runtime::CreateCelExpressionBuilder;
using ::google::api::expr::runtime::InterpreterOptions;
using ::google::api::expr::runtime::RegisterBuiltinFunctions;

struct TestCase {
  std::string test_name;
  std::string expr;
  CelValue result;
};

void RunBenchmark(const TestCase& test_case, benchmark::State& state) {
  ASSERT_OK_AND_ASSIGN(ParsedExpr parsed_expr, Parse(test_case.expr));

  google::protobuf::Arena arena;
  InterpreterOptions options;
  options.enable_qualified_identifier_rewrites = true;
  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry(), options));
  ASSERT_OK(RegisterSetsFunctions(builder->GetRegistry()->InternalGetRegistry(),
                                  cel::RuntimeOptions{}));
  ASSERT_OK_AND_ASSIGN(
      auto cel_expr, builder->CreateExpression(&(parsed_expr.expr()), nullptr));

  int len = state.range(0);
  std::vector<CelValue> x;
  std::vector<CelValue> y;
  if (test_case.result.BoolOrDie()) {
    x.reserve(len + 1);
    y.reserve(len + 1);
    x.push_back(CelValue::CreateInt64(2));
    y.push_back(CelValue::CreateInt64(1));
  } else {
    x.reserve(len);
    y.reserve(len);
  }
  for (int i = 0; i < len; i++) {
    x.push_back(CelValue::CreateInt64(1));
    y.push_back(CelValue::CreateInt64(2));
  }

  ContainerBackedListImpl cel_x(std::move(x));
  ContainerBackedListImpl cel_y(std::move(y));

  Activation activation;
  activation.InsertValue("x", CelValue::CreateList(&cel_x));
  activation.InsertValue("y", CelValue::CreateList(&cel_y));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsBool());
    ASSERT_EQ(result.BoolOrDie(), test_case.result.BoolOrDie())
        << test_case.test_name;
  }
}

void BM_SetsIntersectsTrue(benchmark::State& state) {
  RunBenchmark({"sets.intersects_true", "sets.intersects(x, y)",
                CelValue::CreateBool(true)},
               state);
}

void BM_SetsIntersectsFalse(benchmark::State& state) {
  RunBenchmark({"sets.intersects_false", "sets.intersects(x, y)",
                CelValue::CreateBool(false)},
               state);
}

void BM_SetsIntersectsComprehensionTrue(benchmark::State& state) {
  RunBenchmark({"comprehension_intersects_true", "x.exists(i, i in y)",
                CelValue::CreateBool(true)},
               state);
}

void BM_SetsIntersectsComprehensionFalse(benchmark::State& state) {
  RunBenchmark({"comprehension_intersects_false", "x.exists(i, i in y)",
                CelValue::CreateBool(false)},
               state);
}

void BM_SetsEquivalentTrue(benchmark::State& state) {
  RunBenchmark({"sets.equivalent_true", "sets.equivalent(x, y)",
                CelValue::CreateBool(true)},
               state);
}

void BM_SetsEquivalentFalse(benchmark::State& state) {
  RunBenchmark({"sets.equivalent_false", "sets.equivalent(x, y)",
                CelValue::CreateBool(false)},
               state);
}

void BM_SetsEquivalentComprehensionTrue(benchmark::State& state) {
  RunBenchmark(
      {"comprehension_equivalent_true", "x.all(i, i in y) && y.all(j, j in x)",
       CelValue::CreateBool(true)},
      state);
}

void BM_SetsEquivalentComprehensionFalse(benchmark::State& state) {
  RunBenchmark(
      {"comprehension_equivalent_false", "x.all(i, i in y) && y.all(j, j in x)",
       CelValue::CreateBool(false)},
      state);
}
BENCHMARK(BM_SetsIntersectsComprehensionTrue)->Range(1, 1 << 8);
BENCHMARK(BM_SetsIntersectsComprehensionFalse)->Range(1, 1 << 8);
BENCHMARK(BM_SetsIntersectsTrue)->Range(1, 1 << 8);
BENCHMARK(BM_SetsIntersectsFalse)->Range(1, 1 << 8);

BENCHMARK(BM_SetsEquivalentComprehensionTrue)->Range(1, 1 << 8);
BENCHMARK(BM_SetsEquivalentComprehensionFalse)->Range(1, 1 << 8);
BENCHMARK(BM_SetsEquivalentTrue)->Range(1, 1 << 8);
BENCHMARK(BM_SetsEquivalentFalse)->Range(1, 1 << 8);

}  // namespace
}  // namespace cel::extensions
