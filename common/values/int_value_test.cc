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

#include <cstdint>
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
#include "internal/testing.h"

namespace cel {
namespace {

using testing::An;
using testing::Ne;
using cel::internal::IsOkAndHolds;

TEST(IntValue, Kind) {
  EXPECT_EQ(IntValue(1).kind(), IntValue::kKind);
  EXPECT_EQ(Value(IntValue(1)).kind(), IntValue::kKind);
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

TEST(IntValue, GetSerializedSize) {
  EXPECT_THAT(IntValue().GetSerializedSize(), IsOkAndHolds(0));
}

TEST(IntValue, ConvertToAny) {
  EXPECT_THAT(IntValue().ConvertToAny(),
              IsOkAndHolds(MakeAny(MakeTypeUrl("google.protobuf.Int64Value"),
                                   absl::Cord())));
}

TEST(IntValue, ConvertToJson) {
  EXPECT_THAT(IntValue(1).ConvertToJson(), IsOkAndHolds(Json(1.0)));
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

TEST(IntValue, HashValue) {
  EXPECT_EQ(absl::HashOf(IntValue(1)), absl::HashOf(int64_t{1}));
}

TEST(IntValue, Equality) {
  EXPECT_NE(IntValue(0), 1);
  EXPECT_NE(1, IntValue(0));
  EXPECT_NE(IntValue(0), IntValue(1));
}

TEST(IntValue, LessThan) {
  EXPECT_LT(IntValue(0), 1);
  EXPECT_LT(0, IntValue(1));
  EXPECT_LT(IntValue(0), IntValue(1));
}

TEST(IntValueView, Kind) {
  EXPECT_EQ(IntValueView(1).kind(), IntValueView::kKind);
  EXPECT_EQ(ValueView(IntValueView(1)).kind(), IntValueView::kKind);
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

TEST(IntValueView, GetSerializedSize) {
  EXPECT_THAT(IntValueView().GetSerializedSize(), IsOkAndHolds(0));
}

TEST(IntValueView, ConvertToAny) {
  EXPECT_THAT(IntValueView().ConvertToAny(),
              IsOkAndHolds(MakeAny(MakeTypeUrl("google.protobuf.Int64Value"),
                                   absl::Cord())));
}

TEST(IntValueView, ConvertToJson) {
  EXPECT_THAT(IntValueView(1).ConvertToJson(), IsOkAndHolds(Json(1.0)));
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

TEST(IntValueView, HashValue) {
  EXPECT_EQ(absl::HashOf(IntValueView(1)), absl::HashOf(int64_t{1}));
}

TEST(IntValueView, Equality) {
  EXPECT_NE(IntValueView(IntValue(0)), 1);
  EXPECT_NE(1, IntValueView(0));
  EXPECT_NE(IntValueView(0), IntValueView(1));
  EXPECT_NE(IntValueView(0), IntValue(1));
  EXPECT_NE(IntValue(1), IntValueView(0));
}

TEST(IntValueView, LessThan) {
  EXPECT_LT(IntValueView(0), 1);
  EXPECT_LT(0, IntValueView(1));
  EXPECT_LT(IntValueView(0), IntValueView(1));
  EXPECT_LT(IntValueView(0), IntValue(1));
  EXPECT_LT(IntValue(0), IntValueView(1));
}

}  // namespace
}  // namespace cel
