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

TEST(DoubleWrapperType, Kind) {
  EXPECT_EQ(DoubleWrapperType().kind(), DoubleWrapperType::kKind);
  EXPECT_EQ(Type(DoubleWrapperType()).kind(), DoubleWrapperType::kKind);
}

TEST(DoubleWrapperType, Name) {
  EXPECT_EQ(DoubleWrapperType().name(), DoubleWrapperType::kName);
  EXPECT_EQ(Type(DoubleWrapperType()).name(), DoubleWrapperType::kName);
}

TEST(DoubleWrapperType, DebugString) {
  {
    std::ostringstream out;
    out << DoubleWrapperType();
    EXPECT_EQ(out.str(), DoubleWrapperType::kName);
  }
  {
    std::ostringstream out;
    out << Type(DoubleWrapperType());
    EXPECT_EQ(out.str(), DoubleWrapperType::kName);
  }
}

TEST(DoubleWrapperType, Hash) {
  EXPECT_EQ(absl::HashOf(DoubleWrapperType()),
            absl::HashOf(DoubleWrapperType()));
  EXPECT_EQ(absl::HashOf(Type(DoubleWrapperType())),
            absl::HashOf(DoubleWrapperType()));
}

TEST(DoubleWrapperType, Equal) {
  EXPECT_EQ(DoubleWrapperType(), DoubleWrapperType());
  EXPECT_EQ(Type(DoubleWrapperType()), DoubleWrapperType());
  EXPECT_EQ(DoubleWrapperType(), Type(DoubleWrapperType()));
  EXPECT_EQ(Type(DoubleWrapperType()), Type(DoubleWrapperType()));
}

TEST(DoubleWrapperType, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(DoubleWrapperType()),
            NativeTypeId::For<DoubleWrapperType>());
  EXPECT_EQ(NativeTypeId::Of(Type(DoubleWrapperType())),
            NativeTypeId::For<DoubleWrapperType>());
}

TEST(DoubleWrapperType, InstanceOf) {
  EXPECT_TRUE(InstanceOf<DoubleWrapperType>(DoubleWrapperType()));
  EXPECT_TRUE(InstanceOf<DoubleWrapperType>(Type(DoubleWrapperType())));
}

TEST(DoubleWrapperType, Cast) {
  EXPECT_THAT(Cast<DoubleWrapperType>(DoubleWrapperType()),
              An<DoubleWrapperType>());
  EXPECT_THAT(Cast<DoubleWrapperType>(Type(DoubleWrapperType())),
              An<DoubleWrapperType>());
}

TEST(DoubleWrapperType, As) {
  EXPECT_THAT(As<DoubleWrapperType>(DoubleWrapperType()), Ne(absl::nullopt));
  EXPECT_THAT(As<DoubleWrapperType>(Type(DoubleWrapperType())),
              Ne(absl::nullopt));
}

TEST(DoubleWrapperTypeView, Kind) {
  EXPECT_EQ(DoubleWrapperTypeView().kind(), DoubleWrapperTypeView::kKind);
  EXPECT_EQ(TypeView(DoubleWrapperTypeView()).kind(),
            DoubleWrapperTypeView::kKind);
}

TEST(DoubleWrapperTypeView, Name) {
  EXPECT_EQ(DoubleWrapperTypeView().name(), DoubleWrapperTypeView::kName);
  EXPECT_EQ(TypeView(DoubleWrapperTypeView()).name(),
            DoubleWrapperTypeView::kName);
}

TEST(DoubleWrapperTypeView, DebugString) {
  {
    std::ostringstream out;
    out << DoubleWrapperTypeView();
    EXPECT_EQ(out.str(), DoubleWrapperTypeView::kName);
  }
  {
    std::ostringstream out;
    out << TypeView(DoubleWrapperTypeView());
    EXPECT_EQ(out.str(), DoubleWrapperTypeView::kName);
  }
}

TEST(DoubleWrapperTypeView, Hash) {
  EXPECT_EQ(absl::HashOf(DoubleWrapperTypeView()),
            absl::HashOf(DoubleWrapperTypeView()));
  EXPECT_EQ(absl::HashOf(TypeView(DoubleWrapperTypeView())),
            absl::HashOf(DoubleWrapperTypeView()));
  EXPECT_EQ(absl::HashOf(DoubleWrapperTypeView()),
            absl::HashOf(DoubleWrapperType()));
  EXPECT_EQ(absl::HashOf(TypeView(DoubleWrapperTypeView())),
            absl::HashOf(DoubleWrapperType()));
}

TEST(DoubleWrapperTypeView, Equal) {
  EXPECT_EQ(DoubleWrapperTypeView(), DoubleWrapperTypeView());
  EXPECT_EQ(TypeView(DoubleWrapperTypeView()), DoubleWrapperTypeView());
  EXPECT_EQ(DoubleWrapperTypeView(), TypeView(DoubleWrapperTypeView()));
  EXPECT_EQ(TypeView(DoubleWrapperTypeView()),
            TypeView(DoubleWrapperTypeView()));
  EXPECT_EQ(DoubleWrapperTypeView(), DoubleWrapperType());
  EXPECT_EQ(TypeView(DoubleWrapperTypeView()), DoubleWrapperType());
  EXPECT_EQ(TypeView(DoubleWrapperTypeView()), Type(DoubleWrapperType()));
  EXPECT_EQ(DoubleWrapperType(), DoubleWrapperTypeView());
  EXPECT_EQ(DoubleWrapperType(), DoubleWrapperTypeView());
  EXPECT_EQ(DoubleWrapperType(), TypeView(DoubleWrapperTypeView()));
  EXPECT_EQ(Type(DoubleWrapperType()), TypeView(DoubleWrapperTypeView()));
  EXPECT_EQ(DoubleWrapperTypeView(), DoubleWrapperType());
}

TEST(DoubleWrapperTypeView, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(DoubleWrapperTypeView()),
            NativeTypeId::For<DoubleWrapperTypeView>());
  EXPECT_EQ(NativeTypeId::Of(TypeView(DoubleWrapperTypeView())),
            NativeTypeId::For<DoubleWrapperTypeView>());
}

TEST(DoubleWrapperTypeView, InstanceOf) {
  EXPECT_TRUE(InstanceOf<DoubleWrapperTypeView>(DoubleWrapperTypeView()));
  EXPECT_TRUE(
      InstanceOf<DoubleWrapperTypeView>(TypeView(DoubleWrapperTypeView())));
}

TEST(DoubleWrapperTypeView, Cast) {
  EXPECT_THAT(Cast<DoubleWrapperTypeView>(DoubleWrapperTypeView()),
              An<DoubleWrapperTypeView>());
  EXPECT_THAT(Cast<DoubleWrapperTypeView>(TypeView(DoubleWrapperTypeView())),
              An<DoubleWrapperTypeView>());
}

TEST(DoubleWrapperTypeView, As) {
  EXPECT_THAT(As<DoubleWrapperTypeView>(DoubleWrapperTypeView()),
              Ne(absl::nullopt));
  EXPECT_THAT(As<DoubleWrapperTypeView>(TypeView(DoubleWrapperTypeView())),
              Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
