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

TEST(IntValue, Kind) {
  EXPECT_EQ(IntValue(1).kind(), IntValue::kKind);
  EXPECT_EQ(Value(IntValue(1)).kind(), IntValue::kKind);
}

TEST(IntValue, Type) {
  EXPECT_EQ(IntValue(1).type(), IntType());
  EXPECT_EQ(Value(IntValue(1)).type(), IntType());
}

TEST(IntValue, DebugString) {
  {
    std::ostringstream out;
    out << IntValue(1);
    EXPECT_EQ(out.str(), "1");
  }
  {
    std::ostringstream out;
    out << Value(IntValue(1));
    EXPECT_EQ(out.str(), "1");
  }
}

TEST(IntValue, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(IntValue(1)), NativeTypeId::For<IntValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(IntValue(1))),
            NativeTypeId::For<IntValue>());
}

TEST(IntValue, InstanceOf) {
  EXPECT_TRUE(InstanceOf<IntValue>(IntValue(1)));
  EXPECT_TRUE(InstanceOf<IntValue>(Value(IntValue(1))));
}

TEST(IntValue, Cast) {
  EXPECT_THAT(Cast<IntValue>(IntValue(1)), An<IntValue>());
  EXPECT_THAT(Cast<IntValue>(Value(IntValue(1))), An<IntValue>());
}

TEST(IntValue, As) {
  EXPECT_THAT(As<IntValue>(IntValue(1)), Ne(absl::nullopt));
  EXPECT_THAT(As<IntValue>(Value(IntValue(1))), Ne(absl::nullopt));
}

TEST(IntValueView, Kind) {
  EXPECT_EQ(IntValueView(1).kind(), IntValueView::kKind);
  EXPECT_EQ(ValueView(IntValueView(1)).kind(), IntValueView::kKind);
}

TEST(IntValueView, Type) {
  EXPECT_EQ(IntValueView(1).type(), IntType());
  EXPECT_EQ(ValueView(IntValueView(1)).type(), IntType());
}

TEST(IntValueView, DebugString) {
  {
    std::ostringstream out;
    out << IntValueView(1);
    EXPECT_EQ(out.str(), "1");
  }
  {
    std::ostringstream out;
    out << ValueView(IntValueView(1));
    EXPECT_EQ(out.str(), "1");
  }
}

TEST(IntValueView, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(IntValueView(1)),
            NativeTypeId::For<IntValueView>());
  EXPECT_EQ(NativeTypeId::Of(ValueView(IntValueView(1))),
            NativeTypeId::For<IntValueView>());
}

TEST(IntValueView, InstanceOf) {
  EXPECT_TRUE(InstanceOf<IntValueView>(IntValueView(1)));
  EXPECT_TRUE(InstanceOf<IntValueView>(ValueView(IntValueView(1))));
}

TEST(IntValueView, Cast) {
  EXPECT_THAT(Cast<IntValueView>(IntValueView(1)), An<IntValueView>());
  EXPECT_THAT(Cast<IntValueView>(ValueView(IntValueView(1))),
              An<IntValueView>());
}

TEST(IntValueView, As) {
  EXPECT_THAT(As<IntValueView>(IntValueView(1)), Ne(absl::nullopt));
  EXPECT_THAT(As<IntValueView>(ValueView(IntValueView(1))), Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
