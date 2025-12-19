// Copyright 2025 Google LLC
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

#include "codelab/exercise10.h"

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "internal/testing.h"

namespace cel_codelab {
namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::HasSubstr;

TEST(Exercise10, IpInRange) {
  EXPECT_THAT(CompileAndEvaluateExercise10(
                  R"cel(
                    net.parseAddressMatcher("8.8.4.0-8.8.4.255")
                      .containsAddress(
                        net.parseAddress(ip)
                      )
                    )cel",
                  "8.8.4.4"),
              IsOkAndHolds(true));
}

TEST(Exercise10, IpNotInRange) {
  EXPECT_THAT(CompileAndEvaluateExercise10(
                  R"cel(
                    net.parseAddressMatcher("8.8.4.0-8.8.4.255")
                      .containsAddress(
                        net.parseAddress(ip)
                      )
                    )cel",
                  "8.8.8.8"),
              IsOkAndHolds(false));
}

TEST(Exercise10, IpEqual) {
  EXPECT_THAT(CompileAndEvaluateExercise10(
                  R"cel(
                      net.parseAddress("8.8.4.4") == net.parseAddress(ip)
                    )cel",
                  "8.8.4.4"),
              IsOkAndHolds(true));
}

TEST(Exercise10, IpInequal) {
  EXPECT_THAT(CompileAndEvaluateExercise10(
                  R"cel(
                    net.parseAddress("8.8.4.4") == net.parseAddress(ip)
                  )cel",
                  "8.8.8.8"),
              IsOkAndHolds(false));
}

TEST(Exercise10, IpInvalid) {
  EXPECT_THAT(CompileAndEvaluateExercise10(
                  R"cel(
                    net.parseAddress("8.8.4.4") == net.parseAddress(ip)
                  )cel",
                  "8.8"),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("invalid address")));
}

}  // namespace
}  // namespace cel_codelab
