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

TEST(StringType, Kind) {
  EXPECT_EQ(StringType().kind(), StringType::kKind);
  EXPECT_EQ(Type(StringType()).kind(), StringType::kKind);
}

TEST(StringType, Name) {
  EXPECT_EQ(StringType().name(), StringType::kName);
  EXPECT_EQ(Type(StringType()).name(), StringType::kName);
}

TEST(StringType, DebugString) {
  {
    std::ostringstream out;
    out << StringType();
    EXPECT_EQ(out.str(), StringType::kName);
  }
  {
    std::ostringstream out;
    out << Type(StringType());
    EXPECT_EQ(out.str(), StringType::kName);
  }
}

TEST(StringType, Hash) {
  EXPECT_EQ(absl::HashOf(StringType()), absl::HashOf(StringType()));
}

TEST(StringType, Equal) {
  EXPECT_EQ(StringType(), StringType());
  EXPECT_EQ(Type(StringType()), StringType());
  EXPECT_EQ(StringType(), Type(StringType()));
  EXPECT_EQ(Type(StringType()), Type(StringType()));
}

TEST(StringType, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(StringType()), NativeTypeId::For<StringType>());
  EXPECT_EQ(NativeTypeId::Of(Type(StringType())),
            NativeTypeId::For<StringType>());
}

TEST(StringType, InstanceOf) {
  EXPECT_TRUE(InstanceOf<StringType>(StringType()));
  EXPECT_TRUE(InstanceOf<StringType>(Type(StringType())));
}

TEST(StringType, Cast) {
  EXPECT_THAT(Cast<StringType>(StringType()), An<StringType>());
  EXPECT_THAT(Cast<StringType>(Type(StringType())), An<StringType>());
}

TEST(StringType, As) {
  EXPECT_THAT(As<StringType>(StringType()), Ne(absl::nullopt));
  EXPECT_THAT(As<StringType>(Type(StringType())), Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
