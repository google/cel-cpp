// Copyright 2022 Google LLC
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

#include "eval/public/cel_number.h"

#include <cstdint>
#include <limits>

#include "absl/types/optional.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {
namespace {

using testing::Optional;

constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
constexpr double kInfinity = std::numeric_limits<double>::infinity();

TEST(CelNumber, Basic) {
  EXPECT_GT(CelNumber(1.1), CelNumber::FromInt64(1));
  EXPECT_LT(CelNumber::FromUint64(1), CelNumber(1.1));
  EXPECT_EQ(CelNumber(1.1), CelNumber(1.1));

  EXPECT_EQ(CelNumber::FromUint64(1), CelNumber::FromUint64(1));
  EXPECT_EQ(CelNumber::FromInt64(1), CelNumber::FromUint64(1));
  EXPECT_GT(CelNumber::FromUint64(1), CelNumber::FromInt64(-1));

  EXPECT_EQ(CelNumber::FromInt64(-1), CelNumber::FromInt64(-1));
}

TEST(CelNumber, GetNumberFromCelValue) {
  EXPECT_THAT(GetNumberFromCelValue(CelValue::CreateDouble(1.1)),
              Optional(CelNumber::FromDouble(1.1)));
  EXPECT_THAT(GetNumberFromCelValue(CelValue::CreateInt64(1)),
              Optional(CelNumber::FromDouble(1.0)));
  EXPECT_THAT(GetNumberFromCelValue(CelValue::CreateUint64(1)),
              Optional(CelNumber::FromDouble(1.0)));

  EXPECT_EQ(GetNumberFromCelValue(CelValue::CreateDuration(absl::Seconds(1))),
            absl::nullopt);
}

TEST(CelNumber, Conversions) {
  EXPECT_TRUE(CelNumber::FromDouble(1.0).LosslessConvertibleToInt());
  EXPECT_TRUE(CelNumber::FromDouble(1.0).LosslessConvertibleToUint());
  EXPECT_FALSE(CelNumber::FromDouble(1.1).LosslessConvertibleToInt());
  EXPECT_FALSE(CelNumber::FromDouble(1.1).LosslessConvertibleToUint());
  EXPECT_TRUE(CelNumber::FromDouble(-1.0).LosslessConvertibleToInt());
  EXPECT_FALSE(CelNumber::FromDouble(-1.0).LosslessConvertibleToUint());
  EXPECT_TRUE(
      CelNumber::FromDouble(kDoubleToIntMin).LosslessConvertibleToInt());

  // Need to add/substract a large number since double resolution is low at this
  // range.
  EXPECT_FALSE(CelNumber::FromDouble(kMaxDoubleRepresentableAsUint +
                                     RoundingError<uint64_t>())
                   .LosslessConvertibleToUint());
  EXPECT_FALSE(CelNumber::FromDouble(kMaxDoubleRepresentableAsInt +
                                     RoundingError<int64_t>())
                   .LosslessConvertibleToInt());
  EXPECT_FALSE(
      CelNumber::FromDouble(kDoubleToIntMin - 1025).LosslessConvertibleToInt());

  EXPECT_EQ(CelNumber::FromInt64(1).AsUint(), 1u);
  EXPECT_EQ(CelNumber::FromUint64(1).AsInt(), 1);
  EXPECT_EQ(CelNumber::FromDouble(1.0).AsUint(), 1);
  EXPECT_EQ(CelNumber::FromDouble(1.0).AsInt(), 1);
}

}  // namespace
}  // namespace google::api::expr::runtime
