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

TEST(ListType, Default) {
  ListType list_type;
  EXPECT_EQ(list_type.element(), DynType());
}

TEST(ListTypeView, Default) {
  ListTypeView list_type;
  EXPECT_EQ(list_type.element(), DynType());
}

class ListTypeTest : public TestWithParam<MemoryManagement> {
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

TEST_P(ListTypeTest, Kind) {
  EXPECT_EQ(ListType(memory_manager(), BoolType()).kind(), ListType::kKind);
  EXPECT_EQ(Type(ListType(memory_manager(), BoolType())).kind(),
            ListType::kKind);
}

TEST_P(ListTypeTest, Name) {
  EXPECT_EQ(ListType(memory_manager(), BoolType()).name(), ListType::kName);
  EXPECT_EQ(Type(ListType(memory_manager(), BoolType())).name(),
            ListType::kName);
}

TEST_P(ListTypeTest, DebugString) {
  {
    std::ostringstream out;
    out << ListType(memory_manager(), BoolType());
    EXPECT_EQ(out.str(), "list<bool>");
  }
  {
    std::ostringstream out;
    out << Type(ListType(memory_manager(), BoolType()));
    EXPECT_EQ(out.str(), "list<bool>");
  }
}

TEST_P(ListTypeTest, Hash) {
  EXPECT_EQ(absl::HashOf(ListType(memory_manager(), BoolType())),
            absl::HashOf(ListType(memory_manager(), BoolType())));
  EXPECT_EQ(absl::HashOf(Type(ListType(memory_manager(), BoolType()))),
            absl::HashOf(ListType(memory_manager(), BoolType())));
}

TEST_P(ListTypeTest, Equal) {
  EXPECT_EQ(ListType(memory_manager(), BoolType()),
            ListType(memory_manager(), BoolType()));
  EXPECT_EQ(Type(ListType(memory_manager(), BoolType())),
            ListType(memory_manager(), BoolType()));
  EXPECT_EQ(ListType(memory_manager(), BoolType()),
            Type(ListType(memory_manager(), BoolType())));
  EXPECT_EQ(Type(ListType(memory_manager(), BoolType())),
            Type(ListType(memory_manager(), BoolType())));
}

TEST_P(ListTypeTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(ListType(memory_manager(), BoolType())),
            NativeTypeId::For<ListType>());
  EXPECT_EQ(NativeTypeId::Of(Type(ListType(memory_manager(), BoolType()))),
            NativeTypeId::For<ListType>());
}

TEST_P(ListTypeTest, InstanceOf) {
  EXPECT_TRUE(InstanceOf<ListType>(ListType(memory_manager(), BoolType())));
  EXPECT_TRUE(
      InstanceOf<ListType>(Type(ListType(memory_manager(), BoolType()))));
}

TEST_P(ListTypeTest, Cast) {
  EXPECT_THAT(Cast<ListType>(ListType(memory_manager(), BoolType())),
              An<ListType>());
  EXPECT_THAT(Cast<ListType>(Type(ListType(memory_manager(), BoolType()))),
              An<ListType>());
}

TEST_P(ListTypeTest, As) {
  EXPECT_THAT(As<ListType>(ListType(memory_manager(), BoolType())),
              Ne(absl::nullopt));
  EXPECT_THAT(As<ListType>(Type(ListType(memory_manager(), BoolType()))),
              Ne(absl::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    ListTypeTest, ListTypeTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    ListTypeTest::ToString);

class ListTypeViewTest : public TestWithParam<MemoryManagement> {
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

TEST_P(ListTypeViewTest, Kind) {
  auto type = ListType(memory_manager(), BoolType());
  EXPECT_EQ(ListTypeView(type).kind(), ListTypeView::kKind);
  EXPECT_EQ(TypeView(ListTypeView(type)).kind(), ListTypeView::kKind);
}

TEST_P(ListTypeViewTest, Name) {
  auto type = ListType(memory_manager(), BoolType());
  EXPECT_EQ(ListTypeView(type).name(), ListTypeView::kName);
  EXPECT_EQ(TypeView(ListTypeView(type)).name(), ListTypeView::kName);
}

TEST_P(ListTypeViewTest, DebugString) {
  auto type = ListType(memory_manager(), BoolType());
  {
    std::ostringstream out;
    out << ListTypeView(type);
    EXPECT_EQ(out.str(), "list<bool>");
  }
  {
    std::ostringstream out;
    out << TypeView(ListTypeView(type));
    EXPECT_EQ(out.str(), "list<bool>");
  }
}

TEST_P(ListTypeViewTest, Hash) {
  auto type = ListType(memory_manager(), BoolType());
  EXPECT_EQ(absl::HashOf(ListTypeView(type)), absl::HashOf(ListTypeView(type)));
  EXPECT_EQ(absl::HashOf(TypeView(ListTypeView(type))),
            absl::HashOf(ListTypeView(type)));
  EXPECT_EQ(absl::HashOf(ListTypeView(type)), absl::HashOf(ListType(type)));
  EXPECT_EQ(absl::HashOf(TypeView(ListTypeView(type))),
            absl::HashOf(ListType(type)));
}

TEST_P(ListTypeViewTest, Equal) {
  auto type = ListType(memory_manager(), BoolType());
  EXPECT_EQ(ListTypeView(type), ListTypeView(type));
  EXPECT_EQ(TypeView(ListTypeView(type)), ListTypeView(type));
  EXPECT_EQ(ListTypeView(type), TypeView(ListTypeView(type)));
  EXPECT_EQ(TypeView(ListTypeView(type)), TypeView(ListTypeView(type)));
  EXPECT_EQ(ListTypeView(type), ListType(type));
  EXPECT_EQ(TypeView(ListTypeView(type)), ListType(type));
  EXPECT_EQ(TypeView(ListTypeView(type)), Type(ListType(type)));
  EXPECT_EQ(ListType(type), ListTypeView(type));
  EXPECT_EQ(ListType(type), ListTypeView(type));
  EXPECT_EQ(ListType(type), TypeView(ListTypeView(type)));
  EXPECT_EQ(Type(ListType(type)), TypeView(ListTypeView(type)));
  EXPECT_EQ(ListTypeView(type), ListType(type));
}

TEST_P(ListTypeViewTest, NativeTypeId) {
  auto type = ListType(memory_manager(), BoolType());
  EXPECT_EQ(NativeTypeId::Of(ListTypeView(type)),
            NativeTypeId::For<ListTypeView>());
  EXPECT_EQ(NativeTypeId::Of(TypeView(ListTypeView(type))),
            NativeTypeId::For<ListTypeView>());
}

TEST_P(ListTypeViewTest, InstanceOf) {
  auto type = ListType(memory_manager(), BoolType());
  EXPECT_TRUE(InstanceOf<ListTypeView>(ListTypeView(type)));
  EXPECT_TRUE(InstanceOf<ListTypeView>(TypeView(ListTypeView(type))));
}

TEST_P(ListTypeViewTest, Cast) {
  auto type = ListType(memory_manager(), BoolType());
  EXPECT_THAT(Cast<ListTypeView>(ListTypeView(type)), An<ListTypeView>());
  EXPECT_THAT(Cast<ListTypeView>(TypeView(ListTypeView(type))),
              An<ListTypeView>());
}

TEST_P(ListTypeViewTest, As) {
  auto type = ListType(memory_manager(), BoolType());
  EXPECT_THAT(As<ListTypeView>(ListTypeView(type)), Ne(absl::nullopt));
  EXPECT_THAT(As<ListTypeView>(TypeView(ListTypeView(type))),
              Ne(absl::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    ListTypeViewTest, ListTypeViewTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    ListTypeViewTest::ToString);

}  // namespace
}  // namespace cel
