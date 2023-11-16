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

#include "common/type.h"

#include <sstream>

#include "absl/hash/hash.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/native_type.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::An;
using testing::Ne;

TEST(TypeType, Kind) {
  EXPECT_EQ(TypeType().kind(), TypeType::kKind);
  EXPECT_EQ(Type(TypeType()).kind(), TypeType::kKind);
}

TEST(TypeType, Name) {
  EXPECT_EQ(TypeType().name(), TypeType::kName);
  EXPECT_EQ(Type(TypeType()).name(), TypeType::kName);
}

TEST(TypeType, DebugString) {
  {
    std::ostringstream out;
    out << TypeType();
    EXPECT_EQ(out.str(), TypeType::kName);
  }
  {
    std::ostringstream out;
    out << Type(TypeType());
    EXPECT_EQ(out.str(), TypeType::kName);
  }
}

TEST(TypeType, Hash) {
  EXPECT_EQ(absl::HashOf(TypeType()), absl::HashOf(TypeType()));
  EXPECT_EQ(absl::HashOf(Type(TypeType())), absl::HashOf(TypeType()));
}

TEST(TypeType, Equal) {
  EXPECT_EQ(TypeType(), TypeType());
  EXPECT_EQ(Type(TypeType()), TypeType());
  EXPECT_EQ(TypeType(), Type(TypeType()));
  EXPECT_EQ(Type(TypeType()), Type(TypeType()));
}

TEST(TypeType, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(TypeType()), NativeTypeId::For<TypeType>());
  EXPECT_EQ(NativeTypeId::Of(Type(TypeType())), NativeTypeId::For<TypeType>());
}

TEST(TypeType, InstanceOf) {
  EXPECT_TRUE(InstanceOf<TypeType>(TypeType()));
  EXPECT_TRUE(InstanceOf<TypeType>(Type(TypeType())));
}

TEST(TypeType, Cast) {
  EXPECT_THAT(Cast<TypeType>(TypeType()), An<TypeType>());
  EXPECT_THAT(Cast<TypeType>(Type(TypeType())), An<TypeType>());
}

TEST(TypeType, As) {
  EXPECT_THAT(As<TypeType>(TypeType()), Ne(absl::nullopt));
  EXPECT_THAT(As<TypeType>(Type(TypeType())), Ne(absl::nullopt));
}

TEST(TypeTypeView, Kind) {
  EXPECT_EQ(TypeTypeView().kind(), TypeTypeView::kKind);
  EXPECT_EQ(TypeView(TypeTypeView()).kind(), TypeTypeView::kKind);
}

TEST(TypeTypeView, Name) {
  EXPECT_EQ(TypeTypeView().name(), TypeTypeView::kName);
  EXPECT_EQ(TypeView(TypeTypeView()).name(), TypeTypeView::kName);
}

TEST(TypeTypeView, DebugString) {
  {
    std::ostringstream out;
    out << TypeTypeView();
    EXPECT_EQ(out.str(), TypeTypeView::kName);
  }
  {
    std::ostringstream out;
    out << TypeView(TypeTypeView());
    EXPECT_EQ(out.str(), TypeTypeView::kName);
  }
}

TEST(TypeTypeView, Hash) {
  EXPECT_EQ(absl::HashOf(TypeTypeView()), absl::HashOf(TypeTypeView()));
  EXPECT_EQ(absl::HashOf(TypeView(TypeTypeView())),
            absl::HashOf(TypeTypeView()));
  EXPECT_EQ(absl::HashOf(TypeTypeView()), absl::HashOf(TypeType()));
  EXPECT_EQ(absl::HashOf(TypeView(TypeTypeView())), absl::HashOf(TypeType()));
}

TEST(TypeTypeView, Equal) {
  EXPECT_EQ(TypeTypeView(), TypeTypeView());
  EXPECT_EQ(TypeView(TypeTypeView()), TypeTypeView());
  EXPECT_EQ(TypeTypeView(), TypeView(TypeTypeView()));
  EXPECT_EQ(TypeView(TypeTypeView()), TypeView(TypeTypeView()));
  EXPECT_EQ(TypeTypeView(), TypeType());
  EXPECT_EQ(TypeView(TypeTypeView()), TypeType());
  EXPECT_EQ(TypeView(TypeTypeView()), Type(TypeType()));
  EXPECT_EQ(TypeType(), TypeTypeView());
  EXPECT_EQ(TypeType(), TypeTypeView());
  EXPECT_EQ(TypeType(), TypeView(TypeTypeView()));
  EXPECT_EQ(Type(TypeType()), TypeView(TypeTypeView()));
  EXPECT_EQ(TypeTypeView(), TypeType());
}

TEST(TypeTypeView, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(TypeTypeView()),
            NativeTypeId::For<TypeTypeView>());
  EXPECT_EQ(NativeTypeId::Of(TypeView(TypeTypeView())),
            NativeTypeId::For<TypeTypeView>());
}

TEST(TypeTypeView, InstanceOf) {
  EXPECT_TRUE(InstanceOf<TypeTypeView>(TypeTypeView()));
  EXPECT_TRUE(InstanceOf<TypeTypeView>(TypeView(TypeTypeView())));
}

TEST(TypeTypeView, Cast) {
  EXPECT_THAT(Cast<TypeTypeView>(TypeTypeView()), An<TypeTypeView>());
  EXPECT_THAT(Cast<TypeTypeView>(TypeView(TypeTypeView())), An<TypeTypeView>());
}

TEST(TypeTypeView, As) {
  EXPECT_THAT(As<TypeTypeView>(TypeTypeView()), Ne(absl::nullopt));
  EXPECT_THAT(As<TypeTypeView>(TypeView(TypeTypeView())), Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
