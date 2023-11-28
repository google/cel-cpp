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

TEST(UnknownValue, Kind) {
  EXPECT_EQ(UnknownValue().kind(), UnknownValue::kKind);
  EXPECT_EQ(Value(UnknownValue()).kind(), UnknownValue::kKind);
}

TEST(UnknownValue, Type) {
  EXPECT_EQ(UnknownValue().type(), UnknownType());
  EXPECT_EQ(Value(UnknownValue()).type(), UnknownType());
}

TEST(UnknownValue, DebugString) {
  {
    std::ostringstream out;
    out << UnknownValue();
    EXPECT_EQ(out.str(), "");
  }
  {
    std::ostringstream out;
    out << Value(UnknownValue());
    EXPECT_EQ(out.str(), "");
  }
}

TEST(UnknownValue, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(UnknownValue()),
            NativeTypeId::For<UnknownValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(UnknownValue())),
            NativeTypeId::For<UnknownValue>());
}

TEST(UnknownValue, InstanceOf) {
  EXPECT_TRUE(InstanceOf<UnknownValue>(UnknownValue()));
  EXPECT_TRUE(InstanceOf<UnknownValue>(Value(UnknownValue())));
}

TEST(UnknownValue, Cast) {
  EXPECT_THAT(Cast<UnknownValue>(UnknownValue()), An<UnknownValue>());
  EXPECT_THAT(Cast<UnknownValue>(Value(UnknownValue())), An<UnknownValue>());
}

TEST(UnknownValue, As) {
  EXPECT_THAT(As<UnknownValue>(UnknownValue()), Ne(absl::nullopt));
  EXPECT_THAT(As<UnknownValue>(Value(UnknownValue())), Ne(absl::nullopt));
}

TEST(UnknownValueView, Kind) {
  EXPECT_EQ(UnknownValueView().kind(), UnknownValueView::kKind);
  EXPECT_EQ(ValueView(UnknownValueView()).kind(), UnknownValueView::kKind);
}

TEST(UnknownValueView, Type) {
  EXPECT_EQ(UnknownValueView().type(), UnknownType());
  EXPECT_EQ(ValueView(UnknownValueView()).type(), UnknownType());
}

TEST(UnknownValueView, DebugString) {
  {
    std::ostringstream out;
    out << UnknownValueView();
    EXPECT_EQ(out.str(), "");
  }
  {
    std::ostringstream out;
    out << ValueView(UnknownValueView());
    EXPECT_EQ(out.str(), "");
  }
}

TEST(UnknownValueView, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(UnknownValueView()),
            NativeTypeId::For<UnknownValueView>());
  EXPECT_EQ(NativeTypeId::Of(ValueView(UnknownValueView())),
            NativeTypeId::For<UnknownValueView>());
}

TEST(UnknownValueView, InstanceOf) {
  EXPECT_TRUE(InstanceOf<UnknownValueView>(UnknownValueView()));
  EXPECT_TRUE(InstanceOf<UnknownValueView>(ValueView(UnknownValueView())));
}

TEST(UnknownValueView, Cast) {
  EXPECT_THAT(Cast<UnknownValueView>(UnknownValueView()),
              An<UnknownValueView>());
  EXPECT_THAT(Cast<UnknownValueView>(ValueView(UnknownValueView())),
              An<UnknownValueView>());
}

TEST(UnknownValueView, As) {
  EXPECT_THAT(As<UnknownValueView>(UnknownValueView()), Ne(absl::nullopt));
  EXPECT_THAT(As<UnknownValueView>(ValueView(UnknownValueView())),
              Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
