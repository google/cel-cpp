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

#include <cmath>
#include <sstream>

#include "absl/strings/cord.h"
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

using DoubleValueTest = common_internal::ThreadCompatibleValueTest<>;

TEST_P(DoubleValueTest, Kind) {
  EXPECT_EQ(DoubleValue(1.0).kind(), DoubleValue::kKind);
  EXPECT_EQ(Value(DoubleValue(1.0)).kind(), DoubleValue::kKind);
}

TEST_P(DoubleValueTest, DebugString) {
  {
    std::ostringstream out;
    out << DoubleValue(0.0);
    EXPECT_EQ(out.str(), "0.0");
  }
  {
    std::ostringstream out;
    out << DoubleValue(1.0);
    EXPECT_EQ(out.str(), "1.0");
  }
  {
    std::ostringstream out;
    out << DoubleValue(1.1);
    EXPECT_EQ(out.str(), "1.1");
  }
  {
    std::ostringstream out;
    out << DoubleValue(NAN);
    EXPECT_EQ(out.str(), "nan");
  }
  {
    std::ostringstream out;
    out << DoubleValue(INFINITY);
    EXPECT_EQ(out.str(), "+infinity");
  }
  {
    std::ostringstream out;
    out << DoubleValue(-INFINITY);
    EXPECT_EQ(out.str(), "-infinity");
  }
  {
    std::ostringstream out;
    out << Value(DoubleValue(0.0));
    EXPECT_EQ(out.str(), "0.0");
  }
}

TEST_P(DoubleValueTest, GetSerializedSize) {
  EXPECT_THAT(DoubleValue().GetSerializedSize(value_manager()),
              IsOkAndHolds(0));
}

TEST_P(DoubleValueTest, ConvertToAny) {
  EXPECT_THAT(DoubleValue().ConvertToAny(value_manager()),
              IsOkAndHolds(MakeAny(MakeTypeUrl("google.protobuf.DoubleValue"),
                                   absl::Cord())));
}

TEST_P(DoubleValueTest, ConvertToJson) {
  EXPECT_THAT(DoubleValue(1.0).ConvertToJson(value_manager()),
              IsOkAndHolds(Json(1.0)));
}

TEST_P(DoubleValueTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(DoubleValue(1.0)),
            NativeTypeId::For<DoubleValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(DoubleValue(1.0))),
            NativeTypeId::For<DoubleValue>());
}

TEST_P(DoubleValueTest, InstanceOf) {
  EXPECT_TRUE(InstanceOf<DoubleValue>(DoubleValue(1.0)));
  EXPECT_TRUE(InstanceOf<DoubleValue>(Value(DoubleValue(1.0))));
}

TEST_P(DoubleValueTest, Cast) {
  EXPECT_THAT(Cast<DoubleValue>(DoubleValue(1.0)), An<DoubleValue>());
  EXPECT_THAT(Cast<DoubleValue>(Value(DoubleValue(1.0))), An<DoubleValue>());
}

TEST_P(DoubleValueTest, As) {
  EXPECT_THAT(As<DoubleValue>(DoubleValue(1.0)), Ne(absl::nullopt));
  EXPECT_THAT(As<DoubleValue>(Value(DoubleValue(1.0))), Ne(absl::nullopt));
}

TEST_P(DoubleValueTest, Equality) {
  EXPECT_NE(DoubleValue(0.0), 1.0);
  EXPECT_NE(1.0, DoubleValue(0.0));
  EXPECT_NE(DoubleValue(0.0), DoubleValue(1.0));
}

INSTANTIATE_TEST_SUITE_P(
    DoubleValueTest, DoubleValueTest,
    ::testing::Combine(::testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting)),
    DoubleValueTest::ToString);

using DoubleValueViewTest = common_internal::ThreadCompatibleValueTest<>;

TEST_P(DoubleValueViewTest, Kind) {
  EXPECT_EQ(DoubleValueView(1.0).kind(), DoubleValueView::kKind);
  EXPECT_EQ(ValueView(DoubleValueView(1.0)).kind(), DoubleValueView::kKind);
}

TEST_P(DoubleValueViewTest, DebugString) {
  {
    std::ostringstream out;
    out << DoubleValueView(0.0);
    EXPECT_EQ(out.str(), "0.0");
  }
  {
    std::ostringstream out;
    out << DoubleValueView(1.0);
    EXPECT_EQ(out.str(), "1.0");
  }
  {
    std::ostringstream out;
    out << DoubleValueView(1.1);
    EXPECT_EQ(out.str(), "1.1");
  }
  {
    std::ostringstream out;
    out << DoubleValueView(NAN);
    EXPECT_EQ(out.str(), "nan");
  }
  {
    std::ostringstream out;
    out << DoubleValueView(INFINITY);
    EXPECT_EQ(out.str(), "+infinity");
  }
  {
    std::ostringstream out;
    out << DoubleValueView(-INFINITY);
    EXPECT_EQ(out.str(), "-infinity");
  }
  {
    std::ostringstream out;
    out << ValueView(DoubleValueView(0.0));
    EXPECT_EQ(out.str(), "0.0");
  }
}

TEST_P(DoubleValueViewTest, GetSerializedSize) {
  EXPECT_THAT(DoubleValue().GetSerializedSize(value_manager()),
              IsOkAndHolds(0));
}

TEST_P(DoubleValueViewTest, ConvertToAny) {
  EXPECT_THAT(DoubleValue().ConvertToAny(value_manager()),
              IsOkAndHolds(MakeAny(MakeTypeUrl("google.protobuf.DoubleValue"),
                                   absl::Cord())));
}

TEST_P(DoubleValueViewTest, ConvertToJson) {
  EXPECT_THAT(DoubleValueView(1.0).ConvertToJson(value_manager()),
              IsOkAndHolds(Json(1.0)));
}

TEST_P(DoubleValueViewTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(DoubleValueView(1.0)),
            NativeTypeId::For<DoubleValueView>());
  EXPECT_EQ(NativeTypeId::Of(ValueView(DoubleValueView(1.0))),
            NativeTypeId::For<DoubleValueView>());
}

TEST_P(DoubleValueViewTest, InstanceOf) {
  EXPECT_TRUE(InstanceOf<DoubleValueView>(DoubleValueView(1.0)));
  EXPECT_TRUE(InstanceOf<DoubleValueView>(ValueView(DoubleValueView(1.0))));
}

TEST_P(DoubleValueViewTest, Cast) {
  EXPECT_THAT(Cast<DoubleValueView>(DoubleValueView(1.0)),
              An<DoubleValueView>());
  EXPECT_THAT(Cast<DoubleValueView>(ValueView(DoubleValueView(1.0))),
              An<DoubleValueView>());
}

TEST_P(DoubleValueViewTest, As) {
  EXPECT_THAT(As<DoubleValueView>(DoubleValueView(1.0)), Ne(absl::nullopt));
  EXPECT_THAT(As<DoubleValueView>(ValueView(DoubleValueView(1.0))),
              Ne(absl::nullopt));
}

TEST_P(DoubleValueViewTest, Equality) {
  EXPECT_NE(DoubleValueView(DoubleValue(0.0)), 1.0);
  EXPECT_NE(1.0, DoubleValueView(0.0));
  EXPECT_NE(DoubleValueView(0.0), DoubleValueView(1.0));
  EXPECT_NE(DoubleValueView(0.0), DoubleValue(1.0));
  EXPECT_NE(DoubleValue(1.0), DoubleValueView(0.0));
}

INSTANTIATE_TEST_SUITE_P(
    DoubleValueViewTest, DoubleValueViewTest,
    ::testing::Combine(::testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting)),
    DoubleValueViewTest::ToString);

}  // namespace
}  // namespace cel
