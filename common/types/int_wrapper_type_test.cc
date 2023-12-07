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

#include "absl/hash/hash.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/native_type.h"
#include "common/type.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::An;
using testing::Ne;

TEST(IntWrapperType, Kind) {
  EXPECT_EQ(IntWrapperType().kind(), IntWrapperType::kKind);
  EXPECT_EQ(Type(IntWrapperType()).kind(), IntWrapperType::kKind);
}

TEST(IntWrapperType, Name) {
  EXPECT_EQ(IntWrapperType().name(), IntWrapperType::kName);
  EXPECT_EQ(Type(IntWrapperType()).name(), IntWrapperType::kName);
}

TEST(IntWrapperType, DebugString) {
  {
    std::ostringstream out;
    out << IntWrapperType();
    EXPECT_EQ(out.str(), IntWrapperType::kName);
  }
  {
    std::ostringstream out;
    out << Type(IntWrapperType());
    EXPECT_EQ(out.str(), IntWrapperType::kName);
  }
}

TEST(IntWrapperType, Hash) {
  EXPECT_EQ(absl::HashOf(IntWrapperType()), absl::HashOf(IntWrapperType()));
}

TEST(IntWrapperType, Equal) {
  EXPECT_EQ(IntWrapperType(), IntWrapperType());
  EXPECT_EQ(Type(IntWrapperType()), IntWrapperType());
  EXPECT_EQ(IntWrapperType(), Type(IntWrapperType()));
  EXPECT_EQ(Type(IntWrapperType()), Type(IntWrapperType()));
}

TEST(IntWrapperType, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(IntWrapperType()),
            NativeTypeId::For<IntWrapperType>());
  EXPECT_EQ(NativeTypeId::Of(Type(IntWrapperType())),
            NativeTypeId::For<IntWrapperType>());
}

TEST(IntWrapperType, InstanceOf) {
  EXPECT_TRUE(InstanceOf<IntWrapperType>(IntWrapperType()));
  EXPECT_TRUE(InstanceOf<IntWrapperType>(Type(IntWrapperType())));
}

TEST(IntWrapperType, Cast) {
  EXPECT_THAT(Cast<IntWrapperType>(IntWrapperType()), An<IntWrapperType>());
  EXPECT_THAT(Cast<IntWrapperType>(Type(IntWrapperType())),
              An<IntWrapperType>());
}

TEST(IntWrapperType, As) {
  EXPECT_THAT(As<IntWrapperType>(IntWrapperType()), Ne(absl::nullopt));
  EXPECT_THAT(As<IntWrapperType>(Type(IntWrapperType())), Ne(absl::nullopt));
}

TEST(IntWrapperTypeView, Kind) {
  EXPECT_EQ(IntWrapperTypeView().kind(), IntWrapperTypeView::kKind);
  EXPECT_EQ(TypeView(IntWrapperTypeView()).kind(), IntWrapperTypeView::kKind);
}

TEST(IntWrapperTypeView, Name) {
  EXPECT_EQ(IntWrapperTypeView().name(), IntWrapperTypeView::kName);
  EXPECT_EQ(TypeView(IntWrapperTypeView()).name(), IntWrapperTypeView::kName);
}

TEST(IntWrapperTypeView, DebugString) {
  {
    std::ostringstream out;
    out << IntWrapperTypeView();
    EXPECT_EQ(out.str(), IntWrapperTypeView::kName);
  }
  {
    std::ostringstream out;
    out << TypeView(IntWrapperTypeView());
    EXPECT_EQ(out.str(), IntWrapperTypeView::kName);
  }
}

TEST(IntWrapperTypeView, Hash) {
  EXPECT_EQ(absl::HashOf(IntWrapperTypeView()),
            absl::HashOf(IntWrapperTypeView()));
  EXPECT_EQ(absl::HashOf(IntWrapperTypeView()), absl::HashOf(IntWrapperType()));
}

TEST(IntWrapperTypeView, Equal) {
  EXPECT_EQ(IntWrapperTypeView(), IntWrapperTypeView());
  EXPECT_EQ(TypeView(IntWrapperTypeView()), IntWrapperTypeView());
  EXPECT_EQ(IntWrapperTypeView(), TypeView(IntWrapperTypeView()));
  EXPECT_EQ(TypeView(IntWrapperTypeView()), TypeView(IntWrapperTypeView()));
  EXPECT_EQ(IntWrapperTypeView(), IntWrapperType());
  EXPECT_EQ(TypeView(IntWrapperTypeView()), IntWrapperType());
  EXPECT_EQ(TypeView(IntWrapperTypeView()), Type(IntWrapperType()));
  EXPECT_EQ(IntWrapperType(), IntWrapperTypeView());
  EXPECT_EQ(IntWrapperType(), IntWrapperTypeView());
  EXPECT_EQ(IntWrapperType(), TypeView(IntWrapperTypeView()));
  EXPECT_EQ(Type(IntWrapperType()), TypeView(IntWrapperTypeView()));
  EXPECT_EQ(IntWrapperTypeView(), IntWrapperType());
}

TEST(IntWrapperTypeView, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(IntWrapperTypeView()),
            NativeTypeId::For<IntWrapperTypeView>());
  EXPECT_EQ(NativeTypeId::Of(TypeView(IntWrapperTypeView())),
            NativeTypeId::For<IntWrapperTypeView>());
}

TEST(IntWrapperTypeView, InstanceOf) {
  EXPECT_TRUE(InstanceOf<IntWrapperTypeView>(IntWrapperTypeView()));
  EXPECT_TRUE(InstanceOf<IntWrapperTypeView>(TypeView(IntWrapperTypeView())));
}

TEST(IntWrapperTypeView, Cast) {
  EXPECT_THAT(Cast<IntWrapperTypeView>(IntWrapperTypeView()),
              An<IntWrapperTypeView>());
  EXPECT_THAT(Cast<IntWrapperTypeView>(TypeView(IntWrapperTypeView())),
              An<IntWrapperTypeView>());
}

TEST(IntWrapperTypeView, As) {
  EXPECT_THAT(As<IntWrapperTypeView>(IntWrapperTypeView()), Ne(absl::nullopt));
  EXPECT_THAT(As<IntWrapperTypeView>(TypeView(IntWrapperTypeView())),
              Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
