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

TEST(DurationType, Kind) {
  EXPECT_EQ(DurationType().kind(), DurationType::kKind);
  EXPECT_EQ(Type(DurationType()).kind(), DurationType::kKind);
}

TEST(DurationType, Name) {
  EXPECT_EQ(DurationType().name(), DurationType::kName);
  EXPECT_EQ(Type(DurationType()).name(), DurationType::kName);
}

TEST(DurationType, DebugString) {
  {
    std::ostringstream out;
    out << DurationType();
    EXPECT_EQ(out.str(), DurationType::kName);
  }
  {
    std::ostringstream out;
    out << Type(DurationType());
    EXPECT_EQ(out.str(), DurationType::kName);
  }
}

TEST(DurationType, Hash) {
  EXPECT_EQ(absl::HashOf(DurationType()), absl::HashOf(DurationType()));
  EXPECT_EQ(absl::HashOf(Type(DurationType())), absl::HashOf(DurationType()));
}

TEST(DurationType, Equal) {
  EXPECT_EQ(DurationType(), DurationType());
  EXPECT_EQ(Type(DurationType()), DurationType());
  EXPECT_EQ(DurationType(), Type(DurationType()));
  EXPECT_EQ(Type(DurationType()), Type(DurationType()));
}

TEST(DurationType, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(DurationType()),
            NativeTypeId::For<DurationType>());
  EXPECT_EQ(NativeTypeId::Of(Type(DurationType())),
            NativeTypeId::For<DurationType>());
}

TEST(DurationType, InstanceOf) {
  EXPECT_TRUE(InstanceOf<DurationType>(DurationType()));
  EXPECT_TRUE(InstanceOf<DurationType>(Type(DurationType())));
}

TEST(DurationType, Cast) {
  EXPECT_THAT(Cast<DurationType>(DurationType()), An<DurationType>());
  EXPECT_THAT(Cast<DurationType>(Type(DurationType())), An<DurationType>());
}

TEST(DurationType, As) {
  EXPECT_THAT(As<DurationType>(DurationType()), Ne(absl::nullopt));
  EXPECT_THAT(As<DurationType>(Type(DurationType())), Ne(absl::nullopt));
}

TEST(DurationTypeView, Kind) {
  EXPECT_EQ(DurationTypeView().kind(), DurationTypeView::kKind);
  EXPECT_EQ(TypeView(DurationTypeView()).kind(), DurationTypeView::kKind);
}

TEST(DurationTypeView, Name) {
  EXPECT_EQ(DurationTypeView().name(), DurationTypeView::kName);
  EXPECT_EQ(TypeView(DurationTypeView()).name(), DurationTypeView::kName);
}

TEST(DurationTypeView, DebugString) {
  {
    std::ostringstream out;
    out << DurationTypeView();
    EXPECT_EQ(out.str(), DurationTypeView::kName);
  }
  {
    std::ostringstream out;
    out << TypeView(DurationTypeView());
    EXPECT_EQ(out.str(), DurationTypeView::kName);
  }
}

TEST(DurationTypeView, Hash) {
  EXPECT_EQ(absl::HashOf(DurationTypeView()), absl::HashOf(DurationTypeView()));
  EXPECT_EQ(absl::HashOf(TypeView(DurationTypeView())),
            absl::HashOf(DurationTypeView()));
  EXPECT_EQ(absl::HashOf(DurationTypeView()), absl::HashOf(DurationType()));
  EXPECT_EQ(absl::HashOf(TypeView(DurationTypeView())),
            absl::HashOf(DurationType()));
}

TEST(DurationTypeView, Equal) {
  EXPECT_EQ(DurationTypeView(), DurationTypeView());
  EXPECT_EQ(TypeView(DurationTypeView()), DurationTypeView());
  EXPECT_EQ(DurationTypeView(), TypeView(DurationTypeView()));
  EXPECT_EQ(TypeView(DurationTypeView()), TypeView(DurationTypeView()));
  EXPECT_EQ(DurationTypeView(), DurationType());
  EXPECT_EQ(TypeView(DurationTypeView()), DurationType());
  EXPECT_EQ(TypeView(DurationTypeView()), Type(DurationType()));
  EXPECT_EQ(DurationType(), DurationTypeView());
  EXPECT_EQ(DurationType(), DurationTypeView());
  EXPECT_EQ(DurationType(), TypeView(DurationTypeView()));
  EXPECT_EQ(Type(DurationType()), TypeView(DurationTypeView()));
  EXPECT_EQ(DurationTypeView(), DurationType());
}

TEST(DurationTypeView, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(DurationTypeView()),
            NativeTypeId::For<DurationTypeView>());
  EXPECT_EQ(NativeTypeId::Of(TypeView(DurationTypeView())),
            NativeTypeId::For<DurationTypeView>());
}

TEST(DurationTypeView, InstanceOf) {
  EXPECT_TRUE(InstanceOf<DurationTypeView>(DurationTypeView()));
  EXPECT_TRUE(InstanceOf<DurationTypeView>(TypeView(DurationTypeView())));
}

TEST(DurationTypeView, Cast) {
  EXPECT_THAT(Cast<DurationTypeView>(DurationTypeView()),
              An<DurationTypeView>());
  EXPECT_THAT(Cast<DurationTypeView>(TypeView(DurationTypeView())),
              An<DurationTypeView>());
}

TEST(DurationTypeView, As) {
  EXPECT_THAT(As<DurationTypeView>(DurationTypeView()), Ne(absl::nullopt));
  EXPECT_THAT(As<DurationTypeView>(TypeView(DurationTypeView())),
              Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
