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

class FunctionTypeTest : public common_internal::ThreadCompatibleMemoryTest<> {
};

TEST_P(FunctionTypeTest, Kind) {
  EXPECT_EQ(FunctionType(memory_manager(), DynType{}, {BytesType()}).kind(),
            FunctionType::kKind);
  EXPECT_EQ(
      Type(FunctionType(memory_manager(), DynType{}, {BytesType()})).kind(),
      FunctionType::kKind);
}

TEST_P(FunctionTypeTest, Name) {
  EXPECT_EQ(FunctionType(memory_manager(), DynType{}, {BytesType()}).name(),
            "function");
  EXPECT_EQ(
      Type(FunctionType(memory_manager(), DynType{}, {BytesType()})).name(),
      "function");
}

TEST_P(FunctionTypeTest, DebugString) {
  {
    std::ostringstream out;
    out << FunctionType(memory_manager(), DynType{}, {BytesType()});
    EXPECT_EQ(out.str(), "(bytes) -> dyn");
  }
  {
    std::ostringstream out;
    out << Type(FunctionType(memory_manager(), DynType{}, {BytesType()}));
    EXPECT_EQ(out.str(), "(bytes) -> dyn");
  }
}

TEST_P(FunctionTypeTest, Hash) {
  EXPECT_EQ(
      absl::HashOf(FunctionType(memory_manager(), DynType{}, {BytesType()})),
      absl::HashOf(FunctionType(memory_manager(), DynType{}, {BytesType()})));
}

TEST_P(FunctionTypeTest, Equal) {
  EXPECT_EQ(FunctionType(memory_manager(), DynType{}, {BytesType()}),
            FunctionType(memory_manager(), DynType{}, {BytesType()}));
  EXPECT_EQ(Type(FunctionType(memory_manager(), DynType{}, {BytesType()})),
            FunctionType(memory_manager(), DynType{}, {BytesType()}));
  EXPECT_EQ(FunctionType(memory_manager(), DynType{}, {BytesType()}),
            Type(FunctionType(memory_manager(), DynType{}, {BytesType()})));
  EXPECT_EQ(Type(FunctionType(memory_manager(), DynType{}, {BytesType()})),
            Type(FunctionType(memory_manager(), DynType{}, {BytesType()})));
}

TEST_P(FunctionTypeTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(
                FunctionType(memory_manager(), DynType{}, {BytesType()})),
            NativeTypeId::For<FunctionType>());
  EXPECT_EQ(NativeTypeId::Of(
                Type(FunctionType(memory_manager(), DynType{}, {BytesType()}))),
            NativeTypeId::For<FunctionType>());
}

TEST_P(FunctionTypeTest, InstanceOf) {
  EXPECT_TRUE(InstanceOf<FunctionType>(
      FunctionType(memory_manager(), DynType{}, {BytesType()})));
  EXPECT_TRUE(InstanceOf<FunctionType>(
      Type(FunctionType(memory_manager(), DynType{}, {BytesType()}))));
}

TEST_P(FunctionTypeTest, Cast) {
  EXPECT_THAT(Cast<FunctionType>(
                  FunctionType(memory_manager(), DynType{}, {BytesType()})),
              An<FunctionType>());
  EXPECT_THAT(Cast<FunctionType>(Type(
                  FunctionType(memory_manager(), DynType{}, {BytesType()}))),
              An<FunctionType>());
}

TEST_P(FunctionTypeTest, As) {
  EXPECT_THAT(As<FunctionType>(
                  FunctionType(memory_manager(), DynType{}, {BytesType()})),
              Ne(absl::nullopt));
  EXPECT_THAT(As<FunctionType>(Type(
                  FunctionType(memory_manager(), DynType{}, {BytesType()}))),
              Ne(absl::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    FunctionTypeTest, FunctionTypeTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    FunctionTypeTest::ToString);

class FunctionTypeViewTest
    : public common_internal::ThreadCompatibleMemoryTest<> {};

TEST_P(FunctionTypeViewTest, Kind) {
  auto type = FunctionType(memory_manager(), DynType{}, {BytesType()});
  EXPECT_EQ(FunctionTypeView(type).kind(), FunctionTypeView::kKind);
  EXPECT_EQ(TypeView(FunctionTypeView(type)).kind(), FunctionTypeView::kKind);
}

TEST_P(FunctionTypeViewTest, Name) {
  auto type = FunctionType(memory_manager(), DynType{}, {BytesType()});
  EXPECT_EQ(FunctionTypeView(type).name(), "function");
  EXPECT_EQ(TypeView(FunctionTypeView(type)).name(), "function");
}

TEST_P(FunctionTypeViewTest, DebugString) {
  auto type = FunctionType(memory_manager(), DynType{}, {BytesType()});
  {
    std::ostringstream out;
    out << FunctionTypeView(type);
    EXPECT_EQ(out.str(), "(bytes) -> dyn");
  }
  {
    std::ostringstream out;
    out << TypeView(FunctionTypeView(type));
    EXPECT_EQ(out.str(), "(bytes) -> dyn");
  }
}

TEST_P(FunctionTypeViewTest, Hash) {
  auto type = FunctionType(memory_manager(), DynType{}, {BytesType()});
  EXPECT_EQ(absl::HashOf(FunctionTypeView(type)),
            absl::HashOf(FunctionTypeView(type)));
  EXPECT_EQ(absl::HashOf(FunctionTypeView(type)),
            absl::HashOf(FunctionType(type)));
}

TEST_P(FunctionTypeViewTest, Equal) {
  auto type = FunctionType(memory_manager(), DynType{}, {BytesType()});
  EXPECT_EQ(FunctionTypeView(type), FunctionTypeView(type));
  EXPECT_EQ(TypeView(FunctionTypeView(type)), FunctionTypeView(type));
  EXPECT_EQ(FunctionTypeView(type), TypeView(FunctionTypeView(type)));
  EXPECT_EQ(TypeView(FunctionTypeView(type)), TypeView(FunctionTypeView(type)));
  EXPECT_EQ(FunctionTypeView(type), FunctionType(type));
  EXPECT_EQ(TypeView(FunctionTypeView(type)), FunctionType(type));
  EXPECT_EQ(TypeView(FunctionTypeView(type)), Type(FunctionType(type)));
  EXPECT_EQ(FunctionType(type), FunctionTypeView(type));
  EXPECT_EQ(FunctionType(type), FunctionTypeView(type));
  EXPECT_EQ(FunctionType(type), TypeView(FunctionTypeView(type)));
  EXPECT_EQ(Type(FunctionType(type)), TypeView(FunctionTypeView(type)));
  EXPECT_EQ(FunctionTypeView(type), FunctionType(type));
}

TEST_P(FunctionTypeViewTest, NativeTypeId) {
  auto type = FunctionType(memory_manager(), DynType{}, {BytesType()});
  EXPECT_EQ(NativeTypeId::Of(FunctionTypeView(type)),
            NativeTypeId::For<FunctionTypeView>());
  EXPECT_EQ(NativeTypeId::Of(TypeView(FunctionTypeView(type))),
            NativeTypeId::For<FunctionTypeView>());
}

TEST_P(FunctionTypeViewTest, InstanceOf) {
  auto type = FunctionType(memory_manager(), DynType{}, {BytesType()});
  EXPECT_TRUE(InstanceOf<FunctionTypeView>(FunctionTypeView(type)));
  EXPECT_TRUE(InstanceOf<FunctionTypeView>(TypeView(FunctionTypeView(type))));
}

TEST_P(FunctionTypeViewTest, Cast) {
  auto type = FunctionType(memory_manager(), DynType{}, {BytesType()});
  EXPECT_THAT(Cast<FunctionTypeView>(FunctionTypeView(type)),
              An<FunctionTypeView>());
  EXPECT_THAT(Cast<FunctionTypeView>(TypeView(FunctionTypeView(type))),
              An<FunctionTypeView>());
}

TEST_P(FunctionTypeViewTest, As) {
  auto type = FunctionType(memory_manager(), DynType{}, {BytesType()});
  EXPECT_THAT(As<FunctionTypeView>(FunctionTypeView(type)), Ne(absl::nullopt));
  EXPECT_THAT(As<FunctionTypeView>(TypeView(FunctionTypeView(type))),
              Ne(absl::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    FunctionTypeViewTest, FunctionTypeViewTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    FunctionTypeViewTest::ToString);

}  // namespace
}  // namespace cel
