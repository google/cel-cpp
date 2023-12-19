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

TEST(UintValue, Kind) {
  EXPECT_EQ(UintValue(1).kind(), UintValue::kKind);
  EXPECT_EQ(Value(UintValue(1)).kind(), UintValue::kKind);
}

TEST(UintValue, Type) {
  EXPECT_EQ(UintValue(1).type(), UintType());
  EXPECT_EQ(Value(UintValue(1)).type(), UintType());
}

TEST(UintValue, DebugString) {
  {
    std::ostringstream out;
    out << UintValue(1);
    EXPECT_EQ(out.str(), "1u");
  }
  {
    std::ostringstream out;
    out << Value(UintValue(1));
    EXPECT_EQ(out.str(), "1u");
  }
}

TEST(UintValue, GetSerializedSize) {
  EXPECT_THAT(UintValue().GetSerializedSize(), IsOkAndHolds(0));
}

TEST(UintValue, ConvertToAny) {
  EXPECT_THAT(UintValue().ConvertToAny(),
              IsOkAndHolds(MakeAny(MakeTypeUrl("google.protobuf.UInt64Value"),
                                   absl::Cord())));
}

TEST(UintValue, ConvertToJson) {
  EXPECT_THAT(UintValue(1).ConvertToJson(), IsOkAndHolds(Json(1.0)));
}

TEST(UintValue, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(UintValue(1)), NativeTypeId::For<UintValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(UintValue(1))),
            NativeTypeId::For<UintValue>());
}

TEST(UintValue, InstanceOf) {
  EXPECT_TRUE(InstanceOf<UintValue>(UintValue(1)));
  EXPECT_TRUE(InstanceOf<UintValue>(Value(UintValue(1))));
}

TEST(UintValue, Cast) {
  EXPECT_THAT(Cast<UintValue>(UintValue(1)), An<UintValue>());
  EXPECT_THAT(Cast<UintValue>(Value(UintValue(1))), An<UintValue>());
}

TEST(UintValue, As) {
  EXPECT_THAT(As<UintValue>(UintValue(1)), Ne(absl::nullopt));
  EXPECT_THAT(As<UintValue>(Value(UintValue(1))), Ne(absl::nullopt));
}

TEST(UintValue, HashValue) {
  EXPECT_EQ(absl::HashOf(UintValue(1)), absl::HashOf(uint64_t{1}));
}

TEST(UintValue, Equality) {
  EXPECT_NE(UintValue(0u), 1u);
  EXPECT_NE(1u, UintValue(0u));
  EXPECT_NE(UintValue(0u), UintValue(1u));
}

TEST(UintValue, LessThan) {
  EXPECT_LT(UintValue(0), 1);
  EXPECT_LT(0, UintValue(1));
  EXPECT_LT(UintValue(0), UintValue(1));
}

TEST(UintValueView, Kind) {
  EXPECT_EQ(UintValueView(1).kind(), UintValueView::kKind);
  EXPECT_EQ(ValueView(UintValueView(1)).kind(), UintValueView::kKind);
}

TEST(UintValueView, Type) {
  EXPECT_EQ(UintValueView(1).type(), UintType());
  EXPECT_EQ(ValueView(UintValueView(1)).type(), UintType());
}

TEST(UintValueView, DebugString) {
  {
    std::ostringstream out;
    out << UintValueView(1);
    EXPECT_EQ(out.str(), "1u");
  }
  {
    std::ostringstream out;
    out << ValueView(UintValueView(1));
    EXPECT_EQ(out.str(), "1u");
  }
}

TEST(UintValueView, GetSerializedSize) {
  EXPECT_THAT(UintValueView().GetSerializedSize(), IsOkAndHolds(0));
}

TEST(UintValueView, ConvertToAny) {
  EXPECT_THAT(UintValueView().ConvertToAny(),
              IsOkAndHolds(MakeAny(MakeTypeUrl("google.protobuf.UInt64Value"),
                                   absl::Cord())));
}

TEST(UintValueView, ConvertToJson) {
  EXPECT_THAT(UintValueView(1).ConvertToJson(), IsOkAndHolds(Json(1.0)));
}

TEST(UintValueView, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(UintValueView(1)),
            NativeTypeId::For<UintValueView>());
  EXPECT_EQ(NativeTypeId::Of(ValueView(UintValueView(1))),
            NativeTypeId::For<UintValueView>());
}

TEST(UintValueView, InstanceOf) {
  EXPECT_TRUE(InstanceOf<UintValueView>(UintValueView(1)));
  EXPECT_TRUE(InstanceOf<UintValueView>(ValueView(UintValueView(1))));
}

TEST(UintValueView, Cast) {
  EXPECT_THAT(Cast<UintValueView>(UintValueView(1)), An<UintValueView>());
  EXPECT_THAT(Cast<UintValueView>(ValueView(UintValueView(1))),
              An<UintValueView>());
}

TEST(UintValueView, As) {
  EXPECT_THAT(As<UintValueView>(UintValueView(1)), Ne(absl::nullopt));
  EXPECT_THAT(As<UintValueView>(ValueView(UintValueView(1))),
              Ne(absl::nullopt));
}

TEST(UintValueView, HashValue) {
  EXPECT_EQ(absl::HashOf(UintValueView(1)), absl::HashOf(uint64_t{1}));
}

TEST(UintValueView, Equality) {
  EXPECT_NE(UintValueView(UintValue(0u)), 1u);
  EXPECT_NE(1u, UintValueView(0u));
  EXPECT_NE(UintValueView(0u), UintValueView(1u));
  EXPECT_NE(UintValueView(0u), UintValue(1u));
  EXPECT_NE(UintValue(1u), UintValueView(0u));
}

TEST(IntValueView, LessThan) {
  EXPECT_LT(UintValueView(0), 1);
  EXPECT_LT(0, UintValueView(1));
  EXPECT_LT(UintValueView(0), UintValueView(1));
  EXPECT_LT(UintValueView(0), UintValue(1));
  EXPECT_LT(UintValue(0), UintValueView(1));
}

}  // namespace
}  // namespace cel
