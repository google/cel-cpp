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

#include "absl/strings/cord.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "common/any.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::An;
using testing::Ne;
using cel::internal::IsOkAndHolds;

using DurationValueTest = common_internal::ThreadCompatibleValueTest<>;

TEST_P(DurationValueTest, Kind) {
  EXPECT_EQ(DurationValue().kind(), DurationValue::kKind);
  EXPECT_EQ(Value(DurationValue(absl::Seconds(1))).kind(),
            DurationValue::kKind);
}

TEST_P(DurationValueTest, DebugString) {
  {
    std::ostringstream out;
    out << DurationValue(absl::Seconds(1));
    EXPECT_EQ(out.str(), "1s");
  }
  {
    std::ostringstream out;
    out << Value(DurationValue(absl::Seconds(1)));
    EXPECT_EQ(out.str(), "1s");
  }
}

TEST_P(DurationValueTest, GetSerializedSize) {
  EXPECT_THAT(DurationValue().GetSerializedSize(value_manager()),
              IsOkAndHolds(0));
}

TEST_P(DurationValueTest, ConvertToAny) {
  EXPECT_THAT(DurationValue().ConvertToAny(value_manager()),
              IsOkAndHolds(MakeAny(MakeTypeUrl("google.protobuf.Duration"),
                                   absl::Cord())));
}

TEST_P(DurationValueTest, ConvertToJson) {
  EXPECT_THAT(DurationValue().ConvertToJson(value_manager()),
              IsOkAndHolds(Json(JsonString("0s"))));
}

TEST_P(DurationValueTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(DurationValue(absl::Seconds(1))),
            NativeTypeId::For<DurationValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(DurationValue(absl::Seconds(1)))),
            NativeTypeId::For<DurationValue>());
}

TEST_P(DurationValueTest, InstanceOf) {
  EXPECT_TRUE(InstanceOf<DurationValue>(DurationValue(absl::Seconds(1))));
  EXPECT_TRUE(
      InstanceOf<DurationValue>(Value(DurationValue(absl::Seconds(1)))));
}

TEST_P(DurationValueTest, Cast) {
  EXPECT_THAT(Cast<DurationValue>(DurationValue(absl::Seconds(1))),
              An<DurationValue>());
  EXPECT_THAT(Cast<DurationValue>(Value(DurationValue(absl::Seconds(1)))),
              An<DurationValue>());
}

TEST_P(DurationValueTest, As) {
  EXPECT_THAT(As<DurationValue>(DurationValue(absl::Seconds(1))),
              Ne(absl::nullopt));
  EXPECT_THAT(As<DurationValue>(Value(DurationValue(absl::Seconds(1)))),
              Ne(absl::nullopt));
}

TEST_P(DurationValueTest, Equality) {
  EXPECT_NE(DurationValue(absl::ZeroDuration()), absl::Seconds(1));
  EXPECT_NE(absl::Seconds(1), DurationValue(absl::ZeroDuration()));
  EXPECT_NE(DurationValue(absl::ZeroDuration()),
            DurationValue(absl::Seconds(1)));
}

INSTANTIATE_TEST_SUITE_P(
    DurationValueTest, DurationValueTest,
    ::testing::Combine(::testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting)),
    DurationValueTest::ToString);

using DurationValueViewTest = common_internal::ThreadCompatibleValueTest<>;

TEST_P(DurationValueViewTest, Kind) {
  EXPECT_EQ(DurationValueView(absl::Seconds(1)).kind(),
            DurationValueView::kKind);
  EXPECT_EQ(ValueView(DurationValueView(absl::Seconds(1))).kind(),
            DurationValueView::kKind);
}

TEST_P(DurationValueViewTest, DebugString) {
  {
    std::ostringstream out;
    out << DurationValueView(absl::Seconds(1));
    EXPECT_EQ(out.str(), "1s");
  }
  {
    std::ostringstream out;
    out << ValueView(DurationValueView(absl::Seconds(1)));
    EXPECT_EQ(out.str(), "1s");
  }
}

TEST_P(DurationValueViewTest, GetSerializedSize) {
  EXPECT_THAT(DurationValueView().GetSerializedSize(value_manager()),
              IsOkAndHolds(0));
}

TEST_P(DurationValueViewTest, ConvertToAny) {
  EXPECT_THAT(DurationValueView().ConvertToAny(value_manager()),
              IsOkAndHolds(MakeAny(MakeTypeUrl("google.protobuf.Duration"),
                                   absl::Cord())));
}

TEST_P(DurationValueViewTest, ConvertToJson) {
  EXPECT_THAT(DurationValueView().ConvertToJson(value_manager()),
              IsOkAndHolds(Json(JsonString("0s"))));
}

TEST_P(DurationValueViewTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(DurationValueView(absl::Seconds(1))),
            NativeTypeId::For<DurationValueView>());
  EXPECT_EQ(NativeTypeId::Of(ValueView(DurationValueView(absl::Seconds(1)))),
            NativeTypeId::For<DurationValueView>());
}

TEST_P(DurationValueViewTest, InstanceOf) {
  EXPECT_TRUE(
      InstanceOf<DurationValueView>(DurationValueView(absl::Seconds(1))));
  EXPECT_TRUE(InstanceOf<DurationValueView>(
      ValueView(DurationValueView(absl::Seconds(1)))));
}

TEST_P(DurationValueViewTest, Cast) {
  EXPECT_THAT(Cast<DurationValueView>(DurationValueView(absl::Seconds(1))),
              An<DurationValueView>());
  EXPECT_THAT(
      Cast<DurationValueView>(ValueView(DurationValueView(absl::Seconds(1)))),
      An<DurationValueView>());
}

TEST_P(DurationValueViewTest, As) {
  EXPECT_THAT(As<DurationValueView>(DurationValueView(absl::Seconds(1))),
              Ne(absl::nullopt));
  EXPECT_THAT(
      As<DurationValueView>(ValueView(DurationValueView(absl::Seconds(1)))),
      Ne(absl::nullopt));
}

TEST_P(DurationValueViewTest, Equality) {
  EXPECT_NE(DurationValueView(DurationValue(absl::ZeroDuration())),
            absl::Seconds(1));
  EXPECT_NE(absl::Seconds(1), DurationValueView(absl::ZeroDuration()));
  EXPECT_NE(DurationValueView(absl::ZeroDuration()),
            DurationValueView(absl::Seconds(1)));
  EXPECT_NE(DurationValueView(absl::ZeroDuration()),
            DurationValue(absl::Seconds(1)));
  EXPECT_NE(DurationValue(absl::Seconds(1)),
            DurationValueView(absl::ZeroDuration()));
}

INSTANTIATE_TEST_SUITE_P(
    DurationValueViewTest, DurationValueViewTest,
    ::testing::Combine(::testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting)),
    DurationValueViewTest::ToString);

}  // namespace
}  // namespace cel
