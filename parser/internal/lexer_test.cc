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

#include "parser/internal/lexer.h"

#include <cstdint>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "internal/testing.h"

namespace cel::parser_internal {
namespace {

MATCHER_P3(IsToken, source, expected_type, expected_text, "") {
  if (arg.type != expected_type) {
    *result_listener << "type is " << TokenTypeToString(arg.type)
                     << " (expected " << TokenTypeToString(expected_type)
                     << ")";
    return false;
  }
  std::string_view actual_text = source.substr(arg.start, arg.end - arg.start);
  if (actual_text != expected_text) {
    *result_listener << "text is '" << actual_text << "' (expected '"
                     << expected_text << "')";
    return false;
  }
  return true;
}

struct LexerTestCase {
  std::string_view name;
  std::string_view source;
  std::vector<std::pair<TokenType, std::string_view>> expected_tokens;
};

using LexerTest = testing::TestWithParam<LexerTestCase>;

TEST_P(LexerTest, LexesSuccessTokens) {
  const LexerTestCase& test_case = GetParam();
  Lexer lexer(test_case.source);

  for (const auto& [type, text] : test_case.expected_tokens) {
    EXPECT_THAT(lexer.Lex(), IsToken(test_case.source, type, text));
  }
  EXPECT_THAT(lexer.Lex(), IsToken(test_case.source, TokenType::kEnd, ""));
}

INSTANTIATE_TEST_SUITE_P(
    LexerTest, LexerTest,
    testing::ValuesIn<LexerTestCase>({
        {"NullSource", std::string_view(nullptr, 0), {}},
        {"Empty", "", {}},
        {"Whitespace", " \n  ", {{TokenType::kWhitespace, " \n  "}}},
        {"KeywordsAndIdents",
         "null false true in as return foo_bar _foo_bar _ `quoted.ident`",
         {{TokenType::kNull, "null"},
          {TokenType::kWhitespace, " "},
          {TokenType::kFalse, "false"},
          {TokenType::kWhitespace, " "},
          {TokenType::kTrue, "true"},
          {TokenType::kWhitespace, " "},
          {TokenType::kIn, "in"},
          {TokenType::kWhitespace, " "},
          {TokenType::kReservedWord, "as"},
          {TokenType::kWhitespace, " "},
          {TokenType::kReservedWord, "return"},
          {TokenType::kWhitespace, " "},
          {TokenType::kIdent, "foo_bar"},
          {TokenType::kWhitespace, " "},
          {TokenType::kIdent, "_foo_bar"},
          {TokenType::kWhitespace, " "},
          {TokenType::kIdent, "_"},
          {TokenType::kWhitespace, " "},
          {TokenType::kIdent, "`quoted.ident`"}}},
        {"Numbers",
         "123 45u 0x1A 3.14 .5 1e6 2.5e-3 45U 0x1Au 0x1AU",
         {{TokenType::kInt, "123"},
          {TokenType::kWhitespace, " "},
          {TokenType::kUint, "45u"},
          {TokenType::kWhitespace, " "},
          {TokenType::kInt, "0x1A"},
          {TokenType::kWhitespace, " "},
          {TokenType::kFloat, "3.14"},
          {TokenType::kWhitespace, " "},
          {TokenType::kFloat, ".5"},
          {TokenType::kWhitespace, " "},
          {TokenType::kFloat, "1e6"},
          {TokenType::kWhitespace, " "},
          {TokenType::kFloat, "2.5e-3"},
          {TokenType::kWhitespace, " "},
          {TokenType::kUint, "45U"},
          {TokenType::kWhitespace, " "},
          {TokenType::kUint, "0x1Au"},
          {TokenType::kWhitespace, " "},
          {TokenType::kUint, "0x1AU"}}},
        {"IntEOF", "123456", {{TokenType::kInt, "123456"}}},
        {"HexIntEOF", "0x1A2B", {{TokenType::kInt, "0x1A2B"}}},
        {"FloatPositiveExponentEOF", "1e+6", {{TokenType::kFloat, "1e+6"}}},
        {"FloatEOF", ".12345", {{TokenType::kFloat, ".12345"}}},
        {"IntDotIdent",
         "1.foo",
         {{TokenType::kInt, "1"},
          {TokenType::kDot, "."},
          {TokenType::kIdent, "foo"}}},
        {"IntDotWhitespace",
         "1. ",
         {{TokenType::kInt, "1"},
          {TokenType::kDot, "."},
          {TokenType::kWhitespace, " "}}},
        {"IntDotEOF", "1.", {{TokenType::kInt, "1"}, {TokenType::kDot, "."}}},
        {"DotAtEOFBeforeDigit",
         std::string_view(".6", /*length=*/1),
         {{TokenType::kDot, "."}}},
        {"DotAtEOFBeforeIdent",
         std::string_view(".a", /*length=*/1),
         {{TokenType::kDot, "."}}},
        {"ZeroNumbers",
         "0 0u 0x0",
         {{TokenType::kInt, "0"},
          {TokenType::kWhitespace, " "},
          {TokenType::kUint, "0u"},
          {TokenType::kWhitespace, " "},
          {TokenType::kInt, "0x0"}}},
        {"StringsAndBytes",
         R"("hello" 'world' """multi
line""" r"raw" b"bytes" rb'\x00' '''multi
single''' R"raw_upper" B"bytes_upper" b'''multi
bytes''' br"raw_bytes" `a.b-c/d e`
"\a\b\f\n\r\t\v\"\'\\\?\` \x1A \u00A0 \U0001F600 \012")",
         {{TokenType::kString, "\"hello\""},
          {TokenType::kWhitespace, " "},
          {TokenType::kString, "'world'"},
          {TokenType::kWhitespace, " "},
          {TokenType::kString, "\"\"\"multi\nline\"\"\""},
          {TokenType::kWhitespace, " "},
          {TokenType::kString, "r\"raw\""},
          {TokenType::kWhitespace, " "},
          {TokenType::kBytes, "b\"bytes\""},
          {TokenType::kWhitespace, " "},
          {TokenType::kBytes, "rb'\\x00'"},
          {TokenType::kWhitespace, " "},
          {TokenType::kString, "'''multi\nsingle'''"},
          {TokenType::kWhitespace, " "},
          {TokenType::kString, "R\"raw_upper\""},
          {TokenType::kWhitespace, " "},
          {TokenType::kBytes, "B\"bytes_upper\""},
          {TokenType::kWhitespace, " "},
          {TokenType::kBytes, "b'''multi\nbytes'''"},
          {TokenType::kWhitespace, " "},
          {TokenType::kBytes, "br\"raw_bytes\""},
          {TokenType::kWhitespace, " "},
          {TokenType::kIdent, "`a.b-c/d e`"},
          {TokenType::kWhitespace, "\n"},
          {TokenType::kString,
           "\"\\a\\b\\f\\n\\r\\t\\v\\\"\\'\\\\\\?\\` \\x1A \\u00A0 \\U0001F600 "
           "\\012\""}}},
        {"EmptyStrings",
         "\"\" '' \"\"\"\"\"\" '''''' r\"\" r'' r\"\"\"\"\"\" r'''''' b\"\" "
         "b'' b\"\"\"\"\"\" b''''''",
         {{TokenType::kString, "\"\""},
          {TokenType::kWhitespace, " "},
          {TokenType::kString, "''"},
          {TokenType::kWhitespace, " "},
          {TokenType::kString, "\"\"\"\"\"\""},
          {TokenType::kWhitespace, " "},
          {TokenType::kString, "''''''"},
          {TokenType::kWhitespace, " "},
          {TokenType::kString, "r\"\""},
          {TokenType::kWhitespace, " "},
          {TokenType::kString, "r''"},
          {TokenType::kWhitespace, " "},
          {TokenType::kString, "r\"\"\"\"\"\""},
          {TokenType::kWhitespace, " "},
          {TokenType::kString, "r''''''"},
          {TokenType::kWhitespace, " "},
          {TokenType::kBytes, "b\"\""},
          {TokenType::kWhitespace, " "},
          {TokenType::kBytes, "b''"},
          {TokenType::kWhitespace, " "},
          {TokenType::kBytes, "b\"\"\"\"\"\""},
          {TokenType::kWhitespace, " "},
          {TokenType::kBytes, "b''''''"}}},
        {"OperatorsAndDelimiters",
         ". , + - * / % == != < <= > >= && || ! ? : [] { } ( )",
         {{TokenType::kDot, "."},
          {TokenType::kWhitespace, " "},
          {TokenType::kComma, ","},
          {TokenType::kWhitespace, " "},
          {TokenType::kPlus, "+"},
          {TokenType::kWhitespace, " "},
          {TokenType::kMinus, "-"},
          {TokenType::kWhitespace, " "},
          {TokenType::kAsterisk, "*"},
          {TokenType::kWhitespace, " "},
          {TokenType::kSlash, "/"},
          {TokenType::kWhitespace, " "},
          {TokenType::kPercent, "%"},
          {TokenType::kWhitespace, " "},
          {TokenType::kEqualEqual, "=="},
          {TokenType::kWhitespace, " "},
          {TokenType::kExclamationEqual, "!="},
          {TokenType::kWhitespace, " "},
          {TokenType::kLess, "<"},
          {TokenType::kWhitespace, " "},
          {TokenType::kLessEqual, "<="},
          {TokenType::kWhitespace, " "},
          {TokenType::kGreater, ">"},
          {TokenType::kWhitespace, " "},
          {TokenType::kGreaterEqual, ">="},
          {TokenType::kWhitespace, " "},
          {TokenType::kLogicalAnd, "&&"},
          {TokenType::kWhitespace, " "},
          {TokenType::kLogicalOr, "||"},
          {TokenType::kWhitespace, " "},
          {TokenType::kExclamation, "!"},
          {TokenType::kWhitespace, " "},
          {TokenType::kQuestion, "?"},
          {TokenType::kWhitespace, " "},
          {TokenType::kColon, ":"},
          {TokenType::kWhitespace, " "},
          {TokenType::kLeftBracket, "["},
          {TokenType::kRightBracket, "]"},
          {TokenType::kWhitespace, " "},
          {TokenType::kLeftBrace, "{"},
          {TokenType::kWhitespace, " "},
          {TokenType::kRightBrace, "}"},
          {TokenType::kWhitespace, " "},
          {TokenType::kLeftParen, "("},
          {TokenType::kWhitespace, " "},
          {TokenType::kRightParen, ")"}}},
        {"Comments",
         "a\n// comment\nb",
         {{TokenType::kIdent, "a"},
          {TokenType::kWhitespace, "\n"},
          {TokenType::kComment, "// comment\n"},
          {TokenType::kIdent, "b"}}},
        {"CommentWithoutTrailingNewlineEOF",
         "// comment without trailing newline",
         {{TokenType::kComment, "// comment without trailing newline"}}},
        {"CommentAfterTokenWithoutTrailingNewlineEOF",
         "a // comment without trailing newline",
         {{TokenType::kIdent, "a"},
          {TokenType::kWhitespace, " "},
          {TokenType::kComment, "// comment without trailing newline"}}},
    }),
    [](const testing::TestParamInfo<LexerTestCase>& info) {
      return std::string(info.param.name);
    });

TEST(LexerTest, LineOffsets) {
  std::string_view source = "a\n// comment\nb";
  std::vector<int32_t> line_offsets;
  Lexer lexer(source, &line_offsets);

  EXPECT_THAT(lexer.Lex(), IsToken(source, TokenType::kIdent, "a"));
  EXPECT_THAT(lexer.Lex(), IsToken(source, TokenType::kWhitespace, "\n"));
  EXPECT_THAT(lexer.Lex(),
              IsToken(source, TokenType::kComment, "// comment\n"));
  EXPECT_THAT(lexer.Lex(), IsToken(source, TokenType::kIdent, "b"));

  ASSERT_EQ(line_offsets.size(), 2);
  EXPECT_EQ(line_offsets[0], 2);
  EXPECT_EQ(line_offsets[1], 13);
}

TEST(LexerTest, LineOffsetsInStringsAndIdentifiers) {
  std::string_view source =
      "'''multi\nline'''\n\"another\nline\"\n`ident\nhere`";
  std::vector<int32_t> line_offsets;
  Lexer lexer(source, &line_offsets);

  EXPECT_THAT(lexer.Lex(),
              IsToken(source, TokenType::kString, "'''multi\nline'''"));
  EXPECT_THAT(lexer.Lex(), IsToken(source, TokenType::kWhitespace, "\n"));
  EXPECT_THAT(lexer.Lex(),
              IsToken(source, TokenType::kString, "\"another\nline\""));
  EXPECT_THAT(lexer.Lex(), IsToken(source, TokenType::kWhitespace, "\n"));
  EXPECT_THAT(lexer.Lex(), IsToken(source, TokenType::kIdent, "`ident\nhere`"));
  EXPECT_THAT(lexer.Lex(), IsToken(source, TokenType::kEnd, ""));

  ASSERT_EQ(line_offsets.size(), 5);
  EXPECT_EQ(line_offsets[0], 9);
  EXPECT_EQ(line_offsets[1], 17);
  EXPECT_EQ(line_offsets[2], 26);
  EXPECT_EQ(line_offsets[3], 32);
  EXPECT_EQ(line_offsets[4], 39);
}

struct LexerErrorTestCase {
  std::string_view source;
  std::string_view expected_error_message;
  int32_t expected_position;
};

using LexerErrorTest = testing::TestWithParam<LexerErrorTestCase>;

TEST_P(LexerErrorTest, LexesErrorTokenAndStoresError) {
  const LexerErrorTestCase& test_case = GetParam();
  Lexer lexer(test_case.source);
  Token token = lexer.Lex();
  EXPECT_EQ(token.type, TokenType::kError);
  EXPECT_EQ(lexer.GetError().message, test_case.expected_error_message);
  EXPECT_EQ(lexer.GetPosition(), test_case.expected_position);
}

INSTANTIATE_TEST_SUITE_P(
    ErrorCases, LexerErrorTest,
    testing::Values(
        LexerErrorTestCase{
            .source = "\"unterminated",
            .expected_error_message = "unterminated string literal",
            .expected_position = 13,
        },
        LexerErrorTestCase{
            .source = "0x",
            .expected_error_message =
                "integral literal missing digits after hexadecimal separator",
            .expected_position = 2,
        },
        LexerErrorTestCase{
            .source = "@",
            .expected_error_message = "unexpected character",
            .expected_position = 1,
        },
        LexerErrorTestCase{
            .source = "0x1A_invalid",
            .expected_error_message =
                "int literal has unexpected trailing characters",
            .expected_position = 5,
        },
        LexerErrorTestCase{
            .source = "123_invalid",
            .expected_error_message =
                "int literal has unexpected trailing characters",
            .expected_position = 4,
        },
        LexerErrorTestCase{
            .source = "1x0",
            .expected_error_message =
                "int literal has unexpected trailing characters",
            .expected_position = 2,
        },
        LexerErrorTestCase{
            .source = "2x",
            .expected_error_message =
                "int literal has unexpected trailing characters",
            .expected_position = 2,
        },
        LexerErrorTestCase{
            .source = "`unterminated quoted",
            .expected_error_message = "unterminated quoted identifier",
            .expected_position = 20,
        },
        LexerErrorTestCase{
            .source = "'''unterminated multi",
            .expected_error_message = "unterminated string literal",
            .expected_position = 21,
        },
        LexerErrorTestCase{
            .source = "r'unterminated raw",
            .expected_error_message = "unterminated string literal",
            .expected_position = 18,
        },
        LexerErrorTestCase{
            .source = "b'unterminated bytes",
            .expected_error_message = "unterminated bytes literal",
            .expected_position = 20,
        },
        LexerErrorTestCase{
            .source = "1e",
            .expected_error_message =
                "floating point literal missing digits after exponent "
                "separator",
            .expected_position = 2,
        }));

}  // namespace
}  // namespace cel::parser_internal
