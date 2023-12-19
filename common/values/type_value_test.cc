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

#include "absl/status/status.h"
#include "absl/strings/cord.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/value.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::An;
using testing::Ne;
using cel::internal::StatusIs;

TEST(TypeValue, Kind) {
  EXPECT_EQ(TypeValue(AnyType()).kind(), TypeValue::kKind);
  EXPECT_EQ(Value(TypeValue(AnyType())).kind(), TypeValue::kKind);
}

TEST(TypeValue, Type) {
  EXPECT_EQ(TypeValue(AnyType()).type(), TypeType());
  EXPECT_EQ(Value(TypeValue(AnyType())).type(), TypeType());
}

TEST(TypeValue, DebugString) {
  {
    std::ostringstream out;
    out << TypeValue(AnyType());
    EXPECT_EQ(out.str(), "google.protobuf.Any");
  }
  {
    std::ostringstream out;
    out << Value(TypeValue(AnyType()));
    EXPECT_EQ(out.str(), "google.protobuf.Any");
  }
}

TEST(TypeValue, GetSerializedSize) {
  EXPECT_THAT(TypeValue(AnyType()).GetSerializedSize(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(TypeValue, SerializeTo) {
  absl::Cord value;
  EXPECT_THAT(TypeValue(AnyType()).SerializeTo(value),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(TypeValue, Serialize) {
  EXPECT_THAT(TypeValue(AnyType()).Serialize(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(TypeValue, GetTypeUrl) {
  EXPECT_THAT(TypeValue(AnyType()).GetTypeUrl(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(TypeValue, ConvertToAny) {
  EXPECT_THAT(TypeValue(AnyType()).ConvertToAny(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(TypeValue, ConvertToJson) {
  EXPECT_THAT(TypeValue(AnyType()).ConvertToJson(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(TypeValue, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(TypeValue(AnyType())),
            NativeTypeId::For<TypeValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(TypeValue(AnyType()))),
            NativeTypeId::For<TypeValue>());
}

TEST(TypeValue, InstanceOf) {
  EXPECT_TRUE(InstanceOf<TypeValue>(TypeValue(AnyType())));
  EXPECT_TRUE(InstanceOf<TypeValue>(Value(TypeValue(AnyType()))));
}

TEST(TypeValue, Cast) {
  EXPECT_THAT(Cast<TypeValue>(TypeValue(AnyType())), An<TypeValue>());
  EXPECT_THAT(Cast<TypeValue>(Value(TypeValue(AnyType()))), An<TypeValue>());
}

TEST(TypeValue, As) {
  EXPECT_THAT(As<TypeValue>(TypeValue(AnyType())), Ne(absl::nullopt));
  EXPECT_THAT(As<TypeValue>(Value(TypeValue(AnyType()))), Ne(absl::nullopt));
}

TEST(TypeValueView, Kind) {
  EXPECT_EQ(TypeValueView(AnyTypeView()).kind(), TypeValueView::kKind);
  EXPECT_EQ(ValueView(TypeValueView(AnyTypeView())).kind(),
            TypeValueView::kKind);
}

TEST(TypeValueView, Type) {
  EXPECT_EQ(TypeValueView(AnyTypeView()).type(), TypeType());
  EXPECT_EQ(ValueView(TypeValueView(AnyTypeView())).type(), TypeType());
}

TEST(TypeValueView, DebugString) {
  {
    std::ostringstream out;
    out << TypeValueView(AnyTypeView());
    EXPECT_EQ(out.str(), "google.protobuf.Any");
  }
  {
    std::ostringstream out;
    out << ValueView(TypeValueView(AnyTypeView()));
    EXPECT_EQ(out.str(), "google.protobuf.Any");
  }
}

TEST(TypeValueView, GetSerializedSize) {
  EXPECT_THAT(TypeValueView(AnyTypeView()).GetSerializedSize(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(TypeValueView, SerializeTo) {
  absl::Cord value;
  EXPECT_THAT(TypeValueView(AnyTypeView()).SerializeTo(value),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(TypeValueView, Serialize) {
  EXPECT_THAT(TypeValueView(AnyTypeView()).Serialize(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(TypeValueView, GetTypeUrl) {
  EXPECT_THAT(TypeValueView(AnyTypeView()).GetTypeUrl(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(TypeValueView, ConvertToAny) {
  EXPECT_THAT(TypeValueView(AnyTypeView()).ConvertToAny(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(TypeValueView, ConvertToJson) {
  EXPECT_THAT(TypeValueView(AnyTypeView()).ConvertToJson(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(TypeValueView, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(TypeValueView(AnyTypeView())),
            NativeTypeId::For<TypeValueView>());
  EXPECT_EQ(NativeTypeId::Of(ValueView(TypeValueView(AnyTypeView()))),
            NativeTypeId::For<TypeValueView>());
}

TEST(TypeValueView, InstanceOf) {
  EXPECT_TRUE(InstanceOf<TypeValueView>(TypeValueView(AnyTypeView())));
  EXPECT_TRUE(
      InstanceOf<TypeValueView>(ValueView(TypeValueView(AnyTypeView()))));
}

TEST(TypeValueView, Cast) {
  EXPECT_THAT(Cast<TypeValueView>(TypeValueView(AnyTypeView())),
              An<TypeValueView>());
  EXPECT_THAT(Cast<TypeValueView>(ValueView(TypeValueView(AnyTypeView()))),
              An<TypeValueView>());
}

TEST(TypeValueView, As) {
  EXPECT_THAT(As<TypeValueView>(TypeValueView(AnyTypeView())),
              Ne(absl::nullopt));
  EXPECT_THAT(As<TypeValueView>(ValueView(TypeValueView(AnyTypeView()))),
              Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
