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
#include <string>

#include "absl/hash/hash.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/memory_testing.h"
#include "common/native_type.h"
#include "common/type.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::An;
using testing::Ne;
using testing::TestParamInfo;
using testing::TestWithParam;

TEST(MapType, Default) {
  MapType map_type;
  EXPECT_EQ(map_type.key(), DynType());
  EXPECT_EQ(map_type.value(), DynType());
}

class MapTypeTest : public common_internal::ThreadCompatibleMemoryTest<> {};

TEST_P(MapTypeTest, Kind) {
  EXPECT_EQ(MapType(memory_manager(), StringType(), BytesType()).kind(),
            MapType::kKind);
  EXPECT_EQ(Type(MapType(memory_manager(), StringType(), BytesType())).kind(),
            MapType::kKind);
}

TEST_P(MapTypeTest, Name) {
  EXPECT_EQ(MapType(memory_manager(), StringType(), BytesType()).name(),
            MapType::kName);
  EXPECT_EQ(Type(MapType(memory_manager(), StringType(), BytesType())).name(),
            MapType::kName);
}

TEST_P(MapTypeTest, DebugString) {
  {
    std::ostringstream out;
    out << MapType(memory_manager(), StringType(), BytesType());
    EXPECT_EQ(out.str(), "map<string, bytes>");
  }
  {
    std::ostringstream out;
    out << Type(MapType(memory_manager(), StringType(), BytesType()));
    EXPECT_EQ(out.str(), "map<string, bytes>");
  }
}

TEST_P(MapTypeTest, Hash) {
  EXPECT_EQ(absl::HashOf(MapType(memory_manager(), StringType(), BytesType())),
            absl::HashOf(MapType(memory_manager(), StringType(), BytesType())));
}

TEST_P(MapTypeTest, Equal) {
  EXPECT_EQ(MapType(memory_manager(), StringType(), BytesType()),
            MapType(memory_manager(), StringType(), BytesType()));
  EXPECT_EQ(Type(MapType(memory_manager(), StringType(), BytesType())),
            MapType(memory_manager(), StringType(), BytesType()));
  EXPECT_EQ(MapType(memory_manager(), StringType(), BytesType()),
            Type(MapType(memory_manager(), StringType(), BytesType())));
  EXPECT_EQ(Type(MapType(memory_manager(), StringType(), BytesType())),
            Type(MapType(memory_manager(), StringType(), BytesType())));
}

TEST_P(MapTypeTest, NativeTypeId) {
  EXPECT_EQ(
      NativeTypeId::Of(MapType(memory_manager(), StringType(), BytesType())),
      NativeTypeId::For<MapType>());
  EXPECT_EQ(NativeTypeId::Of(
                Type(MapType(memory_manager(), StringType(), BytesType()))),
            NativeTypeId::For<MapType>());
}

TEST_P(MapTypeTest, InstanceOf) {
  EXPECT_TRUE(InstanceOf<MapType>(
      MapType(memory_manager(), StringType(), BytesType())));
  EXPECT_TRUE(InstanceOf<MapType>(
      Type(MapType(memory_manager(), StringType(), BytesType()))));
}

TEST_P(MapTypeTest, Cast) {
  EXPECT_THAT(
      Cast<MapType>(MapType(memory_manager(), StringType(), BytesType())),
      An<MapType>());
  EXPECT_THAT(
      Cast<MapType>(Type(MapType(memory_manager(), StringType(), BytesType()))),
      An<MapType>());
}

TEST_P(MapTypeTest, As) {
  EXPECT_THAT(As<MapType>(MapType(memory_manager(), StringType(), BytesType())),
              Ne(absl::nullopt));
  EXPECT_THAT(
      As<MapType>(Type(MapType(memory_manager(), StringType(), BytesType()))),
      Ne(absl::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    MapTypeTest, MapTypeTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    MapTypeTest::ToString);

}  // namespace
}  // namespace cel
