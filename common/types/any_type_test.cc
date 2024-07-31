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

#include <sstream>

#include "absl/hash/hash.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/native_type.h"
#include "common/type.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::An;
using testing::Ne;

TEST(AnyType, Kind) {
  EXPECT_EQ(AnyType().kind(), AnyType::kKind);
  EXPECT_EQ(Type(AnyType()).kind(), AnyType::kKind);
}

TEST(AnyType, Name) {
  EXPECT_EQ(AnyType().name(), AnyType::kName);
  EXPECT_EQ(Type(AnyType()).name(), AnyType::kName);
}

TEST(AnyType, DebugString) {
  {
    std::ostringstream out;
    out << AnyType();
    EXPECT_EQ(out.str(), AnyType::kName);
  }
  {
    std::ostringstream out;
    out << Type(AnyType());
    EXPECT_EQ(out.str(), AnyType::kName);
  }
}

TEST(AnyType, Hash) {
  EXPECT_EQ(absl::HashOf(AnyType()), absl::HashOf(AnyType()));
}

TEST(AnyType, Equal) {
  EXPECT_EQ(AnyType(), AnyType());
  EXPECT_EQ(Type(AnyType()), AnyType());
  EXPECT_EQ(AnyType(), Type(AnyType()));
  EXPECT_EQ(Type(AnyType()), Type(AnyType()));
}

TEST(AnyType, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(AnyType()), NativeTypeId::For<AnyType>());
  EXPECT_EQ(NativeTypeId::Of(Type(AnyType())), NativeTypeId::For<AnyType>());
}

TEST(AnyType, InstanceOf) {
  EXPECT_TRUE(InstanceOf<AnyType>(AnyType()));
  EXPECT_TRUE(InstanceOf<AnyType>(Type(AnyType())));
}

TEST(AnyType, Cast) {
  EXPECT_THAT(Cast<AnyType>(AnyType()), An<AnyType>());
  EXPECT_THAT(Cast<AnyType>(Type(AnyType())), An<AnyType>());
}

TEST(AnyType, As) {
  EXPECT_THAT(As<AnyType>(AnyType()), Ne(absl::nullopt));
  EXPECT_THAT(As<AnyType>(Type(AnyType())), Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
