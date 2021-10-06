#include "benchmark/benchmark.h"

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/text_format.h"
#include "absl/base/attributes.h"
#include "absl/container/node_hash_set.h"
#include "absl/strings/match.h"
#include "eval/public/activation.h"
#include "eval/public/builtin_func_registrar.h"
#include "eval/public/cel_expr_builder_factory.h"
#include "eval/public/cel_expression.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/tests/request_context.pb.h"
#include "internal/status_macros.h"
#include "internal/testing.h"

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
  ASSERT_OK(reg_status);

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
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&root_expr, &source_info));

  for (auto _ : state) {
    google::protobuf::Arena arena;
    Activation activation;
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsInt64());
    ASSERT_TRUE(result.Int64OrDie() == len + 1);
  }
}

BENCHMARK(BM_Eval)->Range(1, 32768);

// Benchmark test
// Evaluates cel expression:
// '"a" + "a" + "a" .... + "a"'
static void BM_EvalString(benchmark::State& state) {
  auto builder = CreateCelExpressionBuilder();
  auto reg_status = RegisterBuiltinFunctions(builder->GetRegistry());
  ASSERT_OK(reg_status);

  int len = state.range(0);

  Expr root_expr;
  Expr* cur_expr = &root_expr;

  for (int i = 0; i < len; i++) {
    Expr::Call* call = cur_expr->mutable_call_expr();
    call->set_function("_+_");
    call->add_args()->mutable_const_expr()->set_string_value("a");
    cur_expr = call->add_args();
  }

  cur_expr->mutable_const_expr()->set_string_value("a");

  SourceInfo source_info;
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&root_expr, &source_info));

  for (auto _ : state) {
    google::protobuf::Arena arena;
    Activation activation;
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsString());
    ASSERT_TRUE(result.StringOrDie().value().size() == len + 1);
  }
}

BENCHMARK(BM_EvalString)->Range(1, 32768);

std::string CELAstFlattenedMap() {
  return R"(
call_expr: <
  function: "_&&_"
  args: <
    call_expr: <
      function: "!_"
      args: <
        call_expr: <
          function: "@in"
          args: <
            ident_expr: <
              name: "ip"
            >
          >
          args: <
            list_expr: <
              elements: <
                const_expr: <
                  string_value: "10.0.1.4"
                >
              >
              elements: <
                const_expr: <
                  string_value: "10.0.1.5"
                >
              >
              elements: <
                const_expr: <
                  string_value: "10.0.1.6"
                >
              >
            >
          >
        >
      >
    >
  >
  args: <
    call_expr: <
      function: "_||_"
      args: <
        call_expr: <
          function: "_||_"
          args: <
            call_expr: <
              function: "_&&_"
              args: <
                call_expr: <
                  target: <
                    ident_expr: <
                      name: "path"
                    >
                  >
                  function: "startsWith"
                  args: <
                    const_expr: <
                      string_value: "v1"
                    >
                  >
                >
              >
              args: <
                call_expr: <
                  function: "@in"
                  args: <
                    ident_expr: <
                      name: "token"
                    >
                  >
                  args: <
                    list_expr: <
                      elements: <
                        const_expr: <
                          string_value: "v1"
                        >
                      >
                      elements: <
                        const_expr: <
                          string_value: "v2"
                        >
                      >
                      elements: <
                        const_expr: <
                          string_value: "admin"
                        >
                      >
                    >
                  >
                >
              >
            >
          >
          args: <
            call_expr: <
              function: "_&&_"
              args: <
                call_expr: <
                  target: <
                    ident_expr: <
                      name: "path"
                    >
                  >
                  function: "startsWith"
                  args: <
                    const_expr: <
                      string_value: "v2"
                    >
                  >
                >
              >
              args: <
                call_expr: <
                  function: "@in"
                  args: <
                    ident_expr: <
                      name: "token"
                    >
                  >
                  args: <
                    list_expr: <
                      elements: <
                        const_expr: <
                          string_value: "v2"
                        >
                      >
                      elements: <
                        const_expr: <
                          string_value: "admin"
                        >
                      >
                    >
                  >
                >
              >
            >
          >
        >
      >
      args: <
        call_expr: <
          function: "_&&_"
          args: <
            call_expr: <
              function: "_&&_"
              args: <
                call_expr: <
                  target: <
                    ident_expr: <
                      name: "path"
                    >
                  >
                  function: "startsWith"
                  args: <
                    const_expr: <
                      string_value: "/admin"
                    >
                  >
                >
              >
              args: <
                call_expr: <
                  function: "_==_"
                  args: <
                    ident_expr: <
                      name: "token"
                    >
                  >
                  args: <
                    const_expr: <
                      string_value: "admin"
                    >
                  >
                >
              >
            >
          >
          args: <
            call_expr: <
              function: "@in"
              args: <
                ident_expr: <
                  name: "ip"
                >
              >
              args: <
                list_expr: <
                  elements: <
                    const_expr: <
                      string_value: "10.0.1.1"
                    >
                  >
                  elements: <
                    const_expr: <
                      string_value: "10.0.1.2"
                    >
                  >
                  elements: <
                    const_expr: <
                      string_value: "10.0.1.3"
                    >
                  >
                >
              >
            >
          >
        >
      >
    >
  >
>
)";
}

// This proto is obtained from CELAstFlattenedMap by replacing "ip", "token",
// and "path" idents with selector expressions for "request.ip",
// "request.token", and "request.path".
std::string CELAst() {
  return R"(
call_expr: <
  function: "_&&_"
  args: <
    call_expr: <
      function: "!_"
      args: <
        call_expr: <
          function: "@in"
          args: <
            select_expr: <
              operand: <
                ident_expr: <
                  name: "request"
                >
              >
              field: "ip"
            >
          >
          args: <
            list_expr: <
              elements: <
                const_expr: <
                  string_value: "10.0.1.4"
                >
              >
              elements: <
                const_expr: <
                  string_value: "10.0.1.5"
                >
              >
              elements: <
                const_expr: <
                  string_value: "10.0.1.6"
                >
              >
            >
          >
        >
      >
    >
  >
  args: <
    call_expr: <
      function: "_||_"
      args: <
        call_expr: <
          function: "_||_"
          args: <
            call_expr: <
              function: "_&&_"
              args: <
                call_expr: <
                  target: <
                    select_expr: <
                      operand: <
                        ident_expr: <
                          name: "request"
                        >
                      >
                      field: "path"
                    >
                  >
                  function: "startsWith"
                  args: <
                    const_expr: <
                      string_value: "v1"
                    >
                  >
                >
              >
              args: <
                call_expr: <
                  function: "@in"
                  args: <
                    select_expr: <
                      operand: <
                        ident_expr: <
                          name: "request"
                        >
                      >
                      field: "token"
                    >
                  >
                  args: <
                    list_expr: <
                      elements: <
                        const_expr: <
                          string_value: "v1"
                        >
                      >
                      elements: <
                        const_expr: <
                          string_value: "v2"
                        >
                      >
                      elements: <
                        const_expr: <
                          string_value: "admin"
                        >
                      >
                    >
                  >
                >
              >
            >
          >
          args: <
            call_expr: <
              function: "_&&_"
              args: <
                call_expr: <
                  target: <
                    select_expr: <
                      operand: <
                        ident_expr: <
                          name: "request"
                        >
                      >
                      field: "path"
                    >
                  >
                  function: "startsWith"
                  args: <
                    const_expr: <
                      string_value: "v2"
                    >
                  >
                >
              >
              args: <
                call_expr: <
                  function: "@in"
                  args: <
                    select_expr: <
                      operand: <
                        ident_expr: <
                          name: "request"
                        >
                      >
                      field: "token"
                    >
                  >
                  args: <
                    list_expr: <
                      elements: <
                        const_expr: <
                          string_value: "v2"
                        >
                      >
                      elements: <
                        const_expr: <
                          string_value: "admin"
                        >
                      >
                    >
                  >
                >
              >
            >
          >
        >
      >
      args: <
        call_expr: <
          function: "_&&_"
          args: <
            call_expr: <
              function: "_&&_"
              args: <
                call_expr: <
                  target: <
                    select_expr: <
                      operand: <
                        ident_expr: <
                          name: "request"
                        >
                      >
                      field: "path"
                    >
                  >
                  function: "startsWith"
                  args: <
                    const_expr: <
                      string_value: "/admin"
                    >
                  >
                >
              >
              args: <
                call_expr: <
                  function: "_==_"
                  args: <
                    select_expr: <
                      operand: <
                        ident_expr: <
                          name: "request"
                        >
                      >
                      field: "token"
                    >
                  >
                  args: <
                    const_expr: <
                      string_value: "admin"
                    >
                  >
                >
              >
            >
          >
          args: <
            call_expr: <
              function: "@in"
              args: <
                select_expr: <
                  operand: <
                    ident_expr: <
                      name: "request"
                    >
                  >
                  field: "ip"
                >
              >
              args: <
                list_expr: <
                  elements: <
                    const_expr: <
                      string_value: "10.0.1.1"
                    >
                  >
                  elements: <
                    const_expr: <
                      string_value: "10.0.1.2"
                    >
                  >
                  elements: <
                    const_expr: <
                      string_value: "10.0.1.3"
                    >
                  >
                >
              >
            >
          >
        >
      >
    >
  >
>
)";
}

const char kIP[] = "10.0.1.2";
const char kPath[] = "/admin/edit";
const char kToken[] = "admin";

ABSL_ATTRIBUTE_NOINLINE
bool NativeCheck(std::map<std::string, std::string>& attributes,
                 const std::unordered_set<std::string>& denylists,
                 const absl::node_hash_set<std::string>& allowlists) {
  auto& ip = attributes["ip"];
  auto& path = attributes["path"];
  auto& token = attributes["token"];
  if (denylists.find(ip) != denylists.end()) {
    return false;
  }
  if (absl::StartsWith(path, "v1")) {
    if (token == "v1" || token == "v2" || token == "admin") {
      return true;
    }
  } else if (absl::StartsWith(path, "v2")) {
    if (token == "v2" || token == "admin") {
      return true;
    }
  } else if (absl::StartsWith(path, "/admin")) {
    if (token == "admin") {
      if (allowlists.find(ip) != allowlists.end()) {
        return true;
      }
    }
  }
  return false;
}

void BM_PolicyNative(benchmark::State& state) {
  const auto denylists =
      std::unordered_set<std::string>{"10.0.1.4", "10.0.1.5", "10.0.1.6"};
  const auto allowlists =
      absl::node_hash_set<std::string>{"10.0.1.1", "10.0.1.2", "10.0.1.3"};
  auto attributes = std::map<std::string, std::string>{
      {"ip", kIP}, {"token", kToken}, {"path", kPath}};
  for (auto _ : state) {
    auto result = NativeCheck(attributes, denylists, allowlists);
    ASSERT_TRUE(result);
  }
}

BENCHMARK(BM_PolicyNative);

/*
  Evaluates an expression:

  !(ip in ["10.0.1.4", "10.0.1.5", "10.0.1.6"]) &&
   (
    (path.startsWith("v1") && token in ["v1", "v2", "admin"]) ||
    (path.startsWith("v2") && token in ["v2", "admin"]) ||
    (path.startsWith("/admin") && token == "admin" && ip in ["10.0.1.1",
  "10.0.1.2", "10.0.1.3"])
   )
*/
void BM_PolicySymbolic(benchmark::State& state) {
  google::protobuf::Arena arena;
  Expr expr;
  google::protobuf::TextFormat::ParseFromString(CELAstFlattenedMap(), &expr);

  InterpreterOptions options;
  options.constant_folding = true;
  options.constant_arena = &arena;

  auto builder = CreateCelExpressionBuilder(options);
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  SourceInfo source_info;
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&expr, &source_info));

  Activation activation;
  activation.InsertValue("ip", CelValue::CreateStringView(kIP));
  activation.InsertValue("path", CelValue::CreateStringView(kPath));
  activation.InsertValue("token", CelValue::CreateStringView(kToken));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.BoolOrDie());
  }
}

BENCHMARK(BM_PolicySymbolic);

class RequestMap : public CelMap {
 public:
  absl::optional<CelValue> operator[](CelValue key) const override {
    if (!key.IsString()) {
      return {};
    }
    auto value = key.StringOrDie().value();
    if (value == "ip") {
      return CelValue::CreateStringView(kIP);
    } else if (value == "path") {
      return CelValue::CreateStringView(kPath);
    } else if (value == "token") {
      return CelValue::CreateStringView(kToken);
    }
    return {};
  }
  int size() const override { return 3; }
  const CelList* ListKeys() const override { return nullptr; }
};

// Uses a lazily constructed map container for "ip", "path", and "token".
void BM_PolicySymbolicMap(benchmark::State& state) {
  google::protobuf::Arena arena;
  Expr expr;
  google::protobuf::TextFormat::ParseFromString(CELAst(), &expr);

  auto builder = CreateCelExpressionBuilder();
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  SourceInfo source_info;
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&expr, &source_info));

  Activation activation;
  RequestMap request;
  activation.InsertValue("request", CelValue::CreateMap(&request));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.BoolOrDie());
  }
}

BENCHMARK(BM_PolicySymbolicMap);

// Uses a protobuf container for "ip", "path", and "token".
void BM_PolicySymbolicProto(benchmark::State& state) {
  google::protobuf::Arena arena;
  Expr expr;
  google::protobuf::TextFormat::ParseFromString(CELAst(), &expr);

  auto builder = CreateCelExpressionBuilder();
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));

  SourceInfo source_info;
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&expr, &source_info));

  Activation activation;
  RequestContext request;
  request.set_ip(kIP);
  request.set_path(kPath);
  request.set_token(kToken);
  activation.InsertValue("request",
                         CelProtoWrapper::CreateMessage(&request, &arena));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.BoolOrDie());
  }
}

BENCHMARK(BM_PolicySymbolicProto);

constexpr char kListSum[] = R"(
id: 1
comprehension_expr: <
  accu_var: "__result__"
  iter_var: "x"
  iter_range: <
    id: 2
    ident_expr: <
      name: "list"
    >
  >
  accu_init: <
    id: 3
    const_expr: <
      int64_value: 0
    >
  >
  loop_step: <
    id: 4
    call_expr: <
      function: "_+_"
      args: <
        id: 5
        ident_expr: <
          name: "__result__"
        >
      >
      args: <
        id: 6
        ident_expr: <
          name: "x"
        >
      >
    >
  >
  loop_condition: <
    id: 7
    const_expr: <
      bool_value: true
    >
  >
  result: <
    id: 8
    ident_expr: <
      name: "__result__"
    >
  >
>)";

void BM_Comprehension(benchmark::State& state) {
  google::protobuf::Arena arena;
  Expr expr;
  Activation activation;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kListSum, &expr));

  int len = state.range(0);
  std::vector<CelValue> list;
  list.reserve(len);
  for (int i = 0; i < len; i++) {
    list.push_back(CelValue::CreateInt64(1));
  }

  ContainerBackedListImpl cel_list(std::move(list));
  activation.InsertValue("list", CelValue::CreateList(&cel_list));
  auto builder = CreateCelExpressionBuilder();
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&expr, nullptr));
  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsInt64());
    ASSERT_EQ(result.Int64OrDie(), len);
  }
}

BENCHMARK(BM_Comprehension)->Range(1, 1 << 20);

// has(request.path) && !has(request.ip)
constexpr char kHas[] = R"(
call_expr: <
  function: "_&&_"
  args: <
    select_expr: <
      operand: <
        ident_expr: <
          name: "request"
        >
      >
      field: "path"
      test_only: true
    >
  >
  args: <
    call_expr: <
      function: "!_"
      args: <
        select_expr: <
          operand: <
            ident_expr: <
              name: "request"
            >
          >
          field: "ip"
          test_only: true
        >
      >
    >
  >
>)";

void BM_HasMap(benchmark::State& state) {
  google::protobuf::Arena arena;
  Expr expr;
  Activation activation;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kHas, &expr));
  auto builder = CreateCelExpressionBuilder();
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&expr, nullptr));

  std::vector<std::pair<CelValue, CelValue>> map_pairs{
      {CelValue::CreateStringView("path"), CelValue::CreateStringView("path")}};
  auto cel_map =
      CreateContainerBackedMap(absl::Span<std::pair<CelValue, CelValue>>(
          map_pairs.data(), map_pairs.size()));
  activation.InsertValue("request", CelValue::CreateMap((*cel_map).get()));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsBool());
    ASSERT_TRUE(result.BoolOrDie());
  }
}

BENCHMARK(BM_HasMap);

void BM_HasProto(benchmark::State& state) {
  google::protobuf::Arena arena;
  Expr expr;
  Activation activation;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kHas, &expr));
  auto builder = CreateCelExpressionBuilder();
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&expr, nullptr));

  RequestContext request;
  request.set_path(kPath);
  request.set_token(kToken);
  activation.InsertValue("request",
                         CelProtoWrapper::CreateMessage(&request, &arena));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsBool());
    ASSERT_TRUE(result.BoolOrDie());
  }
}

BENCHMARK(BM_HasProto);

// has(request.headers.create_time) && !has(request.headers.update_time)
constexpr char kHasProtoMap[] = R"(
call_expr: <
  function: "_&&_"
  args: <
    select_expr: <
      operand: <
        select_expr: <
          operand: <
            ident_expr: <
              name: "request"
            >
          >
          field: "headers"
        >
      >
      field: "create_time"
      test_only: true
    >
  >
  args: <
    call_expr: <
      function: "!_"
      args: <
        select_expr: <
          operand: <
            select_expr: <
              operand: <
                ident_expr: <
                  name: "request"
                >
              >
              field: "headers"
            >
          >
          field: "update_time"
          test_only: true
        >
      >
    >
  >
>)";

void BM_HasProtoMap(benchmark::State& state) {
  google::protobuf::Arena arena;
  Expr expr;
  Activation activation;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kHasProtoMap, &expr));
  auto builder = CreateCelExpressionBuilder();
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&expr, nullptr));

  RequestContext request;
  request.mutable_headers()->insert({"create_time", "2021-01-01"});
  activation.InsertValue("request",
                         CelProtoWrapper::CreateMessage(&request, &arena));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsBool());
    ASSERT_TRUE(result.BoolOrDie());
  }
}

BENCHMARK(BM_HasProtoMap);

// request.headers.create_time == "2021-01-01"
constexpr char kReadProtoMap[] = R"(
call_expr: <
  function: "_==_"
  args: <
    select_expr: <
      operand: <
        select_expr: <
          operand: <
            ident_expr: <
              name: "request"
            >
          >
          field: "headers"
        >
      >
      field: "create_time"
    >
  >
  args: <
    const_expr: <
      string_value: "2021-01-01"
    >
  >
>)";

void BM_ReadProtoMap(benchmark::State& state) {
  google::protobuf::Arena arena;
  Expr expr;
  Activation activation;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kReadProtoMap, &expr));
  auto builder = CreateCelExpressionBuilder();
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&expr, nullptr));

  RequestContext request;
  request.mutable_headers()->insert({"create_time", "2021-01-01"});
  activation.InsertValue("request",
                         CelProtoWrapper::CreateMessage(&request, &arena));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsBool());
    ASSERT_TRUE(result.BoolOrDie());
  }
}

BENCHMARK(BM_ReadProtoMap);

// Sum a square with a nested comprehension
constexpr char kNestedListSum[] = R"(
id: 1
comprehension_expr: <
  accu_var: "__result__"
  iter_var: "x"
  iter_range: <
    id: 2
    ident_expr: <
      name: "list"
    >
  >
  accu_init: <
    id: 3
    const_expr: <
      int64_value: 0
    >
  >
  loop_step: <
    id: 4
    call_expr: <
      function: "_+_"
      args: <
        id: 5
        ident_expr: <
          name: "__result__"
        >
      >
      args: <
        id: 6
        comprehension_expr: <
          accu_var: "__result__"
          iter_var: "x"
          iter_range: <
            id: 9
            ident_expr: <
              name: "list"
            >
          >
          accu_init: <
            id: 10
            const_expr: <
              int64_value: 0
            >
          >
          loop_step: <
            id: 11
            call_expr: <
              function: "_+_"
              args: <
                id: 12
                ident_expr: <
                  name: "__result__"
                >
              >
              args: <
                id: 13
                ident_expr: <
                  name: "x"
                >
              >
            >
          >
          loop_condition: <
            id: 14
            const_expr: <
              bool_value: true
            >
          >
          result: <
            id: 15
            ident_expr: <
              name: "__result__"
            >
          >
        >
      >
    >
  >
  loop_condition: <
    id: 7
    const_expr: <
      bool_value: true
    >
  >
  result: <
    id: 8
    ident_expr: <
      name: "__result__"
    >
  >
>)";

void BM_NestedComprehension(benchmark::State& state) {
  google::protobuf::Arena arena;
  Expr expr;
  Activation activation;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(kNestedListSum, &expr));

  int len = state.range(0);
  std::vector<CelValue> list;
  list.reserve(len);
  for (int i = 0; i < len; i++) {
    list.push_back(CelValue::CreateInt64(1));
  }

  ContainerBackedListImpl cel_list(std::move(list));
  activation.InsertValue("list", CelValue::CreateList(&cel_list));
  auto builder = CreateCelExpressionBuilder();
  ASSERT_OK(RegisterBuiltinFunctions(builder->GetRegistry()));
  ASSERT_OK_AND_ASSIGN(auto cel_expr,
                       builder->CreateExpression(&expr, nullptr));

  for (auto _ : state) {
    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr->Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsInt64());
    ASSERT_EQ(result.Int64OrDie(), len * len);
  }
}

BENCHMARK(BM_NestedComprehension)->Range(1, 1 << 10);

void BM_ComprehensionCpp(benchmark::State& state) {
  google::protobuf::Arena arena;
  Expr expr;
  Activation activation;

  int len = state.range(0);
  std::vector<CelValue> list;
  list.reserve(len);
  for (int i = 0; i < len; i++) {
    list.push_back(CelValue::CreateInt64(1));
  }

  auto op = [&list]() {
    int sum = 0;
    for (const auto& value : list) {
      sum += value.Int64OrDie();
    }
    return sum;
  };
  for (auto _ : state) {
    int result = op();
    ASSERT_EQ(result, len);
  }
}

BENCHMARK(BM_ComprehensionCpp)->Range(1, 1 << 20);

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
