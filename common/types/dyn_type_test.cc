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

TEST(DynType, Kind) {
  EXPECT_EQ(DynType().kind(), DynType::kKind);
  EXPECT_EQ(Type(DynType()).kind(), DynType::kKind);
}

TEST(DynType, Name) {
  EXPECT_EQ(DynType().name(), DynType::kName);
  EXPECT_EQ(Type(DynType()).name(), DynType::kName);
}

TEST(DynType, DebugString) {
  {
    std::ostringstream out;
    out << DynType();
    EXPECT_EQ(out.str(), DynType::kName);
  }
  {
    std::ostringstream out;
    out << Type(DynType());
    EXPECT_EQ(out.str(), DynType::kName);
  }
}

TEST(DynType, Hash) {
  EXPECT_EQ(absl::HashOf(DynType()), absl::HashOf(DynType()));
  EXPECT_EQ(absl::HashOf(Type(DynType())), absl::HashOf(DynType()));
}

TEST(DynType, Equal) {
  EXPECT_EQ(DynType(), DynType());
  EXPECT_EQ(Type(DynType()), DynType());
  EXPECT_EQ(DynType(), Type(DynType()));
  EXPECT_EQ(Type(DynType()), Type(DynType()));
}

TEST(DynType, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(DynType()), NativeTypeId::For<DynType>());
  EXPECT_EQ(NativeTypeId::Of(Type(DynType())), NativeTypeId::For<DynType>());
}

TEST(DynType, InstanceOf) {
  EXPECT_TRUE(InstanceOf<DynType>(DynType()));
  EXPECT_TRUE(InstanceOf<DynType>(Type(DynType())));
}

TEST(DynType, Cast) {
  EXPECT_THAT(Cast<DynType>(DynType()), An<DynType>());
  EXPECT_THAT(Cast<DynType>(Type(DynType())), An<DynType>());
}

TEST(DynType, As) {
  EXPECT_THAT(As<DynType>(DynType()), Ne(absl::nullopt));
  EXPECT_THAT(As<DynType>(Type(DynType())), Ne(absl::nullopt));
}

TEST(DynTypeView, Kind) {
  EXPECT_EQ(DynTypeView().kind(), DynTypeView::kKind);
  EXPECT_EQ(TypeView(DynTypeView()).kind(), DynTypeView::kKind);
}

TEST(DynTypeView, Name) {
  EXPECT_EQ(DynTypeView().name(), DynTypeView::kName);
  EXPECT_EQ(TypeView(DynTypeView()).name(), DynTypeView::kName);
}

TEST(DynTypeView, DebugString) {
  {
    std::ostringstream out;
    out << DynTypeView();
    EXPECT_EQ(out.str(), DynTypeView::kName);
  }
  {
    std::ostringstream out;
    out << TypeView(DynTypeView());
    EXPECT_EQ(out.str(), DynTypeView::kName);
  }
}

TEST(DynTypeView, Hash) {
  EXPECT_EQ(absl::HashOf(DynTypeView()), absl::HashOf(DynTypeView()));
  EXPECT_EQ(absl::HashOf(TypeView(DynTypeView())), absl::HashOf(DynTypeView()));
  EXPECT_EQ(absl::HashOf(DynTypeView()), absl::HashOf(DynType()));
  EXPECT_EQ(absl::HashOf(TypeView(DynTypeView())), absl::HashOf(DynType()));
}

TEST(DynTypeView, Equal) {
  EXPECT_EQ(DynTypeView(), DynTypeView());
  EXPECT_EQ(TypeView(DynTypeView()), DynTypeView());
  EXPECT_EQ(DynTypeView(), TypeView(DynTypeView()));
  EXPECT_EQ(TypeView(DynTypeView()), TypeView(DynTypeView()));
  EXPECT_EQ(DynTypeView(), DynType());
  EXPECT_EQ(TypeView(DynTypeView()), DynType());
  EXPECT_EQ(TypeView(DynTypeView()), Type(DynType()));
  EXPECT_EQ(DynType(), DynTypeView());
  EXPECT_EQ(DynType(), DynTypeView());
  EXPECT_EQ(DynType(), TypeView(DynTypeView()));
  EXPECT_EQ(Type(DynType()), TypeView(DynTypeView()));
  EXPECT_EQ(DynTypeView(), DynType());
}

TEST(DynTypeView, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(DynTypeView()), NativeTypeId::For<DynTypeView>());
  EXPECT_EQ(NativeTypeId::Of(TypeView(DynTypeView())),
            NativeTypeId::For<DynTypeView>());
}

TEST(DynTypeView, InstanceOf) {
  EXPECT_TRUE(InstanceOf<DynTypeView>(DynTypeView()));
  EXPECT_TRUE(InstanceOf<DynTypeView>(TypeView(DynTypeView())));
}

TEST(DynTypeView, Cast) {
  EXPECT_THAT(Cast<DynTypeView>(DynTypeView()), An<DynTypeView>());
  EXPECT_THAT(Cast<DynTypeView>(TypeView(DynTypeView())), An<DynTypeView>());
}

TEST(DynTypeView, As) {
  EXPECT_THAT(As<DynTypeView>(DynTypeView()), Ne(absl::nullopt));
  EXPECT_THAT(As<DynTypeView>(TypeView(DynTypeView())), Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
