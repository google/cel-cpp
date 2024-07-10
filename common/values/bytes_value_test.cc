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
#include "common/value_testing.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::An;
using testing::Ne;
using cel::internal::IsOkAndHolds;

using BytesValueTest = common_internal::ThreadCompatibleValueTest<>;

TEST_P(BytesValueTest, Kind) {
  EXPECT_EQ(BytesValue("foo").kind(), BytesValue::kKind);
  EXPECT_EQ(Value(BytesValue(absl::Cord("foo"))).kind(), BytesValue::kKind);
}

TEST_P(BytesValueTest, DebugString) {
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

TEST_P(BytesValueTest, GetSerializedSize) {
  EXPECT_THAT(BytesValue().GetSerializedSize(value_manager()), IsOkAndHolds(0));
}

TEST_P(BytesValueTest, ConvertToAny) {
  EXPECT_THAT(BytesValue().ConvertToAny(value_manager()),
              IsOkAndHolds(MakeAny(MakeTypeUrl("google.protobuf.BytesValue"),
                                   absl::Cord())));
}

TEST_P(BytesValueTest, ConvertToJson) {
  EXPECT_THAT(BytesValue("foo").ConvertToJson(value_manager()),
              IsOkAndHolds(Json(JsonBytes("foo"))));
}

TEST_P(BytesValueTest, NativeValue) {
  std::string scratch;
  EXPECT_EQ(BytesValue("foo").NativeString(), "foo");
  EXPECT_EQ(BytesValue("foo").NativeString(scratch), "foo");
  EXPECT_EQ(BytesValue("foo").NativeCord(), "foo");
}

TEST_P(BytesValueTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(BytesValue("foo")),
            NativeTypeId::For<BytesValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(BytesValue(absl::Cord("foo")))),
            NativeTypeId::For<BytesValue>());
}

TEST_P(BytesValueTest, InstanceOf) {
  EXPECT_TRUE(InstanceOf<BytesValue>(BytesValue("foo")));
  EXPECT_TRUE(InstanceOf<BytesValue>(Value(BytesValue(absl::Cord("foo")))));
}

TEST_P(BytesValueTest, Cast) {
  EXPECT_THAT(Cast<BytesValue>(BytesValue("foo")), An<BytesValue>());
  EXPECT_THAT(Cast<BytesValue>(Value(BytesValue(absl::Cord("foo")))),
              An<BytesValue>());
}

TEST_P(BytesValueTest, As) {
  EXPECT_THAT(As<BytesValue>(BytesValue("foo")), Ne(absl::nullopt));
  EXPECT_THAT(As<BytesValue>(Value(BytesValue(absl::Cord("foo")))),
              Ne(absl::nullopt));
}

TEST_P(BytesValueTest, StringViewEquality) {
  // NOLINTBEGIN(readability/check)
  EXPECT_TRUE(BytesValue("foo") == "foo");
  EXPECT_FALSE(BytesValue("foo") == "bar");

  EXPECT_TRUE("foo" == BytesValue("foo"));
  EXPECT_FALSE("bar" == BytesValue("foo"));
  // NOLINTEND(readability/check)
}

TEST_P(BytesValueTest, StringViewInequality) {
  // NOLINTBEGIN(readability/check)
  EXPECT_FALSE(BytesValue("foo") != "foo");
  EXPECT_TRUE(BytesValue("foo") != "bar");

  EXPECT_FALSE("foo" != BytesValue("foo"));
  EXPECT_TRUE("bar" != BytesValue("foo"));
  // NOLINTEND(readability/check)
}

INSTANTIATE_TEST_SUITE_P(
    BytesValueTest, BytesValueTest,
    ::testing::Combine(::testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting)),
    BytesValueTest::ToString);

using BytesValueViewTest = common_internal::ThreadCompatibleValueTest<>;

TEST_P(BytesValueViewTest, Kind) {
  EXPECT_EQ(BytesValueView("foo").kind(), BytesValueView::kKind);
  EXPECT_EQ(ValueView(BytesValueView("foo")).kind(), BytesValueView::kKind);
}

TEST_P(BytesValueViewTest, DebugString) {
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

TEST_P(BytesValueViewTest, GetSerializedSize) {
  EXPECT_THAT(BytesValueView().GetSerializedSize(value_manager()),
              IsOkAndHolds(0));
}

TEST_P(BytesValueViewTest, ConvertToAny) {
  EXPECT_THAT(BytesValueView().ConvertToAny(value_manager()),
              IsOkAndHolds(MakeAny(MakeTypeUrl("google.protobuf.BytesValue"),
                                   absl::Cord())));
}

TEST_P(BytesValueViewTest, ConvertToJson) {
  EXPECT_THAT(BytesValueView("foo").ConvertToJson(value_manager()),
              IsOkAndHolds(Json(JsonBytes("foo"))));
}

TEST_P(BytesValueViewTest, StringViewEquality) {
  // NOLINTBEGIN(readability/check)
  EXPECT_TRUE(BytesValueView("foo") == "foo");
  EXPECT_FALSE(BytesValueView("foo") == "bar");

  EXPECT_TRUE("foo" == BytesValueView("foo"));
  EXPECT_FALSE("bar" == BytesValueView("foo"));
  // NOLINTEND(readability/check)
}

TEST_P(BytesValueViewTest, StringViewInequality) {
  // NOLINTBEGIN(readability/check)
  EXPECT_FALSE(BytesValueView("foo") != "foo");
  EXPECT_TRUE(BytesValueView("foo") != "bar");

  EXPECT_FALSE("foo" != BytesValueView("foo"));
  EXPECT_TRUE("bar" != BytesValueView("foo"));
  // NOLINTEND(readability/check)
}

TEST_P(BytesValueViewTest, NativeValue) {
  std::string scratch;
  EXPECT_EQ(BytesValueView(BytesValue("foo")).NativeString(), "foo");
  EXPECT_EQ(BytesValueView(BytesValue("foo")).NativeString(scratch), "foo");
  EXPECT_EQ(BytesValueView(BytesValue("foo")).NativeCord(), "foo");
}

TEST_P(BytesValueViewTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(BytesValueView("foo")),
            NativeTypeId::For<BytesValueView>());
  EXPECT_EQ(NativeTypeId::Of(ValueView(BytesValueView("foo"))),
            NativeTypeId::For<BytesValueView>());
}

TEST_P(BytesValueViewTest, InstanceOf) {
  EXPECT_TRUE(InstanceOf<BytesValueView>(BytesValueView("foo")));
  EXPECT_TRUE(InstanceOf<BytesValueView>(ValueView(BytesValueView("foo"))));
}

TEST_P(BytesValueViewTest, Cast) {
  EXPECT_THAT(Cast<BytesValueView>(BytesValueView("foo")),
              An<BytesValueView>());
  EXPECT_THAT(Cast<BytesValueView>(ValueView(BytesValueView("foo"))),
              An<BytesValueView>());
}

TEST_P(BytesValueViewTest, As) {
  EXPECT_THAT(As<BytesValueView>(BytesValueView("foo")), Ne(absl::nullopt));
  EXPECT_THAT(As<BytesValueView>(ValueView(BytesValueView("foo"))),
              Ne(absl::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    BytesValueViewTest, BytesValueViewTest,
    ::testing::Combine(::testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting)),
    BytesValueViewTest::ToString);

}  // namespace
}  // namespace cel
