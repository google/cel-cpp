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
  EXPECT_EQ(KindToString(Kind::kWrapper), "*wrapper*");
  EXPECT_EQ(KindToString(Kind::kOpaque), "*opaque*");
  EXPECT_EQ(KindToString(static_cast<Kind>(std::numeric_limits<int>::max())),
            "*error*");
}

TEST(Kind, TypeKindRoundtrip) {
  EXPECT_EQ(TypeKindToKind(KindToTypeKind(Kind::kBool)), Kind::kBool);
}

TEST(Kind, ValueKindRoundtrip) {
  EXPECT_EQ(ValueKindToKind(KindToValueKind(Kind::kBool)), Kind::kBool);
}

TEST(Kind, IsTypeKind) {
  EXPECT_TRUE(KindIsTypeKind(Kind::kBool));
  EXPECT_TRUE(KindIsTypeKind(Kind::kAny));
  EXPECT_TRUE(KindIsTypeKind(Kind::kDyn));
  EXPECT_TRUE(KindIsTypeKind(Kind::kWrapper));
}

TEST(Kind, IsValueKind) {
  EXPECT_TRUE(KindIsValueKind(Kind::kBool));
  EXPECT_FALSE(KindIsValueKind(Kind::kAny));
  EXPECT_FALSE(KindIsValueKind(Kind::kDyn));
  EXPECT_FALSE(KindIsValueKind(Kind::kWrapper));
}

TEST(Kind, Equality) {
  EXPECT_EQ(Kind::kBool, TypeKind::kBool);
  EXPECT_EQ(TypeKind::kBool, Kind::kBool);

  EXPECT_EQ(Kind::kBool, ValueKind::kBool);
  EXPECT_EQ(ValueKind::kBool, Kind::kBool);

  EXPECT_EQ(TypeKind::kBool, ValueKind::kBool);
  EXPECT_EQ(ValueKind::kBool, TypeKind::kBool);

  EXPECT_NE(Kind::kBool, TypeKind::kInt);
  EXPECT_NE(TypeKind::kInt, Kind::kBool);

  EXPECT_NE(Kind::kBool, ValueKind::kInt);
  EXPECT_NE(ValueKind::kInt, Kind::kBool);

  EXPECT_NE(TypeKind::kBool, ValueKind::kInt);
  EXPECT_NE(ValueKind::kInt, TypeKind::kBool);
}

TEST(TypeKind, ToString) {
  EXPECT_EQ(TypeKindToString(TypeKind::kBool), KindToString(Kind::kBool));
}

TEST(ValueKind, ToString) {
  EXPECT_EQ(ValueKindToString(ValueKind::kBool), KindToString(Kind::kBool));
}

}  // namespace
}  // namespace cel
