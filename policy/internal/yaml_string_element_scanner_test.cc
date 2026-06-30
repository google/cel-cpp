// Copyright 2026 Google LLC
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

#include "policy/internal/yaml_string_element_scanner.h"

#include "absl/strings/string_view.h"
#include "common/source.h"
#include "internal/testing.h"

namespace cel::policy_internal {
namespace {

using ::testing::Eq;

TEST(YamlStringElementScannerTest, QuotedScalars) {
  ASSERT_OK_AND_ASSIGN(auto source, NewSource("expression: \"a + b\""));
  YamlStringElement element =
      ScanYamlStringElement(source->content(), 12, "a + b");
  EXPECT_THAT(element.starting_position, Eq(12));
  EXPECT_THAT(element.quoted, Eq(true));
  ASSERT_THAT(element.source_range.has_value(), Eq(true));
  EXPECT_THAT(element.source_range->begin, Eq(13));
  EXPECT_THAT(element.source_range->end, Eq(18));

  ASSERT_OK_AND_ASSIGN(auto source2, NewSource("expression: 'a + b'"));
  YamlStringElement element2 =
      ScanYamlStringElement(source2->content(), 12, "a + b");
  EXPECT_THAT(element2.starting_position, Eq(12));
  EXPECT_THAT(element2.quoted, Eq(true));
  ASSERT_THAT(element2.source_range.has_value(), Eq(true));
  EXPECT_THAT(element2.source_range->begin, Eq(13));
  EXPECT_THAT(element2.source_range->end, Eq(18));

  ASSERT_OK_AND_ASSIGN(auto source3, NewSource("expression: \"a + \\n b\""));
  YamlStringElement element3 =
      ScanYamlStringElement(source3->content(), 12, "a + \n b");
  EXPECT_THAT(element3.starting_position, Eq(12));
  EXPECT_THAT(element3.quoted, Eq(true));
  ASSERT_THAT(element3.source_range.has_value(), Eq(true));
  EXPECT_THAT(element3.source_range->begin, Eq(13));
  EXPECT_THAT(element3.source_range->end, Eq(21));
}

TEST(YamlStringElementScannerTest, PlainScalars) {
  ASSERT_OK_AND_ASSIGN(auto source, NewSource("expression: a + b"));
  YamlStringElement element =
      ScanYamlStringElement(source->content(), 12, "a + b");
  EXPECT_THAT(element.starting_position, Eq(12));
  EXPECT_THAT(element.quoted, Eq(false));
  ASSERT_THAT(element.source_range.has_value(), Eq(true));
  EXPECT_THAT(element.source_range->begin, Eq(12));
  EXPECT_THAT(element.source_range->end, Eq(17));

  ASSERT_OK_AND_ASSIGN(auto source2, NewSource("expression: a +\n  b"));
  YamlStringElement element2 =
      ScanYamlStringElement(source2->content(), 12, "a + b");
  EXPECT_THAT(element2.starting_position, Eq(12));
  EXPECT_THAT(element2.quoted, Eq(false));
  ASSERT_THAT(element2.source_range.has_value(), Eq(true));
  EXPECT_THAT(element2.source_range->begin, Eq(12));
  EXPECT_THAT(element2.source_range->end, Eq(19));
}

TEST(YamlStringElementScannerTest, BlockScalars) {
  ASSERT_OK_AND_ASSIGN(auto source, NewSource("expression: |\n  a + b\n"));
  YamlStringElement element =
      ScanYamlStringElement(source->content(), 12, "a + b\n");
  EXPECT_THAT(element.starting_position, Eq(12));
  EXPECT_THAT(element.quoted, Eq(false));
  ASSERT_THAT(element.source_range.has_value(), Eq(true));
  EXPECT_THAT(element.source_range->begin, Eq(16));
  EXPECT_THAT(element.source_range->end, Eq(21));

  ASSERT_OK_AND_ASSIGN(auto source2, NewSource("expression: >2-\n    a + b\n"));
  YamlStringElement element2 =
      ScanYamlStringElement(source2->content(), 12, "a + b");
  EXPECT_THAT(element2.starting_position, Eq(12));
  EXPECT_THAT(element2.quoted, Eq(false));
  ASSERT_THAT(element2.source_range.has_value(), Eq(true));
  EXPECT_THAT(element2.source_range->begin, Eq(20));
  EXPECT_THAT(element2.source_range->end, Eq(25));
}

TEST(YamlStringElementScannerTest, InvalidPosition) {
  ASSERT_OK_AND_ASSIGN(auto source, NewSource("expression: a + b"));
  YamlStringElement element =
      ScanYamlStringElement(source->content(), 100, "a + b");
  EXPECT_THAT(element.starting_position, Eq(100));
  EXPECT_THAT(element.quoted, Eq(false));
  EXPECT_THAT(element.source_range.has_value(), Eq(false));
}

}  // namespace
}  // namespace cel::policy_internal
