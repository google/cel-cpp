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

TEST(BytesType, Kind) {
  EXPECT_EQ(BytesType().kind(), BytesType::kKind);
  EXPECT_EQ(Type(BytesType()).kind(), BytesType::kKind);
}

TEST(BytesType, Name) {
  EXPECT_EQ(BytesType().name(), BytesType::kName);
  EXPECT_EQ(Type(BytesType()).name(), BytesType::kName);
}

TEST(BytesType, DebugString) {
  {
    std::ostringstream out;
    out << BytesType();
    EXPECT_EQ(out.str(), BytesType::kName);
  }
  {
    std::ostringstream out;
    out << Type(BytesType());
    EXPECT_EQ(out.str(), BytesType::kName);
  }
}

TEST(BytesType, Hash) {
  EXPECT_EQ(absl::HashOf(BytesType()), absl::HashOf(BytesType()));
}

TEST(BytesType, Equal) {
  EXPECT_EQ(BytesType(), BytesType());
  EXPECT_EQ(Type(BytesType()), BytesType());
  EXPECT_EQ(BytesType(), Type(BytesType()));
  EXPECT_EQ(Type(BytesType()), Type(BytesType()));
}

TEST(BytesType, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(BytesType()), NativeTypeId::For<BytesType>());
  EXPECT_EQ(NativeTypeId::Of(Type(BytesType())),
            NativeTypeId::For<BytesType>());
}

TEST(BytesType, InstanceOf) {
  EXPECT_TRUE(InstanceOf<BytesType>(BytesType()));
  EXPECT_TRUE(InstanceOf<BytesType>(Type(BytesType())));
}

TEST(BytesType, Cast) {
  EXPECT_THAT(Cast<BytesType>(BytesType()), An<BytesType>());
  EXPECT_THAT(Cast<BytesType>(Type(BytesType())), An<BytesType>());
}

TEST(BytesType, As) {
  EXPECT_THAT(As<BytesType>(BytesType()), Ne(absl::nullopt));
  EXPECT_THAT(As<BytesType>(Type(BytesType())), Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
