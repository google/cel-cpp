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

#include "common/arena_bytes_pool.h"

#include "internal/testing.h"
#include "google/protobuf/arena.h"

namespace cel {
namespace {

TEST(ArenaBytesPool, InternBytes) {
  google::protobuf::Arena arena;
  auto bytes_pool = NewArenaBytesPool(&arena);
  auto expected = bytes_pool->InternBytes("Hello World!");
  auto got = bytes_pool->InternBytes("Hello World!");
  EXPECT_EQ(expected.data(), got.data());
}

}  // namespace
}  // namespace cel
