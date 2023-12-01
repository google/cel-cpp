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

TEST(DoubleValue, Kind) {
  EXPECT_EQ(DoubleValue(1.0).kind(), DoubleValue::kKind);
  EXPECT_EQ(Value(DoubleValue(1.0)).kind(), DoubleValue::kKind);
}

TEST(DoubleValue, Type) {
  EXPECT_EQ(DoubleValue(1.0).type(), DoubleType());
  EXPECT_EQ(Value(DoubleValue(1.0)).type(), DoubleType());
}

TEST(DoubleValue, DebugString) {
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

TEST(DoubleValue, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(DoubleValue(1.0)),
            NativeTypeId::For<DoubleValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(DoubleValue(1.0))),
            NativeTypeId::For<DoubleValue>());
}

TEST(DoubleValue, InstanceOf) {
  EXPECT_TRUE(InstanceOf<DoubleValue>(DoubleValue(1.0)));
  EXPECT_TRUE(InstanceOf<DoubleValue>(Value(DoubleValue(1.0))));
}

TEST(DoubleValue, Cast) {
  EXPECT_THAT(Cast<DoubleValue>(DoubleValue(1.0)), An<DoubleValue>());
  EXPECT_THAT(Cast<DoubleValue>(Value(DoubleValue(1.0))), An<DoubleValue>());
}

TEST(DoubleValue, As) {
  EXPECT_THAT(As<DoubleValue>(DoubleValue(1.0)), Ne(absl::nullopt));
  EXPECT_THAT(As<DoubleValue>(Value(DoubleValue(1.0))), Ne(absl::nullopt));
}

TEST(DoubleValue, Equality) {
  EXPECT_NE(DoubleValue(0.0), 1.0);
  EXPECT_NE(1.0, DoubleValue(0.0));
  EXPECT_NE(DoubleValue(0.0), DoubleValue(1.0));
}

TEST(DoubleValueView, Kind) {
  EXPECT_EQ(DoubleValueView(1.0).kind(), DoubleValueView::kKind);
  EXPECT_EQ(ValueView(DoubleValueView(1.0)).kind(), DoubleValueView::kKind);
}

TEST(DoubleValueView, Type) {
  EXPECT_EQ(DoubleValueView(1.0).type(), DoubleType());
  EXPECT_EQ(ValueView(DoubleValueView(1.0)).type(), DoubleType());
}

TEST(DoubleValueView, DebugString) {
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

TEST(DoubleValueView, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(DoubleValueView(1.0)),
            NativeTypeId::For<DoubleValueView>());
  EXPECT_EQ(NativeTypeId::Of(ValueView(DoubleValueView(1.0))),
            NativeTypeId::For<DoubleValueView>());
}

TEST(DoubleValueView, InstanceOf) {
  EXPECT_TRUE(InstanceOf<DoubleValueView>(DoubleValueView(1.0)));
  EXPECT_TRUE(InstanceOf<DoubleValueView>(ValueView(DoubleValueView(1.0))));
}

TEST(DoubleValueView, Cast) {
  EXPECT_THAT(Cast<DoubleValueView>(DoubleValueView(1.0)),
              An<DoubleValueView>());
  EXPECT_THAT(Cast<DoubleValueView>(ValueView(DoubleValueView(1.0))),
              An<DoubleValueView>());
}

TEST(DoubleValueView, As) {
  EXPECT_THAT(As<DoubleValueView>(DoubleValueView(1.0)), Ne(absl::nullopt));
  EXPECT_THAT(As<DoubleValueView>(ValueView(DoubleValueView(1.0))),
              Ne(absl::nullopt));
}

TEST(DoubleValueView, Equality) {
  EXPECT_NE(DoubleValueView(DoubleValue(0.0)), 1.0);
  EXPECT_NE(1.0, DoubleValueView(0.0));
  EXPECT_NE(DoubleValueView(0.0), DoubleValueView(1.0));
  EXPECT_NE(DoubleValueView(0.0), DoubleValue(1.0));
  EXPECT_NE(DoubleValue(1.0), DoubleValueView(0.0));
}

}  // namespace
}  // namespace cel
