// Copyright 2022 Google LLC
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

#include "internal/rtti.h"

#include "absl/hash/hash_testing.h"
#include "internal/testing.h"

namespace cel::internal {
namespace {

struct Type1 {};

struct Type2 {};

TEST(TypeInfo, Default) { EXPECT_EQ(TypeInfo(), TypeInfo()); }

TEST(TypeId, SupportsAbslHash) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      {TypeInfo(), TypeId<Type1>(), TypeId<Type2>()}));
}

}  // namespace
}  // namespace cel::internal
