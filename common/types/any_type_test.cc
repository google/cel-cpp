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

TEST(AnyType, Kind) {
  EXPECT_EQ(AnyType().kind(), AnyType::kKind);
  EXPECT_EQ(Type(AnyType()).kind(), AnyType::kKind);
}

TEST(AnyType, Name) {
  EXPECT_EQ(AnyType().name(), AnyType::kName);
  EXPECT_EQ(Type(AnyType()).name(), AnyType::kName);
}

TEST(AnyType, DebugString) {
  {
    std::ostringstream out;
    out << AnyType();
    EXPECT_EQ(out.str(), AnyType::kName);
  }
  {
    std::ostringstream out;
    out << Type(AnyType());
    EXPECT_EQ(out.str(), AnyType::kName);
  }
}

TEST(AnyType, Hash) {
  EXPECT_EQ(absl::HashOf(AnyType()), absl::HashOf(AnyType()));
}

TEST(AnyType, Equal) {
  EXPECT_EQ(AnyType(), AnyType());
  EXPECT_EQ(Type(AnyType()), AnyType());
  EXPECT_EQ(AnyType(), Type(AnyType()));
  EXPECT_EQ(Type(AnyType()), Type(AnyType()));
}

TEST(AnyType, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(AnyType()), NativeTypeId::For<AnyType>());
  EXPECT_EQ(NativeTypeId::Of(Type(AnyType())), NativeTypeId::For<AnyType>());
}

TEST(AnyType, InstanceOf) {
  EXPECT_TRUE(InstanceOf<AnyType>(AnyType()));
  EXPECT_TRUE(InstanceOf<AnyType>(Type(AnyType())));
}

TEST(AnyType, Cast) {
  EXPECT_THAT(Cast<AnyType>(AnyType()), An<AnyType>());
  EXPECT_THAT(Cast<AnyType>(Type(AnyType())), An<AnyType>());
}

TEST(AnyType, As) {
  EXPECT_THAT(As<AnyType>(AnyType()), Ne(absl::nullopt));
  EXPECT_THAT(As<AnyType>(Type(AnyType())), Ne(absl::nullopt));
}

TEST(AnyTypeView, Kind) {
  EXPECT_EQ(AnyTypeView().kind(), AnyTypeView::kKind);
  EXPECT_EQ(TypeView(AnyTypeView()).kind(), AnyTypeView::kKind);
}

TEST(AnyTypeView, Name) {
  EXPECT_EQ(AnyTypeView().name(), AnyTypeView::kName);
  EXPECT_EQ(TypeView(AnyTypeView()).name(), AnyTypeView::kName);
}

TEST(AnyTypeView, DebugString) {
  {
    std::ostringstream out;
    out << AnyTypeView();
    EXPECT_EQ(out.str(), AnyTypeView::kName);
  }
  {
    std::ostringstream out;
    out << TypeView(AnyTypeView());
    EXPECT_EQ(out.str(), AnyTypeView::kName);
  }
}

TEST(AnyTypeView, Hash) {
  EXPECT_EQ(absl::HashOf(AnyTypeView()), absl::HashOf(AnyTypeView()));
  EXPECT_EQ(absl::HashOf(AnyTypeView()), absl::HashOf(AnyType()));
}

TEST(AnyTypeView, Equal) {
  EXPECT_EQ(AnyTypeView(), AnyTypeView());
  EXPECT_EQ(TypeView(AnyTypeView()), AnyTypeView());
  EXPECT_EQ(AnyTypeView(), TypeView(AnyTypeView()));
  EXPECT_EQ(TypeView(AnyTypeView()), TypeView(AnyTypeView()));
  EXPECT_EQ(AnyTypeView(), AnyType());
  EXPECT_EQ(TypeView(AnyTypeView()), AnyType());
  EXPECT_EQ(TypeView(AnyTypeView()), Type(AnyType()));
  EXPECT_EQ(AnyType(), AnyTypeView());
  EXPECT_EQ(AnyType(), AnyTypeView());
  EXPECT_EQ(AnyType(), TypeView(AnyTypeView()));
  EXPECT_EQ(Type(AnyType()), TypeView(AnyTypeView()));
  EXPECT_EQ(AnyTypeView(), AnyType());
}

TEST(AnyTypeView, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(AnyTypeView()), NativeTypeId::For<AnyTypeView>());
  EXPECT_EQ(NativeTypeId::Of(TypeView(AnyTypeView())),
            NativeTypeId::For<AnyTypeView>());
}

TEST(AnyTypeView, InstanceOf) {
  EXPECT_TRUE(InstanceOf<AnyTypeView>(AnyTypeView()));
  EXPECT_TRUE(InstanceOf<AnyTypeView>(TypeView(AnyTypeView())));
}

TEST(AnyTypeView, Cast) {
  EXPECT_THAT(Cast<AnyTypeView>(AnyTypeView()), An<AnyTypeView>());
  EXPECT_THAT(Cast<AnyTypeView>(TypeView(AnyTypeView())), An<AnyTypeView>());
}

TEST(AnyTypeView, As) {
  EXPECT_THAT(As<AnyTypeView>(AnyTypeView()), Ne(absl::nullopt));
  EXPECT_THAT(As<AnyTypeView>(TypeView(AnyTypeView())), Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
