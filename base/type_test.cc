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

#include "base/type.h"

#include <type_traits>
#include <utility>

#include "absl/hash/hash_testing.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::SizeIs;

template <class T>
constexpr void IS_INITIALIZED(T&) {}

TEST(Type, TypeTraits) {
  EXPECT_TRUE(std::is_default_constructible_v<Type>);
  EXPECT_TRUE(std::is_copy_constructible_v<Type>);
  EXPECT_TRUE(std::is_move_constructible_v<Type>);
  EXPECT_TRUE(std::is_copy_assignable_v<Type>);
  EXPECT_TRUE(std::is_move_assignable_v<Type>);
  EXPECT_TRUE(std::is_swappable_v<Type>);
}

TEST(Type, DefaultConstructor) {
  Type type;
  EXPECT_EQ(type, Type::Null());
}

TEST(Type, CopyConstructor) {
  Type type(Type::Int());
  EXPECT_EQ(type, Type::Int());
}

TEST(Type, MoveConstructor) {
  Type from(Type::Int());
  Type to(std::move(from));
  IS_INITIALIZED(from);
  EXPECT_EQ(from, Type::Null());
  EXPECT_EQ(to, Type::Int());
}

TEST(Type, CopyAssignment) {
  Type type;
  type = Type::Int();
  EXPECT_EQ(type, Type::Int());
}

TEST(Type, MoveAssignment) {
  Type from(Type::Int());
  Type to;
  to = std::move(from);
  IS_INITIALIZED(from);
  EXPECT_EQ(from, Type::Null());
  EXPECT_EQ(to, Type::Int());
}

TEST(Type, Swap) {
  Type lhs = Type::Int();
  Type rhs = Type::Uint();
  std::swap(lhs, rhs);
  EXPECT_EQ(lhs, Type::Uint());
  EXPECT_EQ(rhs, Type::Int());
}

// The below tests could be made parameterized but doing so requires the
// extension for struct member initiation by name for it to be worth it. That
// feature is not available in C++17.

TEST(Type, Null) {
  EXPECT_EQ(Type::Null().kind(), Kind::kNullType);
  EXPECT_EQ(Type::Null().name(), "null_type");
  EXPECT_THAT(Type::Null().parameters(), SizeIs(0));
  EXPECT_TRUE(Type::Null().IsNull());
  EXPECT_FALSE(Type::Null().IsDyn());
  EXPECT_FALSE(Type::Null().IsAny());
  EXPECT_FALSE(Type::Null().IsBool());
  EXPECT_FALSE(Type::Null().IsInt());
  EXPECT_FALSE(Type::Null().IsUint());
  EXPECT_FALSE(Type::Null().IsDouble());
  EXPECT_FALSE(Type::Null().IsString());
  EXPECT_FALSE(Type::Null().IsBytes());
  EXPECT_FALSE(Type::Null().IsDuration());
  EXPECT_FALSE(Type::Null().IsTimestamp());
}

TEST(Type, Error) {
  EXPECT_EQ(Type::Error().kind(), Kind::kError);
  EXPECT_EQ(Type::Error().name(), "*error*");
  EXPECT_THAT(Type::Error().parameters(), SizeIs(0));
  EXPECT_FALSE(Type::Error().IsNull());
  EXPECT_FALSE(Type::Error().IsDyn());
  EXPECT_FALSE(Type::Error().IsAny());
  EXPECT_FALSE(Type::Error().IsBool());
  EXPECT_FALSE(Type::Error().IsInt());
  EXPECT_FALSE(Type::Error().IsUint());
  EXPECT_FALSE(Type::Error().IsDouble());
  EXPECT_FALSE(Type::Error().IsString());
  EXPECT_FALSE(Type::Error().IsBytes());
  EXPECT_FALSE(Type::Error().IsDuration());
  EXPECT_FALSE(Type::Error().IsTimestamp());
}

TEST(Type, Dyn) {
  EXPECT_EQ(Type::Dyn().kind(), Kind::kDyn);
  EXPECT_EQ(Type::Dyn().name(), "dyn");
  EXPECT_THAT(Type::Dyn().parameters(), SizeIs(0));
  EXPECT_FALSE(Type::Dyn().IsNull());
  EXPECT_TRUE(Type::Dyn().IsDyn());
  EXPECT_FALSE(Type::Dyn().IsAny());
  EXPECT_FALSE(Type::Dyn().IsBool());
  EXPECT_FALSE(Type::Dyn().IsInt());
  EXPECT_FALSE(Type::Dyn().IsUint());
  EXPECT_FALSE(Type::Dyn().IsDouble());
  EXPECT_FALSE(Type::Dyn().IsString());
  EXPECT_FALSE(Type::Dyn().IsBytes());
  EXPECT_FALSE(Type::Dyn().IsDuration());
  EXPECT_FALSE(Type::Dyn().IsTimestamp());
}

TEST(Type, Any) {
  EXPECT_EQ(Type::Any().kind(), Kind::kAny);
  EXPECT_EQ(Type::Any().name(), "google.protobuf.Any");
  EXPECT_THAT(Type::Any().parameters(), SizeIs(0));
  EXPECT_FALSE(Type::Any().IsNull());
  EXPECT_FALSE(Type::Any().IsDyn());
  EXPECT_TRUE(Type::Any().IsAny());
  EXPECT_FALSE(Type::Any().IsBool());
  EXPECT_FALSE(Type::Any().IsInt());
  EXPECT_FALSE(Type::Any().IsUint());
  EXPECT_FALSE(Type::Any().IsDouble());
  EXPECT_FALSE(Type::Any().IsString());
  EXPECT_FALSE(Type::Any().IsBytes());
  EXPECT_FALSE(Type::Any().IsDuration());
  EXPECT_FALSE(Type::Any().IsTimestamp());
}

TEST(Type, Bool) {
  EXPECT_EQ(Type::Bool().kind(), Kind::kBool);
  EXPECT_EQ(Type::Bool().name(), "bool");
  EXPECT_THAT(Type::Bool().parameters(), SizeIs(0));
  EXPECT_FALSE(Type::Bool().IsNull());
  EXPECT_FALSE(Type::Bool().IsDyn());
  EXPECT_FALSE(Type::Bool().IsAny());
  EXPECT_TRUE(Type::Bool().IsBool());
  EXPECT_FALSE(Type::Bool().IsInt());
  EXPECT_FALSE(Type::Bool().IsUint());
  EXPECT_FALSE(Type::Bool().IsDouble());
  EXPECT_FALSE(Type::Bool().IsString());
  EXPECT_FALSE(Type::Bool().IsBytes());
  EXPECT_FALSE(Type::Bool().IsDuration());
  EXPECT_FALSE(Type::Bool().IsTimestamp());
}

TEST(Type, Int) {
  EXPECT_EQ(Type::Int().kind(), Kind::kInt);
  EXPECT_EQ(Type::Int().name(), "int");
  EXPECT_THAT(Type::Int().parameters(), SizeIs(0));
  EXPECT_FALSE(Type::Int().IsNull());
  EXPECT_FALSE(Type::Int().IsDyn());
  EXPECT_FALSE(Type::Int().IsAny());
  EXPECT_FALSE(Type::Int().IsBool());
  EXPECT_TRUE(Type::Int().IsInt());
  EXPECT_FALSE(Type::Int().IsUint());
  EXPECT_FALSE(Type::Int().IsDouble());
  EXPECT_FALSE(Type::Int().IsString());
  EXPECT_FALSE(Type::Int().IsBytes());
  EXPECT_FALSE(Type::Int().IsDuration());
  EXPECT_FALSE(Type::Int().IsTimestamp());
}

TEST(Type, Uint) {
  EXPECT_EQ(Type::Uint().kind(), Kind::kUint);
  EXPECT_EQ(Type::Uint().name(), "uint");
  EXPECT_THAT(Type::Uint().parameters(), SizeIs(0));
  EXPECT_FALSE(Type::Uint().IsNull());
  EXPECT_FALSE(Type::Uint().IsDyn());
  EXPECT_FALSE(Type::Uint().IsAny());
  EXPECT_FALSE(Type::Uint().IsBool());
  EXPECT_FALSE(Type::Uint().IsInt());
  EXPECT_TRUE(Type::Uint().IsUint());
  EXPECT_FALSE(Type::Uint().IsDouble());
  EXPECT_FALSE(Type::Uint().IsString());
  EXPECT_FALSE(Type::Uint().IsBytes());
  EXPECT_FALSE(Type::Uint().IsDuration());
  EXPECT_FALSE(Type::Uint().IsTimestamp());
}

TEST(Type, Double) {
  EXPECT_EQ(Type::Double().kind(), Kind::kDouble);
  EXPECT_EQ(Type::Double().name(), "double");
  EXPECT_THAT(Type::Double().parameters(), SizeIs(0));
  EXPECT_FALSE(Type::Double().IsNull());
  EXPECT_FALSE(Type::Double().IsDyn());
  EXPECT_FALSE(Type::Double().IsAny());
  EXPECT_FALSE(Type::Double().IsBool());
  EXPECT_FALSE(Type::Double().IsInt());
  EXPECT_FALSE(Type::Double().IsUint());
  EXPECT_TRUE(Type::Double().IsDouble());
  EXPECT_FALSE(Type::Double().IsString());
  EXPECT_FALSE(Type::Double().IsBytes());
  EXPECT_FALSE(Type::Double().IsDuration());
  EXPECT_FALSE(Type::Double().IsTimestamp());
}

TEST(Type, String) {
  EXPECT_EQ(Type::String().kind(), Kind::kString);
  EXPECT_EQ(Type::String().name(), "string");
  EXPECT_THAT(Type::String().parameters(), SizeIs(0));
  EXPECT_FALSE(Type::String().IsNull());
  EXPECT_FALSE(Type::String().IsDyn());
  EXPECT_FALSE(Type::String().IsAny());
  EXPECT_FALSE(Type::String().IsBool());
  EXPECT_FALSE(Type::String().IsInt());
  EXPECT_FALSE(Type::String().IsUint());
  EXPECT_FALSE(Type::String().IsDouble());
  EXPECT_TRUE(Type::String().IsString());
  EXPECT_FALSE(Type::String().IsBytes());
  EXPECT_FALSE(Type::String().IsDuration());
  EXPECT_FALSE(Type::String().IsTimestamp());
}

TEST(Type, Bytes) {
  EXPECT_EQ(Type::Bytes().kind(), Kind::kBytes);
  EXPECT_EQ(Type::Bytes().name(), "bytes");
  EXPECT_THAT(Type::Bytes().parameters(), SizeIs(0));
  EXPECT_FALSE(Type::Bytes().IsNull());
  EXPECT_FALSE(Type::Bytes().IsDyn());
  EXPECT_FALSE(Type::Bytes().IsAny());
  EXPECT_FALSE(Type::Bytes().IsBool());
  EXPECT_FALSE(Type::Bytes().IsInt());
  EXPECT_FALSE(Type::Bytes().IsUint());
  EXPECT_FALSE(Type::Bytes().IsDouble());
  EXPECT_FALSE(Type::Bytes().IsString());
  EXPECT_TRUE(Type::Bytes().IsBytes());
  EXPECT_FALSE(Type::Bytes().IsDuration());
  EXPECT_FALSE(Type::Bytes().IsTimestamp());
}

TEST(Type, Duration) {
  EXPECT_EQ(Type::Duration().kind(), Kind::kDuration);
  EXPECT_EQ(Type::Duration().name(), "google.protobuf.Duration");
  EXPECT_THAT(Type::Duration().parameters(), SizeIs(0));
  EXPECT_FALSE(Type::Duration().IsNull());
  EXPECT_FALSE(Type::Duration().IsDyn());
  EXPECT_FALSE(Type::Duration().IsAny());
  EXPECT_FALSE(Type::Duration().IsBool());
  EXPECT_FALSE(Type::Duration().IsInt());
  EXPECT_FALSE(Type::Duration().IsUint());
  EXPECT_FALSE(Type::Duration().IsDouble());
  EXPECT_FALSE(Type::Duration().IsString());
  EXPECT_FALSE(Type::Duration().IsBytes());
  EXPECT_TRUE(Type::Duration().IsDuration());
  EXPECT_FALSE(Type::Duration().IsTimestamp());
}

TEST(Type, Timestamp) {
  EXPECT_EQ(Type::Timestamp().kind(), Kind::kTimestamp);
  EXPECT_EQ(Type::Timestamp().name(), "google.protobuf.Timestamp");
  EXPECT_THAT(Type::Timestamp().parameters(), SizeIs(0));
  EXPECT_FALSE(Type::Timestamp().IsNull());
  EXPECT_FALSE(Type::Timestamp().IsDyn());
  EXPECT_FALSE(Type::Timestamp().IsAny());
  EXPECT_FALSE(Type::Timestamp().IsBool());
  EXPECT_FALSE(Type::Timestamp().IsInt());
  EXPECT_FALSE(Type::Timestamp().IsUint());
  EXPECT_FALSE(Type::Timestamp().IsDouble());
  EXPECT_FALSE(Type::Timestamp().IsString());
  EXPECT_FALSE(Type::Timestamp().IsBytes());
  EXPECT_FALSE(Type::Timestamp().IsDuration());
  EXPECT_TRUE(Type::Timestamp().IsTimestamp());
}

TEST(Type, SupportsAbslHash) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      Type::Error(),
      Type::Null(),
      Type::Dyn(),
      Type::Any(),
      Type::Bool(),
      Type::Int(),
      Type::Uint(),
      Type::Double(),
      Type::String(),
      Type::Bytes(),
      Type::Duration(),
      Type::Timestamp(),
  }));
}

}  // namespace
}  // namespace cel
