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

#include <limits>

#include "absl/types/optional.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {
namespace {

using testing::Optional;

constexpr double kNan = std::numeric_limits<double>::quiet_NaN();
constexpr double kInfinity = std::numeric_limits<double>::infinity();

static_assert(CelNumber(1.0f) == CelNumber::FromInt64(1), "double == int");
static_assert(CelNumber(1.0f) == CelNumber::FromUint64(1), "double == uint");
static_assert(CelNumber(1.0f) == CelNumber(1.0f), "double == double");
static_assert(CelNumber::FromInt64(1) == CelNumber::FromInt64(1), "int == int");
static_assert(CelNumber::FromInt64(1) == CelNumber::FromUint64(1),
              "int == uint");
static_assert(CelNumber::FromInt64(1) == CelNumber(1.0f), "int == double");
static_assert(CelNumber::FromUint64(1) == CelNumber::FromInt64(1),
              "uint == int");
static_assert(CelNumber::FromUint64(1) == CelNumber::FromUint64(1),
              "uint == uint");
static_assert(CelNumber::FromUint64(1) == CelNumber(1.0f), "uint == double");

static_assert(CelNumber(1.0f) >= CelNumber::FromInt64(1), "double >= int");
static_assert(CelNumber(1.0f) >= CelNumber::FromUint64(1), "double >= uint");
static_assert(CelNumber(1.0f) >= CelNumber(1.0f), "double >= double");
static_assert(CelNumber::FromInt64(1) >= CelNumber::FromInt64(1), "int >= int");
static_assert(CelNumber::FromInt64(1) >= CelNumber::FromUint64(1),
              "int >= uint");
static_assert(CelNumber::FromInt64(1) >= CelNumber(1.0f), "int >= double");
static_assert(CelNumber::FromUint64(1) >= CelNumber::FromInt64(1),
              "uint >= int");
static_assert(CelNumber::FromUint64(1) >= CelNumber::FromUint64(1),
              "uint >= uint");
static_assert(CelNumber::FromUint64(1) >= CelNumber(1.0f), "uint >= double");

static_assert(CelNumber(1.0f) <= CelNumber::FromInt64(1), "double <= int");
static_assert(CelNumber(1.0f) <= CelNumber::FromUint64(1), "double <= uint");
static_assert(CelNumber(1.0f) <= CelNumber(1.0f), "double <= double");
static_assert(CelNumber::FromInt64(1) <= CelNumber::FromInt64(1), "int <= int");
static_assert(CelNumber::FromInt64(1) <= CelNumber::FromUint64(1),
              "int <= uint");
static_assert(CelNumber::FromInt64(1) <= CelNumber(1.0f), "int <= double");
static_assert(CelNumber::FromUint64(1) <= CelNumber::FromInt64(1),
              "uint <= int");
static_assert(CelNumber::FromUint64(1) <= CelNumber::FromUint64(1),
              "uint <= uint");
static_assert(CelNumber::FromUint64(1) <= CelNumber(1.0f), "uint <= double");

static_assert(CelNumber(1.5f) > CelNumber::FromInt64(1), "double > int");
static_assert(CelNumber(1.5f) > CelNumber::FromUint64(1), "double > uint");
static_assert(CelNumber(1.5f) > CelNumber(1.0f), "double > double");
static_assert(CelNumber::FromInt64(2) > CelNumber::FromInt64(1), "int > int");
static_assert(CelNumber::FromInt64(2) > CelNumber::FromUint64(1), "int > uint");
static_assert(CelNumber::FromInt64(2) > CelNumber(1.5f), "int > double");
static_assert(CelNumber::FromUint64(2) > CelNumber::FromInt64(1), "uint > int");
static_assert(CelNumber::FromUint64(2) > CelNumber::FromUint64(1),
              "uint > uint");
static_assert(CelNumber::FromUint64(2) > CelNumber(1.5f), "uint > double");

static_assert(CelNumber(1.0f) < CelNumber::FromInt64(2), "double < int");
static_assert(CelNumber(1.0f) < CelNumber::FromUint64(2), "double < uint");
static_assert(CelNumber(1.0f) < CelNumber(1.1f), "double < double");
static_assert(CelNumber::FromInt64(1) < CelNumber::FromInt64(2), "int < int");
static_assert(CelNumber::FromInt64(1) < CelNumber::FromUint64(2), "int < uint");
static_assert(CelNumber::FromInt64(1) < CelNumber(1.5f), "int < double");
static_assert(CelNumber::FromUint64(1) < CelNumber::FromInt64(2), "uint < int");
static_assert(CelNumber::FromUint64(1) < CelNumber::FromUint64(2),
              "uint < uint");
static_assert(CelNumber::FromUint64(1) < CelNumber(1.5f), "uint < double");

static_assert(CelNumber(kNan) != CelNumber(kNan), "nan != nan");
static_assert(!(CelNumber(kNan) == CelNumber(kNan)), "nan == nan");
static_assert(!(CelNumber(kNan) > CelNumber(kNan)), "nan > nan");
static_assert(!(CelNumber(kNan) < CelNumber(kNan)), "nan < nan");
static_assert(!(CelNumber(kNan) >= CelNumber(kNan)), "nan >= nan");
static_assert(!(CelNumber(kNan) <= CelNumber(kNan)), "nan <= nan");

static_assert(CelNumber(kNan) != CelNumber::FromInt64(1), "nan != int");
static_assert(!(CelNumber(kNan) == CelNumber::FromInt64(1)), "nan == int");
static_assert(!(CelNumber(kNan) > CelNumber::FromInt64(1)), "nan > int");
static_assert(!(CelNumber(kNan) < CelNumber::FromInt64(1)), "nan < int");
static_assert(!(CelNumber(kNan) >= CelNumber::FromInt64(1)), "nan >= int");
static_assert(!(CelNumber(kNan) <= CelNumber::FromInt64(1)), "nan <= int");

static_assert(!(CelNumber(kInfinity) != CelNumber(kInfinity)), "inf != inf");
static_assert(CelNumber(kInfinity) == CelNumber(kInfinity), "inf == inf");
static_assert(!(CelNumber(kInfinity) > CelNumber(kInfinity)), "inf > inf");
static_assert(!(CelNumber(kInfinity) < CelNumber(kInfinity)), "inf < inf");
static_assert(CelNumber(kInfinity) >= CelNumber(kInfinity), "inf >= inf");
static_assert(CelNumber(kInfinity) <= CelNumber(kInfinity), "inf <= inf");

static_assert(CelNumber(kInfinity) != CelNumber::FromInt64(1), "inf != int");
static_assert(!(CelNumber(kInfinity) == CelNumber::FromInt64(1)), "inf == int");
static_assert(CelNumber(kInfinity) > CelNumber::FromInt64(1), "inf > int");
static_assert(!(CelNumber(kInfinity) < CelNumber::FromInt64(1)), "inf < int");
static_assert(CelNumber(kInfinity) >= CelNumber::FromInt64(1), "inf >= int");
static_assert(!(CelNumber(kInfinity) <= CelNumber::FromInt64(1)), "inf <= int");

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

}  // namespace
}  // namespace google::api::expr::runtime
