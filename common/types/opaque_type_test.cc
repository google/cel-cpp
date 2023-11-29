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

class OpaqueTypeTest : public TestWithParam<MemoryManagement> {
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

TEST_P(OpaqueTypeTest, Kind) {
  EXPECT_EQ(OpaqueType(memory_manager(), "test.Opaque", {BytesType()}).kind(),
            OpaqueType::kKind);
  EXPECT_EQ(
      Type(OpaqueType(memory_manager(), "test.Opaque", {BytesType()})).kind(),
      OpaqueType::kKind);
}

TEST_P(OpaqueTypeTest, Name) {
  EXPECT_EQ(OpaqueType(memory_manager(), "test.Opaque", {BytesType()}).name(),
            "test.Opaque");
  EXPECT_EQ(
      Type(OpaqueType(memory_manager(), "test.Opaque", {BytesType()})).name(),
      "test.Opaque");
}

TEST_P(OpaqueTypeTest, DebugString) {
  {
    std::ostringstream out;
    out << OpaqueType(memory_manager(), "test.Opaque", {BytesType()});
    EXPECT_EQ(out.str(), "test.Opaque<bytes>");
  }
  {
    std::ostringstream out;
    out << Type(OpaqueType(memory_manager(), "test.Opaque", {BytesType()}));
    EXPECT_EQ(out.str(), "test.Opaque<bytes>");
  }
  {
    std::ostringstream out;
    out << OpaqueType(memory_manager(), "test.Opaque", {});
    EXPECT_EQ(out.str(), "test.Opaque");
  }
}

TEST_P(OpaqueTypeTest, Hash) {
  EXPECT_EQ(
      absl::HashOf(OpaqueType(memory_manager(), "test.Opaque", {BytesType()})),
      absl::HashOf(OpaqueType(memory_manager(), "test.Opaque", {BytesType()})));
  EXPECT_EQ(
      absl::HashOf(
          Type(OpaqueType(memory_manager(), "test.Opaque", {BytesType()}))),
      absl::HashOf(OpaqueType(memory_manager(), "test.Opaque", {BytesType()})));
}

TEST_P(OpaqueTypeTest, Equal) {
  EXPECT_EQ(OpaqueType(memory_manager(), "test.Opaque", {BytesType()}),
            OpaqueType(memory_manager(), "test.Opaque", {BytesType()}));
  EXPECT_EQ(Type(OpaqueType(memory_manager(), "test.Opaque", {BytesType()})),
            OpaqueType(memory_manager(), "test.Opaque", {BytesType()}));
  EXPECT_EQ(OpaqueType(memory_manager(), "test.Opaque", {BytesType()}),
            Type(OpaqueType(memory_manager(), "test.Opaque", {BytesType()})));
  EXPECT_EQ(Type(OpaqueType(memory_manager(), "test.Opaque", {BytesType()})),
            Type(OpaqueType(memory_manager(), "test.Opaque", {BytesType()})));
}

TEST_P(OpaqueTypeTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(
                OpaqueType(memory_manager(), "test.Opaque", {BytesType()})),
            NativeTypeId::For<OpaqueType>());
  EXPECT_EQ(NativeTypeId::Of(Type(
                OpaqueType(memory_manager(), "test.Opaque", {BytesType()}))),
            NativeTypeId::For<OpaqueType>());
}

TEST_P(OpaqueTypeTest, InstanceOf) {
  EXPECT_TRUE(InstanceOf<OpaqueType>(
      OpaqueType(memory_manager(), "test.Opaque", {BytesType()})));
  EXPECT_TRUE(InstanceOf<OpaqueType>(
      Type(OpaqueType(memory_manager(), "test.Opaque", {BytesType()}))));
}

TEST_P(OpaqueTypeTest, Cast) {
  EXPECT_THAT(Cast<OpaqueType>(
                  OpaqueType(memory_manager(), "test.Opaque", {BytesType()})),
              An<OpaqueType>());
  EXPECT_THAT(Cast<OpaqueType>(Type(
                  OpaqueType(memory_manager(), "test.Opaque", {BytesType()}))),
              An<OpaqueType>());
}

TEST_P(OpaqueTypeTest, As) {
  EXPECT_THAT(As<OpaqueType>(
                  OpaqueType(memory_manager(), "test.Opaque", {BytesType()})),
              Ne(absl::nullopt));
  EXPECT_THAT(As<OpaqueType>(Type(
                  OpaqueType(memory_manager(), "test.Opaque", {BytesType()}))),
              Ne(absl::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    OpaqueTypeTest, OpaqueTypeTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    OpaqueTypeTest::ToString);

class OpaqueTypeViewTest : public TestWithParam<MemoryManagement> {
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

TEST_P(OpaqueTypeViewTest, Kind) {
  auto type = OpaqueType(memory_manager(), "test.Opaque", {BytesType()});
  EXPECT_EQ(OpaqueTypeView(type).kind(), OpaqueTypeView::kKind);
  EXPECT_EQ(TypeView(OpaqueTypeView(type)).kind(), OpaqueTypeView::kKind);
}

TEST_P(OpaqueTypeViewTest, Name) {
  auto type = OpaqueType(memory_manager(), "test.Opaque", {BytesType()});
  EXPECT_EQ(OpaqueTypeView(type).name(), "test.Opaque");
  EXPECT_EQ(TypeView(OpaqueTypeView(type)).name(), "test.Opaque");
}

TEST_P(OpaqueTypeViewTest, DebugString) {
  auto type = OpaqueType(memory_manager(), "test.Opaque", {BytesType()});
  {
    std::ostringstream out;
    out << OpaqueTypeView(type);
    EXPECT_EQ(out.str(), "test.Opaque<bytes>");
  }
  {
    std::ostringstream out;
    out << TypeView(OpaqueTypeView(type));
    EXPECT_EQ(out.str(), "test.Opaque<bytes>");
  }
}

TEST_P(OpaqueTypeViewTest, Hash) {
  auto type = OpaqueType(memory_manager(), "test.Opaque", {BytesType()});
  EXPECT_EQ(absl::HashOf(OpaqueTypeView(type)),
            absl::HashOf(OpaqueTypeView(type)));
  EXPECT_EQ(absl::HashOf(TypeView(OpaqueTypeView(type))),
            absl::HashOf(OpaqueTypeView(type)));
  EXPECT_EQ(absl::HashOf(OpaqueTypeView(type)), absl::HashOf(OpaqueType(type)));
  EXPECT_EQ(absl::HashOf(TypeView(OpaqueTypeView(type))),
            absl::HashOf(OpaqueType(type)));
}

TEST_P(OpaqueTypeViewTest, Equal) {
  auto type = OpaqueType(memory_manager(), "test.Opaque", {BytesType()});
  EXPECT_EQ(OpaqueTypeView(type), OpaqueTypeView(type));
  EXPECT_EQ(TypeView(OpaqueTypeView(type)), OpaqueTypeView(type));
  EXPECT_EQ(OpaqueTypeView(type), TypeView(OpaqueTypeView(type)));
  EXPECT_EQ(TypeView(OpaqueTypeView(type)), TypeView(OpaqueTypeView(type)));
  EXPECT_EQ(OpaqueTypeView(type), OpaqueType(type));
  EXPECT_EQ(TypeView(OpaqueTypeView(type)), OpaqueType(type));
  EXPECT_EQ(TypeView(OpaqueTypeView(type)), Type(OpaqueType(type)));
  EXPECT_EQ(OpaqueType(type), OpaqueTypeView(type));
  EXPECT_EQ(OpaqueType(type), OpaqueTypeView(type));
  EXPECT_EQ(OpaqueType(type), TypeView(OpaqueTypeView(type)));
  EXPECT_EQ(Type(OpaqueType(type)), TypeView(OpaqueTypeView(type)));
  EXPECT_EQ(OpaqueTypeView(type), OpaqueType(type));
}

TEST_P(OpaqueTypeViewTest, NativeTypeId) {
  auto type = OpaqueType(memory_manager(), "test.Opaque", {BytesType()});
  EXPECT_EQ(NativeTypeId::Of(OpaqueTypeView(type)),
            NativeTypeId::For<OpaqueTypeView>());
  EXPECT_EQ(NativeTypeId::Of(TypeView(OpaqueTypeView(type))),
            NativeTypeId::For<OpaqueTypeView>());
}

TEST_P(OpaqueTypeViewTest, InstanceOf) {
  auto type = OpaqueType(memory_manager(), "test.Opaque", {BytesType()});
  EXPECT_TRUE(InstanceOf<OpaqueTypeView>(OpaqueTypeView(type)));
  EXPECT_TRUE(InstanceOf<OpaqueTypeView>(TypeView(OpaqueTypeView(type))));
}

TEST_P(OpaqueTypeViewTest, Cast) {
  auto type = OpaqueType(memory_manager(), "test.Opaque", {BytesType()});
  EXPECT_THAT(Cast<OpaqueTypeView>(OpaqueTypeView(type)), An<OpaqueTypeView>());
  EXPECT_THAT(Cast<OpaqueTypeView>(TypeView(OpaqueTypeView(type))),
              An<OpaqueTypeView>());
}

TEST_P(OpaqueTypeViewTest, As) {
  auto type = OpaqueType(memory_manager(), "test.Opaque", {BytesType()});
  EXPECT_THAT(As<OpaqueTypeView>(OpaqueTypeView(type)), Ne(absl::nullopt));
  EXPECT_THAT(As<OpaqueTypeView>(TypeView(OpaqueTypeView(type))),
              Ne(absl::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    OpaqueTypeViewTest, OpaqueTypeViewTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    OpaqueTypeViewTest::ToString);

}  // namespace
}  // namespace cel
