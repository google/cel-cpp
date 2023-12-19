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

#include "absl/strings/cord.h"
#include "absl/strings/cord_test_helpers.h"
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

TEST(BytesValue, Kind) {
  EXPECT_EQ(BytesValue("foo").kind(), BytesValue::kKind);
  EXPECT_EQ(Value(BytesValue(absl::Cord("foo"))).kind(), BytesValue::kKind);
}

TEST(BytesValue, Type) {
  EXPECT_EQ(BytesValue("foo").type(), BytesType());
  EXPECT_EQ(Value(BytesValue(absl::Cord("foo"))).type(), BytesType());
}

TEST(BytesValue, DebugString) {
  {
    std::ostringstream out;
    out << BytesValue("foo");
    EXPECT_EQ(out.str(), "b\"foo\"");
  }
  {
    std::ostringstream out;
    out << BytesValue(absl::MakeFragmentedCord({"f", "o", "o"}));
    EXPECT_EQ(out.str(), "b\"foo\"");
  }
  {
    std::ostringstream out;
    out << Value(BytesValue(absl::Cord("foo")));
    EXPECT_EQ(out.str(), "b\"foo\"");
  }
}

TEST(BytesValue, GetSerializedSize) {
  EXPECT_THAT(BytesValue().GetSerializedSize(), IsOkAndHolds(0));
}

TEST(BytesValue, ConvertToAny) {
  EXPECT_THAT(BytesValue().ConvertToAny(),
              IsOkAndHolds(MakeAny(MakeTypeUrl("google.protobuf.BytesValue"),
                                   absl::Cord())));
}

TEST(BytesValue, ConvertToJson) {
  EXPECT_THAT(BytesValue("foo").ConvertToJson(),
              IsOkAndHolds(Json(JsonBytes("foo"))));
}

TEST(BytesValue, NativeValue) {
  std::string scratch;
  EXPECT_EQ(BytesValue("foo").NativeString(), "foo");
  EXPECT_EQ(BytesValue("foo").NativeString(scratch), "foo");
  EXPECT_EQ(BytesValue("foo").NativeCord(), "foo");
}

TEST(BytesValue, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(BytesValue("foo")),
            NativeTypeId::For<BytesValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(BytesValue(absl::Cord("foo")))),
            NativeTypeId::For<BytesValue>());
}

TEST(BytesValue, InstanceOf) {
  EXPECT_TRUE(InstanceOf<BytesValue>(BytesValue("foo")));
  EXPECT_TRUE(InstanceOf<BytesValue>(Value(BytesValue(absl::Cord("foo")))));
}

TEST(BytesValue, Cast) {
  EXPECT_THAT(Cast<BytesValue>(BytesValue("foo")), An<BytesValue>());
  EXPECT_THAT(Cast<BytesValue>(Value(BytesValue(absl::Cord("foo")))),
              An<BytesValue>());
}

TEST(BytesValue, As) {
  EXPECT_THAT(As<BytesValue>(BytesValue("foo")), Ne(absl::nullopt));
  EXPECT_THAT(As<BytesValue>(Value(BytesValue(absl::Cord("foo")))),
              Ne(absl::nullopt));
}

TEST(BytesValueView, Kind) {
  EXPECT_EQ(BytesValueView("foo").kind(), BytesValueView::kKind);
  EXPECT_EQ(ValueView(BytesValueView("foo")).kind(), BytesValueView::kKind);
}

TEST(BytesValueView, Type) {
  EXPECT_EQ(BytesValueView("foo").type(), BytesType());
  EXPECT_EQ(ValueView(BytesValueView("foo")).type(), BytesType());
}

TEST(BytesValueView, DebugString) {
  {
    std::ostringstream out;
    out << BytesValueView("foo");
    EXPECT_EQ(out.str(), "b\"foo\"");
  }
  {
    std::ostringstream out;
    out << ValueView(BytesValueView("foo"));
    EXPECT_EQ(out.str(), "b\"foo\"");
  }
}

TEST(BytesValueView, GetSerializedSize) {
  EXPECT_THAT(BytesValueView().GetSerializedSize(), IsOkAndHolds(0));
}

TEST(BytesValueView, ConvertToAny) {
  EXPECT_THAT(BytesValueView().ConvertToAny(),
              IsOkAndHolds(MakeAny(MakeTypeUrl("google.protobuf.BytesValue"),
                                   absl::Cord())));
}

TEST(BytesValueView, ConvertToJson) {
  EXPECT_THAT(BytesValueView("foo").ConvertToJson(),
              IsOkAndHolds(Json(JsonBytes("foo"))));
}

TEST(BytesValueView, NativeValue) {
  std::string scratch;
  EXPECT_EQ(BytesValueView(BytesValue("foo")).NativeString(), "foo");
  EXPECT_EQ(BytesValueView(BytesValue("foo")).NativeString(scratch), "foo");
  EXPECT_EQ(BytesValueView(BytesValue("foo")).NativeCord(), "foo");
}

TEST(BytesValueView, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(BytesValueView("foo")),
            NativeTypeId::For<BytesValueView>());
  EXPECT_EQ(NativeTypeId::Of(ValueView(BytesValueView("foo"))),
            NativeTypeId::For<BytesValueView>());
}

TEST(BytesValueView, InstanceOf) {
  EXPECT_TRUE(InstanceOf<BytesValueView>(BytesValueView("foo")));
  EXPECT_TRUE(InstanceOf<BytesValueView>(ValueView(BytesValueView("foo"))));
}

TEST(BytesValueView, Cast) {
  EXPECT_THAT(Cast<BytesValueView>(BytesValueView("foo")),
              An<BytesValueView>());
  EXPECT_THAT(Cast<BytesValueView>(ValueView(BytesValueView("foo"))),
              An<BytesValueView>());
}

TEST(BytesValueView, As) {
  EXPECT_THAT(As<BytesValueView>(BytesValueView("foo")), Ne(absl::nullopt));
  EXPECT_THAT(As<BytesValueView>(ValueView(BytesValueView("foo"))),
              Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
