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

#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/value.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::An;
using testing::Ne;

TEST(DurationValue, Kind) {
  EXPECT_EQ(DurationValue().kind(), DurationValue::kKind);
  EXPECT_EQ(Value(DurationValue(absl::Seconds(1))).kind(),
            DurationValue::kKind);
}

TEST(DurationValue, Type) {
  EXPECT_EQ(DurationValue(absl::Seconds(1)).type(), DurationType());
  EXPECT_EQ(Value(DurationValue(absl::Seconds(1))).type(), DurationType());
}

TEST(DurationValue, DebugString) {
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

TEST(DurationValue, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(DurationValue(absl::Seconds(1))),
            NativeTypeId::For<DurationValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(DurationValue(absl::Seconds(1)))),
            NativeTypeId::For<DurationValue>());
}

TEST(DurationValue, InstanceOf) {
  EXPECT_TRUE(InstanceOf<DurationValue>(DurationValue(absl::Seconds(1))));
  EXPECT_TRUE(
      InstanceOf<DurationValue>(Value(DurationValue(absl::Seconds(1)))));
}

TEST(DurationValue, Cast) {
  EXPECT_THAT(Cast<DurationValue>(DurationValue(absl::Seconds(1))),
              An<DurationValue>());
  EXPECT_THAT(Cast<DurationValue>(Value(DurationValue(absl::Seconds(1)))),
              An<DurationValue>());
}

TEST(DurationValue, As) {
  EXPECT_THAT(As<DurationValue>(DurationValue(absl::Seconds(1))),
              Ne(absl::nullopt));
  EXPECT_THAT(As<DurationValue>(Value(DurationValue(absl::Seconds(1)))),
              Ne(absl::nullopt));
}

TEST(DurationValue, Equality) {
  EXPECT_NE(DurationValue(absl::ZeroDuration()), absl::Seconds(1));
  EXPECT_NE(absl::Seconds(1), DurationValue(absl::ZeroDuration()));
  EXPECT_NE(DurationValue(absl::ZeroDuration()),
            DurationValue(absl::Seconds(1)));
}

TEST(DurationValueView, Kind) {
  EXPECT_EQ(DurationValueView(absl::Seconds(1)).kind(),
            DurationValueView::kKind);
  EXPECT_EQ(ValueView(DurationValueView(absl::Seconds(1))).kind(),
            DurationValueView::kKind);
}

TEST(DurationValueView, Type) {
  EXPECT_EQ(DurationValueView(absl::Seconds(1)).type(), DurationType());
  EXPECT_EQ(ValueView(DurationValueView(absl::Seconds(1))).type(),
            DurationType());
}

TEST(DurationValueView, DebugString) {
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

TEST(DurationValueView, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(DurationValueView(absl::Seconds(1))),
            NativeTypeId::For<DurationValueView>());
  EXPECT_EQ(NativeTypeId::Of(ValueView(DurationValueView(absl::Seconds(1)))),
            NativeTypeId::For<DurationValueView>());
}

TEST(DurationValueView, InstanceOf) {
  EXPECT_TRUE(
      InstanceOf<DurationValueView>(DurationValueView(absl::Seconds(1))));
  EXPECT_TRUE(InstanceOf<DurationValueView>(
      ValueView(DurationValueView(absl::Seconds(1)))));
}

TEST(DurationValueView, Cast) {
  EXPECT_THAT(Cast<DurationValueView>(DurationValueView(absl::Seconds(1))),
              An<DurationValueView>());
  EXPECT_THAT(
      Cast<DurationValueView>(ValueView(DurationValueView(absl::Seconds(1)))),
      An<DurationValueView>());
}

TEST(DurationValueView, As) {
  EXPECT_THAT(As<DurationValueView>(DurationValueView(absl::Seconds(1))),
              Ne(absl::nullopt));
  EXPECT_THAT(
      As<DurationValueView>(ValueView(DurationValueView(absl::Seconds(1)))),
      Ne(absl::nullopt));
}

TEST(DurationValueView, Equality) {
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

}  // namespace
}  // namespace cel
