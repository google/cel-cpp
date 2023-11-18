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

TEST(UnknownType, Kind) {
  EXPECT_EQ(UnknownType().kind(), UnknownType::kKind);
  EXPECT_EQ(Type(UnknownType()).kind(), UnknownType::kKind);
}

TEST(UnknownType, Name) {
  EXPECT_EQ(UnknownType().name(), UnknownType::kName);
  EXPECT_EQ(Type(UnknownType()).name(), UnknownType::kName);
}

TEST(UnknownType, DebugString) {
  {
    std::ostringstream out;
    out << UnknownType();
    EXPECT_EQ(out.str(), UnknownType::kName);
  }
  {
    std::ostringstream out;
    out << Type(UnknownType());
    EXPECT_EQ(out.str(), UnknownType::kName);
  }
}

TEST(UnknownType, Hash) {
  EXPECT_EQ(absl::HashOf(UnknownType()), absl::HashOf(UnknownType()));
  EXPECT_EQ(absl::HashOf(Type(UnknownType())), absl::HashOf(UnknownType()));
}

TEST(UnknownType, Equal) {
  EXPECT_EQ(UnknownType(), UnknownType());
  EXPECT_EQ(Type(UnknownType()), UnknownType());
  EXPECT_EQ(UnknownType(), Type(UnknownType()));
  EXPECT_EQ(Type(UnknownType()), Type(UnknownType()));
}

TEST(UnknownType, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(UnknownType()), NativeTypeId::For<UnknownType>());
  EXPECT_EQ(NativeTypeId::Of(Type(UnknownType())),
            NativeTypeId::For<UnknownType>());
}

TEST(UnknownType, InstanceOf) {
  EXPECT_TRUE(InstanceOf<UnknownType>(UnknownType()));
  EXPECT_TRUE(InstanceOf<UnknownType>(Type(UnknownType())));
}

TEST(UnknownType, Cast) {
  EXPECT_THAT(Cast<UnknownType>(UnknownType()), An<UnknownType>());
  EXPECT_THAT(Cast<UnknownType>(Type(UnknownType())), An<UnknownType>());
}

TEST(UnknownType, As) {
  EXPECT_THAT(As<UnknownType>(UnknownType()), Ne(absl::nullopt));
  EXPECT_THAT(As<UnknownType>(Type(UnknownType())), Ne(absl::nullopt));
}

TEST(UnknownTypeView, Kind) {
  EXPECT_EQ(UnknownTypeView().kind(), UnknownTypeView::kKind);
  EXPECT_EQ(TypeView(UnknownTypeView()).kind(), UnknownTypeView::kKind);
}

TEST(UnknownTypeView, Name) {
  EXPECT_EQ(UnknownTypeView().name(), UnknownTypeView::kName);
  EXPECT_EQ(TypeView(UnknownTypeView()).name(), UnknownTypeView::kName);
}

TEST(UnknownTypeView, DebugString) {
  {
    std::ostringstream out;
    out << UnknownTypeView();
    EXPECT_EQ(out.str(), UnknownTypeView::kName);
  }
  {
    std::ostringstream out;
    out << TypeView(UnknownTypeView());
    EXPECT_EQ(out.str(), UnknownTypeView::kName);
  }
}

TEST(UnknownTypeView, Hash) {
  EXPECT_EQ(absl::HashOf(UnknownTypeView()), absl::HashOf(UnknownTypeView()));
  EXPECT_EQ(absl::HashOf(TypeView(UnknownTypeView())),
            absl::HashOf(UnknownTypeView()));
  EXPECT_EQ(absl::HashOf(UnknownTypeView()), absl::HashOf(UnknownType()));
  EXPECT_EQ(absl::HashOf(TypeView(UnknownTypeView())),
            absl::HashOf(UnknownType()));
}

TEST(UnknownTypeView, Equal) {
  EXPECT_EQ(UnknownTypeView(), UnknownTypeView());
  EXPECT_EQ(TypeView(UnknownTypeView()), UnknownTypeView());
  EXPECT_EQ(UnknownTypeView(), TypeView(UnknownTypeView()));
  EXPECT_EQ(TypeView(UnknownTypeView()), TypeView(UnknownTypeView()));
  EXPECT_EQ(UnknownTypeView(), UnknownType());
  EXPECT_EQ(TypeView(UnknownTypeView()), UnknownType());
  EXPECT_EQ(TypeView(UnknownTypeView()), Type(UnknownType()));
  EXPECT_EQ(UnknownType(), UnknownTypeView());
  EXPECT_EQ(UnknownType(), UnknownTypeView());
  EXPECT_EQ(UnknownType(), TypeView(UnknownTypeView()));
  EXPECT_EQ(Type(UnknownType()), TypeView(UnknownTypeView()));
  EXPECT_EQ(UnknownTypeView(), UnknownType());
}

TEST(UnknownTypeView, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(UnknownTypeView()),
            NativeTypeId::For<UnknownTypeView>());
  EXPECT_EQ(NativeTypeId::Of(TypeView(UnknownTypeView())),
            NativeTypeId::For<UnknownTypeView>());
}

TEST(UnknownTypeView, InstanceOf) {
  EXPECT_TRUE(InstanceOf<UnknownTypeView>(UnknownTypeView()));
  EXPECT_TRUE(InstanceOf<UnknownTypeView>(TypeView(UnknownTypeView())));
}

TEST(UnknownTypeView, Cast) {
  EXPECT_THAT(Cast<UnknownTypeView>(UnknownTypeView()), An<UnknownTypeView>());
  EXPECT_THAT(Cast<UnknownTypeView>(TypeView(UnknownTypeView())),
              An<UnknownTypeView>());
}

TEST(UnknownTypeView, As) {
  EXPECT_THAT(As<UnknownTypeView>(UnknownTypeView()), Ne(absl::nullopt));
  EXPECT_THAT(As<UnknownTypeView>(TypeView(UnknownTypeView())),
              Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
