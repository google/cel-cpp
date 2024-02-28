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

#include "absl/hash/hash.h"
#include "absl/strings/cord.h"
#include "absl/types/optional.h"
#include "common/any.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::An;
using testing::Ne;
using cel::internal::IsOkAndHolds;

using BoolValueTest = common_internal::ThreadCompatibleValueTest<>;

TEST_P(BoolValueTest, Kind) {
  EXPECT_EQ(BoolValue(true).kind(), BoolValue::kKind);
  EXPECT_EQ(Value(BoolValue(true)).kind(), BoolValue::kKind);
}

TEST_P(BoolValueTest, DebugString) {
  {
    std::ostringstream out;
    out << BoolValue(true);
    EXPECT_EQ(out.str(), "true");
  }
  {
    std::ostringstream out;
    out << Value(BoolValue(true));
    EXPECT_EQ(out.str(), "true");
  }
}

TEST_P(BoolValueTest, GetSerializedSize) {
  EXPECT_THAT(BoolValue(false).GetSerializedSize(value_manager()),
              IsOkAndHolds(0));
  EXPECT_THAT(BoolValue(true).GetSerializedSize(value_manager()),
              IsOkAndHolds(2));
}

TEST_P(BoolValueTest, ConvertToAny) {
  EXPECT_THAT(BoolValue(false).ConvertToAny(value_manager()),
              IsOkAndHolds(MakeAny(MakeTypeUrl("google.protobuf.BoolValue"),
                                   absl::Cord())));
}

TEST_P(BoolValueTest, ConvertToJson) {
  EXPECT_THAT(BoolValue(false).ConvertToJson(value_manager()),
              IsOkAndHolds(Json(false)));
}

TEST_P(BoolValueTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(BoolValue(true)), NativeTypeId::For<BoolValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(BoolValue(true))),
            NativeTypeId::For<BoolValue>());
}

TEST_P(BoolValueTest, InstanceOf) {
  EXPECT_TRUE(InstanceOf<BoolValue>(BoolValue(true)));
  EXPECT_TRUE(InstanceOf<BoolValue>(Value(BoolValue(true))));
}

TEST_P(BoolValueTest, Cast) {
  EXPECT_THAT(Cast<BoolValue>(BoolValue(true)), An<BoolValue>());
  EXPECT_THAT(Cast<BoolValue>(Value(BoolValue(true))), An<BoolValue>());
}

TEST_P(BoolValueTest, As) {
  EXPECT_THAT(As<BoolValue>(BoolValue(true)), Ne(absl::nullopt));
  EXPECT_THAT(As<BoolValue>(Value(BoolValue(true))), Ne(absl::nullopt));
}

TEST_P(BoolValueTest, HashValue) {
  EXPECT_EQ(absl::HashOf(BoolValue(true)), absl::HashOf(true));
}

TEST_P(BoolValueTest, Equality) {
  EXPECT_NE(BoolValue(false), true);
  EXPECT_NE(true, BoolValue(false));
  EXPECT_NE(BoolValue(false), BoolValue(true));
}

TEST_P(BoolValueTest, LessThan) {
  EXPECT_LT(BoolValue(false), true);
  EXPECT_LT(false, BoolValue(true));
  EXPECT_LT(BoolValue(false), BoolValue(true));
}

INSTANTIATE_TEST_SUITE_P(
    BoolValueTest, BoolValueTest,
    ::testing::Combine(::testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting)),
    BoolValueTest::ToString);

using BoolValueViewTest = common_internal::ThreadCompatibleValueTest<>;

TEST_P(BoolValueViewTest, Kind) {
  EXPECT_EQ(BoolValueView(true).kind(), BoolValueView::kKind);
  EXPECT_EQ(ValueView(BoolValueView(true)).kind(), BoolValueView::kKind);
}

TEST_P(BoolValueViewTest, DebugString) {
  {
    std::ostringstream out;
    out << BoolValueView(true);
    EXPECT_EQ(out.str(), "true");
  }
  {
    std::ostringstream out;
    out << ValueView(BoolValueView(true));
    EXPECT_EQ(out.str(), "true");
  }
}

TEST_P(BoolValueViewTest, GetSerializedSize) {
  EXPECT_THAT(BoolValueView(false).GetSerializedSize(value_manager()),
              IsOkAndHolds(0));
  EXPECT_THAT(BoolValueView(true).GetSerializedSize(value_manager()),
              IsOkAndHolds(2));
}

TEST_P(BoolValueViewTest, ConvertToAny) {
  EXPECT_THAT(BoolValueView(false).ConvertToAny(value_manager()),
              IsOkAndHolds(MakeAny(MakeTypeUrl("google.protobuf.BoolValue"),
                                   absl::Cord())));
}

TEST_P(BoolValueViewTest, ConvertToJson) {
  EXPECT_THAT(BoolValueView(false).ConvertToJson(value_manager()),
              IsOkAndHolds(Json(false)));
}

TEST_P(BoolValueViewTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(BoolValueView(true)),
            NativeTypeId::For<BoolValueView>());
  EXPECT_EQ(NativeTypeId::Of(ValueView(BoolValueView(true))),
            NativeTypeId::For<BoolValueView>());
}

TEST_P(BoolValueViewTest, InstanceOf) {
  EXPECT_TRUE(InstanceOf<BoolValueView>(BoolValueView(true)));
  EXPECT_TRUE(InstanceOf<BoolValueView>(ValueView(BoolValueView(true))));
}

TEST_P(BoolValueViewTest, Cast) {
  EXPECT_THAT(Cast<BoolValueView>(BoolValueView(true)), An<BoolValueView>());
  EXPECT_THAT(Cast<BoolValueView>(ValueView(BoolValueView(true))),
              An<BoolValueView>());
}

TEST_P(BoolValueViewTest, As) {
  EXPECT_THAT(As<BoolValueView>(BoolValueView(true)), Ne(absl::nullopt));
  EXPECT_THAT(As<BoolValueView>(ValueView(BoolValueView(true))),
              Ne(absl::nullopt));
}

TEST_P(BoolValueViewTest, HashValue) {
  EXPECT_EQ(absl::HashOf(BoolValueView(true)), absl::HashOf(true));
}

TEST_P(BoolValueViewTest, Equality) {
  EXPECT_NE(BoolValueView(BoolValue(false)), true);
  EXPECT_NE(true, BoolValueView(false));
  EXPECT_NE(BoolValueView(false), BoolValueView(true));
  EXPECT_NE(BoolValueView(false), BoolValue(true));
  EXPECT_NE(BoolValue(true), BoolValueView(false));
}

TEST_P(BoolValueViewTest, LessThan) {
  EXPECT_LT(BoolValueView(false), true);
  EXPECT_LT(false, BoolValueView(true));
  EXPECT_LT(BoolValueView(false), BoolValueView(true));
  EXPECT_LT(BoolValueView(false), BoolValue(true));
  EXPECT_LT(BoolValue(false), BoolValueView(true));
}

INSTANTIATE_TEST_SUITE_P(
    BoolValueViewTest, BoolValueViewTest,
    ::testing::Combine(::testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting)),
    BoolValueViewTest::ToString);

}  // namespace
}  // namespace cel
