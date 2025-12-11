// Copyright 2025 Google LLC
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

#include "runtime/embedder_context.h"

#include <cstdint>

#include "absl/types/optional.h"
#include "common/typeinfo.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::testing::Optional;

TEST(EmbedderContextTest, From) {
  struct TestScope {};
  EmbedderContext context = EmbedderContext::From<TestScope>(int64_t{42});
  EXPECT_THAT((context.Get<TestScope, int64_t>()), Optional(42));
  EXPECT_EQ((context.Get<TestScope, uint64_t>()), absl::nullopt);

  EmbedderContext context2 = EmbedderContext::From<TestScope>(uint64_t{42});
  EXPECT_THAT((context2.Get<TestScope, uint64_t>()), Optional(42));
  EXPECT_EQ((context2.Get<TestScope, int64_t>()), absl::nullopt);

  // Side effect, but checking that we keep a dense range.
  EXPECT_EQ(cel::TypeIdInSet<TestScope>::Size(), 2);
}

TEST(EmbedderContextTest, FromOutOfLine) {
  struct TestScope {};
  EmbedderContext context =
      EmbedderContext::From<TestScope>(int64_t{42}, uint64_t{43}, double{44});

  EXPECT_THAT((context.Get<TestScope, int64_t>()), Optional(42));
  EXPECT_THAT((context.Get<TestScope, uint64_t>()), Optional(43));
  EXPECT_THAT((context.Get<TestScope, double>()), Optional(44));
  EXPECT_EQ((context.Get<TestScope, bool>()), absl::nullopt);

  // Note: Referencing a type not intended to be stored will still reserve a
  // slot in the TypeIdInSet.
  EXPECT_EQ(cel::TypeIdInSet<TestScope>::Size(), 4);
}

TEST(EmbedderContextTest, FromPtrs) {
  struct TestScope {};
  struct TestPointee {
  } foo;
  int64_t pointee2;

  EmbedderContext context = EmbedderContext::From<TestScope>(
      &foo, const_cast<const int64_t*>(&pointee2));
  EXPECT_EQ((context.Get<TestScope, const int64_t*>()), &pointee2);
  EXPECT_EQ((context.Get<TestScope, TestPointee*>()), &foo);

  EmbedderContext context2 = EmbedderContext::From<TestScope>(&foo);
  EXPECT_EQ((context2.Get<TestScope, int64_t*>()), nullptr);
  EXPECT_EQ((context2.Get<TestScope, TestPointee*>()), &foo);

  // Note: const int* not the same as int*.
  EXPECT_EQ(cel::TypeIdInSet<TestScope>::Size(), 3);
}

TEST(EmbedderContextTest, FromDefaultScope) {
  EmbedderContext context = EmbedderContext::From(int64_t{42});
  EXPECT_THAT((context.Get<int64_t>()), Optional(42));
  EXPECT_EQ((context.Get<uint64_t>()), absl::nullopt);
}

// These death assertions are only enabled when compiled in debug mode.
// Caller is responsible for adequately testing since we're limited in what
// we can statically check due to the type-erasure.
TEST(EmbedderContextDeathTest, GetWithWrongScope) {
  struct TestScope {};
  EmbedderContext context = EmbedderContext::From<TestScope>(int64_t{42});
  EXPECT_DEBUG_DEATH(
      { context.Get<int64_t>(); }, "EmbedderContext::Get wrong scope");
}

}  // namespace
}  // namespace cel
