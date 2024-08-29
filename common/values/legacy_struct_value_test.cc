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

#include "common/values/legacy_struct_value.h"

#include "common/memory.h"
#include "common/value_kind.h"
#include "common/value_testing.h"
#include "internal/testing.h"

namespace cel::common_internal {
namespace {

using ::testing::_;

class LegacyStructValueTest : public ThreadCompatibleValueTest<> {};

TEST_P(LegacyStructValueTest, Kind) {
  EXPECT_EQ(LegacyStructValue(0, 0).kind(), ValueKind::kStruct);
}

TEST_P(LegacyStructValueTest, GetTypeName) {
  EXPECT_DEATH(static_cast<void>(LegacyStructValue(0, 0).GetTypeName()), _);
}

TEST_P(LegacyStructValueTest, DebugString) {
  EXPECT_DEATH(static_cast<void>(LegacyStructValue(0, 0).DebugString()), _);
}

TEST_P(LegacyStructValueTest, SerializeTo) {
  absl::Cord serialize_value;
  EXPECT_DEATH(static_cast<void>(LegacyStructValue(0, 0).SerializeTo(
                   value_manager(), serialize_value)),
               _);
}

TEST_P(LegacyStructValueTest, ConvertToJson) {
  EXPECT_DEATH(
      static_cast<void>(LegacyStructValue(0, 0).ConvertToJson(value_manager())),
      _);
}

TEST_P(LegacyStructValueTest, GetFieldByName) {
  Value scratch;
  EXPECT_DEATH(static_cast<void>(LegacyStructValue(0, 0).GetFieldByName(
                   value_manager(), "", scratch)),
               _);
}

TEST_P(LegacyStructValueTest, GetFieldByNumber) {
  Value scratch;
  EXPECT_DEATH(static_cast<void>(LegacyStructValue(0, 0).GetFieldByNumber(
                   value_manager(), 0, scratch)),
               _);
}

TEST_P(LegacyStructValueTest, HasFieldByName) {
  EXPECT_DEATH(static_cast<void>(LegacyStructValue(0, 0).HasFieldByName("")),
               _);
}

TEST_P(LegacyStructValueTest, HasFieldByNumber) {
  EXPECT_DEATH(static_cast<void>(LegacyStructValue(0, 0).HasFieldByNumber(0)),
               _);
}

INSTANTIATE_TEST_SUITE_P(
    LegacyStructValueTest, LegacyStructValueTest,
    ::testing::Combine(::testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting)),
    LegacyStructValueTest::ToString);

}  // namespace
}  // namespace cel::common_internal
