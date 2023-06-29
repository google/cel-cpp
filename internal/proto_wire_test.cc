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

#include "internal/proto_wire.h"

#include <limits>

#include "absl/strings/cord.h"
#include "internal/testing.h"

namespace cel::internal {
namespace {

TEST(Varint, Size) {
  EXPECT_EQ(VarintSize(int32_t{-1}),
            VarintSize(std::numeric_limits<uint64_t>::max()));
  EXPECT_EQ(VarintSize(int64_t{-1}),
            VarintSize(std::numeric_limits<uint64_t>::max()));
}

TEST(Varint, MaxSize) {
  EXPECT_EQ(kMaxVarintSize<bool>, 1);
  EXPECT_EQ(kMaxVarintSize<int32_t>, 10);
  EXPECT_EQ(kMaxVarintSize<int64_t>, 10);
  EXPECT_EQ(kMaxVarintSize<uint32_t>, 5);
  EXPECT_EQ(kMaxVarintSize<uint64_t>, 10);
}

namespace {

template <typename T>
absl::Cord VarintEncode(T value) {
  absl::Cord cord;
  internal::VarintEncode(value, cord);
  return cord;
}

}  // namespace

TEST(Varint, Encode) {
  EXPECT_EQ(VarintEncode(true), "\x01");
  EXPECT_EQ(VarintEncode(int32_t{1}), "\x01");
  EXPECT_EQ(VarintEncode(int64_t{1}), "\x01");
  EXPECT_EQ(VarintEncode(uint32_t{1}), "\x01");
  EXPECT_EQ(VarintEncode(uint64_t{1}), "\x01");
  EXPECT_EQ(VarintEncode(int32_t{-1}),
            VarintEncode(std::numeric_limits<uint64_t>::max()));
  EXPECT_EQ(VarintEncode(int64_t{-1}),
            VarintEncode(std::numeric_limits<uint64_t>::max()));
  EXPECT_EQ(VarintEncode(std::numeric_limits<uint32_t>::max()),
            "\xff\xff\xff\xff\x0f");
  EXPECT_EQ(VarintEncode(std::numeric_limits<uint64_t>::max()),
            "\xff\xff\xff\xff\xff\xff\xff\xff\xff\x01");
}

namespace {

template <typename T>
absl::Cord Fixed64Encode(T value) {
  absl::Cord cord;
  internal::Fixed64Encode(value, cord);
  return cord;
}

}  // namespace

TEST(Fixed64, Encode) {
  EXPECT_EQ(Fixed64Encode(0.0), Fixed64Encode(uint64_t{0}));
}

}  // namespace
}  // namespace cel::internal
