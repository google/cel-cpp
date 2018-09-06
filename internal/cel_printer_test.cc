#include "internal/cel_printer.h"

#include <string>

#include "gtest/gtest.h"

namespace google {
namespace api {
namespace expr {
namespace internal {

TEST(CelPrinterTest, ToString_String) {
  EXPECT_EQ("\"\"", ToString(""));
  EXPECT_EQ("\"hi\"", ToString(std::string("hi")));
  EXPECT_EQ("\"h\\000i\"", ToString(absl::string_view("h\000i", 3)));
}

TEST(CelPrinterTest, ToString_Number) {
  EXPECT_EQ("1", ToString(1));
  EXPECT_EQ("1u", ToString(1u));
  EXPECT_EQ("1.0", ToString(1.0));
}

TEST(CelPrinterTest, ToListString) {
  EXPECT_EQ("[]", ToListString({}));
  EXPECT_EQ("[1]", ToListString({1}));
  EXPECT_EQ("[1u, 2u]", ToListString({1u, 2u}));
  EXPECT_EQ("[1.0, 2.0, 3.5]", ToListString({1.0, 2.0, 3.5}));
  auto actual = ToListString<absl::string_view>(
      {"one", "2", absl::string_view("h\000i", 3), std::string("4")});
  EXPECT_EQ("[\"one\", \"2\", \"h\\000i\", \"4\"]", actual);

  EXPECT_EQ("[1, 2, 3]", ToListString<std::vector<int>>({1, 2, 3}));
}

TEST(CelPrinterTest, EntryPrinter) {
  EntryPrinter<MapJoinPolicy> printer;
  EXPECT_EQ("1: 2u", printer(1, 2u));
  EXPECT_EQ("1: 2u", printer(std::make_pair(1, 2u)));
}

TEST(CelPrinterTest, ToMapString) {
  EXPECT_EQ("{}", ToMapString({}));
  EXPECT_EQ("{1: 2u}", ToMapString({std::make_pair(1, 2u)}));
  EXPECT_EQ("{1: 2u, 3: 4u}", ToMapString({std::make_pair(1, 2u), {3, 4u}}));

  auto actual = ToMapString<std::map<int, unsigned int>>({{3, 4u}, {1, 2u}});
  EXPECT_EQ("{1: 2u, 3: 4u}", actual);
  actual = ToMapString<std::pair<int, unsigned int>>({{3, 4u}, {1, 2u}});
  EXPECT_EQ("{3: 4u, 1: 2u}", actual);
}

TEST(CelPrinterTest, SequenceBuilder) {
  SequenceBuilder<MapJoinPolicy> builder;
  builder.Add(1, 2u);
  builder.Add(3.0, "four");
  EXPECT_EQ("name{1: 2u, 3.0: \"four\"}", builder.Build("name"));
}

TEST(CelPrinterTest, ToCallString) {
  EXPECT_EQ("()", ToCallString(""));
  EXPECT_EQ("name()", ToCallString("name"));
  EXPECT_EQ("name(1)", ToCallString("name", 1));
  EXPECT_EQ("name(1, 2u)", ToCallString("name", 1, 2u));
  EXPECT_EQ("name(1, 2u, 3.0)", ToCallString("name", 1, 2u, 3.0));
  EXPECT_EQ("name(1, 2u, 3.0, \"4\")", ToCallString("name", 1, 2u, 3.0, "4"));
}

TEST(CelPrinterTest, ToObjectString) {
  EXPECT_EQ("{}", ToObjectString(""));
  EXPECT_EQ("object_type{}", ToObjectString("object_type"));
  EXPECT_EQ("object_type{1}", ToObjectString("object_type", "", 1));
  EXPECT_EQ("object_type{1, uint=2u}",
            ToObjectString("object_type", "", 1, "uint", 2u));
  EXPECT_EQ("object_type{1, uint=2u, 3.0}",
            ToObjectString("object_type", "", 1, "uint", 2u, "", 3.0));
  EXPECT_EQ(
      "object_type{1, uint=2u, 3.0, string=\"4\"}",
      ToObjectString("object_type", "", 1, "uint", 2u, "", 3.0, "string", "4"));
}

TEST(CelPrinterTest, NaN) {
  EXPECT_EQ("nan", ToString(std::numeric_limits<double>::quiet_NaN()));
}

TEST(CelPrinterTest, Inf) {
  EXPECT_EQ("inf", ToString(std::numeric_limits<double>::infinity()));
  EXPECT_EQ("-inf", ToString(-std::numeric_limits<double>::infinity()));
}

}  // namespace internal
}  // namespace expr
}  // namespace api
}  // namespace google
