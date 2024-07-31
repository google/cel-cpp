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

class OpaqueTypeTest : public common_internal::ThreadCompatibleMemoryTest<> {};

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

}  // namespace
}  // namespace cel
