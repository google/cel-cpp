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
#include "common/type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::An;
using testing::Ne;
using cel::internal::IsOkAndHolds;

using TimestampValueTest = common_internal::ThreadCompatibleValueTest<>;

TEST_P(TimestampValueTest, Kind) {
  EXPECT_EQ(TimestampValue().kind(), TimestampValue::kKind);
  EXPECT_EQ(Value(TimestampValue(absl::UnixEpoch() + absl::Seconds(1))).kind(),
            TimestampValue::kKind);
}

TEST_P(TimestampValueTest, DebugString) {
  {
    std::ostringstream out;
    out << TimestampValue(absl::UnixEpoch() + absl::Seconds(1));
    EXPECT_EQ(out.str(), "1970-01-01T00:00:01Z");
  }
  {
    std::ostringstream out;
    out << Value(TimestampValue(absl::UnixEpoch() + absl::Seconds(1)));
    EXPECT_EQ(out.str(), "1970-01-01T00:00:01Z");
  }
}

TEST_P(TimestampValueTest, GetSerializedSize) {
  EXPECT_THAT(TimestampValue().GetSerializedSize(value_manager()),
              IsOkAndHolds(0));
}

TEST_P(TimestampValueTest, ConvertToAny) {
  EXPECT_THAT(TimestampValue().ConvertToAny(value_manager()),
              IsOkAndHolds(MakeAny(MakeTypeUrl("google.protobuf.Timestamp"),
                                   absl::Cord())));
}

TEST_P(TimestampValueTest, ConvertToJson) {
  EXPECT_THAT(TimestampValue().ConvertToJson(value_manager()),
              IsOkAndHolds(Json(JsonString("1970-01-01T00:00:00Z"))));
}

TEST_P(TimestampValueTest, NativeTypeId) {
  EXPECT_EQ(
      NativeTypeId::Of(TimestampValue(absl::UnixEpoch() + absl::Seconds(1))),
      NativeTypeId::For<TimestampValue>());
  EXPECT_EQ(NativeTypeId::Of(
                Value(TimestampValue(absl::UnixEpoch() + absl::Seconds(1)))),
            NativeTypeId::For<TimestampValue>());
}

TEST_P(TimestampValueTest, InstanceOf) {
  EXPECT_TRUE(InstanceOf<TimestampValue>(
      TimestampValue(absl::UnixEpoch() + absl::Seconds(1))));
  EXPECT_TRUE(InstanceOf<TimestampValue>(
      Value(TimestampValue(absl::UnixEpoch() + absl::Seconds(1)))));
}

TEST_P(TimestampValueTest, Cast) {
  EXPECT_THAT(Cast<TimestampValue>(
                  TimestampValue(absl::UnixEpoch() + absl::Seconds(1))),
              An<TimestampValue>());
  EXPECT_THAT(Cast<TimestampValue>(
                  Value(TimestampValue(absl::UnixEpoch() + absl::Seconds(1)))),
              An<TimestampValue>());
}

TEST_P(TimestampValueTest, As) {
  EXPECT_THAT(
      As<TimestampValue>(TimestampValue(absl::UnixEpoch() + absl::Seconds(1))),
      Ne(absl::nullopt));
  EXPECT_THAT(As<TimestampValue>(
                  Value(TimestampValue(absl::UnixEpoch() + absl::Seconds(1)))),
              Ne(absl::nullopt));
}

TEST_P(TimestampValueTest, Equality) {
  EXPECT_NE(TimestampValue(absl::UnixEpoch()),
            absl::UnixEpoch() + absl::Seconds(1));
  EXPECT_NE(absl::UnixEpoch() + absl::Seconds(1),
            TimestampValue(absl::UnixEpoch()));
  EXPECT_NE(TimestampValue(absl::UnixEpoch()),
            TimestampValue(absl::UnixEpoch() + absl::Seconds(1)));
}

INSTANTIATE_TEST_SUITE_P(
    TimestampValueTest, TimestampValueTest,
    ::testing::Combine(::testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting)),
    TimestampValueTest::ToString);

using TimestampValueViewTest = common_internal::ThreadCompatibleValueTest<>;

TEST_P(TimestampValueViewTest, Kind) {
  EXPECT_EQ(TimestampValueView(absl::UnixEpoch() + absl::Seconds(1)).kind(),
            TimestampValueView::kKind);
  EXPECT_EQ(ValueView(TimestampValueView(absl::UnixEpoch() + absl::Seconds(1)))
                .kind(),
            TimestampValueView::kKind);
}

TEST_P(TimestampValueViewTest, DebugString) {
  {
    std::ostringstream out;
    out << TimestampValueView(absl::UnixEpoch() + absl::Seconds(1));
    EXPECT_EQ(out.str(), "1970-01-01T00:00:01Z");
  }
  {
    std::ostringstream out;
    out << ValueView(TimestampValueView(absl::UnixEpoch() + absl::Seconds(1)));
    EXPECT_EQ(out.str(), "1970-01-01T00:00:01Z");
  }
}

TEST_P(TimestampValueViewTest, GetSerializedSize) {
  EXPECT_THAT(TimestampValueView().GetSerializedSize(value_manager()),
              IsOkAndHolds(0));
}

TEST_P(TimestampValueViewTest, ConvertToAny) {
  EXPECT_THAT(TimestampValueView().ConvertToAny(value_manager()),
              IsOkAndHolds(MakeAny(MakeTypeUrl("google.protobuf.Timestamp"),
                                   absl::Cord())));
}

TEST_P(TimestampValueViewTest, ConvertToJson) {
  EXPECT_THAT(TimestampValueView().ConvertToJson(value_manager()),
              IsOkAndHolds(Json(JsonString("1970-01-01T00:00:00Z"))));
}

TEST_P(TimestampValueViewTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(
                TimestampValueView(absl::UnixEpoch() + absl::Seconds(1))),
            NativeTypeId::For<TimestampValueView>());
  EXPECT_EQ(NativeTypeId::Of(ValueView(
                TimestampValueView(absl::UnixEpoch() + absl::Seconds(1)))),
            NativeTypeId::For<TimestampValueView>());
}

TEST_P(TimestampValueViewTest, InstanceOf) {
  EXPECT_TRUE(InstanceOf<TimestampValueView>(
      TimestampValueView(absl::UnixEpoch() + absl::Seconds(1))));
  EXPECT_TRUE(InstanceOf<TimestampValueView>(
      ValueView(TimestampValueView(absl::UnixEpoch() + absl::Seconds(1)))));
}

TEST_P(TimestampValueViewTest, Cast) {
  EXPECT_THAT(Cast<TimestampValueView>(
                  TimestampValueView(absl::UnixEpoch() + absl::Seconds(1))),
              An<TimestampValueView>());
  EXPECT_THAT(Cast<TimestampValueView>(ValueView(
                  TimestampValueView(absl::UnixEpoch() + absl::Seconds(1)))),
              An<TimestampValueView>());
}

TEST_P(TimestampValueViewTest, As) {
  EXPECT_THAT(As<TimestampValueView>(
                  TimestampValueView(absl::UnixEpoch() + absl::Seconds(1))),
              Ne(absl::nullopt));
  EXPECT_THAT(As<TimestampValueView>(ValueView(
                  TimestampValueView(absl::UnixEpoch() + absl::Seconds(1)))),
              Ne(absl::nullopt));
}

TEST_P(TimestampValueViewTest, Equality) {
  EXPECT_NE(TimestampValueView(TimestampValue(absl::UnixEpoch())),
            absl::UnixEpoch() + absl::Seconds(1));
  EXPECT_NE(absl::UnixEpoch() + absl::Seconds(1),
            TimestampValueView(absl::UnixEpoch()));
  EXPECT_NE(TimestampValueView(absl::UnixEpoch()),
            TimestampValueView(absl::UnixEpoch() + absl::Seconds(1)));
  EXPECT_NE(TimestampValueView(absl::UnixEpoch()),
            TimestampValue(absl::UnixEpoch() + absl::Seconds(1)));
  EXPECT_NE(TimestampValue(absl::UnixEpoch() + absl::Seconds(1)),
            TimestampValueView(absl::UnixEpoch()));
}

INSTANTIATE_TEST_SUITE_P(
    TimestampValueViewTest, TimestampValueViewTest,
    ::testing::Combine(::testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting)),
    TimestampValueViewTest::ToString);

}  // namespace
}  // namespace cel
