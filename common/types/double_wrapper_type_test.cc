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

TEST(DoubleWrapperType, Kind) {
  EXPECT_EQ(DoubleWrapperType().kind(), DoubleWrapperType::kKind);
  EXPECT_EQ(Type(DoubleWrapperType()).kind(), DoubleWrapperType::kKind);
}

TEST(DoubleWrapperType, Name) {
  EXPECT_EQ(DoubleWrapperType().name(), DoubleWrapperType::kName);
  EXPECT_EQ(Type(DoubleWrapperType()).name(), DoubleWrapperType::kName);
}

TEST(DoubleWrapperType, DebugString) {
  {
    std::ostringstream out;
    out << DoubleWrapperType();
    EXPECT_EQ(out.str(), DoubleWrapperType::kName);
  }
  {
    std::ostringstream out;
    out << Type(DoubleWrapperType());
    EXPECT_EQ(out.str(), DoubleWrapperType::kName);
  }
}

TEST(DoubleWrapperType, Hash) {
  EXPECT_EQ(absl::HashOf(DoubleWrapperType()),
            absl::HashOf(DoubleWrapperType()));
}

TEST(DoubleWrapperType, Equal) {
  EXPECT_EQ(DoubleWrapperType(), DoubleWrapperType());
  EXPECT_EQ(Type(DoubleWrapperType()), DoubleWrapperType());
  EXPECT_EQ(DoubleWrapperType(), Type(DoubleWrapperType()));
  EXPECT_EQ(Type(DoubleWrapperType()), Type(DoubleWrapperType()));
}

TEST(DoubleWrapperType, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(DoubleWrapperType()),
            NativeTypeId::For<DoubleWrapperType>());
  EXPECT_EQ(NativeTypeId::Of(Type(DoubleWrapperType())),
            NativeTypeId::For<DoubleWrapperType>());
}

TEST(DoubleWrapperType, InstanceOf) {
  EXPECT_TRUE(InstanceOf<DoubleWrapperType>(DoubleWrapperType()));
  EXPECT_TRUE(InstanceOf<DoubleWrapperType>(Type(DoubleWrapperType())));
}

TEST(DoubleWrapperType, Cast) {
  EXPECT_THAT(Cast<DoubleWrapperType>(DoubleWrapperType()),
              An<DoubleWrapperType>());
  EXPECT_THAT(Cast<DoubleWrapperType>(Type(DoubleWrapperType())),
              An<DoubleWrapperType>());
}

TEST(DoubleWrapperType, As) {
  EXPECT_THAT(As<DoubleWrapperType>(DoubleWrapperType()), Ne(absl::nullopt));
  EXPECT_THAT(As<DoubleWrapperType>(Type(DoubleWrapperType())),
              Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
