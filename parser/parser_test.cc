#include "parser/parser.h"

#include <list>
#include <memory>

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "common/escaping.h"
#include "parser/source_factory.h"

namespace google {
namespace api {
namespace expr {
namespace parser {
namespace {

using google::api::expr::v1alpha1::Expr;
using google::api::expr::v1alpha1::ParsedExpr;
using testing::Not;

struct TestInfo {
  TestInfo(const std::string& I, const std::string& P,
           const std::string& E = "", const std::string& L = "",
           const std::string& R = "")
      : I(I), P(P), E(E), L(L), R(R) {}

  // I contains the input expression to be parsed.
  std::string I;

  // P contains the type/id adorned debug output of the expression tree.
  std::string P;

  // E contains the expected error output for a failed parse, or "" if the parse
  // is expected to be successful.
  std::string E;

  // L contains the expected source adorned debug output of the expression tree.
  std::string L;

  // R contains the expected enriched source info output of the expression tree.
  std::string R;
};

std::vector<TestInfo> test_cases = {
    // Simple test cases we started with
    {"x * 2",
     "_*_(\n"
     "  x^#1:Expr.Ident#,\n"
     "  2^#3:int64#\n"
     ")^#2:Expr.Call#"},
    {"x * 2u",
     "_*_(\n"
     "  x^#1:Expr.Ident#,\n"
     "  2u^#3:uint64#\n"
     ")^#2:Expr.Call#"},
    {"x * 2.0",
     "_*_(\n"
     "  x^#1:Expr.Ident#,\n"
     "  2.^#3:double#\n"
     ")^#2:Expr.Call#"},
    {"\"\\u2764\"", "\"\u2764\"^#1:string#"},
    {"\"\u2764\"", "\"\u2764\"^#1:string#"},
    {"! false",
     "!_(\n"
     "  false^#2:bool#\n"
     ")^#1:Expr.Call#"},
    {"-a",
     "-_(\n"
     "  a^#2:Expr.Ident#\n"
     ")^#1:Expr.Call#"},
    {"a.b(5)",
     "a^#1:Expr.Ident#.b(\n"
     "  5^#3:int64#\n"
     ")^#2:Expr.Call#"},
    {"a[3]",
     "_[_](\n"
     "  a^#1:Expr.Ident#,\n"
     "  3^#3:int64#\n"
     ")^#2:Expr.Call#"},
    {"SomeMessage{foo: 5, bar: \"xyz\"}",
     "SomeMessage{\n"
     "  foo:5^#4:int64#^#3:Expr.CreateStruct.Entry#,\n"
     "  bar:\"xyz\"^#6:string#^#5:Expr.CreateStruct.Entry#\n"
     "}^#2:Expr.CreateStruct#"},
    {"[3, 4, 5]",
     "[\n"
     "  3^#2:int64#,\n"
     "  4^#3:int64#,\n"
     "  5^#4:int64#\n"
     "]^#1:Expr.CreateList#"},
    {"{foo: 5, bar: \"xyz\"}",
     "{\n"
     "  foo^#3:Expr.Ident#:5^#4:int64#^#2:Expr.CreateStruct.Entry#,\n"
     "  bar^#6:Expr.Ident#:\"xyz\"^#7:string#^#5:Expr.CreateStruct.Entry#\n"
     "}^#1:Expr.CreateStruct#"},
    {"a > 5 && a < 10",
     "_&&_(\n"
     "  _>_(\n"
     "    a^#1:Expr.Ident#,\n"
     "    5^#3:int64#\n"
     "  )^#2:Expr.Call#,\n"
     "  _<_(\n"
     "    a^#4:Expr.Ident#,\n"
     "    10^#6:int64#\n"
     "  )^#5:Expr.Call#\n"
     ")^#7:Expr.Call#"},
    {"a < 5 || a > 10",
     "_||_(\n"
     "  _<_(\n"
     "    a^#1:Expr.Ident#,\n"
     "    5^#3:int64#\n"
     "  )^#2:Expr.Call#,\n"
     "  _>_(\n"
     "    a^#4:Expr.Ident#,\n"
     "    10^#6:int64#\n"
     "  )^#5:Expr.Call#\n"
     ")^#7:Expr.Call#"},
    {"{", "",
     "ERROR: <input>:1:2: Syntax error: mismatched input '<EOF>' expecting "
     "{'[', "
     "'{', '}', '(', '.', '-', '!', 'true', 'false', 'null', NUM_FLOAT, "
     "NUM_INT, "
     "NUM_UINT, STRING, BYTES, IDENTIFIER}\n | {\n"
     " | .^"},

    // test cases from Go
    {"\"A\"", "\"A\"^#1:string#"},
    {"true", "true^#1:bool#"},
    {"false", "false^#1:bool#"},
    {"0", "0^#1:int64#"},
    {"42", "42^#1:int64#"},
    {"0u", "0u^#1:uint64#"},
    {"23u", "23u^#1:uint64#"},
    {"24u", "24u^#1:uint64#"},
    {"-1", "-1^#1:int64#"},
    {"4--4",
     "_-_(\n"
     "  4^#1:int64#,\n"
     "  -4^#3:int64#\n"
     ")^#2:Expr.Call#"},
    {"4--4.1",
     "_-_(\n"
     "  4^#1:int64#,\n"
     "  -4.1^#3:double#\n"
     ")^#2:Expr.Call#"},
    {"b\"abc\"", "b\"abc\"^#1:bytes#"},
    {"23.39", "23.39^#1:double#"},
    {"!a",
     "!_(\n"
     "  a^#2:Expr.Ident#\n"
     ")^#1:Expr.Call#"},
    {"null", "null^#1:NullValue#"},
    {"a", "a^#1:Expr.Ident#"},
    {"a?b:c",
     "_?_:_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#,\n"
     "  c^#4:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {"a || b",
     "_||_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#2:Expr.Ident#\n"
     ")^#3:Expr.Call#"},
    {"a || b || c || d || e || f ",
     "_||_(\n"
     "  _||_(\n"
     "    _||_(\n"
     "      a^#1:Expr.Ident#,\n"
     "      b^#2:Expr.Ident#\n"
     "    )^#3:Expr.Call#,\n"
     "    c^#4:Expr.Ident#\n"
     "  )^#5:Expr.Call#,\n"
     "  _||_(\n"
     "    _||_(\n"
     "      d^#6:Expr.Ident#,\n"
     "      e^#8:Expr.Ident#\n"
     "    )^#9:Expr.Call#,\n"
     "    f^#10:Expr.Ident#\n"
     "  )^#11:Expr.Call#\n"
     ")^#7:Expr.Call#"},
    {"a && b",
     "_&&_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#2:Expr.Ident#\n"
     ")^#3:Expr.Call#"},
    {"a && b && c && d && e && f && g",
     "_&&_(\n"
     "  _&&_(\n"
     "    _&&_(\n"
     "      a^#1:Expr.Ident#,\n"
     "      b^#2:Expr.Ident#\n"
     "    )^#3:Expr.Call#,\n"
     "    _&&_(\n"
     "      c^#4:Expr.Ident#,\n"
     "      d^#6:Expr.Ident#\n"
     "    )^#7:Expr.Call#\n"
     "  )^#5:Expr.Call#,\n"
     "  _&&_(\n"
     "    _&&_(\n"
     "      e^#8:Expr.Ident#,\n"
     "      f^#10:Expr.Ident#\n"
     "    )^#11:Expr.Call#,\n"
     "    g^#12:Expr.Ident#\n"
     "  )^#13:Expr.Call#\n"
     ")^#9:Expr.Call#"},
    {"a && b && c && d || e && f && g && h",
     "_||_(\n"
     "  _&&_(\n"
     "    _&&_(\n"
     "      a^#1:Expr.Ident#,\n"
     "      b^#2:Expr.Ident#\n"
     "    )^#3:Expr.Call#,\n"
     "    _&&_(\n"
     "      c^#4:Expr.Ident#,\n"
     "      d^#6:Expr.Ident#\n"
     "    )^#7:Expr.Call#\n"
     "  )^#5:Expr.Call#,\n"
     "  _&&_(\n"
     "    _&&_(\n"
     "      e^#8:Expr.Ident#,\n"
     "      f^#9:Expr.Ident#\n"
     "    )^#10:Expr.Call#,\n"
     "    _&&_(\n"
     "      g^#11:Expr.Ident#,\n"
     "      h^#13:Expr.Ident#\n"
     "    )^#14:Expr.Call#\n"
     "  )^#12:Expr.Call#\n"
     ")^#15:Expr.Call#"},
    {"a + b",
     "_+_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {"a - b",
     "_-_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {"a * b",
     "_*_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {"a / b",
     "_/_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {
        "a % b",
        "_%_(\n"
        "  a^#1:Expr.Ident#,\n"
        "  b^#3:Expr.Ident#\n"
        ")^#2:Expr.Call#",
    },
    {"a in b",
     "@in(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {"a == b",
     "_==_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {"a != b",
     "_!=_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {"a > b",
     "_>_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {"a >= b",
     "_>=_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {"a < b",
     "_<_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {"a <= b",
     "_<=_(\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {"a.b", "a^#1:Expr.Ident#.b^#2:Expr.Select#"},
    {"a.b.c", "a^#1:Expr.Ident#.b^#2:Expr.Select#.c^#3:Expr.Select#"},
    {"a[b]",
     "_[_](\n"
     "  a^#1:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#\n"
     ")^#2:Expr.Call#"},
    {"foo{ }", "foo{}^#2:Expr.CreateStruct#"},
    {"foo{ a:b }",
     "foo{\n"
     "  a:b^#4:Expr.Ident#^#3:Expr.CreateStruct.Entry#\n"
     "}^#2:Expr.CreateStruct#"},
    {"foo{ a:b, c:d }",
     "foo{\n"
     "  a:b^#4:Expr.Ident#^#3:Expr.CreateStruct.Entry#,\n"
     "  c:d^#6:Expr.Ident#^#5:Expr.CreateStruct.Entry#\n"
     "}^#2:Expr.CreateStruct#"},
    {"{}", "{}^#1:Expr.CreateStruct#"},
    {"{a:b, c:d}",
     "{\n"
     "  a^#3:Expr.Ident#:b^#4:Expr.Ident#^#2:Expr.CreateStruct.Entry#,\n"
     "  c^#6:Expr.Ident#:d^#7:Expr.Ident#^#5:Expr.CreateStruct.Entry#\n"
     "}^#1:Expr.CreateStruct#"},
    {"[]", "[]^#1:Expr.CreateList#"},
    {"[a]",
     "[\n"
     "  a^#2:Expr.Ident#\n"
     "]^#1:Expr.CreateList#"},
    {"[a, b, c]",
     "[\n"
     "  a^#2:Expr.Ident#,\n"
     "  b^#3:Expr.Ident#,\n"
     "  c^#4:Expr.Ident#\n"
     "]^#1:Expr.CreateList#"},
    {"(a)", "a^#1:Expr.Ident#"},
    {"((a))", "a^#1:Expr.Ident#"},
    {"a()", "a()^#1:Expr.Call#"},
    {"a(b)",
     "a(\n"
     "  b^#2:Expr.Ident#\n"
     ")^#1:Expr.Call#"},
    {"a(b, c)",
     "a(\n"
     "  b^#2:Expr.Ident#,\n"
     "  c^#3:Expr.Ident#\n"
     ")^#1:Expr.Call#"},
    {"a.b()", "a^#1:Expr.Ident#.b()^#2:Expr.Call#"},
    {
        "a.b(c)",
        "a^#1:Expr.Ident#.b(\n"
        "  c^#3:Expr.Ident#\n"
        ")^#2:Expr.Call#",
        /* E */ "",
        "a^#1[1,0]#.b(\n"
        "  c^#3[1,4]#\n"
        ")^#2[1,3]#",
        "[1,0,0]^#[2,3,3]^#[3,4,4]",
    },
    {
        "aaa.bbb(ccc)",
        "aaa^#1:Expr.Ident#.bbb(\n"
        "  ccc^#3:Expr.Ident#\n"
        ")^#2:Expr.Call#",
        /* E */ "",
        "aaa^#1[1,0]#.bbb(\n"
        "  ccc^#3[1,8]#\n"
        ")^#2[1,7]#",
        "[1,0,2]^#[2,7,7]^#[3,8,10]",
    },

    // Parse error tests
    {"*@a | b", "",
     "ERROR: <input>:1:2: Syntax error: token recognition error at: '@'\n"
     " | *@a | b\n"
     " | .^\n"
     "ERROR: <input>:1:1: Syntax error: extraneous input '*' expecting {'[', "
     "'{', "
     "'(', '.', '-', '!', 'true', 'false', 'null', NUM_FLOAT, NUM_INT, "
     "NUM_UINT, STRING, BYTES, IDENTIFIER}\n"
     " | *@a | b\n"
     " | ^\n"
     "ERROR: <input>:1:5: Syntax error: token recognition error at: '| '\n"
     " | *@a | b\n"
     " | ....^\n"
     "ERROR: <input>:1:7: Syntax error: extraneous input 'b' expecting <EOF>\n"
     " | *@a | b\n"
     " | ......^"},
    {"a | b", "",
     "ERROR: <input>:1:3: Syntax error: token recognition error at: '| '\n"
     " | a | b\n"
     " | ..^\n"
     "ERROR: <input>:1:5: Syntax error: extraneous input 'b' expecting <EOF>\n"
     " | a | b\n"
     " | ....^"},

    // Macro tests
    {
        "has(m.f)",
        "m^#2:Expr.Ident#.f~test-only~^#4:Expr.Select#",
        "",
        "m^#2[1,4]#.f~test-only~^#4[1,3]#",
        "[1,3,3]^#[2,4,4]^#[3,5,5]^#[4,3,3]",
    },
    {"m.exists_one(v, f)",
     "__comprehension__(\n"
     "  // Variable\n"
     "  v,\n"
     "  // Target\n"
     "  m^#1:Expr.Ident#,\n"
     "  // Accumulator\n"
     "  __result__,\n"
     "  // Init\n"
     "  0^#5:int64#,\n"
     "  // LoopCondition\n"
     "  _<=_(\n"
     "    __result__^#7:Expr.Ident#,\n"
     "    1^#6:int64#\n"
     "  )^#8:Expr.Call#,\n"
     "  // LoopStep\n"
     "  _?_:_(\n"
     "    f^#4:Expr.Ident#,\n"
     "    _+_(\n"
     "      __result__^#9:Expr.Ident#,\n"
     "      1^#6:int64#\n"
     "    )^#10:Expr.Call#,\n"
     "    __result__^#11:Expr.Ident#\n"
     "  )^#12:Expr.Call#,\n"
     "  // Result\n"
     "  _==_(\n"
     "    __result__^#13:Expr.Ident#,\n"
     "    1^#6:int64#\n"
     "  )^#14:Expr.Call#)^#15:Expr.Comprehension#"},
    {"m.map(v, f)",
     "__comprehension__(\n"
     "  // Variable\n"
     "  v,\n"
     "  // Target\n"
     "  m^#1:Expr.Ident#,\n"
     "  // Accumulator\n"
     "  __result__,\n"
     "  // Init\n"
     "  []^#6:Expr.CreateList#,\n"
     "  // LoopCondition\n"
     "  true^#7:bool#,\n"
     "  // LoopStep\n"
     "  _+_(\n"
     "    __result__^#5:Expr.Ident#,\n"
     "    [\n"
     "      f^#4:Expr.Ident#\n"
     "    ]^#8:Expr.CreateList#\n"
     "  )^#9:Expr.Call#,\n"
     "  // Result\n"
     "  __result__^#5:Expr.Ident#)^#10:Expr.Comprehension#"},
    {"m.map(v, p, f)",
     "__comprehension__(\n"
     "  // Variable\n"
     "  v,\n"
     "  // Target\n"
     "  m^#1:Expr.Ident#,\n"
     "  // Accumulator\n"
     "  __result__,\n"
     "  // Init\n"
     "  []^#7:Expr.CreateList#,\n"
     "  // LoopCondition\n"
     "  true^#8:bool#,\n"
     "  // LoopStep\n"
     "  _?_:_(\n"
     "    p^#4:Expr.Ident#,\n"
     "    _+_(\n"
     "      __result__^#6:Expr.Ident#,\n"
     "      [\n"
     "        f^#5:Expr.Ident#\n"
     "      ]^#9:Expr.CreateList#\n"
     "    )^#10:Expr.Call#,\n"
     "    __result__^#6:Expr.Ident#\n"
     "  )^#11:Expr.Call#,\n"
     "  // Result\n"
     "  __result__^#6:Expr.Ident#)^#12:Expr.Comprehension#"},
    {"m.filter(v, p)",
     "__comprehension__(\n"
     "  // Variable\n"
     "  v,\n"
     "  // Target\n"
     "  m^#1:Expr.Ident#,\n"
     "  // Accumulator\n"
     "  __result__,\n"
     "  // Init\n"
     "  []^#6:Expr.CreateList#,\n"
     "  // LoopCondition\n"
     "  true^#7:bool#,\n"
     "  // LoopStep\n"
     "  _?_:_(\n"
     "    p^#4:Expr.Ident#,\n"
     "    _+_(\n"
     "      __result__^#5:Expr.Ident#,\n"
     "      [\n"
     "        v^#3:Expr.Ident#\n"
     "      ]^#8:Expr.CreateList#\n"
     "    )^#9:Expr.Call#,\n"
     "    __result__^#5:Expr.Ident#\n"
     "  )^#10:Expr.Call#,\n"
     "  // Result\n"
     "  __result__^#5:Expr.Ident#)^#11:Expr.Comprehension#"},

    // Tests from Java parser
    {"[] + [1,2,3,] + [4]",
     "_+_(\n"
     "  _+_(\n"
     "    []^#1:Expr.CreateList#,\n"
     "    [\n"
     "      1^#4:int64#,\n"
     "      2^#5:int64#,\n"
     "      3^#6:int64#\n"
     "    ]^#3:Expr.CreateList#\n"
     "  )^#2:Expr.Call#,\n"
     "  [\n"
     "    4^#9:int64#\n"
     "  ]^#8:Expr.CreateList#\n"
     ")^#7:Expr.Call#"},
    {"{1:2u, 2:3u}",
     "{\n"
     "  1^#3:int64#:2u^#4:uint64#^#2:Expr.CreateStruct.Entry#,\n"
     "  2^#6:int64#:3u^#7:uint64#^#5:Expr.CreateStruct.Entry#\n"
     "}^#1:Expr.CreateStruct#"},
    {"TestAllTypes{single_int32: 1, single_int64: 2}",
     "TestAllTypes{\n"
     "  single_int32:1^#4:int64#^#3:Expr.CreateStruct.Entry#,\n"
     "  single_int64:2^#6:int64#^#5:Expr.CreateStruct.Entry#\n"
     "}^#2:Expr.CreateStruct#"},
    {"TestAllTypes(){single_int32: 1, single_int64: 2}", "",
     "ERROR: <input>:1:13: expected a qualified name\n"
     " | TestAllTypes(){single_int32: 1, single_int64: 2}\n"
     " | ............^"},
    {"size(x) == x.size()",
     "_==_(\n"
     "  size(\n"
     "    x^#2:Expr.Ident#\n"
     "  )^#1:Expr.Call#,\n"
     "  x^#4:Expr.Ident#.size()^#5:Expr.Call#\n"
     ")^#3:Expr.Call#"},
    {"1 + $", "",
     "ERROR: <input>:1:5: Syntax error: token recognition error at: '$'\n"
     " | 1 + $\n"
     " | ....^\n"
     "ERROR: <input>:1:6: Syntax error: mismatched input '<EOF>' expecting "
     "{'[', "
     "'{', '(', '.', '-', '!', 'true', 'false', 'null', NUM_FLOAT, NUM_INT, "
     "NUM_UINT, STRING, BYTES, IDENTIFIER}\n"
     " | 1 + $\n"
     " | .....^"},
    {"1 + 2\n"
     "3 +",
     "",
     "ERROR: <input>:2:1: Syntax error: mismatched input '3' expecting <EOF>\n"
     " | 3 +\n"
     " | ^"},
    {"\"\\\"\"", "\"\\\"\"^#1:string#"},
    {"[1,3,4][0]",
     "_[_](\n"
     "  [\n"
     "    1^#2:int64#,\n"
     "    3^#3:int64#,\n"
     "    4^#4:int64#\n"
     "  ]^#1:Expr.CreateList#,\n"
     "  0^#6:int64#\n"
     ")^#5:Expr.Call#"},
    {"1.all(2, 3)", "",
     "ERROR: <input>:1:7: argument must be a simple name\n"
     " | 1.all(2, 3)\n"
     " | ......^"},
    {"x[\"a\"].single_int32 == 23",
     "_==_(\n"
     "  _[_](\n"
     "    x^#1:Expr.Ident#,\n"
     "    \"a\"^#3:string#\n"
     "  )^#2:Expr.Call#.single_int32^#4:Expr.Select#,\n"
     "  23^#6:int64#\n"
     ")^#5:Expr.Call#"},
    {"x.single_nested_message != null",
     "_!=_(\n"
     "  x^#1:Expr.Ident#.single_nested_message^#2:Expr.Select#,\n"
     "  null^#4:NullValue#\n"
     ")^#3:Expr.Call#"},
    {"false && !true || false ? 2 : 3",
     "_?_:_(\n"
     "  _||_(\n"
     "    _&&_(\n"
     "      false^#1:bool#,\n"
     "      !_(\n"
     "        true^#3:bool#\n"
     "      )^#2:Expr.Call#\n"
     "    )^#4:Expr.Call#,\n"
     "    false^#5:bool#\n"
     "  )^#6:Expr.Call#,\n"
     "  2^#8:int64#,\n"
     "  3^#9:int64#\n"
     ")^#7:Expr.Call#"},
    {"b\"abc\" + B\"def\"",
     "_+_(\n"
     "  b\"abc\"^#1:bytes#,\n"
     "  b\"def\"^#3:bytes#\n"
     ")^#2:Expr.Call#"},
    {"1 + 2 * 3 - 1 / 2 == 6 % 1",
     "_==_(\n"
     "  _-_(\n"
     "    _+_(\n"
     "      1^#1:int64#,\n"
     "      _*_(\n"
     "        2^#3:int64#,\n"
     "        3^#5:int64#\n"
     "      )^#4:Expr.Call#\n"
     "    )^#2:Expr.Call#,\n"
     "    _/_(\n"
     "      1^#7:int64#,\n"
     "      2^#9:int64#\n"
     "    )^#8:Expr.Call#\n"
     "  )^#6:Expr.Call#,\n"
     "  _%_(\n"
     "    6^#11:int64#,\n"
     "    1^#13:int64#\n"
     "  )^#12:Expr.Call#\n"
     ")^#10:Expr.Call#"},
    {"1 + +", "",
     "ERROR: <input>:1:5: Syntax error: mismatched input '+' expecting {'[', "
     "'{',"
     " '(', '.', '-', '!', 'true', 'false', 'null', NUM_FLOAT, NUM_INT, "
     "NUM_UINT,"
     " STRING, BYTES, IDENTIFIER}\n"
     " | 1 + +\n"
     " | ....^\n"
     "ERROR: <input>:1:6: Syntax error: mismatched input '<EOF>' expecting "
     "{'[', "
     "'{', '(', '.', '-', '!', 'true', 'false', 'null', NUM_FLOAT, NUM_INT, "
     "NUM_UINT, STRING, BYTES, IDENTIFIER}\n"
     " | 1 + +\n"
     " | .....^"},
    {"\"abc\" + \"def\"",
     "_+_(\n"
     "  \"abc\"^#1:string#,\n"
     "  \"def\"^#3:string#\n"
     ")^#2:Expr.Call#"},
    {"{\"a\": 1}.\"a\"", "",
     "ERROR: <input>:1:10: Syntax error: mismatched input '\"a\"' "
     "expecting IDENTIFIER\n"
     " | {\"a\": 1}.\"a\"\n"
     " | .........^"},
    {"\"\\xC3\\XBF\"", "\"Ã¿\"^#1:string#"},
    {"\"\\303\\277\"", "\"Ã¿\"^#1:string#"},
    {"\"hi\\u263A \\u263Athere\"", "\"hi☺ ☺there\"^#1:string#"},
    {"\"\\U000003A8\\?\"", "\"Ψ?\"^#1:string#"},
    {"\"\\a\\b\\f\\n\\r\\t\\v'\\\"\\\\\\? Legal escapes\"",
     "\"\\a\\b\\f\\n\\r\\t\\v'\\\"\\? Legal escapes\"^#1:string#"},
    {"\"\\xFh\"", "",
     "ERROR: <input>:1:1: Syntax error: token recognition error at: '\"\\xFh'\n"
     " | \"\\xFh\"\n"
     " | ^\n"
     "ERROR: <input>:1:6: Syntax error: token recognition error at: '\"'\n"
     " | \"\\xFh\"\n"
     " | .....^\n"
     "ERROR: <input>:1:7: Syntax error: mismatched input '<EOF>' expecting "
     "{'[', "
     "'{', '(', '.', '-', '!', 'true', 'false', 'null', NUM_FLOAT, NUM_INT, "
     "NUM_UINT, STRING, BYTES, IDENTIFIER}\n"
     " | \"\\xFh\"\n"
     " | ......^"},
    {"\"\\a\\b\\f\\n\\r\\t\\v\\'\\\"\\\\\\? Illegal escape \\>\"", "",
     "ERROR: <input>:1:1: Syntax error: token recognition error at: "
     "'\"\\a\\b\\f\\n\\r\\t\\v\\'\\\"\\\\\\? Illegal escape \\>'\n"
     " | \"\\a\\b\\f\\n\\r\\t\\v\\'\\\"\\\\\\? Illegal escape \\>\"\n"
     " | ^\n"
     "ERROR: <input>:1:42: Syntax error: token recognition error at: '\"'\n"
     " | \"\\a\\b\\f\\n\\r\\t\\v\\'\\\"\\\\\\? Illegal escape \\>\"\n"
     " | .........................................^\n"
     "ERROR: <input>:1:43: Syntax error: mismatched input '<EOF>' expecting "
     "{'[',"
     " '{', '(', '.', '-', '!', 'true', 'false', 'null', NUM_FLOAT, NUM_INT, "
     "NUM_UINT, STRING, BYTES, IDENTIFIER}\n"
     " | \"\\a\\b\\f\\n\\r\\t\\v\\'\\\"\\\\\\? Illegal escape \\>\"\n"
     " | ..........................................^"},
    {"'😁' in ['😁', '😑', '😦']",
     "@in(\n"
     "  \"😁\"^#1:string#,\n"
     "  [\n"
     "    \"😁\"^#4:string#,\n"
     "    \"😑\"^#5:string#,\n"
     "    \"😦\"^#6:string#\n"
     "  ]^#3:Expr.CreateList#\n"
     ")^#2:Expr.Call#"},
    {"'😁' in ['😁', '😑', '😦']\n"
     "   && in.😁",
     "",
     "ERROR: <input>:2:7: Syntax error: extraneous input 'in' expecting {'[', "
     "'{', '(', '.', '-', '!', 'true', 'false', 'null', NUM_FLOAT, NUM_INT, "
     "NUM_UINT, STRING, BYTES, IDENTIFIER}\n"
     " |    && in.😁\n"
     " | ......^\n"
     "ERROR: <input>:2:10: Syntax error: token recognition error at: '😁'\n"
     " |    && in.😁\n"
     " | .........^\n"
     "ERROR: <input>:2:11: Syntax error: missing IDENTIFIER at '<EOF>'\n"
     " |    && in.😁\n"
     " | ..........^"},
    {"as", "",
     "ERROR: <input>:1:1: reserved identifier: as\n"
     " | as\n"
     " | ^"},
    {"break", "",
     "ERROR: <input>:1:1: reserved identifier: break\n"
     " | break\n"
     " | ^"},
    {"const", "",
     "ERROR: <input>:1:1: reserved identifier: const\n"
     " | const\n"
     " | ^"},
    {"continue", "",
     "ERROR: <input>:1:1: reserved identifier: continue\n"
     " | continue\n"
     " | ^"},
    {"else", "",
     "ERROR: <input>:1:1: reserved identifier: else\n"
     " | else\n"
     " | ^"},
    {"for", "",
     "ERROR: <input>:1:1: reserved identifier: for\n"
     " | for\n"
     " | ^"},
    {"function", "",
     "ERROR: <input>:1:1: reserved identifier: function\n"
     " | function\n"
     " | ^"},
    {"if", "",
     "ERROR: <input>:1:1: reserved identifier: if\n"
     " | if\n"
     " | ^"},
    {"import", "",
     "ERROR: <input>:1:1: reserved identifier: import\n"
     " | import\n"
     " | ^"},
    {"in", "",
     "ERROR: <input>:1:1: Syntax error: mismatched input 'in' expecting {'[', "
     "'{', '(', '.', '-', '!', 'true', 'false', 'null', NUM_FLOAT, NUM_INT, "
     "NUM_UINT, STRING, BYTES, IDENTIFIER}\n"
     " | in\n"
     " | ^\n"
     "ERROR: <input>:1:3: Syntax error: mismatched input '<EOF>' expecting "
     "{'[', "
     "'{', '(', '.', '-', '!', 'true', 'false', 'null', NUM_FLOAT, NUM_INT, "
     "NUM_UINT, STRING, BYTES, IDENTIFIER}\n"
     " | in\n"
     " | ..^"},
    {"let", "",
     "ERROR: <input>:1:1: reserved identifier: let\n"
     " | let\n"
     " | ^"},
    {"loop", "",
     "ERROR: <input>:1:1: reserved identifier: loop\n"
     " | loop\n"
     " | ^"},
    {"package", "",
     "ERROR: <input>:1:1: reserved identifier: package\n"
     " | package\n"
     " | ^"},
    {"namespace", "",
     "ERROR: <input>:1:1: reserved identifier: namespace\n"
     " | namespace\n"
     " | ^"},
    {"return", "",
     "ERROR: <input>:1:1: reserved identifier: return\n"
     " | return\n"
     " | ^"},
    {"var", "",
     "ERROR: <input>:1:1: reserved identifier: var\n"
     " | var\n"
     " | ^"},
    {"void", "",
     "ERROR: <input>:1:1: reserved identifier: void\n"
     " | void\n"
     " | ^"},
    {"while", "",
     "ERROR: <input>:1:1: reserved identifier: while\n"
     " | while\n"
     " | ^"},
    {"[1, 2, 3].map(var, var * var)", "",
     "ERROR: <input>:1:15: reserved identifier: var\n"
     " | [1, 2, 3].map(var, var * var)\n"
     " | ..............^\n"
     "ERROR: <input>:1:20: reserved identifier: var\n"
     " | [1, 2, 3].map(var, var * var)\n"
     " | ...................^\n"
     "ERROR: <input>:1:26: reserved identifier: var\n"
     " | [1, 2, 3].map(var, var * var)\n"
     " | .........................^\n"
     "ERROR: <input>:1:15: argument is not an identifier\n"
     " | [1, 2, 3].map(var, var * var)\n"
     " | ..............^"}};

class ExpressionAdorner {
 public:
  virtual ~ExpressionAdorner() {}
  virtual std::string adorn(const Expr& e) const = 0;
  virtual std::string adorn(const Expr::CreateStruct::Entry& e) const = 0;
};

class KindAndIdAdorner : public ExpressionAdorner {
 public:
  std::string adorn(const Expr& e) const override {
    if (e.has_const_expr()) {
      auto& const_expr = e.const_expr();
      auto reflection = const_expr.GetReflection();
      auto oneof = const_expr.GetDescriptor()->FindOneofByName("constant_kind");
      auto field_desc = reflection->GetOneofFieldDescriptor(const_expr, oneof);
      auto enum_desc = field_desc->enum_type();
      if (enum_desc) {
        return absl::StrFormat("^#%d:%s#", e.id(), nameChain(enum_desc));
      } else {
        return absl::StrFormat("^#%d:%s#", e.id(), field_desc->type_name());
      }
    } else {
      auto reflection = e.GetReflection();
      auto oneof = e.GetDescriptor()->FindOneofByName("expr_kind");
      auto desc = reflection->GetOneofFieldDescriptor(e, oneof)->message_type();
      return absl::StrFormat("^#%d:%s#", e.id(), nameChain(desc));
    }
  }

  std::string adorn(const Expr::CreateStruct::Entry& e) const override {
    return absl::StrFormat("^#%d:Expr.CreateStruct.Entry#", e.id());
  }

 private:
  template <class T>
  std::string nameChain(const T* descriptor) const {
    std::list<std::string> name_chain{descriptor->name()};
    const google::protobuf::Descriptor* desc = descriptor->containing_type();
    while (desc) {
      name_chain.push_front(desc->name());
      desc = desc->containing_type();
    }
    return absl::StrJoin(name_chain, ".");
  }
};

class LocationAdorner : public ExpressionAdorner {
 public:
  LocationAdorner(const google::api::expr::v1alpha1::SourceInfo& source_info)
      : source_info_(source_info) {}

  absl::optional<std::pair<int32_t, int32_t>> getLocation(int64_t id) const {
    absl::optional<std::pair<int32_t, int32_t>> location;
    const auto& positions = source_info_.positions();
    if (positions.find(id) == positions.end()) {
      return location;
    }
    int32_t pos = positions.at(id);

    int32_t line = 1;
    for (int i = 0; i < source_info_.line_offsets_size(); ++i) {
      if (source_info_.line_offsets(i) > pos) {
        break;
      } else {
        line += 1;
      }
    }
    int32_t col = pos;
    if (line > 1) {
      col = pos - source_info_.line_offsets(line - 2);
    }
    return std::make_pair(line, col);
  }

  std::string adorn(const Expr& e) const override {
    auto loc = getLocation(e.id());
    if (loc) {
      return absl::StrFormat("^#%d[%d,%d]#", e.id(), loc->first, loc->second);
    } else {
      return absl::StrFormat("^#%d[NO_POS]#", e.id());
    }
  }

  std::string adorn(const Expr::CreateStruct::Entry& e) const override {
    auto loc = getLocation(e.id());
    if (loc) {
      return absl::StrFormat("^#%d[%d,%d]#", e.id(), loc->first, loc->second);
    } else {
      return absl::StrFormat("^#%d[NO_POS]#", e.id());
    }
  }

 private:
  template <class T>
  std::string nameChain(const T* descriptor) const {
    std::list<std::string> name_chain{descriptor->name()};
    const google::protobuf::Descriptor* desc = descriptor->containing_type();
    while (desc) {
      name_chain.push_front(desc->name());
      desc = desc->containing_type();
    }
    return absl::StrJoin(name_chain, ".");
  }

 private:
  const google::api::expr::v1alpha1::SourceInfo& source_info_;
};

class DebugWriter {
 public:
  DebugWriter(const ExpressionAdorner& adorner)
      : adorner_(adorner), line_start_(true), indent_(0) {}

  void appendExpr(const Expr& e) {
    switch (e.expr_kind_case()) {
      case Expr::kConstExpr:
        append(formatLiteral(e.const_expr()));
        break;
      case Expr::kIdentExpr:
        append(e.ident_expr().name());
        break;
      case Expr::kSelectExpr:
        appendSelect(e.select_expr());
        break;
      case Expr::kCallExpr:
        appendCall(e.call_expr());
        break;
      case Expr::kListExpr:
        appendList(e.list_expr());
        break;
      case Expr::kStructExpr:
        appendStruct(e.struct_expr());
        break;
      case Expr::kComprehensionExpr:
        appendComprehension(e.comprehension_expr());
        break;
      default:
        break;
    }
    appendAdorn(e);
  }

  void appendSelect(const Expr::Select& sel) {
    appendExpr(sel.operand());
    append(".");
    append(sel.field());
    if (sel.test_only()) {
      append("~test-only~");
    }
  }

  void appendCall(const Expr::Call& call) {
    if (call.has_target()) {
      appendExpr(call.target());
      s_ += ".";
    }
    append(call.function());
    append("(");
    if (call.args_size() > 0) {
      addIndent();
      appendLine();
      for (int i = 0; i < call.args_size(); ++i) {
        const auto& arg = call.args(i);
        if (i > 0) {
          append(",");
          appendLine();
        }
        appendExpr(arg);
      }
      removeIndent();
      appendLine();
    }
    append(")");
  }

  void appendList(const Expr::CreateList& list) {
    append("[");
    if (list.elements_size() > 0) {
      appendLine();
      addIndent();
      for (int i = 0; i < list.elements_size(); ++i) {
        const auto& elem = list.elements(i);
        if (i > 0) {
          append(",");
          appendLine();
        }
        appendExpr(elem);
      }
      removeIndent();
      appendLine();
    }
    append("]");
  }

  void appendStruct(const Expr::CreateStruct& obj) {
    if (obj.message_name().empty()) {
      appendMap(obj);
    } else {
      appendObject(obj);
    }
  }

  void appendMap(const Expr::CreateStruct& obj) {
    append("{");
    if (obj.entries_size() > 0) {
      appendLine();
      addIndent();
      for (int i = 0; i < obj.entries_size(); ++i) {
        const auto& entry = obj.entries(i);
        if (i > 0) {
          append(",");
          appendLine();
        }
        appendExpr(entry.map_key());
        append(":");
        appendExpr(entry.value());
        appendAdorn(entry);
      }
      removeIndent();
      appendLine();
    }
    append("}");
  }

  void appendObject(const Expr::CreateStruct& obj) {
    append(obj.message_name());
    append("{");
    if (obj.entries_size() > 0) {
      appendLine();
      addIndent();
      for (int i = 0; i < obj.entries_size(); ++i) {
        const auto& entry = obj.entries(i);
        if (i > 0) {
          append(",");
          appendLine();
        }
        append(entry.field_key());
        append(":");
        appendExpr(entry.value());
        appendAdorn(entry);
      }
      removeIndent();
      appendLine();
    }
    append("}");
  }

  void appendComprehension(const Expr::Comprehension& comprehension) {
    append("__comprehension__(");
    addIndent();
    appendLine();
    append("// Variable");
    appendLine();
    append(comprehension.iter_var());
    append(",");
    appendLine();
    append("// Target");
    appendLine();
    appendExpr(comprehension.iter_range());
    append(",");
    appendLine();
    append("// Accumulator");
    appendLine();
    append(comprehension.accu_var());
    append(",");
    appendLine();
    append("// Init");
    appendLine();
    appendExpr(comprehension.accu_init());
    append(",");
    appendLine();
    append("// LoopCondition");
    appendLine();
    appendExpr(comprehension.loop_condition());
    append(",");
    appendLine();
    append("// LoopStep");
    appendLine();
    appendExpr(comprehension.loop_step());
    append(",");
    appendLine();
    append("// Result");
    appendLine();
    appendExpr(comprehension.result());
    append(")");
    removeIndent();
  }

  void appendAdorn(const Expr& e) { append(adorner_.adorn(e)); }

  void appendAdorn(const Expr::CreateStruct::Entry& e) {
    append(adorner_.adorn(e));
  }

  void append(const std::string& s) {
    if (line_start_) {
      line_start_ = false;
      for (int i = 0; i < indent_; ++i) {
        s_ += "  ";
      }
    }
    s_ += s;
  }

  void appendLine() {
    s_ += "\n";
    line_start_ = true;
  }

  void addIndent() { indent_ += 1; }

  void removeIndent() {
    if (indent_ > 0) {
      indent_ -= 1;
    }
  }

  std::string formatLiteral(const google::api::expr::v1alpha1::Constant& c) {
    switch (c.constant_kind_case()) {
      case google::api::expr::v1alpha1::Constant::kBoolValue:
        return absl::StrFormat("%s", c.bool_value() ? "true" : "false");
      case google::api::expr::v1alpha1::Constant::kBytesValue:
        return absl::StrFormat("b\"%s\"", c.bytes_value());
      case google::api::expr::v1alpha1::Constant::kDoubleValue: {
        std::string s = absl::StrFormat("%f", c.double_value());
        // remove trailing zeros, i.e., convert 1.600000 to just 1.6 without
        // forcing a specific precision. There seems to be no flag to get this
        // directly from absl::StrFormat.
        auto idx = std::find_if_not(s.rbegin(), s.rend(),
                                    [](const char c) { return c == '0'; });
        s.erase(idx.base(), s.end());
        return s;
      }
      case google::api::expr::v1alpha1::Constant::kInt64Value:
        return absl::StrFormat("%d", c.int64_value());
      case google::api::expr::v1alpha1::Constant::kStringValue:
        return escapeAndQuote(c.string_value());
      case google::api::expr::v1alpha1::Constant::kUint64Value:
        return absl::StrFormat("%uu", c.uint64_value());
      case google::api::expr::v1alpha1::Constant::kNullValue:
        return "null";
      default:
        return "<<ERROR>>";
    }
  }

  std::string toAdornedDebugString(const ParsedExpr& parsed_expr) {
    appendExpr(parsed_expr.expr());
    return s_;
  }

 private:
  std::string s_;
  const ExpressionAdorner& adorner_;
  bool line_start_;
  int indent_;
};

std::string ConvertEnrichedSourceInfoToString(
    const EnrichedSourceInfo& enriched_source_info) {
  std::vector<std::string> offsets;
  for (const auto& offset : enriched_source_info.offsets()) {
    offsets.push_back(absl::StrFormat(
        "[%d,%d,%d]", offset.first, offset.second.first, offset.second.second));
  }
  return absl::StrJoin(offsets, "^#");
}

class ExpressionTest : public testing::TestWithParam<TestInfo> {};

TEST_P(ExpressionTest, Parse) {
  const TestInfo& test_info = GetParam();

  auto result = EnrichedParse(test_info.I, Macro::AllMacros());
  if (test_info.E.empty()) {
    EXPECT_TRUE(result.ok());
  } else {
    EXPECT_FALSE(result.ok());
    EXPECT_EQ(result.status().message(), test_info.E);
  }

  if (!test_info.P.empty()) {
    KindAndIdAdorner kind_and_id_adorner;
    DebugWriter w(kind_and_id_adorner);
    std::string adorned_string =
        w.toAdornedDebugString(result.value().parsed_expr());
    EXPECT_EQ(test_info.P, adorned_string);
  }

  if (!test_info.L.empty()) {
    LocationAdorner location_adorner(
        result.value().parsed_expr().source_info());
    DebugWriter w(location_adorner);
    std::string adorned_string =
        w.toAdornedDebugString(result.value().parsed_expr());
    EXPECT_EQ(test_info.L, adorned_string);
  }

  if (!test_info.R.empty()) {
    EXPECT_EQ(ConvertEnrichedSourceInfoToString(
                  result.value().enriched_source_info()),
              test_info.R);
  }
}

INSTANTIATE_TEST_SUITE_P(CelParserTest2, ExpressionTest,
                         testing::ValuesIn(test_cases));

}  // namespace
}  // namespace parser
}  // namespace expr
}  // namespace api
}  // namespace google
