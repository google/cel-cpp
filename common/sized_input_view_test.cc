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

#include "common/sized_input_view.h"

#include <array>
#include <iterator>
#include <list>
#include <numeric>
#include <string>
#include <vector>

#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::_;
using testing::ElementsAre;

TEST(SizedInputViewTest, EmptyVector) {
  std::vector<int> a;
  SizedInputView<int> v(a);
  // NOLINTNEXTLINE(clang-diagnostic-unreachable-code-loop-increment)
  for (const int& i : v) {
    FAIL() << "Empty range should not iterate " << i;
  }
}

TEST(SizedInputViewTest, Vector) {
  std::vector<int> a = {1, 2, 3};
  SizedInputView<int> v(a);
  auto expected_it = a.begin();
  for (const int& i : v) {
    EXPECT_EQ(i, *expected_it);
    EXPECT_EQ(&i, &*expected_it);
    ++expected_it;
  }
  EXPECT_EQ(expected_it, a.end());
}

TEST(SizedInputViewTest, NonEndComparison) {
  std::vector<int> a = {1, 2, 3};
  SizedInputView<int> v(a);
  auto it = v.begin();
  ++it;
  EXPECT_DEBUG_DEATH({ (void)(v.begin() == it); }, _);
}

TEST(SizedInputViewTest, IteratePastEnd) {
  std::vector<int> a = {1};
  SizedInputView<int> v(a);
  auto it = v.begin();
  ++it;
  EXPECT_DEBUG_DEATH({ ++it; }, _);
}

TEST(SizedInputViewTest, SelfAssignment) {
  std::vector<int> a = {1, 2, 3};
  SizedInputView<int> v(a);
  v = v;
  EXPECT_THAT(v, ElementsAre(1, 2, 3));
}

TEST(SizedInputViewTest, List) {
  std::list<int> a = {1, 2, 3};
  SizedInputView<int> v(a);
  auto expected_it = a.begin();
  for (const int& i : v) {
    EXPECT_EQ(i, *expected_it);
    EXPECT_EQ(&i, &*expected_it);
    ++expected_it;
  }
  EXPECT_EQ(expected_it, a.end());
}

TEST(SizedInputViewTest, Array) {
  int a[] = {1, 2, 3};
  SizedInputView<int> v(a);
  auto expected_it = std::begin(a);
  for (const int& i : v) {
    EXPECT_EQ(i, *expected_it);
    EXPECT_EQ(&i, &*expected_it);
    ++expected_it;
  }
  EXPECT_EQ(expected_it, std::end(a));
}

TEST(SizedInputViewTest, AsFunctionArgument) {
  std::list<int> a = {1, 2, 3};
  auto f = [](const SizedInputView<int>& v) {
    EXPECT_THAT(v, ElementsAre(1, 2, 3));
  };
  f(a);
}

TEST(SizedInputViewTest, InitializerListArgument) {
  auto f = [](const SizedInputView<std::string>& v) {
    EXPECT_THAT(v, ElementsAre("a", "b"));
  };
  f({"a", "b"});
}

TEST(SizedInputViewTest, EmptyConversion) {
  std::vector<std::string> a;
  SizedInputView<absl::string_view> v(a);
  EXPECT_EQ(v.begin(), v.end());
}

TEST(SizedInputViewTest, Conversion) {
  std::vector<std::string> a = {"a", "b", "c"};
  SizedInputView<absl::string_view> v(a);
  auto v_it = v.begin();
  for (const std::string& s : a) {
    EXPECT_EQ(v_it->data(), s.data());
    ++v_it;
  }
}

TEST(SizedInputViewTest, NestedConversion) {
  std::vector<std::list<std::string>> a = {{"a", "b"}, {"c", "d"}};
  SizedInputView<SizedInputView<absl::string_view>> v(a);
  auto v_it = v.begin();
  EXPECT_THAT(*v_it, ElementsAre("a", "b"));
  ++v_it;
  EXPECT_THAT(*v_it, ElementsAre("c", "d"));
  ++v_it;
  EXPECT_EQ(v_it, v.end());

  // Copy and invalidate the source and make sure it still works.
  SizedInputView<SizedInputView<absl::string_view>> c(v);
  v = {};
  auto c_it = c.begin();
  EXPECT_THAT(*c_it, ElementsAre("a", "b"));
  ++c_it;
  EXPECT_THAT(*c_it, ElementsAre("c", "d"));
  ++c_it;
  EXPECT_EQ(c_it, c.end());
}

TEST(SizedInputViewTest, LargeValueConversion) {
  struct ConvertibleFromInt {
    ConvertibleFromInt(int i)  // NOLINT(google-explicit-constructor)
        : value(absl::StrCat(i)) {}
    std::string value;
    int padding[30];
    std::vector<int> vec = {1, 2, 3};
  };
  std::vector<int> a(100);
  std::iota(a.begin(), a.end(), 0);
  SizedInputView<ConvertibleFromInt> v(a);
  auto a_it = a.begin();
  for (const auto& i : v) {
    EXPECT_EQ(i.value, absl::StrCat(*a_it));
    ++a_it;
  }
  EXPECT_EQ(a_it, a.end());

  // Also test partial iteration, to make sure the stashed value gets destroyed.
  SizedInputView<ConvertibleFromInt> v2(a);
  auto it = v2.begin();
  ++it;

  // Make sure copies work after the original is destroyed.
  SizedInputView<ConvertibleFromInt> c(v);
  v = {};
  a_it = a.begin();
  for (const auto& i : c) {
    EXPECT_EQ(i.value, absl::StrCat(*a_it));
    ++a_it;
  }
  EXPECT_EQ(a_it, a.end());
}

TEST(SizedInputViewTest, StlInterop) {
  std::vector<int> a = {1, 2, 3};
  SizedInputView<int> v(a);
  std::vector<int> b = {5, 6, 7};
  b.insert(b.end(), v.begin(), v.end());
}

}  // namespace
}  // namespace cel
