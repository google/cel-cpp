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

#include <sstream>
#include <string>

#include "absl/hash/hash.h"
#include "absl/status/status_matchers.h"
#include "absl/strings/cord.h"
#include "absl/strings/cord_test_helpers.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::testing::Eq;
using ::testing::Optional;

using StringValueTest = common_internal::ValueTest<>;

TEST_F(StringValueTest, Kind) {
  EXPECT_EQ(StringValue("foo").kind(), StringValue::kKind);
  EXPECT_EQ(Value(StringValue(absl::Cord("foo"))).kind(), StringValue::kKind);
}

TEST_F(StringValueTest, DebugString) {
  {
    std::ostringstream out;
    out << StringValue("foo");
    EXPECT_EQ(out.str(), "\"foo\"");
  }
  {
    std::ostringstream out;
    out << StringValue(absl::MakeFragmentedCord({"f", "o", "o"}));
    EXPECT_EQ(out.str(), "\"foo\"");
  }
  {
    std::ostringstream out;
    out << Value(StringValue(absl::Cord("foo")));
    EXPECT_EQ(out.str(), "\"foo\"");
  }
}

TEST_F(StringValueTest, ConvertToJson) {
  auto* message = NewArenaValueMessage();
  EXPECT_THAT(StringValue("foo").ConvertToJson(descriptor_pool(),
                                               message_factory(), message),
              IsOk());
  EXPECT_THAT(*message, EqualsValueTextProto(R"pb(string_value: "foo")pb"));
}

TEST_F(StringValueTest, NativeValue) {
  std::string scratch;
  EXPECT_EQ(StringValue("foo").NativeString(), "foo");
  EXPECT_EQ(StringValue("foo").NativeString(scratch), "foo");
  EXPECT_EQ(StringValue("foo").NativeCord(), "foo");
}

TEST_F(StringValueTest, TryFlat) {
  EXPECT_THAT(StringValue("foo").TryFlat(), Optional(Eq("foo")));
  EXPECT_THAT(
      StringValue(absl::MakeFragmentedCord({"Hello, World!", "World, Hello!"}))
          .TryFlat(),
      Eq(absl::nullopt));
}

TEST_F(StringValueTest, ToString) {
  EXPECT_EQ(StringValue("foo").ToString(), "foo");
  EXPECT_EQ(StringValue(absl::MakeFragmentedCord({"f", "o", "o"})).ToString(),
            "foo");
}

TEST_F(StringValueTest, CopyToString) {
  std::string out;
  StringValue("foo").CopyToString(&out);
  EXPECT_EQ(out, "foo");
  StringValue(absl::MakeFragmentedCord({"f", "o", "o"})).CopyToString(&out);
  EXPECT_EQ(out, "foo");
}

TEST_F(StringValueTest, AppendToString) {
  std::string out;
  StringValue("foo").AppendToString(&out);
  EXPECT_EQ(out, "foo");
  StringValue(absl::MakeFragmentedCord({"f", "o", "o"})).AppendToString(&out);
  EXPECT_EQ(out, "foofoo");
}

TEST_F(StringValueTest, ToCord) {
  EXPECT_EQ(StringValue("foo").ToCord(), "foo");
  EXPECT_EQ(StringValue(absl::MakeFragmentedCord({"f", "o", "o"})).ToCord(),
            "foo");
}

TEST_F(StringValueTest, CopyToCord) {
  absl::Cord out;
  StringValue("foo").CopyToCord(&out);
  EXPECT_EQ(out, "foo");
  StringValue(absl::MakeFragmentedCord({"f", "o", "o"})).CopyToCord(&out);
  EXPECT_EQ(out, "foo");
}

TEST_F(StringValueTest, AppendToCord) {
  absl::Cord out;
  StringValue("foo").AppendToCord(&out);
  EXPECT_EQ(out, "foo");
  StringValue(absl::MakeFragmentedCord({"f", "o", "o"})).AppendToCord(&out);
  EXPECT_EQ(out, "foofoo");
}

TEST_F(StringValueTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(StringValue("foo")),
            NativeTypeId::For<StringValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(StringValue(absl::Cord("foo")))),
            NativeTypeId::For<StringValue>());
}

TEST_F(StringValueTest, HashValue) {
  EXPECT_EQ(absl::HashOf(StringValue("foo")),
            absl::HashOf(absl::string_view("foo")));
  EXPECT_EQ(absl::HashOf(StringValue(absl::string_view("foo"))),
            absl::HashOf(absl::string_view("foo")));
  EXPECT_EQ(absl::HashOf(StringValue(absl::Cord("foo"))),
            absl::HashOf(absl::string_view("foo")));
}

TEST_F(StringValueTest, Equality) {
  EXPECT_NE(StringValue("foo"), "bar");
  EXPECT_NE("bar", StringValue("foo"));
  EXPECT_NE(StringValue("foo"), StringValue("bar"));
  EXPECT_NE(StringValue("foo"), absl::Cord("bar"));
  EXPECT_NE(absl::Cord("bar"), StringValue("foo"));
}

TEST_F(StringValueTest, LessThan) {
  EXPECT_LT(StringValue("bar"), "foo");
  EXPECT_LT("bar", StringValue("foo"));
  EXPECT_LT(StringValue("bar"), StringValue("foo"));
  EXPECT_LT(StringValue("bar"), absl::Cord("foo"));
  EXPECT_LT(absl::Cord("bar"), StringValue("foo"));
}

TEST_F(StringValueTest, StartsWith) {
  EXPECT_TRUE(
      StringValue("This string is large enough to not be stored inline!")
          .StartsWith(StringValue("This string is large enough")));
  EXPECT_TRUE(
      StringValue("This string is large enough to not be stored inline!")
          .StartsWith(StringValue(absl::Cord("This string is large enough"))));
  EXPECT_TRUE(
      StringValue(
          absl::Cord("This string is large enough to not be stored inline!"))
          .StartsWith(StringValue("This string is large enough")));
  EXPECT_TRUE(
      StringValue(
          absl::Cord("This string is large enough to not be stored inline!"))
          .StartsWith(StringValue(absl::Cord("This string is large enough"))));
}

TEST_F(StringValueTest, EndsWith) {
  EXPECT_TRUE(
      StringValue("This string is large enough to not be stored inline!")
          .EndsWith(StringValue("to not be stored inline!")));
  EXPECT_TRUE(
      StringValue("This string is large enough to not be stored inline!")
          .EndsWith(StringValue(absl::Cord("to not be stored inline!"))));
  EXPECT_TRUE(
      StringValue(
          absl::Cord("This string is large enough to not be stored inline!"))
          .EndsWith(StringValue("to not be stored inline!")));
  EXPECT_TRUE(
      StringValue(
          absl::Cord("This string is large enough to not be stored inline!"))
          .EndsWith(StringValue(absl::Cord("to not be stored inline!"))));
}

TEST_F(StringValueTest, Contains) {
  EXPECT_TRUE(
      StringValue("This string is large enough to not be stored inline!")
          .Contains(StringValue("string is large enough")));
  EXPECT_TRUE(
      StringValue("This string is large enough to not be stored inline!")
          .Contains(StringValue(absl::Cord("string is large enough"))));
  EXPECT_TRUE(
      StringValue(
          absl::Cord("This string is large enough to not be stored inline!"))
          .Contains(StringValue("string is large enough")));
  EXPECT_TRUE(
      StringValue(
          absl::Cord("This string is large enough to not be stored inline!"))
          .Contains(StringValue(absl::Cord("string is large enough"))));
}

TEST_F(StringValueTest, LowerAscii) {
  EXPECT_EQ(StringValue("UPPER lower").LowerAscii(arena()), "upper lower");
  EXPECT_EQ(StringValue(absl::Cord("UPPER lower")).LowerAscii(arena()),
            "upper lower");
  EXPECT_EQ(StringValue("upper lower").LowerAscii(arena()), "upper lower");
  EXPECT_EQ(StringValue(absl::Cord("upper lower")).LowerAscii(arena()),
            "upper lower");
  EXPECT_EQ(StringValue("").LowerAscii(arena()), "");
  EXPECT_EQ(StringValue(absl::Cord("")).LowerAscii(arena()), "");
  const std::string kLongMixed =
      "A long STRING with MiXeD case to test conversion to lower case!";
  const std::string kLongLower =
      "a long string with mixed case to test conversion to lower case!";
  EXPECT_EQ(StringValue(absl::Cord(kLongMixed)).LowerAscii(arena()),
            kLongLower);
  std::string very_long_mixed(10000, 'A');
  std::string very_long_lower(10000, 'a');
  EXPECT_EQ(
      StringValue(absl::MakeFragmentedCord({very_long_mixed.substr(0, 5000),
                                            very_long_mixed.substr(5000)}))
          .LowerAscii(arena()),
      very_long_lower);
  EXPECT_EQ(StringValue(absl::MakeFragmentedCord({"hello", "WORLD"}))
                .LowerAscii(arena()),
            "helloworld");
}

TEST_F(StringValueTest, UpperAscii) {
  EXPECT_EQ(StringValue("UPPER lower").UpperAscii(arena()), "UPPER LOWER");
  EXPECT_EQ(StringValue(absl::Cord("UPPER lower")).UpperAscii(arena()),
            "UPPER LOWER");
  EXPECT_EQ(StringValue("UPPER LOWER").UpperAscii(arena()), "UPPER LOWER");
  EXPECT_EQ(StringValue(absl::Cord("UPPER LOWER")).UpperAscii(arena()),
            "UPPER LOWER");
  EXPECT_EQ(StringValue("").UpperAscii(arena()), "");
  EXPECT_EQ(StringValue(absl::Cord("")).UpperAscii(arena()), "");
  const std::string kLongMixed =
      "A long STRING with MiXeD case to test conversion to UPPER case!";
  const std::string kLongUpper =
      "A LONG STRING WITH MIXED CASE TO TEST CONVERSION TO UPPER CASE!";
  EXPECT_EQ(StringValue(absl::Cord(kLongMixed)).UpperAscii(arena()),
            kLongUpper);
  std::string very_long_mixed(10000, 'a');
  std::string very_long_upper(10000, 'A');
  EXPECT_EQ(
      StringValue(absl::MakeFragmentedCord({very_long_mixed.substr(0, 5000),
                                            very_long_mixed.substr(5000)}))
          .UpperAscii(arena()),
      very_long_upper);
  EXPECT_EQ(StringValue(absl::MakeFragmentedCord({"HELLO", "world"}))
                .UpperAscii(arena()),
            "HELLOWORLD");
}

TEST_F(StringValueTest, ReplaceEmptyNeedle) {
  Value result;
  StringValue value_a("a");
  StringValue value_abc("abc");
  StringValue value_empty("");
  StringValue needle("");
  StringValue replacement("bar");

  ASSERT_THAT(value_a.Replace(needle, replacement, -1, arena(), &result),
              IsOk());
  ASSERT_TRUE(result.Is<StringValue>());
  EXPECT_EQ(*result.As<StringValue>(), "barabar");

  ASSERT_THAT(value_abc.Replace(needle, replacement, -1, arena(), &result),
              IsOk());
  ASSERT_TRUE(result.Is<StringValue>());
  EXPECT_EQ(*result.As<StringValue>(), "barabarbbarcbar");

  ASSERT_THAT(value_empty.Replace(needle, replacement, -1, arena(), &result),
              IsOk());
  ASSERT_TRUE(result.Is<StringValue>());
  EXPECT_EQ(*result.As<StringValue>(), "bar");
}

}  // namespace
}  // namespace cel
