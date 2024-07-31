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

TEST(IntWrapperType, Kind) {
  EXPECT_EQ(IntWrapperType().kind(), IntWrapperType::kKind);
  EXPECT_EQ(Type(IntWrapperType()).kind(), IntWrapperType::kKind);
}

TEST(IntWrapperType, Name) {
  EXPECT_EQ(IntWrapperType().name(), IntWrapperType::kName);
  EXPECT_EQ(Type(IntWrapperType()).name(), IntWrapperType::kName);
}

TEST(IntWrapperType, DebugString) {
  {
    std::ostringstream out;
    out << IntWrapperType();
    EXPECT_EQ(out.str(), IntWrapperType::kName);
  }
  {
    std::ostringstream out;
    out << Type(IntWrapperType());
    EXPECT_EQ(out.str(), IntWrapperType::kName);
  }
}

TEST(IntWrapperType, Hash) {
  EXPECT_EQ(absl::HashOf(IntWrapperType()), absl::HashOf(IntWrapperType()));
}

TEST(IntWrapperType, Equal) {
  EXPECT_EQ(IntWrapperType(), IntWrapperType());
  EXPECT_EQ(Type(IntWrapperType()), IntWrapperType());
  EXPECT_EQ(IntWrapperType(), Type(IntWrapperType()));
  EXPECT_EQ(Type(IntWrapperType()), Type(IntWrapperType()));
}

TEST(IntWrapperType, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(IntWrapperType()),
            NativeTypeId::For<IntWrapperType>());
  EXPECT_EQ(NativeTypeId::Of(Type(IntWrapperType())),
            NativeTypeId::For<IntWrapperType>());
}

TEST(IntWrapperType, InstanceOf) {
  EXPECT_TRUE(InstanceOf<IntWrapperType>(IntWrapperType()));
  EXPECT_TRUE(InstanceOf<IntWrapperType>(Type(IntWrapperType())));
}

TEST(IntWrapperType, Cast) {
  EXPECT_THAT(Cast<IntWrapperType>(IntWrapperType()), An<IntWrapperType>());
  EXPECT_THAT(Cast<IntWrapperType>(Type(IntWrapperType())),
              An<IntWrapperType>());
}

TEST(IntWrapperType, As) {
  EXPECT_THAT(As<IntWrapperType>(IntWrapperType()), Ne(absl::nullopt));
  EXPECT_THAT(As<IntWrapperType>(Type(IntWrapperType())), Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
