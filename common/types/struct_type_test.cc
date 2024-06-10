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

class StructTypeTest : public common_internal::ThreadCompatibleMemoryTest<> {};

TEST_P(StructTypeTest, Kind) {
  EXPECT_EQ(StructType(memory_manager(), "test.Struct").kind(),
            StructType::kKind);
  EXPECT_EQ(Type(StructType(memory_manager(), "test.Struct")).kind(),
            StructType::kKind);
}

TEST_P(StructTypeTest, Name) {
  EXPECT_EQ(StructType(memory_manager(), "test.Struct").name(), "test.Struct");
  EXPECT_EQ(Type(StructType(memory_manager(), "test.Struct")).name(),
            "test.Struct");
}

TEST_P(StructTypeTest, DebugString) {
  {
    std::ostringstream out;
    out << StructType(memory_manager(), "test.Struct");
    EXPECT_EQ(out.str(), "test.Struct");
  }
  {
    std::ostringstream out;
    out << Type(StructType(memory_manager(), "test.Struct"));
    EXPECT_EQ(out.str(), "test.Struct");
  }
}

TEST_P(StructTypeTest, Hash) {
  EXPECT_EQ(absl::HashOf(StructType(memory_manager(), "test.Struct")),
            absl::HashOf(StructType(memory_manager(), "test.Struct")));
}

TEST_P(StructTypeTest, Equal) {
  EXPECT_EQ(StructType(memory_manager(), "test.Struct"),
            StructType(memory_manager(), "test.Struct"));
  EXPECT_EQ(Type(StructType(memory_manager(), "test.Struct")),
            StructType(memory_manager(), "test.Struct"));
  EXPECT_EQ(StructType(memory_manager(), "test.Struct"),
            Type(StructType(memory_manager(), "test.Struct")));
  EXPECT_EQ(Type(StructType(memory_manager(), "test.Struct")),
            Type(StructType(memory_manager(), "test.Struct")));
}

TEST_P(StructTypeTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(StructType(memory_manager(), "test.Struct")),
            NativeTypeId::For<StructType>());
  EXPECT_EQ(NativeTypeId::Of(Type(StructType(memory_manager(), "test.Struct"))),
            NativeTypeId::For<StructType>());
}

TEST_P(StructTypeTest, InstanceOf) {
  EXPECT_TRUE(
      InstanceOf<StructType>(StructType(memory_manager(), "test.Struct")));
  EXPECT_TRUE(InstanceOf<StructType>(
      Type(StructType(memory_manager(), "test.Struct"))));
}

TEST_P(StructTypeTest, Cast) {
  EXPECT_THAT(Cast<StructType>(StructType(memory_manager(), "test.Struct")),
              An<StructType>());
  EXPECT_THAT(
      Cast<StructType>(Type(StructType(memory_manager(), "test.Struct"))),
      An<StructType>());
}

TEST_P(StructTypeTest, As) {
  EXPECT_THAT(As<StructType>(StructType(memory_manager(), "test.Struct")),
              Ne(absl::nullopt));
  EXPECT_THAT(As<StructType>(Type(StructType(memory_manager(), "test.Struct"))),
              Ne(absl::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    StructTypeTest, StructTypeTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    StructTypeTest::ToString);

class StructTypeViewTest
    : public common_internal::ThreadCompatibleMemoryTest<> {};

TEST_P(StructTypeViewTest, Kind) {
  auto type = StructType(memory_manager(), "test.Struct");
  EXPECT_EQ(StructTypeView(type).kind(), StructTypeView::kKind);
  EXPECT_EQ(TypeView(StructTypeView(type)).kind(), StructTypeView::kKind);
}

TEST_P(StructTypeViewTest, Name) {
  auto type = StructType(memory_manager(), "test.Struct");
  EXPECT_EQ(StructTypeView(type).name(), "test.Struct");
  EXPECT_EQ(TypeView(StructTypeView(type)).name(), "test.Struct");
}

TEST_P(StructTypeViewTest, DebugString) {
  auto type = StructType(memory_manager(), "test.Struct");
  {
    std::ostringstream out;
    out << StructTypeView(type);
    EXPECT_EQ(out.str(), "test.Struct");
  }
  {
    std::ostringstream out;
    out << TypeView(StructTypeView(type));
    EXPECT_EQ(out.str(), "test.Struct");
  }
}

TEST_P(StructTypeViewTest, Hash) {
  auto type = StructType(memory_manager(), "test.Struct");
  EXPECT_EQ(absl::HashOf(StructTypeView(type)),
            absl::HashOf(StructTypeView(type)));
  EXPECT_EQ(absl::HashOf(StructTypeView(type)), absl::HashOf(StructType(type)));
}

TEST_P(StructTypeViewTest, Equal) {
  auto type = StructType(memory_manager(), "test.Struct");
  EXPECT_EQ(StructTypeView(type), StructTypeView(type));
  EXPECT_EQ(TypeView(StructTypeView(type)), StructTypeView(type));
  EXPECT_EQ(StructTypeView(type), TypeView(StructTypeView(type)));
  EXPECT_EQ(TypeView(StructTypeView(type)), TypeView(StructTypeView(type)));
  EXPECT_EQ(StructTypeView(type), StructType(type));
  EXPECT_EQ(TypeView(StructTypeView(type)), StructType(type));
  EXPECT_EQ(TypeView(StructTypeView(type)), Type(StructType(type)));
  EXPECT_EQ(StructType(type), StructTypeView(type));
  EXPECT_EQ(StructType(type), StructTypeView(type));
  EXPECT_EQ(StructType(type), TypeView(StructTypeView(type)));
  EXPECT_EQ(Type(StructType(type)), TypeView(StructTypeView(type)));
  EXPECT_EQ(StructTypeView(type), StructType(type));
}

TEST_P(StructTypeViewTest, NativeTypeId) {
  auto type = StructType(memory_manager(), "test.Struct");
  EXPECT_EQ(NativeTypeId::Of(StructTypeView(type)),
            NativeTypeId::For<StructTypeView>());
  EXPECT_EQ(NativeTypeId::Of(TypeView(StructTypeView(type))),
            NativeTypeId::For<StructTypeView>());
}

TEST_P(StructTypeViewTest, InstanceOf) {
  auto type = StructType(memory_manager(), "test.Struct");
  EXPECT_TRUE(InstanceOf<StructTypeView>(StructTypeView(type)));
  EXPECT_TRUE(InstanceOf<StructTypeView>(TypeView(StructTypeView(type))));
}

TEST_P(StructTypeViewTest, Cast) {
  auto type = StructType(memory_manager(), "test.Struct");
  EXPECT_THAT(Cast<StructTypeView>(StructTypeView(type)), An<StructTypeView>());
  EXPECT_THAT(Cast<StructTypeView>(TypeView(StructTypeView(type))),
              An<StructTypeView>());
}

TEST_P(StructTypeViewTest, As) {
  auto type = StructType(memory_manager(), "test.Struct");
  EXPECT_THAT(As<StructTypeView>(StructTypeView(type)), Ne(absl::nullopt));
  EXPECT_THAT(As<StructTypeView>(TypeView(StructTypeView(type))),
              Ne(absl::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    StructTypeViewTest, StructTypeViewTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    StructTypeViewTest::ToString);

}  // namespace
}  // namespace cel
