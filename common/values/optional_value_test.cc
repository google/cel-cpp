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
#include <utility>

#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "common/casting.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::absl_testing::StatusIs;

class OptionalValueTest : public common_internal::ValueTest<> {
 public:
  OptionalValue OptionalNone() { return OptionalValue::None(); }

  OptionalValue OptionalOf(Value value) {
    return OptionalValue::Of(std::move(value), arena());
  }
};

TEST_F(OptionalValueTest, Kind) {
  auto value = OptionalNone();
  EXPECT_EQ(value.kind(), OptionalValue::kKind);
  EXPECT_EQ(OpaqueValue(value).kind(), OptionalValue::kKind);
  EXPECT_EQ(Value(value).kind(), OptionalValue::kKind);
}

TEST_F(OptionalValueTest, Type) {
  auto value = OptionalNone();
  EXPECT_EQ(value.GetRuntimeType(), OptionalType());
}

TEST_F(OptionalValueTest, DebugString) {
  auto value = OptionalNone();
  {
    std::ostringstream out;
    out << value;
    EXPECT_EQ(out.str(), "optional.none()");
  }
  {
    std::ostringstream out;
    out << OpaqueValue(value);
    EXPECT_EQ(out.str(), "optional.none()");
  }
  {
    std::ostringstream out;
    out << Value(value);
    EXPECT_EQ(out.str(), "optional.none()");
  }
  {
    std::ostringstream out;
    out << OptionalOf(IntValue());
    EXPECT_EQ(out.str(), "optional(0)");
  }
}

TEST_F(OptionalValueTest, SerializeTo) {
  absl::Cord value;
  EXPECT_THAT(
      OptionalValue().SerializeTo(descriptor_pool(), message_factory(), value),
      StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(OptionalValueTest, ConvertToJson) {
  auto* message = NewArenaValueMessage();
  EXPECT_THAT(OptionalValue().ConvertToJson(descriptor_pool(),
                                            message_factory(), message),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(OptionalValueTest, HasValue) {
  auto value = OptionalNone();
  EXPECT_FALSE(value.HasValue());
  value = OptionalOf(IntValue());
  EXPECT_TRUE(value.HasValue());
}

TEST_F(OptionalValueTest, Value) {
  auto value = OptionalNone();
  auto element = value.Value();
  ASSERT_TRUE(InstanceOf<ErrorValue>(element));
  EXPECT_THAT(Cast<ErrorValue>(element).NativeValue(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  value = OptionalOf(IntValue());
  element = value.Value();
  ASSERT_TRUE(InstanceOf<IntValue>(element));
  EXPECT_EQ(Cast<IntValue>(element), IntValue());
}

}  // namespace
}  // namespace cel
