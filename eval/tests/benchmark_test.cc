#include "benchmark/benchmark.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"

#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_value.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using google::api::expr::v1alpha1::Expr;
using google::api::expr::v1alpha1::SourceInfo;

// Benchmark test
// Evaluates cel expression:
// '1 + 1 + 1 .... +1'
static void BM_Eval(benchmark::State& state) {
  auto builder = CreateCelExpressionBuilder();
  auto reg_status = RegisterBuiltinFunctions(builder->GetRegistry());
  GOOGLE_CHECK(util::IsOk(reg_status));

  int len = state.range(0);

  Expr root_expr;
  Expr* cur_expr = &root_expr;

  for (int i = 0; i < len; i++) {
    Expr::Call* call = cur_expr->mutable_call_expr();
    call->set_function("_+_");
    call->add_args()->mutable_const_expr()->set_int64_value(1);
    cur_expr = call->add_args();
  }

  cur_expr->mutable_const_expr()->set_int64_value(1);

  SourceInfo source_info;
  auto cel_expr_status = builder->CreateExpression(&root_expr, &source_info);
  GOOGLE_CHECK(util::IsOk(cel_expr_status.status()));

  std::unique_ptr<CelExpression> cel_expr =
      std::move(cel_expr_status.ValueOrDie());

  for (auto _ : state) {
    google::protobuf::Arena arena;
    Activation activation;
    auto eval_result = cel_expr->Evaluate(activation, &arena);
    GOOGLE_CHECK(util::IsOk(eval_result.status()));

    CelValue result = eval_result.ValueOrDie();
    GOOGLE_CHECK(result.IsInt64());
    GOOGLE_CHECK(result.Int64OrDie() == len + 1);
  }
}

BENCHMARK(BM_Eval)->Range(1, 32768);

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
