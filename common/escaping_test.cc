#include "common/escaping.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace google {
namespace api {
namespace expr {
namespace parser {
namespace {

using testing::Eq;
using testing::Ne;

struct TestInfo {
  static constexpr char EXPECT_ERROR[] = "--ERROR--";

  TestInfo(const std::string& I, const std::string& O, bool is_bytes = false)
      : I(I), O(O), is_bytes(is_bytes) {}

  // Input string
  std::string I;

  // Expected output string
  std::string O;

  // Indicator whether this is a byte or text string
  bool is_bytes;
};

std::vector<TestInfo> test_cases = {
    {"'hello'", "hello"},
    {R"("")", ""},
    {R"("\\\"")", R"(\")"},
    {R"("\\")", "\\"},
    {"'''x''x'''", "x''x"},
    {R"("""x""x""")", R"(x""x)"},
    // Octal 303 -> Code point 195 (Ã)
    // Octal 277 -> Code point 191 (¿)
    {R"("\303\277")", "Ã¿"},
    // Octal 377 -> Code point 255 (ÿ)
    {R"("\377")", "ÿ"},
    {R"("\u263A\u263A")", "☺☺"},
    {R"("\a\b\f\n\r\t\v\'\"\\\? Legal escapes")",
     "\a\b\f\n\r\t\v'\"\\? Legal escapes"},
    // Illegal escape, expect error
    {R"("\a\b\f\n\r\t\v\'\\"\\\? Illegal escape \>")", TestInfo::EXPECT_ERROR},
    {R"("\u1")", TestInfo::EXPECT_ERROR},

    // The following are interpreted as byte sequences, hence "true"
    {"\"abc\"", "\x61\x62\x63", true},
    {"\"ÿ\"", "\xc3\xbf", true},
    {R"("\303\277")", "\xc3\xbf", true},
    {R"("\377")", "\xff", true},
    {R"("\xc3\xbf")", "\xc3\xbf", true},
    {R"("\xff")", "\xff", true},
    // Bytes unicode escape, expect error
    {R"("\u00ff")", TestInfo::EXPECT_ERROR, true},
    {R"("\z")", TestInfo::EXPECT_ERROR, true},
    {R"("\x1")", TestInfo::EXPECT_ERROR, true},
    {R"("\u1")", TestInfo::EXPECT_ERROR, true},
};

class UnescapeTest : public testing::TestWithParam<TestInfo> {};

TEST_P(UnescapeTest, Unescape) {
  const TestInfo& test_info = GetParam();
  ::testing::internal::ColoredPrintf(::testing::internal::COLOR_GREEN,
                                     "[          ]");
  ::testing::internal::ColoredPrintf(::testing::internal::COLOR_DEFAULT,
                                     " Input: ");
  ::testing::internal::ColoredPrintf(::testing::internal::COLOR_YELLOW, "%s%s",
                                     test_info.I.c_str(),
                                     test_info.is_bytes ? " BYTES" : "");
  if (test_info.O != TestInfo::EXPECT_ERROR) {
    ::testing::internal::ColoredPrintf(::testing::internal::COLOR_DEFAULT,
                                       "  Expected Output: ");
    ::testing::internal::ColoredPrintf(::testing::internal::COLOR_YELLOW,
                                       "%s\n", test_info.O.c_str());
  } else {
    ::testing::internal::ColoredPrintf(::testing::internal::COLOR_YELLOW,
                                       "  Expecting ERROR\n");
  }

  auto result = unescape(test_info.I, test_info.is_bytes);
  if (test_info.O == TestInfo::EXPECT_ERROR) {
    EXPECT_THAT(result, Eq(std::nullopt));
  } else {
    ASSERT_THAT(result, Ne(std::nullopt));
    EXPECT_EQ(*result, test_info.O);
  }
}

INSTANTIATE_TEST_SUITE_P(UnescapeSuite, UnescapeTest,
                         testing::ValuesIn(test_cases));

}  // namespace
}  // namespace parser
}  // namespace expr
}  // namespace api
}  // namespace google
