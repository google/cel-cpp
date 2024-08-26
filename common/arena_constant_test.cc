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

#include "common/arena_constant.h"

#include <cstddef>
#include <cstdint>

#include "absl/hash/hash_testing.h"
#include "absl/time/time.h"
#include "absl/types/variant.h"
#include "common/arena_bytes.h"
#include "common/arena_bytes_pool.h"
#include "common/arena_string.h"
#include "common/arena_string_pool.h"
#include "common/constant.h"
#include "internal/testing.h"
#include "google/protobuf/arena.h"

namespace cel {
namespace {

using testing::An;
using testing::Eq;
using testing::Ne;
using testing::Optional;
using testing::Test;

TEST(ArenaConstantKind, Name) {
  EXPECT_EQ(ArenaConstantKindName(ArenaConstantKind::kUnspecified),
            "UNSPECIFIED");
  EXPECT_EQ(ArenaConstantKindName(ArenaConstantKind::kNull), "NULL");
  EXPECT_EQ(ArenaConstantKindName(ArenaConstantKind::kBool), "BOOL");
  EXPECT_EQ(ArenaConstantKindName(ArenaConstantKind::kInt), "INT");
  EXPECT_EQ(ArenaConstantKindName(ArenaConstantKind::kUint), "UINT");
  EXPECT_EQ(ArenaConstantKindName(ArenaConstantKind::kDouble), "DOUBLE");
  EXPECT_EQ(ArenaConstantKindName(ArenaConstantKind::kBytes), "BYTES");
  EXPECT_EQ(ArenaConstantKindName(ArenaConstantKind::kString), "STRING");
  EXPECT_EQ(ArenaConstantKindName(ArenaConstantKind::kDuration), "DURATION");
  EXPECT_EQ(ArenaConstantKindName(ArenaConstantKind::kTimestamp), "TIMESTAMP");
  EXPECT_EQ(ArenaConstantKindName(static_cast<ArenaConstantKind>(42)), "ERROR");
}

TEST(ArenaConstant, Unspecified) {
  EXPECT_THAT(ArenaConstant(), Eq(ArenaConstant()));
  EXPECT_THAT(ArenaConstant(), Ne(ArenaConstant(true)));
  EXPECT_TRUE(ArenaConstant().IsUnspecified());
  EXPECT_THAT(ArenaConstant().AsUnspecified(), Optional(An<absl::monostate>()));
  EXPECT_THAT(static_cast<absl::monostate>(ArenaConstant()),
              An<absl::monostate>());
}

TEST(ArenaConstant, Null) {
  EXPECT_THAT(ArenaConstant(nullptr), Eq(ArenaConstant(nullptr)));
  EXPECT_THAT(ArenaConstant(nullptr), Ne(ArenaConstant()));
  EXPECT_TRUE(ArenaConstant(nullptr).IsNull());
  EXPECT_THAT(ArenaConstant(nullptr).AsNull(), Optional(An<std::nullptr_t>()));
  EXPECT_THAT(static_cast<std::nullptr_t>(ArenaConstant(nullptr)),
              An<std::nullptr_t>());
}

TEST(ArenaConstant, Bool) {
  EXPECT_THAT(ArenaConstant(true), Eq(ArenaConstant(true)));
  EXPECT_THAT(ArenaConstant(true), Ne(ArenaConstant(false)));
  EXPECT_TRUE(ArenaConstant(true).IsBool());
  EXPECT_THAT(ArenaConstant(true).AsBool(), Optional(Eq(true)));
  EXPECT_THAT(static_cast<bool>(ArenaConstant(true)), Eq(true));
}

TEST(ArenaConstant, Int) {
  EXPECT_THAT(ArenaConstant(int64_t{1}), Eq(ArenaConstant(int64_t{1})));
  EXPECT_THAT(ArenaConstant(int64_t{1}), Ne(ArenaConstant(int64_t{0})));
  EXPECT_TRUE(ArenaConstant(int64_t{1}).IsInt());
  EXPECT_THAT(ArenaConstant(int64_t{1}).AsInt(), Optional(Eq(int64_t{1})));
  EXPECT_THAT(static_cast<int64_t>(ArenaConstant(int64_t{1})), Eq(int64_t{1}));
}

TEST(ArenaConstant, Uint) {
  EXPECT_THAT(ArenaConstant(uint64_t{1}), Eq(ArenaConstant(uint64_t{1})));
  EXPECT_THAT(ArenaConstant(uint64_t{1}), Ne(ArenaConstant(uint64_t{0})));
  EXPECT_TRUE(ArenaConstant(uint64_t{1}).IsUint());
  EXPECT_THAT(ArenaConstant(uint64_t{1}).AsUint(), Optional(Eq(uint64_t{1})));
  EXPECT_THAT(static_cast<uint64_t>(ArenaConstant(uint64_t{1})),
              Eq(uint64_t{1}));
}

TEST(ArenaConstant, Double) {
  EXPECT_THAT(ArenaConstant(1.0), Eq(ArenaConstant(1.0)));
  EXPECT_THAT(ArenaConstant(1.0), Ne(ArenaConstant(0.0)));
  EXPECT_TRUE(ArenaConstant(1.0).IsDouble());
  EXPECT_THAT(ArenaConstant(1.0).AsDouble(), Optional(Eq(1.0)));
  EXPECT_THAT(static_cast<double>(ArenaConstant(1.0)), Eq(1.0));
}

TEST(ArenaConstant, Bytes) {
  auto bytes = ArenaBytes::Static("foo");
  EXPECT_THAT(ArenaConstant(bytes), Eq(ArenaConstant(bytes)));
  EXPECT_THAT(ArenaConstant(bytes), Ne(ArenaConstant(ArenaBytes::Static(""))));
  EXPECT_TRUE(ArenaConstant(bytes).IsBytes());
  EXPECT_THAT(ArenaConstant(bytes).AsBytes(), Optional(Eq(bytes)));
  EXPECT_THAT(static_cast<ArenaBytes>(ArenaConstant(bytes)), Eq(bytes));
}

TEST(ArenaConstant, String) {
  auto string = ArenaString::Static("foo");
  EXPECT_THAT(ArenaConstant(string), Eq(ArenaConstant(string)));
  EXPECT_THAT(ArenaConstant(string),
              Ne(ArenaConstant(ArenaString::Static(""))));
  EXPECT_TRUE(ArenaConstant(string).IsString());
  EXPECT_THAT(ArenaConstant(string).AsString(), Optional(Eq(string)));
  EXPECT_THAT(static_cast<ArenaString>(ArenaConstant(string)), Eq(string));
}

TEST(ArenaConstant, Duration) {
  auto duration = absl::Seconds(1);
  EXPECT_THAT(ArenaConstant(duration), Eq(ArenaConstant(duration)));
  EXPECT_THAT(ArenaConstant(duration), Ne(ArenaConstant(absl::ZeroDuration())));
  EXPECT_TRUE(ArenaConstant(duration).IsDuration());
  EXPECT_THAT(ArenaConstant(duration).AsDuration(), Optional(Eq(duration)));
  EXPECT_THAT(static_cast<absl::Duration>(ArenaConstant(duration)),
              Eq(duration));
}

TEST(ArenaConstant, Timestamp) {
  auto timestamp = absl::UnixEpoch() + absl::Seconds(1);
  EXPECT_THAT(ArenaConstant(timestamp), Eq(ArenaConstant(timestamp)));
  EXPECT_THAT(ArenaConstant(timestamp), Ne(ArenaConstant(absl::UnixEpoch())));
  EXPECT_TRUE(ArenaConstant(timestamp).IsTimestamp());
  EXPECT_THAT(ArenaConstant(timestamp).AsTimestamp(), Optional(Eq(timestamp)));
  EXPECT_THAT(static_cast<absl::Time>(ArenaConstant(timestamp)), Eq(timestamp));
}

TEST(ArenaConstant, ImplementsAbslHashCorrectly) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      {ArenaConstant(), ArenaConstant(nullptr), ArenaConstant(true),
       ArenaConstant(int64_t{1}), ArenaConstant(uint64_t{1}),
       ArenaConstant(1.0), ArenaConstant(ArenaBytes::Static("foo")),
       ArenaConstant(ArenaString::Static("foo")),
       ArenaConstant(absl::Seconds(1)),
       ArenaConstant(absl::UnixEpoch() + absl::Seconds(1))}));
}

TEST(ArenaConstant, Make) {
  google::protobuf::Arena arena;
  auto string_pool = NewArenaStringPool(&arena);
  auto bytes_pool = NewArenaBytesPool(&arena);
  EXPECT_THAT(
      MakeArenaConstant(string_pool.get(), bytes_pool.get(), Constant()),
      Eq(ArenaConstant()));
  EXPECT_THAT(
      MakeArenaConstant(string_pool.get(), bytes_pool.get(), Constant(nullptr)),
      Eq(ArenaConstant(nullptr)));
  EXPECT_THAT(
      MakeArenaConstant(string_pool.get(), bytes_pool.get(), Constant(true)),
      Eq(ArenaConstant(true)));
  EXPECT_THAT(MakeArenaConstant(string_pool.get(), bytes_pool.get(),
                                Constant(int64_t{1})),
              Eq(ArenaConstant(int64_t{1})));
  EXPECT_THAT(MakeArenaConstant(string_pool.get(), bytes_pool.get(),
                                Constant(uint64_t{1})),
              Eq(ArenaConstant(uint64_t{1})));
  EXPECT_THAT(
      MakeArenaConstant(string_pool.get(), bytes_pool.get(), Constant(1.0)),
      Eq(ArenaConstant(1.0)));
  EXPECT_THAT(MakeArenaConstant(string_pool.get(), bytes_pool.get(),
                                Constant(BytesConstant("foo"))),
              Eq(ArenaConstant(ArenaBytes::Static("foo"))));
  EXPECT_THAT(MakeArenaConstant(string_pool.get(), bytes_pool.get(),
                                Constant(StringConstant("foo"))),
              Eq(ArenaConstant(ArenaString::Static("foo"))));
  EXPECT_THAT(MakeArenaConstant(string_pool.get(), bytes_pool.get(),
                                Constant(absl::Seconds(1))),
              Eq(ArenaConstant(absl::Seconds(1))));
  EXPECT_THAT(MakeArenaConstant(string_pool.get(), bytes_pool.get(),
                                Constant(absl::UnixEpoch() + absl::Seconds(1))),
              Eq(ArenaConstant(absl::UnixEpoch() + absl::Seconds(1))));
}

}  // namespace
}  // namespace cel
