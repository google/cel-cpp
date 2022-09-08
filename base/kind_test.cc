// Copyright 2021 Google LLC
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

#include "base/kind.h"

#include <limits>

#include "internal/testing.h"

namespace cel {
namespace {

TEST(Kind, ToString) {
  EXPECT_EQ(KindToString(Kind::kError), "*error*");
  EXPECT_EQ(KindToString(Kind::kNullType), "null_type");
  EXPECT_EQ(KindToString(Kind::kDyn), "dyn");
  EXPECT_EQ(KindToString(Kind::kAny), "any");
  EXPECT_EQ(KindToString(Kind::kType), "type");
  EXPECT_EQ(KindToString(Kind::kBool), "bool");
  EXPECT_EQ(KindToString(Kind::kInt), "int");
  EXPECT_EQ(KindToString(Kind::kUint), "uint");
  EXPECT_EQ(KindToString(Kind::kDouble), "double");
  EXPECT_EQ(KindToString(Kind::kString), "string");
  EXPECT_EQ(KindToString(Kind::kBytes), "bytes");
  EXPECT_EQ(KindToString(Kind::kEnum), "enum");
  EXPECT_EQ(KindToString(Kind::kDuration), "duration");
  EXPECT_EQ(KindToString(Kind::kTimestamp), "timestamp");
  EXPECT_EQ(KindToString(Kind::kList), "list");
  EXPECT_EQ(KindToString(Kind::kMap), "map");
  EXPECT_EQ(KindToString(Kind::kStruct), "struct");
  EXPECT_EQ(KindToString(Kind::kUnknown), "*unknown*");
  EXPECT_EQ(KindToString(static_cast<Kind>(std::numeric_limits<int>::max())),
            "*error*");
}

}  // namespace
}  // namespace cel
