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

TEST(TimestampType, Kind) {
  EXPECT_EQ(TimestampType().kind(), TimestampType::kKind);
  EXPECT_EQ(Type(TimestampType()).kind(), TimestampType::kKind);
}

TEST(TimestampType, Name) {
  EXPECT_EQ(TimestampType().name(), TimestampType::kName);
  EXPECT_EQ(Type(TimestampType()).name(), TimestampType::kName);
}

TEST(TimestampType, DebugString) {
  {
    std::ostringstream out;
    out << TimestampType();
    EXPECT_EQ(out.str(), TimestampType::kName);
  }
  {
    std::ostringstream out;
    out << Type(TimestampType());
    EXPECT_EQ(out.str(), TimestampType::kName);
  }
}

TEST(TimestampType, Hash) {
  EXPECT_EQ(absl::HashOf(TimestampType()), absl::HashOf(TimestampType()));
}

TEST(TimestampType, Equal) {
  EXPECT_EQ(TimestampType(), TimestampType());
  EXPECT_EQ(Type(TimestampType()), TimestampType());
  EXPECT_EQ(TimestampType(), Type(TimestampType()));
  EXPECT_EQ(Type(TimestampType()), Type(TimestampType()));
}

TEST(TimestampType, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(TimestampType()),
            NativeTypeId::For<TimestampType>());
  EXPECT_EQ(NativeTypeId::Of(Type(TimestampType())),
            NativeTypeId::For<TimestampType>());
}

TEST(TimestampType, InstanceOf) {
  EXPECT_TRUE(InstanceOf<TimestampType>(TimestampType()));
  EXPECT_TRUE(InstanceOf<TimestampType>(Type(TimestampType())));
}

TEST(TimestampType, Cast) {
  EXPECT_THAT(Cast<TimestampType>(TimestampType()), An<TimestampType>());
  EXPECT_THAT(Cast<TimestampType>(Type(TimestampType())), An<TimestampType>());
}

TEST(TimestampType, As) {
  EXPECT_THAT(As<TimestampType>(TimestampType()), Ne(absl::nullopt));
  EXPECT_THAT(As<TimestampType>(Type(TimestampType())), Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
