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

TEST(DoubleType, Kind) {
  EXPECT_EQ(DoubleType().kind(), DoubleType::kKind);
  EXPECT_EQ(Type(DoubleType()).kind(), DoubleType::kKind);
}

TEST(DoubleType, Name) {
  EXPECT_EQ(DoubleType().name(), DoubleType::kName);
  EXPECT_EQ(Type(DoubleType()).name(), DoubleType::kName);
}

TEST(DoubleType, DebugString) {
  {
    std::ostringstream out;
    out << DoubleType();
    EXPECT_EQ(out.str(), DoubleType::kName);
  }
  {
    std::ostringstream out;
    out << Type(DoubleType());
    EXPECT_EQ(out.str(), DoubleType::kName);
  }
}

TEST(DoubleType, Hash) {
  EXPECT_EQ(absl::HashOf(DoubleType()), absl::HashOf(DoubleType()));
  EXPECT_EQ(absl::HashOf(Type(DoubleType())), absl::HashOf(DoubleType()));
}

TEST(DoubleType, Equal) {
  EXPECT_EQ(DoubleType(), DoubleType());
  EXPECT_EQ(Type(DoubleType()), DoubleType());
  EXPECT_EQ(DoubleType(), Type(DoubleType()));
  EXPECT_EQ(Type(DoubleType()), Type(DoubleType()));
}

TEST(DoubleType, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(DoubleType()), NativeTypeId::For<DoubleType>());
  EXPECT_EQ(NativeTypeId::Of(Type(DoubleType())),
            NativeTypeId::For<DoubleType>());
}

TEST(DoubleType, InstanceOf) {
  EXPECT_TRUE(InstanceOf<DoubleType>(DoubleType()));
  EXPECT_TRUE(InstanceOf<DoubleType>(Type(DoubleType())));
}

TEST(DoubleType, Cast) {
  EXPECT_THAT(Cast<DoubleType>(DoubleType()), An<DoubleType>());
  EXPECT_THAT(Cast<DoubleType>(Type(DoubleType())), An<DoubleType>());
}

TEST(DoubleType, As) {
  EXPECT_THAT(As<DoubleType>(DoubleType()), Ne(absl::nullopt));
  EXPECT_THAT(As<DoubleType>(Type(DoubleType())), Ne(absl::nullopt));
}

TEST(DoubleTypeView, Kind) {
  EXPECT_EQ(DoubleTypeView().kind(), DoubleTypeView::kKind);
  EXPECT_EQ(TypeView(DoubleTypeView()).kind(), DoubleTypeView::kKind);
}

TEST(DoubleTypeView, Name) {
  EXPECT_EQ(DoubleTypeView().name(), DoubleTypeView::kName);
  EXPECT_EQ(TypeView(DoubleTypeView()).name(), DoubleTypeView::kName);
}

TEST(DoubleTypeView, DebugString) {
  {
    std::ostringstream out;
    out << DoubleTypeView();
    EXPECT_EQ(out.str(), DoubleTypeView::kName);
  }
  {
    std::ostringstream out;
    out << TypeView(DoubleTypeView());
    EXPECT_EQ(out.str(), DoubleTypeView::kName);
  }
}

TEST(DoubleTypeView, Hash) {
  EXPECT_EQ(absl::HashOf(DoubleTypeView()), absl::HashOf(DoubleTypeView()));
  EXPECT_EQ(absl::HashOf(TypeView(DoubleTypeView())),
            absl::HashOf(DoubleTypeView()));
  EXPECT_EQ(absl::HashOf(DoubleTypeView()), absl::HashOf(DoubleType()));
  EXPECT_EQ(absl::HashOf(TypeView(DoubleTypeView())),
            absl::HashOf(DoubleType()));
}

TEST(DoubleTypeView, Equal) {
  EXPECT_EQ(DoubleTypeView(), DoubleTypeView());
  EXPECT_EQ(TypeView(DoubleTypeView()), DoubleTypeView());
  EXPECT_EQ(DoubleTypeView(), TypeView(DoubleTypeView()));
  EXPECT_EQ(TypeView(DoubleTypeView()), TypeView(DoubleTypeView()));
  EXPECT_EQ(DoubleTypeView(), DoubleType());
  EXPECT_EQ(TypeView(DoubleTypeView()), DoubleType());
  EXPECT_EQ(TypeView(DoubleTypeView()), Type(DoubleType()));
  EXPECT_EQ(DoubleType(), DoubleTypeView());
  EXPECT_EQ(DoubleType(), DoubleTypeView());
  EXPECT_EQ(DoubleType(), TypeView(DoubleTypeView()));
  EXPECT_EQ(Type(DoubleType()), TypeView(DoubleTypeView()));
  EXPECT_EQ(DoubleTypeView(), DoubleType());
}

TEST(DoubleTypeView, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(DoubleTypeView()),
            NativeTypeId::For<DoubleTypeView>());
  EXPECT_EQ(NativeTypeId::Of(TypeView(DoubleTypeView())),
            NativeTypeId::For<DoubleTypeView>());
}

TEST(DoubleTypeView, InstanceOf) {
  EXPECT_TRUE(InstanceOf<DoubleTypeView>(DoubleTypeView()));
  EXPECT_TRUE(InstanceOf<DoubleTypeView>(TypeView(DoubleTypeView())));
}

TEST(DoubleTypeView, Cast) {
  EXPECT_THAT(Cast<DoubleTypeView>(DoubleTypeView()), An<DoubleTypeView>());
  EXPECT_THAT(Cast<DoubleTypeView>(TypeView(DoubleTypeView())),
              An<DoubleTypeView>());
}

TEST(DoubleTypeView, As) {
  EXPECT_THAT(As<DoubleTypeView>(DoubleTypeView()), Ne(absl::nullopt));
  EXPECT_THAT(As<DoubleTypeView>(TypeView(DoubleTypeView())),
              Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
