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

#include "absl/status/status_matchers.h"
#include "absl/strings/cord.h"
#include "absl/strings/cord_test_helpers.h"
#include "common/native_type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;

using BytesValueTest = common_internal::ValueTest<>;

TEST_F(BytesValueTest, Kind) {
  EXPECT_EQ(BytesValue("foo").kind(), BytesValue::kKind);
  EXPECT_EQ(Value(BytesValue(absl::Cord("foo"))).kind(), BytesValue::kKind);
}

TEST_F(BytesValueTest, DebugString) {
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

TEST_F(BytesValueTest, ConvertToJson) {
  auto* message = NewArenaValueMessage();
  EXPECT_THAT(BytesValue("foo").ConvertToJson(descriptor_pool(),
                                              message_factory(), message),
              IsOk());
  EXPECT_THAT(*message, EqualsValueTextProto(R"pb(string_value: "Zm9v")pb"));
}

TEST_F(BytesValueTest, NativeValue) {
  std::string scratch;
  EXPECT_EQ(BytesValue("foo").NativeString(), "foo");
  EXPECT_EQ(BytesValue("foo").NativeString(scratch), "foo");
  EXPECT_EQ(BytesValue("foo").NativeCord(), "foo");
}

TEST_F(BytesValueTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(BytesValue("foo")),
            NativeTypeId::For<BytesValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(BytesValue(absl::Cord("foo")))),
            NativeTypeId::For<BytesValue>());
}

TEST_F(BytesValueTest, StringViewEquality) {
  // NOLINTBEGIN(readability/check)
  EXPECT_TRUE(BytesValue("foo") == "foo");
  EXPECT_FALSE(BytesValue("foo") == "bar");

  EXPECT_TRUE("foo" == BytesValue("foo"));
  EXPECT_FALSE("bar" == BytesValue("foo"));
  // NOLINTEND(readability/check)
}

TEST_F(BytesValueTest, StringViewInequality) {
  // NOLINTBEGIN(readability/check)
  EXPECT_FALSE(BytesValue("foo") != "foo");
  EXPECT_TRUE(BytesValue("foo") != "bar");

  EXPECT_FALSE("foo" != BytesValue("foo"));
  EXPECT_TRUE("bar" != BytesValue("foo"));
  // NOLINTEND(readability/check)
}

TEST_F(BytesValueTest, Comparison) {
  EXPECT_LT(BytesValue("bar"), BytesValue("foo"));
  EXPECT_FALSE(BytesValue("foo") < BytesValue("foo"));
  EXPECT_FALSE(BytesValue("foo") < BytesValue("bar"));
}

}  // namespace
}  // namespace cel
