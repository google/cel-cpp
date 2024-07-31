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

TEST(BytesWrapperType, Kind) {
  EXPECT_EQ(BytesWrapperType().kind(), BytesWrapperType::kKind);
  EXPECT_EQ(Type(BytesWrapperType()).kind(), BytesWrapperType::kKind);
}

TEST(BytesWrapperType, Name) {
  EXPECT_EQ(BytesWrapperType().name(), BytesWrapperType::kName);
  EXPECT_EQ(Type(BytesWrapperType()).name(), BytesWrapperType::kName);
}

TEST(BytesWrapperType, DebugString) {
  {
    std::ostringstream out;
    out << BytesWrapperType();
    EXPECT_EQ(out.str(), BytesWrapperType::kName);
  }
  {
    std::ostringstream out;
    out << Type(BytesWrapperType());
    EXPECT_EQ(out.str(), BytesWrapperType::kName);
  }
}

TEST(BytesWrapperType, Hash) {
  EXPECT_EQ(absl::HashOf(BytesWrapperType()), absl::HashOf(BytesWrapperType()));
}

TEST(BytesWrapperType, Equal) {
  EXPECT_EQ(BytesWrapperType(), BytesWrapperType());
  EXPECT_EQ(Type(BytesWrapperType()), BytesWrapperType());
  EXPECT_EQ(BytesWrapperType(), Type(BytesWrapperType()));
  EXPECT_EQ(Type(BytesWrapperType()), Type(BytesWrapperType()));
}

TEST(BytesWrapperType, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(BytesWrapperType()),
            NativeTypeId::For<BytesWrapperType>());
  EXPECT_EQ(NativeTypeId::Of(Type(BytesWrapperType())),
            NativeTypeId::For<BytesWrapperType>());
}

TEST(BytesWrapperType, InstanceOf) {
  EXPECT_TRUE(InstanceOf<BytesWrapperType>(BytesWrapperType()));
  EXPECT_TRUE(InstanceOf<BytesWrapperType>(Type(BytesWrapperType())));
}

TEST(BytesWrapperType, Cast) {
  EXPECT_THAT(Cast<BytesWrapperType>(BytesWrapperType()),
              An<BytesWrapperType>());
  EXPECT_THAT(Cast<BytesWrapperType>(Type(BytesWrapperType())),
              An<BytesWrapperType>());
}

TEST(BytesWrapperType, As) {
  EXPECT_THAT(As<BytesWrapperType>(BytesWrapperType()), Ne(absl::nullopt));
  EXPECT_THAT(As<BytesWrapperType>(Type(BytesWrapperType())),
              Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
