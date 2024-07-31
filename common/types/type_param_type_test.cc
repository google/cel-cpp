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
#include "common/memory_testing.h"
#include "common/native_type.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::An;
using testing::Ne;
using testing::TestParamInfo;
using testing::TestWithParam;

class TypeParamTypeTest : public common_internal::ThreadCompatibleMemoryTest<> {
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

}  // namespace
}  // namespace cel
