// Copyright 2024 Google LLC
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

#include "common/arena_bytes.h"

#include "absl/hash/hash.h"
#include "absl/hash/hash_testing.h"
#include "absl/strings/string_view.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::Eq;
using testing::Ge;
using testing::Gt;
using testing::IsEmpty;
using testing::Le;
using testing::Lt;
using testing::Ne;
using testing::SizeIs;

TEST(ArenaBytes, Default) {
  ArenaBytes string;
  EXPECT_THAT(string, IsEmpty());
  EXPECT_THAT(string, SizeIs(0));
  EXPECT_THAT(string, Eq(ArenaBytes()));
}

TEST(ArenaBytes, Iterator) {
  ArenaBytes string = ArenaBytes::Static("Hello World!");
  auto it = string.cbegin();
  EXPECT_THAT(*it++, Eq('H'));
  EXPECT_THAT(*it++, Eq('e'));
  EXPECT_THAT(*it++, Eq('l'));
  EXPECT_THAT(*it++, Eq('l'));
  EXPECT_THAT(*it++, Eq('o'));
  EXPECT_THAT(*it++, Eq(' '));
  EXPECT_THAT(*it++, Eq('W'));
  EXPECT_THAT(*it++, Eq('o'));
  EXPECT_THAT(*it++, Eq('r'));
  EXPECT_THAT(*it++, Eq('l'));
  EXPECT_THAT(*it++, Eq('d'));
  EXPECT_THAT(*it++, Eq('!'));
  EXPECT_THAT(it, Eq(string.cend()));
}

TEST(ArenaBytes, ReverseIterator) {
  ArenaBytes string = ArenaBytes::Static("Hello World!");
  auto it = string.crbegin();
  EXPECT_THAT(*it++, Eq('!'));
  EXPECT_THAT(*it++, Eq('d'));
  EXPECT_THAT(*it++, Eq('l'));
  EXPECT_THAT(*it++, Eq('r'));
  EXPECT_THAT(*it++, Eq('o'));
  EXPECT_THAT(*it++, Eq('W'));
  EXPECT_THAT(*it++, Eq(' '));
  EXPECT_THAT(*it++, Eq('o'));
  EXPECT_THAT(*it++, Eq('l'));
  EXPECT_THAT(*it++, Eq('l'));
  EXPECT_THAT(*it++, Eq('e'));
  EXPECT_THAT(*it++, Eq('H'));
  EXPECT_THAT(it, Eq(string.crend()));
}

TEST(ArenaBytes, RemovePrefix) {
  ArenaBytes string = ArenaBytes::Static("Hello World!");
  string.remove_prefix(6);
  EXPECT_EQ(string, "World!");
}

TEST(ArenaBytes, RemoveSuffix) {
  ArenaBytes string = ArenaBytes::Static("Hello World!");
  string.remove_suffix(7);
  EXPECT_EQ(string, "Hello");
}

TEST(ArenaBytes, Equal) {
  EXPECT_THAT(ArenaBytes::Static("1"), Eq(ArenaBytes::Static("1")));
}

TEST(ArenaBytes, NotEqual) {
  EXPECT_THAT(ArenaBytes::Static("1"), Ne(ArenaBytes::Static("2")));
}

TEST(ArenaBytes, Less) {
  EXPECT_THAT(ArenaBytes::Static("1"), Lt(ArenaBytes::Static("2")));
}

TEST(ArenaBytes, LessEqual) {
  EXPECT_THAT(ArenaBytes::Static("1"), Le(ArenaBytes::Static("1")));
}

TEST(ArenaBytes, Greater) {
  EXPECT_THAT(ArenaBytes::Static("2"), Gt(ArenaBytes::Static("1")));
}

TEST(ArenaBytes, GreaterEqual) {
  EXPECT_THAT(ArenaBytes::Static("1"), Ge(ArenaBytes::Static("1")));
}

TEST(ArenaBytes, ImplementsAbslHashCorrectly) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      {ArenaBytes::Static(""), ArenaBytes::Static("Hello World!"),
       ArenaBytes::Static("How much wood could a woodchuck chuck if a "
                          "woodchuck could chuck wood?")}));
}

TEST(ArenaBytes, Hash) {
  EXPECT_EQ(absl::HashOf(ArenaBytes::Static("Hello World!")),
            absl::HashOf(absl::string_view("Hello World!")));
}

}  // namespace
}  // namespace cel
