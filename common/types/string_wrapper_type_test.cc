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

TEST(StringWrapperType, Kind) {
  EXPECT_EQ(StringWrapperType().kind(), StringWrapperType::kKind);
  EXPECT_EQ(Type(StringWrapperType()).kind(), StringWrapperType::kKind);
}

TEST(StringWrapperType, Name) {
  EXPECT_EQ(StringWrapperType().name(), StringWrapperType::kName);
  EXPECT_EQ(Type(StringWrapperType()).name(), StringWrapperType::kName);
}

TEST(StringWrapperType, DebugString) {
  {
    std::ostringstream out;
    out << StringWrapperType();
    EXPECT_EQ(out.str(), StringWrapperType::kName);
  }
  {
    std::ostringstream out;
    out << Type(StringWrapperType());
    EXPECT_EQ(out.str(), StringWrapperType::kName);
  }
}

TEST(StringWrapperType, Hash) {
  EXPECT_EQ(absl::HashOf(StringWrapperType()),
            absl::HashOf(StringWrapperType()));
}

TEST(StringWrapperType, Equal) {
  EXPECT_EQ(StringWrapperType(), StringWrapperType());
  EXPECT_EQ(Type(StringWrapperType()), StringWrapperType());
  EXPECT_EQ(StringWrapperType(), Type(StringWrapperType()));
  EXPECT_EQ(Type(StringWrapperType()), Type(StringWrapperType()));
}

TEST(StringWrapperType, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(StringWrapperType()),
            NativeTypeId::For<StringWrapperType>());
  EXPECT_EQ(NativeTypeId::Of(Type(StringWrapperType())),
            NativeTypeId::For<StringWrapperType>());
}

TEST(StringWrapperType, InstanceOf) {
  EXPECT_TRUE(InstanceOf<StringWrapperType>(StringWrapperType()));
  EXPECT_TRUE(InstanceOf<StringWrapperType>(Type(StringWrapperType())));
}

TEST(StringWrapperType, Cast) {
  EXPECT_THAT(Cast<StringWrapperType>(StringWrapperType()),
              An<StringWrapperType>());
  EXPECT_THAT(Cast<StringWrapperType>(Type(StringWrapperType())),
              An<StringWrapperType>());
}

TEST(StringWrapperType, As) {
  EXPECT_THAT(As<StringWrapperType>(StringWrapperType()), Ne(absl::nullopt));
  EXPECT_THAT(As<StringWrapperType>(Type(StringWrapperType())),
              Ne(absl::nullopt));
}

TEST(StringWrapperTypeView, Kind) {
  EXPECT_EQ(StringWrapperTypeView().kind(), StringWrapperTypeView::kKind);
  EXPECT_EQ(TypeView(StringWrapperTypeView()).kind(),
            StringWrapperTypeView::kKind);
}

TEST(StringWrapperTypeView, Name) {
  EXPECT_EQ(StringWrapperTypeView().name(), StringWrapperTypeView::kName);
  EXPECT_EQ(TypeView(StringWrapperTypeView()).name(),
            StringWrapperTypeView::kName);
}

TEST(StringWrapperTypeView, DebugString) {
  {
    std::ostringstream out;
    out << StringWrapperTypeView();
    EXPECT_EQ(out.str(), StringWrapperTypeView::kName);
  }
  {
    std::ostringstream out;
    out << TypeView(StringWrapperTypeView());
    EXPECT_EQ(out.str(), StringWrapperTypeView::kName);
  }
}

TEST(StringWrapperTypeView, Hash) {
  EXPECT_EQ(absl::HashOf(StringWrapperTypeView()),
            absl::HashOf(StringWrapperTypeView()));
  EXPECT_EQ(absl::HashOf(StringWrapperTypeView()),
            absl::HashOf(StringWrapperType()));
}

TEST(StringWrapperTypeView, Equal) {
  EXPECT_EQ(StringWrapperTypeView(), StringWrapperTypeView());
  EXPECT_EQ(TypeView(StringWrapperTypeView()), StringWrapperTypeView());
  EXPECT_EQ(StringWrapperTypeView(), TypeView(StringWrapperTypeView()));
  EXPECT_EQ(TypeView(StringWrapperTypeView()),
            TypeView(StringWrapperTypeView()));
  EXPECT_EQ(StringWrapperTypeView(), StringWrapperType());
  EXPECT_EQ(TypeView(StringWrapperTypeView()), StringWrapperType());
  EXPECT_EQ(TypeView(StringWrapperTypeView()), Type(StringWrapperType()));
  EXPECT_EQ(StringWrapperType(), StringWrapperTypeView());
  EXPECT_EQ(StringWrapperType(), StringWrapperTypeView());
  EXPECT_EQ(StringWrapperType(), TypeView(StringWrapperTypeView()));
  EXPECT_EQ(Type(StringWrapperType()), TypeView(StringWrapperTypeView()));
  EXPECT_EQ(StringWrapperTypeView(), StringWrapperType());
}

TEST(StringWrapperTypeView, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(StringWrapperTypeView()),
            NativeTypeId::For<StringWrapperTypeView>());
  EXPECT_EQ(NativeTypeId::Of(TypeView(StringWrapperTypeView())),
            NativeTypeId::For<StringWrapperTypeView>());
}

TEST(StringWrapperTypeView, InstanceOf) {
  EXPECT_TRUE(InstanceOf<StringWrapperTypeView>(StringWrapperTypeView()));
  EXPECT_TRUE(
      InstanceOf<StringWrapperTypeView>(TypeView(StringWrapperTypeView())));
}

TEST(StringWrapperTypeView, Cast) {
  EXPECT_THAT(Cast<StringWrapperTypeView>(StringWrapperTypeView()),
              An<StringWrapperTypeView>());
  EXPECT_THAT(Cast<StringWrapperTypeView>(TypeView(StringWrapperTypeView())),
              An<StringWrapperTypeView>());
}

TEST(StringWrapperTypeView, As) {
  EXPECT_THAT(As<StringWrapperTypeView>(StringWrapperTypeView()),
              Ne(absl::nullopt));
  EXPECT_THAT(As<StringWrapperTypeView>(TypeView(StringWrapperTypeView())),
              Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
