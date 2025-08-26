// Copyright 2025 Google LLC
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
#include "tools/internal/navigable_ast_internal.h"

#include <iterator>
#include <vector>

#include "absl/types/span.h"
#include "internal/testing.h"

namespace cel::tools_internal {
namespace {

struct TestRangeTraits {
  using UnderlyingType = int;
  static double Adapt(const UnderlyingType& value) {
    return static_cast<double>(value) + 0.5;
  }
};

TEST(NavigableAstRangeTest, BasicIteration) {
  std::vector<int> values{1, 2, 3};
  NavigableAstRange<TestRangeTraits> range(absl::MakeConstSpan(values));
  absl::Span<const int> span(values);
  auto it = range.begin();
  EXPECT_EQ(*it, 1.5);
  EXPECT_EQ(*++it, 2.5);
  EXPECT_EQ(*++it, 3.5);
  EXPECT_EQ(++it, range.end());
  EXPECT_EQ(*--it, 3.5);
  EXPECT_EQ(*--it, 2.5);
  EXPECT_EQ(*--it, 1.5);
  EXPECT_EQ(it, range.begin());
}

}  // namespace
}  // namespace cel::tools_internal
