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

TEST(BytesType, Kind) {
  EXPECT_EQ(BytesType().kind(), BytesType::kKind);
  EXPECT_EQ(Type(BytesType()).kind(), BytesType::kKind);
}

TEST(BytesType, Name) {
  EXPECT_EQ(BytesType().name(), BytesType::kName);
  EXPECT_EQ(Type(BytesType()).name(), BytesType::kName);
}

TEST(BytesType, DebugString) {
  {
    std::ostringstream out;
    out << BytesType();
    EXPECT_EQ(out.str(), BytesType::kName);
  }
  {
    std::ostringstream out;
    out << Type(BytesType());
    EXPECT_EQ(out.str(), BytesType::kName);
  }
}

TEST(BytesType, Hash) {
  EXPECT_EQ(absl::HashOf(BytesType()), absl::HashOf(BytesType()));
  EXPECT_EQ(absl::HashOf(Type(BytesType())), absl::HashOf(BytesType()));
}

TEST(BytesType, Equal) {
  EXPECT_EQ(BytesType(), BytesType());
  EXPECT_EQ(Type(BytesType()), BytesType());
  EXPECT_EQ(BytesType(), Type(BytesType()));
  EXPECT_EQ(Type(BytesType()), Type(BytesType()));
}

TEST(BytesType, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(BytesType()), NativeTypeId::For<BytesType>());
  EXPECT_EQ(NativeTypeId::Of(Type(BytesType())),
            NativeTypeId::For<BytesType>());
}

TEST(BytesType, InstanceOf) {
  EXPECT_TRUE(InstanceOf<BytesType>(BytesType()));
  EXPECT_TRUE(InstanceOf<BytesType>(Type(BytesType())));
}

TEST(BytesType, Cast) {
  EXPECT_THAT(Cast<BytesType>(BytesType()), An<BytesType>());
  EXPECT_THAT(Cast<BytesType>(Type(BytesType())), An<BytesType>());
}

TEST(BytesType, As) {
  EXPECT_THAT(As<BytesType>(BytesType()), Ne(absl::nullopt));
  EXPECT_THAT(As<BytesType>(Type(BytesType())), Ne(absl::nullopt));
}

TEST(BytesTypeView, Kind) {
  EXPECT_EQ(BytesTypeView().kind(), BytesTypeView::kKind);
  EXPECT_EQ(TypeView(BytesTypeView()).kind(), BytesTypeView::kKind);
}

TEST(BytesTypeView, Name) {
  EXPECT_EQ(BytesTypeView().name(), BytesTypeView::kName);
  EXPECT_EQ(TypeView(BytesTypeView()).name(), BytesTypeView::kName);
}

TEST(BytesTypeView, DebugString) {
  {
    std::ostringstream out;
    out << BytesTypeView();
    EXPECT_EQ(out.str(), BytesTypeView::kName);
  }
  {
    std::ostringstream out;
    out << TypeView(BytesTypeView());
    EXPECT_EQ(out.str(), BytesTypeView::kName);
  }
}

TEST(BytesTypeView, Hash) {
  EXPECT_EQ(absl::HashOf(BytesTypeView()), absl::HashOf(BytesTypeView()));
  EXPECT_EQ(absl::HashOf(TypeView(BytesTypeView())),
            absl::HashOf(BytesTypeView()));
  EXPECT_EQ(absl::HashOf(BytesTypeView()), absl::HashOf(BytesType()));
  EXPECT_EQ(absl::HashOf(TypeView(BytesTypeView())), absl::HashOf(BytesType()));
}

TEST(BytesTypeView, Equal) {
  EXPECT_EQ(BytesTypeView(), BytesTypeView());
  EXPECT_EQ(TypeView(BytesTypeView()), BytesTypeView());
  EXPECT_EQ(BytesTypeView(), TypeView(BytesTypeView()));
  EXPECT_EQ(TypeView(BytesTypeView()), TypeView(BytesTypeView()));
  EXPECT_EQ(BytesTypeView(), BytesType());
  EXPECT_EQ(TypeView(BytesTypeView()), BytesType());
  EXPECT_EQ(TypeView(BytesTypeView()), Type(BytesType()));
  EXPECT_EQ(BytesType(), BytesTypeView());
  EXPECT_EQ(BytesType(), BytesTypeView());
  EXPECT_EQ(BytesType(), TypeView(BytesTypeView()));
  EXPECT_EQ(Type(BytesType()), TypeView(BytesTypeView()));
  EXPECT_EQ(BytesTypeView(), BytesType());
}

TEST(BytesTypeView, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(BytesTypeView()),
            NativeTypeId::For<BytesTypeView>());
  EXPECT_EQ(NativeTypeId::Of(TypeView(BytesTypeView())),
            NativeTypeId::For<BytesTypeView>());
}

TEST(BytesTypeView, InstanceOf) {
  EXPECT_TRUE(InstanceOf<BytesTypeView>(BytesTypeView()));
  EXPECT_TRUE(InstanceOf<BytesTypeView>(TypeView(BytesTypeView())));
}

TEST(BytesTypeView, Cast) {
  EXPECT_THAT(Cast<BytesTypeView>(BytesTypeView()), An<BytesTypeView>());
  EXPECT_THAT(Cast<BytesTypeView>(TypeView(BytesTypeView())),
              An<BytesTypeView>());
}

TEST(BytesTypeView, As) {
  EXPECT_THAT(As<BytesTypeView>(BytesTypeView()), Ne(absl::nullopt));
  EXPECT_THAT(As<BytesTypeView>(TypeView(BytesTypeView())), Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
