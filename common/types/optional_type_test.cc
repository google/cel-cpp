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

TEST(OptionalType, Default) {
  OptionalType optional_type;
  EXPECT_EQ(optional_type.parameter(), DynType());
}

class OptionalTypeTest : public common_internal::ThreadCompatibleMemoryTest<> {
};

TEST_P(OptionalTypeTest, Kind) {
  EXPECT_EQ(OptionalType(memory_manager(), BoolType()).kind(),
            OptionalType::kKind);
  EXPECT_EQ(Type(OptionalType(memory_manager(), BoolType())).kind(),
            OptionalType::kKind);
}

TEST_P(OptionalTypeTest, Name) {
  EXPECT_EQ(OptionalType(memory_manager(), BoolType()).name(),
            OptionalType::kName);
  EXPECT_EQ(Type(OptionalType(memory_manager(), BoolType())).name(),
            OptionalType::kName);
}

TEST_P(OptionalTypeTest, DebugString) {
  {
    std::ostringstream out;
    out << OptionalType(memory_manager(), BoolType());
    EXPECT_EQ(out.str(), "optional_type<bool>");
  }
  {
    std::ostringstream out;
    out << Type(OptionalType(memory_manager(), BoolType()));
    EXPECT_EQ(out.str(), "optional_type<bool>");
  }
}

TEST_P(OptionalTypeTest, Parameter) {
  EXPECT_EQ(OptionalType(memory_manager(), BoolType()).parameter(), BoolType());
}

TEST_P(OptionalTypeTest, Hash) {
  EXPECT_EQ(absl::HashOf(OptionalType(memory_manager(), BoolType())),
            absl::HashOf(OptionalType(memory_manager(), BoolType())));
}

TEST_P(OptionalTypeTest, Equal) {
  EXPECT_EQ(OptionalType(memory_manager(), BoolType()),
            OptionalType(memory_manager(), BoolType()));
  EXPECT_EQ(Type(OptionalType(memory_manager(), BoolType())),
            OptionalType(memory_manager(), BoolType()));
  EXPECT_EQ(OptionalType(memory_manager(), BoolType()),
            Type(OptionalType(memory_manager(), BoolType())));
  EXPECT_EQ(Type(OptionalType(memory_manager(), BoolType())),
            Type(OptionalType(memory_manager(), BoolType())));
}

TEST_P(OptionalTypeTest, InstanceOf) {
  EXPECT_TRUE(
      InstanceOf<OptionalType>(OptionalType(memory_manager(), BoolType())));
  EXPECT_TRUE(InstanceOf<OptionalType>(
      Type(OptionalType(memory_manager(), BoolType()))));
  EXPECT_TRUE(
      InstanceOf<OpaqueType>(OptionalType(memory_manager(), BoolType())));
  EXPECT_TRUE(
      InstanceOf<OpaqueType>(Type(OptionalType(memory_manager(), BoolType()))));
}

TEST_P(OptionalTypeTest, Cast) {
  EXPECT_THAT(Cast<OptionalType>(OptionalType(memory_manager(), BoolType())),
              An<OptionalType>());
  EXPECT_THAT(
      Cast<OptionalType>(Type(OptionalType(memory_manager(), BoolType()))),
      An<OptionalType>());
  EXPECT_THAT(Cast<OpaqueType>(OptionalType(memory_manager(), BoolType())),
              An<OpaqueType>());
  EXPECT_THAT(
      Cast<OpaqueType>(Type(OptionalType(memory_manager(), BoolType()))),
      An<OpaqueType>());
}

TEST_P(OptionalTypeTest, As) {
  EXPECT_THAT(As<OptionalType>(OptionalType(memory_manager(), BoolType())),
              Ne(absl::nullopt));
  EXPECT_THAT(
      As<OptionalType>(Type(OptionalType(memory_manager(), BoolType()))),
      Ne(absl::nullopt));
  EXPECT_THAT(As<OpaqueType>(OptionalType(memory_manager(), BoolType())),
              Ne(absl::nullopt));
  EXPECT_THAT(As<OpaqueType>(Type(OptionalType(memory_manager(), BoolType()))),
              Ne(absl::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    OptionalTypeTest, OptionalTypeTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    OptionalTypeTest::ToString);

}  // namespace
}  // namespace cel
