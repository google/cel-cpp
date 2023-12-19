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
#include "absl/strings/string_view.h"
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

TEST(NullValue, Kind) {
  EXPECT_EQ(NullValue().kind(), NullValue::kKind);
  EXPECT_EQ(Value(NullValue()).kind(), NullValue::kKind);
}

TEST(NullValue, Type) {
  EXPECT_EQ(NullValue().type(), NullType());
  EXPECT_EQ(Value(NullValue()).type(), NullType());
}

TEST(NullValue, DebugString) {
  {
    std::ostringstream out;
    out << NullValue();
    EXPECT_EQ(out.str(), "null");
  }
  {
    std::ostringstream out;
    out << Value(NullValue());
    EXPECT_EQ(out.str(), "null");
  }
}

TEST(NullValue, GetSerializedSize) {
  EXPECT_THAT(NullValue().GetSerializedSize(), IsOkAndHolds(2));
}

TEST(NullValue, ConvertToAny) {
  EXPECT_THAT(
      NullValue().ConvertToAny(),
      IsOkAndHolds(MakeAny(MakeTypeUrl("google.protobuf.Value"),
                           absl::Cord(absl::string_view("\010\000", 2)))));
}

TEST(NullValue, ConvertToJson) {
  EXPECT_THAT(NullValue().ConvertToJson(), IsOkAndHolds(Json(kJsonNull)));
}

TEST(NullValue, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(NullValue()), NativeTypeId::For<NullValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(NullValue())),
            NativeTypeId::For<NullValue>());
}

TEST(NullValue, InstanceOf) {
  EXPECT_TRUE(InstanceOf<NullValue>(NullValue()));
  EXPECT_TRUE(InstanceOf<NullValue>(Value(NullValue())));
}

TEST(NullValue, Cast) {
  EXPECT_THAT(Cast<NullValue>(NullValue()), An<NullValue>());
  EXPECT_THAT(Cast<NullValue>(Value(NullValue())), An<NullValue>());
}

TEST(NullValue, As) {
  EXPECT_THAT(As<NullValue>(NullValue()), Ne(absl::nullopt));
  EXPECT_THAT(As<NullValue>(Value(NullValue())), Ne(absl::nullopt));
}

TEST(NullValueView, Kind) {
  EXPECT_EQ(NullValueView().kind(), NullValueView::kKind);
  EXPECT_EQ(ValueView(NullValueView()).kind(), NullValueView::kKind);
}

TEST(NullValueView, Type) {
  EXPECT_EQ(NullValueView().type(), NullType());
  EXPECT_EQ(ValueView(NullValueView()).type(), NullType());
}

TEST(NullValueView, DebugString) {
  {
    std::ostringstream out;
    out << NullValueView();
    EXPECT_EQ(out.str(), "null");
  }
  {
    std::ostringstream out;
    out << ValueView(NullValueView());
    EXPECT_EQ(out.str(), "null");
  }
}

TEST(NullValueView, GetSerializedSize) {
  EXPECT_THAT(NullValueView().GetSerializedSize(), IsOkAndHolds(2));
}

TEST(NullValueView, ConvertToAny) {
  EXPECT_THAT(
      NullValueView().ConvertToAny(),
      IsOkAndHolds(MakeAny(MakeTypeUrl("google.protobuf.Value"),
                           absl::Cord(absl::string_view("\010\000", 2)))));
}

TEST(NullValueView, ConvertToJson) {
  EXPECT_THAT(NullValueView().ConvertToJson(), IsOkAndHolds(Json(kJsonNull)));
}

TEST(NullValueView, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(NullValueView()),
            NativeTypeId::For<NullValueView>());
  EXPECT_EQ(NativeTypeId::Of(ValueView(NullValueView())),
            NativeTypeId::For<NullValueView>());
}

TEST(NullValueView, InstanceOf) {
  EXPECT_TRUE(InstanceOf<NullValueView>(NullValueView()));
  EXPECT_TRUE(InstanceOf<NullValueView>(ValueView(NullValueView())));
}

TEST(NullValueView, Cast) {
  EXPECT_THAT(Cast<NullValueView>(NullValueView()), An<NullValueView>());
  EXPECT_THAT(Cast<NullValueView>(ValueView(NullValueView())),
              An<NullValueView>());
}

TEST(NullValueView, As) {
  EXPECT_THAT(As<NullValueView>(NullValueView()), Ne(absl::nullopt));
  EXPECT_THAT(As<NullValueView>(ValueView(NullValueView())), Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
