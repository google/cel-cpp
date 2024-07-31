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

TEST(ListType, Default) {
  ListType list_type;
  EXPECT_EQ(list_type.element(), DynType());
}

class ListTypeTest : public common_internal::ThreadCompatibleMemoryTest<> {};

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

}  // namespace
}  // namespace cel
