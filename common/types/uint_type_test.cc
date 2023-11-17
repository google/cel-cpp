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

TEST(UintType, Kind) {
  EXPECT_EQ(UintType().kind(), UintType::kKind);
  EXPECT_EQ(Type(UintType()).kind(), UintType::kKind);
}

TEST(UintType, Name) {
  EXPECT_EQ(UintType().name(), UintType::kName);
  EXPECT_EQ(Type(UintType()).name(), UintType::kName);
}

TEST(UintType, DebugString) {
  {
    std::ostringstream out;
    out << UintType();
    EXPECT_EQ(out.str(), UintType::kName);
  }
  {
    std::ostringstream out;
    out << Type(UintType());
    EXPECT_EQ(out.str(), UintType::kName);
  }
}

TEST(UintType, Hash) {
  EXPECT_EQ(absl::HashOf(UintType()), absl::HashOf(UintType()));
  EXPECT_EQ(absl::HashOf(Type(UintType())), absl::HashOf(UintType()));
}

TEST(UintType, Equal) {
  EXPECT_EQ(UintType(), UintType());
  EXPECT_EQ(Type(UintType()), UintType());
  EXPECT_EQ(UintType(), Type(UintType()));
  EXPECT_EQ(Type(UintType()), Type(UintType()));
}

TEST(UintType, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(UintType()), NativeTypeId::For<UintType>());
  EXPECT_EQ(NativeTypeId::Of(Type(UintType())), NativeTypeId::For<UintType>());
}

TEST(UintType, InstanceOf) {
  EXPECT_TRUE(InstanceOf<UintType>(UintType()));
  EXPECT_TRUE(InstanceOf<UintType>(Type(UintType())));
}

TEST(UintType, Cast) {
  EXPECT_THAT(Cast<UintType>(UintType()), An<UintType>());
  EXPECT_THAT(Cast<UintType>(Type(UintType())), An<UintType>());
}

TEST(UintType, As) {
  EXPECT_THAT(As<UintType>(UintType()), Ne(absl::nullopt));
  EXPECT_THAT(As<UintType>(Type(UintType())), Ne(absl::nullopt));
}

TEST(UintTypeView, Kind) {
  EXPECT_EQ(UintTypeView().kind(), UintTypeView::kKind);
  EXPECT_EQ(TypeView(UintTypeView()).kind(), UintTypeView::kKind);
}

TEST(UintTypeView, Name) {
  EXPECT_EQ(UintTypeView().name(), UintTypeView::kName);
  EXPECT_EQ(TypeView(UintTypeView()).name(), UintTypeView::kName);
}

TEST(UintTypeView, DebugString) {
  {
    std::ostringstream out;
    out << UintTypeView();
    EXPECT_EQ(out.str(), UintTypeView::kName);
  }
  {
    std::ostringstream out;
    out << TypeView(UintTypeView());
    EXPECT_EQ(out.str(), UintTypeView::kName);
  }
}

TEST(UintTypeView, Hash) {
  EXPECT_EQ(absl::HashOf(UintTypeView()), absl::HashOf(UintTypeView()));
  EXPECT_EQ(absl::HashOf(TypeView(UintTypeView())),
            absl::HashOf(UintTypeView()));
  EXPECT_EQ(absl::HashOf(UintTypeView()), absl::HashOf(UintType()));
  EXPECT_EQ(absl::HashOf(TypeView(UintTypeView())), absl::HashOf(UintType()));
}

TEST(UintTypeView, Equal) {
  EXPECT_EQ(UintTypeView(), UintTypeView());
  EXPECT_EQ(TypeView(UintTypeView()), UintTypeView());
  EXPECT_EQ(UintTypeView(), TypeView(UintTypeView()));
  EXPECT_EQ(TypeView(UintTypeView()), TypeView(UintTypeView()));
  EXPECT_EQ(UintTypeView(), UintType());
  EXPECT_EQ(TypeView(UintTypeView()), UintType());
  EXPECT_EQ(TypeView(UintTypeView()), Type(UintType()));
  EXPECT_EQ(UintType(), UintTypeView());
  EXPECT_EQ(UintType(), UintTypeView());
  EXPECT_EQ(UintType(), TypeView(UintTypeView()));
  EXPECT_EQ(Type(UintType()), TypeView(UintTypeView()));
  EXPECT_EQ(UintTypeView(), UintType());
}

TEST(UintTypeView, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(UintTypeView()),
            NativeTypeId::For<UintTypeView>());
  EXPECT_EQ(NativeTypeId::Of(TypeView(UintTypeView())),
            NativeTypeId::For<UintTypeView>());
}

TEST(UintTypeView, InstanceOf) {
  EXPECT_TRUE(InstanceOf<UintTypeView>(UintTypeView()));
  EXPECT_TRUE(InstanceOf<UintTypeView>(TypeView(UintTypeView())));
}

TEST(UintTypeView, Cast) {
  EXPECT_THAT(Cast<UintTypeView>(UintTypeView()), An<UintTypeView>());
  EXPECT_THAT(Cast<UintTypeView>(TypeView(UintTypeView())), An<UintTypeView>());
}

TEST(UintTypeView, As) {
  EXPECT_THAT(As<UintTypeView>(UintTypeView()), Ne(absl::nullopt));
  EXPECT_THAT(As<UintTypeView>(TypeView(UintTypeView())), Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
