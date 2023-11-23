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
#include "common/casting.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/value.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::An;
using testing::Ne;

TEST(StringValue, Kind) {
  EXPECT_EQ(StringValue("foo").kind(), StringValue::kKind);
  EXPECT_EQ(Value(StringValue(absl::Cord("foo"))).kind(), StringValue::kKind);
}

TEST(StringValue, Type) {
  EXPECT_EQ(StringValue("foo").type(), StringType());
  EXPECT_EQ(Value(StringValue(absl::Cord("foo"))).type(), StringType());
}

TEST(StringValue, DebugString) {
  {
    std::ostringstream out;
    out << StringValue("foo");
    EXPECT_EQ(out.str(), "\"foo\"");
  }
  {
    std::ostringstream out;
    out << StringValue(absl::MakeFragmentedCord({"f", "o", "o"}));
    EXPECT_EQ(out.str(), "\"foo\"");
  }
  {
    std::ostringstream out;
    out << Value(StringValue(absl::Cord("foo")));
    EXPECT_EQ(out.str(), "\"foo\"");
  }
}

TEST(StringValue, NativeValue) {
  std::string scratch;
  EXPECT_EQ(StringValue("foo").NativeString(), "foo");
  EXPECT_EQ(StringValue("foo").NativeString(scratch), "foo");
  EXPECT_EQ(StringValue("foo").NativeCord(), "foo");
}

TEST(StringValue, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(StringValue("foo")),
            NativeTypeId::For<StringValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(StringValue(absl::Cord("foo")))),
            NativeTypeId::For<StringValue>());
}

TEST(StringValue, InstanceOf) {
  EXPECT_TRUE(InstanceOf<StringValue>(StringValue("foo")));
  EXPECT_TRUE(InstanceOf<StringValue>(Value(StringValue(absl::Cord("foo")))));
}

TEST(StringValue, Cast) {
  EXPECT_THAT(Cast<StringValue>(StringValue("foo")), An<StringValue>());
  EXPECT_THAT(Cast<StringValue>(Value(StringValue(absl::Cord("foo")))),
              An<StringValue>());
}

TEST(StringValue, As) {
  EXPECT_THAT(As<StringValue>(StringValue("foo")), Ne(absl::nullopt));
  EXPECT_THAT(As<StringValue>(Value(StringValue(absl::Cord("foo")))),
              Ne(absl::nullopt));
}

TEST(StringValueView, Kind) {
  EXPECT_EQ(StringValueView("foo").kind(), StringValueView::kKind);
  EXPECT_EQ(ValueView(StringValueView("foo")).kind(), StringValueView::kKind);
}

TEST(StringValueView, Type) {
  EXPECT_EQ(StringValueView("foo").type(), StringType());
  EXPECT_EQ(ValueView(StringValueView("foo")).type(), StringType());
}

TEST(StringValueView, DebugString) {
  {
    std::ostringstream out;
    out << StringValueView("foo");
    EXPECT_EQ(out.str(), "\"foo\"");
  }
  {
    std::ostringstream out;
    out << ValueView(StringValueView("foo"));
    EXPECT_EQ(out.str(), "\"foo\"");
  }
}

TEST(StringValueView, NativeValue) {
  std::string scratch;
  EXPECT_EQ(StringValueView(StringValue("foo")).NativeString(), "foo");
  EXPECT_EQ(StringValueView(StringValue("foo")).NativeString(scratch), "foo");
  EXPECT_EQ(StringValueView(StringValue("foo")).NativeCord(), "foo");
}

TEST(StringValueView, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(StringValueView("foo")),
            NativeTypeId::For<StringValueView>());
  EXPECT_EQ(NativeTypeId::Of(ValueView(StringValueView("foo"))),
            NativeTypeId::For<StringValueView>());
}

TEST(StringValueView, InstanceOf) {
  EXPECT_TRUE(InstanceOf<StringValueView>(StringValueView("foo")));
  EXPECT_TRUE(InstanceOf<StringValueView>(ValueView(StringValueView("foo"))));
}

TEST(StringValueView, Cast) {
  EXPECT_THAT(Cast<StringValueView>(StringValueView("foo")),
              An<StringValueView>());
  EXPECT_THAT(Cast<StringValueView>(ValueView(StringValueView("foo"))),
              An<StringValueView>());
}

TEST(StringValueView, As) {
  EXPECT_THAT(As<StringValueView>(StringValueView("foo")), Ne(absl::nullopt));
  EXPECT_THAT(As<StringValueView>(ValueView(StringValueView("foo"))),
              Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
