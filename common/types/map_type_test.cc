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

TEST(MapTypeView, Default) {
  MapTypeView map_type;
  EXPECT_EQ(map_type.key(), DynType());
  EXPECT_EQ(map_type.value(), DynType());
}

class MapTypeTest : public TestWithParam<MemoryManagement> {
 public:
  void SetUp() override {
    switch (memory_management()) {
      case MemoryManagement::kPooling:
        memory_manager_ =
            MemoryManager::Pooling(NewThreadCompatiblePoolingMemoryManager());
        break;
      case MemoryManagement::kReferenceCounting:
        memory_manager_ = MemoryManager::ReferenceCounting();
        break;
    }
  }

  void TearDown() override { Finish(); }

  void Finish() { memory_manager_.reset(); }

  MemoryManagerRef memory_manager() { return *memory_manager_; }

  MemoryManagement memory_management() const { return GetParam(); }

  static std::string ToString(TestParamInfo<MemoryManagement> param) {
    std::ostringstream out;
    out << param.param;
    return out.str();
  }

 private:
  absl::optional<MemoryManager> memory_manager_;
};

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

class MapTypeViewTest : public TestWithParam<MemoryManagement> {
 public:
  void SetUp() override {
    switch (memory_management()) {
      case MemoryManagement::kPooling:
        memory_manager_ =
            MemoryManager::Pooling(NewThreadCompatiblePoolingMemoryManager());
        break;
      case MemoryManagement::kReferenceCounting:
        memory_manager_ = MemoryManager::ReferenceCounting();
        break;
    }
  }

  void TearDown() override { Finish(); }

  void Finish() { memory_manager_.reset(); }

  MemoryManagerRef memory_manager() { return *memory_manager_; }

  MemoryManagement memory_management() const { return GetParam(); }

  static std::string ToString(TestParamInfo<MemoryManagement> param) {
    std::ostringstream out;
    out << param.param;
    return out.str();
  }

 private:
  absl::optional<MemoryManager> memory_manager_;
};

TEST_P(MapTypeViewTest, Kind) {
  auto type = MapType(memory_manager(), StringType(), BytesType());
  EXPECT_EQ(MapTypeView(type).kind(), MapTypeView::kKind);
  EXPECT_EQ(TypeView(MapTypeView(type)).kind(), MapTypeView::kKind);
}

TEST_P(MapTypeViewTest, Name) {
  auto type = MapType(memory_manager(), StringType(), BytesType());
  EXPECT_EQ(MapTypeView(type).name(), MapTypeView::kName);
  EXPECT_EQ(TypeView(MapTypeView(type)).name(), MapTypeView::kName);
}

TEST_P(MapTypeViewTest, DebugString) {
  auto type = MapType(memory_manager(), StringType(), BytesType());
  {
    std::ostringstream out;
    out << MapTypeView(type);
    EXPECT_EQ(out.str(), "map<string, bytes>");
  }
  {
    std::ostringstream out;
    out << TypeView(MapTypeView(type));
    EXPECT_EQ(out.str(), "map<string, bytes>");
  }
}

TEST_P(MapTypeViewTest, Hash) {
  auto type = MapType(memory_manager(), StringType(), BytesType());
  EXPECT_EQ(absl::HashOf(MapTypeView(type)), absl::HashOf(MapTypeView(type)));
  EXPECT_EQ(absl::HashOf(MapTypeView(type)), absl::HashOf(MapType(type)));
}

TEST_P(MapTypeViewTest, Equal) {
  auto type = MapType(memory_manager(), StringType(), BytesType());
  EXPECT_EQ(MapTypeView(type), MapTypeView(type));
  EXPECT_EQ(TypeView(MapTypeView(type)), MapTypeView(type));
  EXPECT_EQ(MapTypeView(type), TypeView(MapTypeView(type)));
  EXPECT_EQ(TypeView(MapTypeView(type)), TypeView(MapTypeView(type)));
  EXPECT_EQ(MapTypeView(type), MapType(type));
  EXPECT_EQ(TypeView(MapTypeView(type)), MapType(type));
  EXPECT_EQ(TypeView(MapTypeView(type)), Type(MapType(type)));
  EXPECT_EQ(MapType(type), MapTypeView(type));
  EXPECT_EQ(MapType(type), MapTypeView(type));
  EXPECT_EQ(MapType(type), TypeView(MapTypeView(type)));
  EXPECT_EQ(Type(MapType(type)), TypeView(MapTypeView(type)));
  EXPECT_EQ(MapTypeView(type), MapType(type));
}

TEST_P(MapTypeViewTest, NativeTypeId) {
  auto type = MapType(memory_manager(), StringType(), BytesType());
  EXPECT_EQ(NativeTypeId::Of(MapTypeView(type)),
            NativeTypeId::For<MapTypeView>());
  EXPECT_EQ(NativeTypeId::Of(TypeView(MapTypeView(type))),
            NativeTypeId::For<MapTypeView>());
}

TEST_P(MapTypeViewTest, InstanceOf) {
  auto type = MapType(memory_manager(), StringType(), BytesType());
  EXPECT_TRUE(InstanceOf<MapTypeView>(MapTypeView(type)));
  EXPECT_TRUE(InstanceOf<MapTypeView>(TypeView(MapTypeView(type))));
}

TEST_P(MapTypeViewTest, Cast) {
  auto type = MapType(memory_manager(), StringType(), BytesType());
  EXPECT_THAT(Cast<MapTypeView>(MapTypeView(type)), An<MapTypeView>());
  EXPECT_THAT(Cast<MapTypeView>(TypeView(MapTypeView(type))),
              An<MapTypeView>());
}

TEST_P(MapTypeViewTest, As) {
  auto type = MapType(memory_manager(), StringType(), BytesType());
  EXPECT_THAT(As<MapTypeView>(MapTypeView(type)), Ne(absl::nullopt));
  EXPECT_THAT(As<MapTypeView>(TypeView(MapTypeView(type))), Ne(absl::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    MapTypeViewTest, MapTypeViewTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    MapTypeViewTest::ToString);

}  // namespace
}  // namespace cel
