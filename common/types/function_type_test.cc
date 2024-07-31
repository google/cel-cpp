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

}  // namespace
}  // namespace cel
