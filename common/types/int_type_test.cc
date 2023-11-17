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

TEST(IntType, Kind) {
  EXPECT_EQ(IntType().kind(), IntType::kKind);
  EXPECT_EQ(Type(IntType()).kind(), IntType::kKind);
}

TEST(IntType, Name) {
  EXPECT_EQ(IntType().name(), IntType::kName);
  EXPECT_EQ(Type(IntType()).name(), IntType::kName);
}

TEST(IntType, DebugString) {
  {
    std::ostringstream out;
    out << IntType();
    EXPECT_EQ(out.str(), IntType::kName);
  }
  {
    std::ostringstream out;
    out << Type(IntType());
    EXPECT_EQ(out.str(), IntType::kName);
  }
}

TEST(IntType, Hash) {
  EXPECT_EQ(absl::HashOf(IntType()), absl::HashOf(IntType()));
  EXPECT_EQ(absl::HashOf(Type(IntType())), absl::HashOf(IntType()));
}

TEST(IntType, Equal) {
  EXPECT_EQ(IntType(), IntType());
  EXPECT_EQ(Type(IntType()), IntType());
  EXPECT_EQ(IntType(), Type(IntType()));
  EXPECT_EQ(Type(IntType()), Type(IntType()));
}

TEST(IntType, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(IntType()), NativeTypeId::For<IntType>());
  EXPECT_EQ(NativeTypeId::Of(Type(IntType())), NativeTypeId::For<IntType>());
}

TEST(IntType, InstanceOf) {
  EXPECT_TRUE(InstanceOf<IntType>(IntType()));
  EXPECT_TRUE(InstanceOf<IntType>(Type(IntType())));
}

TEST(IntType, Cast) {
  EXPECT_THAT(Cast<IntType>(IntType()), An<IntType>());
  EXPECT_THAT(Cast<IntType>(Type(IntType())), An<IntType>());
}

TEST(IntType, As) {
  EXPECT_THAT(As<IntType>(IntType()), Ne(absl::nullopt));
  EXPECT_THAT(As<IntType>(Type(IntType())), Ne(absl::nullopt));
}

TEST(IntTypeView, Kind) {
  EXPECT_EQ(IntTypeView().kind(), IntTypeView::kKind);
  EXPECT_EQ(TypeView(IntTypeView()).kind(), IntTypeView::kKind);
}

TEST(IntTypeView, Name) {
  EXPECT_EQ(IntTypeView().name(), IntTypeView::kName);
  EXPECT_EQ(TypeView(IntTypeView()).name(), IntTypeView::kName);
}

TEST(IntTypeView, DebugString) {
  {
    std::ostringstream out;
    out << IntTypeView();
    EXPECT_EQ(out.str(), IntTypeView::kName);
  }
  {
    std::ostringstream out;
    out << TypeView(IntTypeView());
    EXPECT_EQ(out.str(), IntTypeView::kName);
  }
}

TEST(IntTypeView, Hash) {
  EXPECT_EQ(absl::HashOf(IntTypeView()), absl::HashOf(IntTypeView()));
  EXPECT_EQ(absl::HashOf(TypeView(IntTypeView())), absl::HashOf(IntTypeView()));
  EXPECT_EQ(absl::HashOf(IntTypeView()), absl::HashOf(IntType()));
  EXPECT_EQ(absl::HashOf(TypeView(IntTypeView())), absl::HashOf(IntType()));
}

TEST(IntTypeView, Equal) {
  EXPECT_EQ(IntTypeView(), IntTypeView());
  EXPECT_EQ(TypeView(IntTypeView()), IntTypeView());
  EXPECT_EQ(IntTypeView(), TypeView(IntTypeView()));
  EXPECT_EQ(TypeView(IntTypeView()), TypeView(IntTypeView()));
  EXPECT_EQ(IntTypeView(), IntType());
  EXPECT_EQ(TypeView(IntTypeView()), IntType());
  EXPECT_EQ(TypeView(IntTypeView()), Type(IntType()));
  EXPECT_EQ(IntType(), IntTypeView());
  EXPECT_EQ(IntType(), IntTypeView());
  EXPECT_EQ(IntType(), TypeView(IntTypeView()));
  EXPECT_EQ(Type(IntType()), TypeView(IntTypeView()));
  EXPECT_EQ(IntTypeView(), IntType());
}

TEST(IntTypeView, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(IntTypeView()), NativeTypeId::For<IntTypeView>());
  EXPECT_EQ(NativeTypeId::Of(TypeView(IntTypeView())),
            NativeTypeId::For<IntTypeView>());
}

TEST(IntTypeView, InstanceOf) {
  EXPECT_TRUE(InstanceOf<IntTypeView>(IntTypeView()));
  EXPECT_TRUE(InstanceOf<IntTypeView>(TypeView(IntTypeView())));
}

TEST(IntTypeView, Cast) {
  EXPECT_THAT(Cast<IntTypeView>(IntTypeView()), An<IntTypeView>());
  EXPECT_THAT(Cast<IntTypeView>(TypeView(IntTypeView())), An<IntTypeView>());
}

TEST(IntTypeView, As) {
  EXPECT_THAT(As<IntTypeView>(IntTypeView()), Ne(absl::nullopt));
  EXPECT_THAT(As<IntTypeView>(TypeView(IntTypeView())), Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
