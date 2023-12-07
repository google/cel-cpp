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

TEST(UintWrapperType, Kind) {
  EXPECT_EQ(UintWrapperType().kind(), UintWrapperType::kKind);
  EXPECT_EQ(Type(UintWrapperType()).kind(), UintWrapperType::kKind);
}

TEST(UintWrapperType, Name) {
  EXPECT_EQ(UintWrapperType().name(), UintWrapperType::kName);
  EXPECT_EQ(Type(UintWrapperType()).name(), UintWrapperType::kName);
}

TEST(UintWrapperType, DebugString) {
  {
    std::ostringstream out;
    out << UintWrapperType();
    EXPECT_EQ(out.str(), UintWrapperType::kName);
  }
  {
    std::ostringstream out;
    out << Type(UintWrapperType());
    EXPECT_EQ(out.str(), UintWrapperType::kName);
  }
}

TEST(UintWrapperType, Hash) {
  EXPECT_EQ(absl::HashOf(UintWrapperType()), absl::HashOf(UintWrapperType()));
}

TEST(UintWrapperType, Equal) {
  EXPECT_EQ(UintWrapperType(), UintWrapperType());
  EXPECT_EQ(Type(UintWrapperType()), UintWrapperType());
  EXPECT_EQ(UintWrapperType(), Type(UintWrapperType()));
  EXPECT_EQ(Type(UintWrapperType()), Type(UintWrapperType()));
}

TEST(UintWrapperType, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(UintWrapperType()),
            NativeTypeId::For<UintWrapperType>());
  EXPECT_EQ(NativeTypeId::Of(Type(UintWrapperType())),
            NativeTypeId::For<UintWrapperType>());
}

TEST(UintWrapperType, InstanceOf) {
  EXPECT_TRUE(InstanceOf<UintWrapperType>(UintWrapperType()));
  EXPECT_TRUE(InstanceOf<UintWrapperType>(Type(UintWrapperType())));
}

TEST(UintWrapperType, Cast) {
  EXPECT_THAT(Cast<UintWrapperType>(UintWrapperType()), An<UintWrapperType>());
  EXPECT_THAT(Cast<UintWrapperType>(Type(UintWrapperType())),
              An<UintWrapperType>());
}

TEST(UintWrapperType, As) {
  EXPECT_THAT(As<UintWrapperType>(UintWrapperType()), Ne(absl::nullopt));
  EXPECT_THAT(As<UintWrapperType>(Type(UintWrapperType())), Ne(absl::nullopt));
}

TEST(UintWrapperTypeView, Kind) {
  EXPECT_EQ(UintWrapperTypeView().kind(), UintWrapperTypeView::kKind);
  EXPECT_EQ(TypeView(UintWrapperTypeView()).kind(), UintWrapperTypeView::kKind);
}

TEST(UintWrapperTypeView, Name) {
  EXPECT_EQ(UintWrapperTypeView().name(), UintWrapperTypeView::kName);
  EXPECT_EQ(TypeView(UintWrapperTypeView()).name(), UintWrapperTypeView::kName);
}

TEST(UintWrapperTypeView, DebugString) {
  {
    std::ostringstream out;
    out << UintWrapperTypeView();
    EXPECT_EQ(out.str(), UintWrapperTypeView::kName);
  }
  {
    std::ostringstream out;
    out << TypeView(UintWrapperTypeView());
    EXPECT_EQ(out.str(), UintWrapperTypeView::kName);
  }
}

TEST(UintWrapperTypeView, Hash) {
  EXPECT_EQ(absl::HashOf(UintWrapperTypeView()),
            absl::HashOf(UintWrapperTypeView()));
  EXPECT_EQ(absl::HashOf(UintWrapperTypeView()),
            absl::HashOf(UintWrapperType()));
}

TEST(UintWrapperTypeView, Equal) {
  EXPECT_EQ(UintWrapperTypeView(), UintWrapperTypeView());
  EXPECT_EQ(TypeView(UintWrapperTypeView()), UintWrapperTypeView());
  EXPECT_EQ(UintWrapperTypeView(), TypeView(UintWrapperTypeView()));
  EXPECT_EQ(TypeView(UintWrapperTypeView()), TypeView(UintWrapperTypeView()));
  EXPECT_EQ(UintWrapperTypeView(), UintWrapperType());
  EXPECT_EQ(TypeView(UintWrapperTypeView()), UintWrapperType());
  EXPECT_EQ(TypeView(UintWrapperTypeView()), Type(UintWrapperType()));
  EXPECT_EQ(UintWrapperType(), UintWrapperTypeView());
  EXPECT_EQ(UintWrapperType(), UintWrapperTypeView());
  EXPECT_EQ(UintWrapperType(), TypeView(UintWrapperTypeView()));
  EXPECT_EQ(Type(UintWrapperType()), TypeView(UintWrapperTypeView()));
  EXPECT_EQ(UintWrapperTypeView(), UintWrapperType());
}

TEST(UintWrapperTypeView, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(UintWrapperTypeView()),
            NativeTypeId::For<UintWrapperTypeView>());
  EXPECT_EQ(NativeTypeId::Of(TypeView(UintWrapperTypeView())),
            NativeTypeId::For<UintWrapperTypeView>());
}

TEST(UintWrapperTypeView, InstanceOf) {
  EXPECT_TRUE(InstanceOf<UintWrapperTypeView>(UintWrapperTypeView()));
  EXPECT_TRUE(InstanceOf<UintWrapperTypeView>(TypeView(UintWrapperTypeView())));
}

TEST(UintWrapperTypeView, Cast) {
  EXPECT_THAT(Cast<UintWrapperTypeView>(UintWrapperTypeView()),
              An<UintWrapperTypeView>());
  EXPECT_THAT(Cast<UintWrapperTypeView>(TypeView(UintWrapperTypeView())),
              An<UintWrapperTypeView>());
}

TEST(UintWrapperTypeView, As) {
  EXPECT_THAT(As<UintWrapperTypeView>(UintWrapperTypeView()),
              Ne(absl::nullopt));
  EXPECT_THAT(As<UintWrapperTypeView>(TypeView(UintWrapperTypeView())),
              Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
