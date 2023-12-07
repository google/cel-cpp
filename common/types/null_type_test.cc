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

TEST(NullType, Kind) {
  EXPECT_EQ(NullType().kind(), NullType::kKind);
  EXPECT_EQ(Type(NullType()).kind(), NullType::kKind);
}

TEST(NullType, Name) {
  EXPECT_EQ(NullType().name(), NullType::kName);
  EXPECT_EQ(Type(NullType()).name(), NullType::kName);
}

TEST(NullType, DebugString) {
  {
    std::ostringstream out;
    out << NullType();
    EXPECT_EQ(out.str(), NullType::kName);
  }
  {
    std::ostringstream out;
    out << Type(NullType());
    EXPECT_EQ(out.str(), NullType::kName);
  }
}

TEST(NullType, Hash) {
  EXPECT_EQ(absl::HashOf(NullType()), absl::HashOf(NullType()));
}

TEST(NullType, Equal) {
  EXPECT_EQ(NullType(), NullType());
  EXPECT_EQ(Type(NullType()), NullType());
  EXPECT_EQ(NullType(), Type(NullType()));
  EXPECT_EQ(Type(NullType()), Type(NullType()));
}

TEST(NullType, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(NullType()), NativeTypeId::For<NullType>());
  EXPECT_EQ(NativeTypeId::Of(Type(NullType())), NativeTypeId::For<NullType>());
}

TEST(NullType, InstanceOf) {
  EXPECT_TRUE(InstanceOf<NullType>(NullType()));
  EXPECT_TRUE(InstanceOf<NullType>(Type(NullType())));
}

TEST(NullType, Cast) {
  EXPECT_THAT(Cast<NullType>(NullType()), An<NullType>());
  EXPECT_THAT(Cast<NullType>(Type(NullType())), An<NullType>());
}

TEST(NullType, As) {
  EXPECT_THAT(As<NullType>(NullType()), Ne(absl::nullopt));
  EXPECT_THAT(As<NullType>(Type(NullType())), Ne(absl::nullopt));
}

TEST(NullTypeView, Kind) {
  EXPECT_EQ(NullTypeView().kind(), NullTypeView::kKind);
  EXPECT_EQ(TypeView(NullTypeView()).kind(), NullTypeView::kKind);
}

TEST(NullTypeView, Name) {
  EXPECT_EQ(NullTypeView().name(), NullTypeView::kName);
  EXPECT_EQ(TypeView(NullTypeView()).name(), NullTypeView::kName);
}

TEST(NullTypeView, DebugString) {
  {
    std::ostringstream out;
    out << NullTypeView();
    EXPECT_EQ(out.str(), NullTypeView::kName);
  }
  {
    std::ostringstream out;
    out << TypeView(NullTypeView());
    EXPECT_EQ(out.str(), NullTypeView::kName);
  }
}

TEST(NullTypeView, Hash) {
  EXPECT_EQ(absl::HashOf(NullTypeView()), absl::HashOf(NullTypeView()));
  EXPECT_EQ(absl::HashOf(NullTypeView()), absl::HashOf(NullType()));
}

TEST(NullTypeView, Equal) {
  EXPECT_EQ(NullTypeView(), NullTypeView());
  EXPECT_EQ(TypeView(NullTypeView()), NullTypeView());
  EXPECT_EQ(NullTypeView(), TypeView(NullTypeView()));
  EXPECT_EQ(TypeView(NullTypeView()), TypeView(NullTypeView()));
  EXPECT_EQ(NullTypeView(), NullType());
  EXPECT_EQ(TypeView(NullTypeView()), NullType());
  EXPECT_EQ(TypeView(NullTypeView()), Type(NullType()));
  EXPECT_EQ(NullType(), NullTypeView());
  EXPECT_EQ(NullType(), NullTypeView());
  EXPECT_EQ(NullType(), TypeView(NullTypeView()));
  EXPECT_EQ(Type(NullType()), TypeView(NullTypeView()));
  EXPECT_EQ(NullTypeView(), NullType());
}

TEST(NullTypeView, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(NullTypeView()),
            NativeTypeId::For<NullTypeView>());
  EXPECT_EQ(NativeTypeId::Of(TypeView(NullTypeView())),
            NativeTypeId::For<NullTypeView>());
}

TEST(NullTypeView, InstanceOf) {
  EXPECT_TRUE(InstanceOf<NullTypeView>(NullTypeView()));
  EXPECT_TRUE(InstanceOf<NullTypeView>(TypeView(NullTypeView())));
}

TEST(NullTypeView, Cast) {
  EXPECT_THAT(Cast<NullTypeView>(NullTypeView()), An<NullTypeView>());
  EXPECT_THAT(Cast<NullTypeView>(TypeView(NullTypeView())), An<NullTypeView>());
}

TEST(NullTypeView, As) {
  EXPECT_THAT(As<NullTypeView>(NullTypeView()), Ne(absl::nullopt));
  EXPECT_THAT(As<NullTypeView>(TypeView(NullTypeView())), Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
