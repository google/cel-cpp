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

TEST(ErrorType, Kind) {
  EXPECT_EQ(ErrorType().kind(), ErrorType::kKind);
  EXPECT_EQ(Type(ErrorType()).kind(), ErrorType::kKind);
}

TEST(ErrorType, Name) {
  EXPECT_EQ(ErrorType().name(), ErrorType::kName);
  EXPECT_EQ(Type(ErrorType()).name(), ErrorType::kName);
}

TEST(ErrorType, DebugString) {
  {
    std::ostringstream out;
    out << ErrorType();
    EXPECT_EQ(out.str(), ErrorType::kName);
  }
  {
    std::ostringstream out;
    out << Type(ErrorType());
    EXPECT_EQ(out.str(), ErrorType::kName);
  }
}

TEST(ErrorType, Hash) {
  EXPECT_EQ(absl::HashOf(ErrorType()), absl::HashOf(ErrorType()));
  EXPECT_EQ(absl::HashOf(Type(ErrorType())), absl::HashOf(ErrorType()));
}

TEST(ErrorType, Equal) {
  EXPECT_EQ(ErrorType(), ErrorType());
  EXPECT_EQ(Type(ErrorType()), ErrorType());
  EXPECT_EQ(ErrorType(), Type(ErrorType()));
  EXPECT_EQ(Type(ErrorType()), Type(ErrorType()));
}

TEST(ErrorType, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(ErrorType()), NativeTypeId::For<ErrorType>());
  EXPECT_EQ(NativeTypeId::Of(Type(ErrorType())),
            NativeTypeId::For<ErrorType>());
}

TEST(ErrorType, InstanceOf) {
  EXPECT_TRUE(InstanceOf<ErrorType>(ErrorType()));
  EXPECT_TRUE(InstanceOf<ErrorType>(Type(ErrorType())));
}

TEST(ErrorType, Cast) {
  EXPECT_THAT(Cast<ErrorType>(ErrorType()), An<ErrorType>());
  EXPECT_THAT(Cast<ErrorType>(Type(ErrorType())), An<ErrorType>());
}

TEST(ErrorType, As) {
  EXPECT_THAT(As<ErrorType>(ErrorType()), Ne(absl::nullopt));
  EXPECT_THAT(As<ErrorType>(Type(ErrorType())), Ne(absl::nullopt));
}

TEST(ErrorTypeView, Kind) {
  EXPECT_EQ(ErrorTypeView().kind(), ErrorTypeView::kKind);
  EXPECT_EQ(TypeView(ErrorTypeView()).kind(), ErrorTypeView::kKind);
}

TEST(ErrorTypeView, Name) {
  EXPECT_EQ(ErrorTypeView().name(), ErrorTypeView::kName);
  EXPECT_EQ(TypeView(ErrorTypeView()).name(), ErrorTypeView::kName);
}

TEST(ErrorTypeView, DebugString) {
  {
    std::ostringstream out;
    out << ErrorTypeView();
    EXPECT_EQ(out.str(), ErrorTypeView::kName);
  }
  {
    std::ostringstream out;
    out << TypeView(ErrorTypeView());
    EXPECT_EQ(out.str(), ErrorTypeView::kName);
  }
}

TEST(ErrorTypeView, Hash) {
  EXPECT_EQ(absl::HashOf(ErrorTypeView()), absl::HashOf(ErrorTypeView()));
  EXPECT_EQ(absl::HashOf(TypeView(ErrorTypeView())),
            absl::HashOf(ErrorTypeView()));
  EXPECT_EQ(absl::HashOf(ErrorTypeView()), absl::HashOf(ErrorType()));
  EXPECT_EQ(absl::HashOf(TypeView(ErrorTypeView())), absl::HashOf(ErrorType()));
}

TEST(ErrorTypeView, Equal) {
  EXPECT_EQ(ErrorTypeView(), ErrorTypeView());
  EXPECT_EQ(TypeView(ErrorTypeView()), ErrorTypeView());
  EXPECT_EQ(ErrorTypeView(), TypeView(ErrorTypeView()));
  EXPECT_EQ(TypeView(ErrorTypeView()), TypeView(ErrorTypeView()));
  EXPECT_EQ(ErrorTypeView(), ErrorType());
  EXPECT_EQ(TypeView(ErrorTypeView()), ErrorType());
  EXPECT_EQ(TypeView(ErrorTypeView()), Type(ErrorType()));
  EXPECT_EQ(ErrorType(), ErrorTypeView());
  EXPECT_EQ(ErrorType(), ErrorTypeView());
  EXPECT_EQ(ErrorType(), TypeView(ErrorTypeView()));
  EXPECT_EQ(Type(ErrorType()), TypeView(ErrorTypeView()));
  EXPECT_EQ(ErrorTypeView(), ErrorType());
}

TEST(ErrorTypeView, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(ErrorTypeView()),
            NativeTypeId::For<ErrorTypeView>());
  EXPECT_EQ(NativeTypeId::Of(TypeView(ErrorTypeView())),
            NativeTypeId::For<ErrorTypeView>());
}

TEST(ErrorTypeView, InstanceOf) {
  EXPECT_TRUE(InstanceOf<ErrorTypeView>(ErrorTypeView()));
  EXPECT_TRUE(InstanceOf<ErrorTypeView>(TypeView(ErrorTypeView())));
}

TEST(ErrorTypeView, Cast) {
  EXPECT_THAT(Cast<ErrorTypeView>(ErrorTypeView()), An<ErrorTypeView>());
  EXPECT_THAT(Cast<ErrorTypeView>(TypeView(ErrorTypeView())),
              An<ErrorTypeView>());
}

TEST(ErrorTypeView, As) {
  EXPECT_THAT(As<ErrorTypeView>(ErrorTypeView()), Ne(absl::nullopt));
  EXPECT_THAT(As<ErrorTypeView>(TypeView(ErrorTypeView())), Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
