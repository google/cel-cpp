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

#include "common/type.h"

#include <sstream>
#include <string>

#include "absl/hash/hash.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::An;
using testing::Ne;
using testing::TestParamInfo;
using testing::TestWithParam;

class TypeParamTypeTest : public TestWithParam<MemoryManagement> {
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

TEST_P(TypeParamTypeTest, Kind) {
  EXPECT_EQ(TypeParamType(memory_manager(), "T").kind(), TypeParamType::kKind);
  EXPECT_EQ(Type(TypeParamType(memory_manager(), "T")).kind(),
            TypeParamType::kKind);
}

TEST_P(TypeParamTypeTest, Name) {
  EXPECT_EQ(TypeParamType(memory_manager(), "T").name(), "T");
  EXPECT_EQ(Type(TypeParamType(memory_manager(), "T")).name(), "T");
}

TEST_P(TypeParamTypeTest, DebugString) {
  {
    std::ostringstream out;
    out << TypeParamType(memory_manager(), "T");
    EXPECT_EQ(out.str(), "T");
  }
  {
    std::ostringstream out;
    out << Type(TypeParamType(memory_manager(), "T"));
    EXPECT_EQ(out.str(), "T");
  }
}

TEST_P(TypeParamTypeTest, Hash) {
  EXPECT_EQ(absl::HashOf(TypeParamType(memory_manager(), "T")),
            absl::HashOf(TypeParamType(memory_manager(), "T")));
}

TEST_P(TypeParamTypeTest, Equal) {
  EXPECT_EQ(TypeParamType(memory_manager(), "T"),
            TypeParamType(memory_manager(), "T"));
  EXPECT_EQ(Type(TypeParamType(memory_manager(), "T")),
            TypeParamType(memory_manager(), "T"));
  EXPECT_EQ(TypeParamType(memory_manager(), "T"),
            Type(TypeParamType(memory_manager(), "T")));
  EXPECT_EQ(Type(TypeParamType(memory_manager(), "T")),
            Type(TypeParamType(memory_manager(), "T")));
}

TEST_P(TypeParamTypeTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(TypeParamType(memory_manager(), "T")),
            NativeTypeId::For<TypeParamType>());
  EXPECT_EQ(NativeTypeId::Of(Type(TypeParamType(memory_manager(), "T"))),
            NativeTypeId::For<TypeParamType>());
}

TEST_P(TypeParamTypeTest, InstanceOf) {
  EXPECT_TRUE(InstanceOf<TypeParamType>(TypeParamType(memory_manager(), "T")));
  EXPECT_TRUE(
      InstanceOf<TypeParamType>(Type(TypeParamType(memory_manager(), "T"))));
}

TEST_P(TypeParamTypeTest, Cast) {
  EXPECT_THAT(Cast<TypeParamType>(TypeParamType(memory_manager(), "T")),
              An<TypeParamType>());
  EXPECT_THAT(Cast<TypeParamType>(Type(TypeParamType(memory_manager(), "T"))),
              An<TypeParamType>());
}

TEST_P(TypeParamTypeTest, As) {
  EXPECT_THAT(As<TypeParamType>(TypeParamType(memory_manager(), "T")),
              Ne(absl::nullopt));
  EXPECT_THAT(As<TypeParamType>(Type(TypeParamType(memory_manager(), "T"))),
              Ne(absl::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    TypeParamTypeTest, TypeParamTypeTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    TypeParamTypeTest::ToString);

class TypeParamTypeViewTest : public TestWithParam<MemoryManagement> {
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

TEST_P(TypeParamTypeViewTest, Kind) {
  auto type = TypeParamType(memory_manager(), "T");
  EXPECT_EQ(TypeParamTypeView(type).kind(), TypeParamTypeView::kKind);
  EXPECT_EQ(TypeView(TypeParamTypeView(type)).kind(), TypeParamTypeView::kKind);
}

TEST_P(TypeParamTypeViewTest, Name) {
  auto type = TypeParamType(memory_manager(), "T");
  EXPECT_EQ(TypeParamTypeView(type).name(), "T");
  EXPECT_EQ(TypeView(TypeParamTypeView(type)).name(), "T");
}

TEST_P(TypeParamTypeViewTest, DebugString) {
  auto type = TypeParamType(memory_manager(), "T");
  {
    std::ostringstream out;
    out << TypeParamTypeView(type);
    EXPECT_EQ(out.str(), "T");
  }
  {
    std::ostringstream out;
    out << TypeView(TypeParamTypeView(type));
    EXPECT_EQ(out.str(), "T");
  }
}

TEST_P(TypeParamTypeViewTest, Hash) {
  auto type = TypeParamType(memory_manager(), "T");
  EXPECT_EQ(absl::HashOf(TypeParamTypeView(type)),
            absl::HashOf(TypeParamTypeView(type)));
  EXPECT_EQ(absl::HashOf(TypeParamTypeView(type)),
            absl::HashOf(TypeParamType(type)));
}

TEST_P(TypeParamTypeViewTest, Equal) {
  auto type = TypeParamType(memory_manager(), "T");
  EXPECT_EQ(TypeParamTypeView(type), TypeParamTypeView(type));
  EXPECT_EQ(TypeView(TypeParamTypeView(type)), TypeParamTypeView(type));
  EXPECT_EQ(TypeParamTypeView(type), TypeView(TypeParamTypeView(type)));
  EXPECT_EQ(TypeView(TypeParamTypeView(type)),
            TypeView(TypeParamTypeView(type)));
  EXPECT_EQ(TypeParamTypeView(type), TypeParamType(type));
  EXPECT_EQ(TypeView(TypeParamTypeView(type)), TypeParamType(type));
  EXPECT_EQ(TypeView(TypeParamTypeView(type)), Type(TypeParamType(type)));
  EXPECT_EQ(TypeParamType(type), TypeParamTypeView(type));
  EXPECT_EQ(TypeParamType(type), TypeParamTypeView(type));
  EXPECT_EQ(TypeParamType(type), TypeView(TypeParamTypeView(type)));
  EXPECT_EQ(Type(TypeParamType(type)), TypeView(TypeParamTypeView(type)));
  EXPECT_EQ(TypeParamTypeView(type), TypeParamType(type));
}

TEST_P(TypeParamTypeViewTest, NativeTypeId) {
  auto type = TypeParamType(memory_manager(), "T");
  EXPECT_EQ(NativeTypeId::Of(TypeParamTypeView(type)),
            NativeTypeId::For<TypeParamTypeView>());
  EXPECT_EQ(NativeTypeId::Of(TypeView(TypeParamTypeView(type))),
            NativeTypeId::For<TypeParamTypeView>());
}

TEST_P(TypeParamTypeViewTest, InstanceOf) {
  auto type = TypeParamType(memory_manager(), "T");
  EXPECT_TRUE(InstanceOf<TypeParamTypeView>(TypeParamTypeView(type)));
  EXPECT_TRUE(InstanceOf<TypeParamTypeView>(TypeView(TypeParamTypeView(type))));
}

TEST_P(TypeParamTypeViewTest, Cast) {
  auto type = TypeParamType(memory_manager(), "T");
  EXPECT_THAT(Cast<TypeParamTypeView>(TypeParamTypeView(type)),
              An<TypeParamTypeView>());
  EXPECT_THAT(Cast<TypeParamTypeView>(TypeView(TypeParamTypeView(type))),
              An<TypeParamTypeView>());
}

TEST_P(TypeParamTypeViewTest, As) {
  auto type = TypeParamType(memory_manager(), "T");
  EXPECT_THAT(As<TypeParamTypeView>(TypeParamTypeView(type)),
              Ne(absl::nullopt));
  EXPECT_THAT(As<TypeParamTypeView>(TypeView(TypeParamTypeView(type))),
              Ne(absl::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    TypeParamTypeViewTest, TypeParamTypeViewTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    TypeParamTypeViewTest::ToString);

}  // namespace
}  // namespace cel
