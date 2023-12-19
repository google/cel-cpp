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

using testing::_;
using testing::An;
using testing::IsEmpty;
using testing::Ne;
using testing::Not;
using cel::internal::StatusIs;

TEST(ErrorValue, Default) {
  ErrorValue value;
  EXPECT_THAT(value.NativeValue(), StatusIs(absl::StatusCode::kUnknown));
}

TEST(ErrorValue, OkStatus) {
  EXPECT_DEBUG_DEATH(static_cast<void>(ErrorValue(absl::OkStatus())), _);
}

TEST(ErrorValue, Kind) {
  EXPECT_EQ(ErrorValue(absl::CancelledError()).kind(), ErrorValue::kKind);
  EXPECT_EQ(Value(ErrorValue(absl::CancelledError())).kind(),
            ErrorValue::kKind);
}

TEST(ErrorValue, Type) {
  EXPECT_EQ(ErrorValue(absl::CancelledError()).type(), ErrorType());
  EXPECT_EQ(Value(ErrorValue(absl::CancelledError())).type(), ErrorType());
}

TEST(ErrorValue, DebugString) {
  {
    std::ostringstream out;
    out << ErrorValue(absl::CancelledError());
    EXPECT_THAT(out.str(), Not(IsEmpty()));
  }
  {
    std::ostringstream out;
    out << Value(ErrorValue(absl::CancelledError()));
    EXPECT_THAT(out.str(), Not(IsEmpty()));
  }
}

TEST(ErrorValue, GetSerializedSize) {
  EXPECT_THAT(ErrorValue().GetSerializedSize(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(ErrorValue, SerializeTo) {
  absl::Cord value;
  EXPECT_THAT(ErrorValue().SerializeTo(value),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(ErrorValue, Serialize) {
  EXPECT_THAT(ErrorValue().Serialize(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(ErrorValue, GetTypeUrl) {
  EXPECT_THAT(ErrorValue().GetTypeUrl(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(ErrorValue, ConvertToAny) {
  EXPECT_THAT(ErrorValue().ConvertToAny(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(ErrorValue, ConvertToJson) {
  EXPECT_THAT(ErrorValue().ConvertToJson(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(ErrorValue, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(ErrorValue(absl::CancelledError())),
            NativeTypeId::For<ErrorValue>());
  EXPECT_EQ(NativeTypeId::Of(Value(ErrorValue(absl::CancelledError()))),
            NativeTypeId::For<ErrorValue>());
}

TEST(ErrorValue, InstanceOf) {
  EXPECT_TRUE(InstanceOf<ErrorValue>(ErrorValue(absl::CancelledError())));
  EXPECT_TRUE(
      InstanceOf<ErrorValue>(Value(ErrorValue(absl::CancelledError()))));
}

TEST(ErrorValue, Cast) {
  EXPECT_THAT(Cast<ErrorValue>(ErrorValue(absl::CancelledError())),
              An<ErrorValue>());
  EXPECT_THAT(Cast<ErrorValue>(Value(ErrorValue(absl::CancelledError()))),
              An<ErrorValue>());
}

TEST(ErrorValue, As) {
  EXPECT_THAT(As<ErrorValue>(ErrorValue(absl::CancelledError())),
              Ne(absl::nullopt));
  EXPECT_THAT(As<ErrorValue>(Value(ErrorValue(absl::CancelledError()))),
              Ne(absl::nullopt));
}

TEST(ErrorValueView, Default) {
  ErrorValueView value;
  EXPECT_THAT(value.NativeValue(), StatusIs(absl::StatusCode::kUnknown));
}

TEST(ErrorValueView, OkStatus) {
  EXPECT_DEBUG_DEATH(static_cast<void>(ErrorValueView(absl::OkStatus())), _);
}

TEST(ErrorValueView, Kind) {
  EXPECT_EQ(ErrorValueView(absl::CancelledError()).kind(),
            ErrorValueView::kKind);
  EXPECT_EQ(ValueView(ErrorValueView(absl::CancelledError())).kind(),
            ErrorValueView::kKind);
}

TEST(ErrorValueView, Type) {
  EXPECT_EQ(ErrorValueView(absl::CancelledError()).type(), ErrorType());
  EXPECT_EQ(ValueView(ErrorValueView(absl::CancelledError())).type(),
            ErrorType());
}

TEST(ErrorValueView, DebugString) {
  {
    std::ostringstream out;
    out << ErrorValueView(absl::CancelledError());
    EXPECT_THAT(out.str(), Not(IsEmpty()));
  }
  {
    std::ostringstream out;
    out << ValueView(ErrorValueView(absl::CancelledError()));
    EXPECT_THAT(out.str(), Not(IsEmpty()));
  }
}

TEST(ErrorValueView, GetSerializedSize) {
  EXPECT_THAT(ErrorValueView().GetSerializedSize(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(ErrorValueView, SerializeTo) {
  absl::Cord value;
  EXPECT_THAT(ErrorValueView().SerializeTo(value),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(ErrorValueView, Serialize) {
  EXPECT_THAT(ErrorValueView().Serialize(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(ErrorValueView, GetTypeUrl) {
  EXPECT_THAT(ErrorValueView().GetTypeUrl(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(ErrorValueView, ConvertToAny) {
  EXPECT_THAT(ErrorValueView().ConvertToAny(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(ErrorValueView, ConvertToJson) {
  EXPECT_THAT(ErrorValueView().ConvertToJson(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST(ErrorValueView, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(ErrorValueView(absl::CancelledError())),
            NativeTypeId::For<ErrorValueView>());
  EXPECT_EQ(NativeTypeId::Of(ValueView(ErrorValueView(absl::CancelledError()))),
            NativeTypeId::For<ErrorValueView>());
}

TEST(ErrorValueView, InstanceOf) {
  EXPECT_TRUE(
      InstanceOf<ErrorValueView>(ErrorValueView(absl::CancelledError())));
  EXPECT_TRUE(InstanceOf<ErrorValueView>(
      ValueView(ErrorValueView(absl::CancelledError()))));
}

TEST(ErrorValueView, Cast) {
  EXPECT_THAT(Cast<ErrorValueView>(ErrorValueView(absl::CancelledError())),
              An<ErrorValueView>());
  EXPECT_THAT(
      Cast<ErrorValueView>(ValueView(ErrorValueView(absl::CancelledError()))),
      An<ErrorValueView>());
}

TEST(ErrorValueView, As) {
  EXPECT_THAT(As<ErrorValueView>(ErrorValueView(absl::CancelledError())),
              Ne(absl::nullopt));
  EXPECT_THAT(
      As<ErrorValueView>(ValueView(ErrorValueView(absl::CancelledError()))),
      Ne(absl::nullopt));
}

}  // namespace
}  // namespace cel
