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
#include "base/handle.h"
#include "base/memory_manager.h"
#include "base/type_factory.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::SizeIs;

class TestTypeFactory final : public TypeFactory {
 public:
  TestTypeFactory() : TypeFactory(MemoryManager::Global()) {}
};

template <class T>
constexpr void IS_INITIALIZED(T&) {}

TEST(Type, TransientHandleTypeTraits) {
  EXPECT_TRUE(std::is_default_constructible_v<Transient<Type>>);
  EXPECT_TRUE(std::is_copy_constructible_v<Transient<Type>>);
  EXPECT_TRUE(std::is_move_constructible_v<Transient<Type>>);
  EXPECT_TRUE(std::is_copy_assignable_v<Transient<Type>>);
  EXPECT_TRUE(std::is_move_assignable_v<Transient<Type>>);
  EXPECT_TRUE(std::is_swappable_v<Transient<Type>>);
  EXPECT_TRUE(std::is_default_constructible_v<Transient<const Type>>);
  EXPECT_TRUE(std::is_copy_constructible_v<Transient<const Type>>);
  EXPECT_TRUE(std::is_move_constructible_v<Transient<const Type>>);
  EXPECT_TRUE(std::is_copy_assignable_v<Transient<const Type>>);
  EXPECT_TRUE(std::is_move_assignable_v<Transient<const Type>>);
  EXPECT_TRUE(std::is_swappable_v<Transient<const Type>>);
}

TEST(Type, PersistentHandleTypeTraits) {
  EXPECT_TRUE(std::is_default_constructible_v<Persistent<Type>>);
  EXPECT_TRUE(std::is_copy_constructible_v<Persistent<Type>>);
  EXPECT_TRUE(std::is_move_constructible_v<Persistent<Type>>);
  EXPECT_TRUE(std::is_copy_assignable_v<Persistent<Type>>);
  EXPECT_TRUE(std::is_move_assignable_v<Persistent<Type>>);
  EXPECT_TRUE(std::is_swappable_v<Persistent<Type>>);
  EXPECT_TRUE(std::is_default_constructible_v<Persistent<const Type>>);
  EXPECT_TRUE(std::is_copy_constructible_v<Persistent<const Type>>);
  EXPECT_TRUE(std::is_move_constructible_v<Persistent<const Type>>);
  EXPECT_TRUE(std::is_copy_assignable_v<Persistent<const Type>>);
  EXPECT_TRUE(std::is_move_assignable_v<Persistent<const Type>>);
  EXPECT_TRUE(std::is_swappable_v<Persistent<const Type>>);
}

TEST(Type, CopyConstructor) {
  TestTypeFactory type_factory;
  Transient<const Type> type(type_factory.GetIntType());
  EXPECT_EQ(type, type_factory.GetIntType());
}

TEST(Type, MoveConstructor) {
  TestTypeFactory type_factory;
  Transient<const Type> from(type_factory.GetIntType());
  Transient<const Type> to(std::move(from));
  IS_INITIALIZED(from);
  EXPECT_EQ(from, type_factory.GetIntType());
  EXPECT_EQ(to, type_factory.GetIntType());
}

TEST(Type, CopyAssignment) {
  TestTypeFactory type_factory;
  Transient<const Type> type(type_factory.GetNullType());
  type = type_factory.GetIntType();
  EXPECT_EQ(type, type_factory.GetIntType());
}

TEST(Type, MoveAssignment) {
  TestTypeFactory type_factory;
  Transient<const Type> from(type_factory.GetIntType());
  Transient<const Type> to(type_factory.GetNullType());
  to = std::move(from);
  IS_INITIALIZED(from);
  EXPECT_EQ(from, type_factory.GetIntType());
  EXPECT_EQ(to, type_factory.GetIntType());
}

TEST(Type, Swap) {
  TestTypeFactory type_factory;
  Transient<const Type> lhs = type_factory.GetIntType();
  Transient<const Type> rhs = type_factory.GetUintType();
  std::swap(lhs, rhs);
  EXPECT_EQ(lhs, type_factory.GetUintType());
  EXPECT_EQ(rhs, type_factory.GetIntType());
}

// The below tests could be made parameterized but doing so requires the
// extension for struct member initiation by name for it to be worth it. That
// feature is not available in C++17.

TEST(Type, Null) {
  TestTypeFactory type_factory;
  EXPECT_EQ(type_factory.GetNullType()->kind(), Kind::kNullType);
  EXPECT_EQ(type_factory.GetNullType()->name(), "null_type");
  EXPECT_THAT(type_factory.GetNullType()->parameters(), SizeIs(0));
  EXPECT_TRUE(type_factory.GetNullType().Is<NullType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<DynType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<AnyType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<BoolType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<IntType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<UintType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<DoubleType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<StringType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<BytesType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<DurationType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<TimestampType>());
}

TEST(Type, Error) {
  TestTypeFactory type_factory;
  EXPECT_EQ(type_factory.GetErrorType()->kind(), Kind::kError);
  EXPECT_EQ(type_factory.GetErrorType()->name(), "*error*");
  EXPECT_THAT(type_factory.GetErrorType()->parameters(), SizeIs(0));
  EXPECT_FALSE(type_factory.GetErrorType().Is<NullType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<DynType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<AnyType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<BoolType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<IntType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<UintType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<DoubleType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<StringType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<BytesType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<DurationType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<TimestampType>());
}

TEST(Type, Dyn) {
  TestTypeFactory type_factory;
  EXPECT_EQ(type_factory.GetDynType()->kind(), Kind::kDyn);
  EXPECT_EQ(type_factory.GetDynType()->name(), "dyn");
  EXPECT_THAT(type_factory.GetDynType()->parameters(), SizeIs(0));
  EXPECT_FALSE(type_factory.GetDynType().Is<NullType>());
  EXPECT_TRUE(type_factory.GetDynType().Is<DynType>());
  EXPECT_FALSE(type_factory.GetDynType().Is<AnyType>());
  EXPECT_FALSE(type_factory.GetDynType().Is<BoolType>());
  EXPECT_FALSE(type_factory.GetDynType().Is<IntType>());
  EXPECT_FALSE(type_factory.GetDynType().Is<UintType>());
  EXPECT_FALSE(type_factory.GetDynType().Is<DoubleType>());
  EXPECT_FALSE(type_factory.GetDynType().Is<StringType>());
  EXPECT_FALSE(type_factory.GetDynType().Is<BytesType>());
  EXPECT_FALSE(type_factory.GetDynType().Is<DurationType>());
  EXPECT_FALSE(type_factory.GetDynType().Is<TimestampType>());
}

TEST(Type, Any) {
  TestTypeFactory type_factory;
  EXPECT_EQ(type_factory.GetAnyType()->kind(), Kind::kAny);
  EXPECT_EQ(type_factory.GetAnyType()->name(), "google.protobuf.Any");
  EXPECT_THAT(type_factory.GetAnyType()->parameters(), SizeIs(0));
  EXPECT_FALSE(type_factory.GetAnyType().Is<NullType>());
  EXPECT_FALSE(type_factory.GetAnyType().Is<DynType>());
  EXPECT_TRUE(type_factory.GetAnyType().Is<AnyType>());
  EXPECT_FALSE(type_factory.GetAnyType().Is<BoolType>());
  EXPECT_FALSE(type_factory.GetAnyType().Is<IntType>());
  EXPECT_FALSE(type_factory.GetAnyType().Is<UintType>());
  EXPECT_FALSE(type_factory.GetAnyType().Is<DoubleType>());
  EXPECT_FALSE(type_factory.GetAnyType().Is<StringType>());
  EXPECT_FALSE(type_factory.GetAnyType().Is<BytesType>());
  EXPECT_FALSE(type_factory.GetAnyType().Is<DurationType>());
  EXPECT_FALSE(type_factory.GetAnyType().Is<TimestampType>());
}

TEST(Type, Bool) {
  TestTypeFactory type_factory;
  EXPECT_EQ(type_factory.GetBoolType()->kind(), Kind::kBool);
  EXPECT_EQ(type_factory.GetBoolType()->name(), "bool");
  EXPECT_THAT(type_factory.GetBoolType()->parameters(), SizeIs(0));
  EXPECT_FALSE(type_factory.GetBoolType().Is<NullType>());
  EXPECT_FALSE(type_factory.GetBoolType().Is<DynType>());
  EXPECT_FALSE(type_factory.GetBoolType().Is<AnyType>());
  EXPECT_TRUE(type_factory.GetBoolType().Is<BoolType>());
  EXPECT_FALSE(type_factory.GetBoolType().Is<IntType>());
  EXPECT_FALSE(type_factory.GetBoolType().Is<UintType>());
  EXPECT_FALSE(type_factory.GetBoolType().Is<DoubleType>());
  EXPECT_FALSE(type_factory.GetBoolType().Is<StringType>());
  EXPECT_FALSE(type_factory.GetBoolType().Is<BytesType>());
  EXPECT_FALSE(type_factory.GetBoolType().Is<DurationType>());
  EXPECT_FALSE(type_factory.GetBoolType().Is<TimestampType>());
}

TEST(Type, Int) {
  TestTypeFactory type_factory;
  EXPECT_EQ(type_factory.GetIntType()->kind(), Kind::kInt);
  EXPECT_EQ(type_factory.GetIntType()->name(), "int");
  EXPECT_THAT(type_factory.GetIntType()->parameters(), SizeIs(0));
  EXPECT_FALSE(type_factory.GetIntType().Is<NullType>());
  EXPECT_FALSE(type_factory.GetIntType().Is<DynType>());
  EXPECT_FALSE(type_factory.GetIntType().Is<AnyType>());
  EXPECT_FALSE(type_factory.GetIntType().Is<BoolType>());
  EXPECT_TRUE(type_factory.GetIntType().Is<IntType>());
  EXPECT_FALSE(type_factory.GetIntType().Is<UintType>());
  EXPECT_FALSE(type_factory.GetIntType().Is<DoubleType>());
  EXPECT_FALSE(type_factory.GetIntType().Is<StringType>());
  EXPECT_FALSE(type_factory.GetIntType().Is<BytesType>());
  EXPECT_FALSE(type_factory.GetIntType().Is<DurationType>());
  EXPECT_FALSE(type_factory.GetIntType().Is<TimestampType>());
}

TEST(Type, Uint) {
  TestTypeFactory type_factory;
  EXPECT_EQ(type_factory.GetUintType()->kind(), Kind::kUint);
  EXPECT_EQ(type_factory.GetUintType()->name(), "uint");
  EXPECT_THAT(type_factory.GetUintType()->parameters(), SizeIs(0));
  EXPECT_FALSE(type_factory.GetUintType().Is<NullType>());
  EXPECT_FALSE(type_factory.GetUintType().Is<DynType>());
  EXPECT_FALSE(type_factory.GetUintType().Is<AnyType>());
  EXPECT_FALSE(type_factory.GetUintType().Is<BoolType>());
  EXPECT_FALSE(type_factory.GetUintType().Is<IntType>());
  EXPECT_TRUE(type_factory.GetUintType().Is<UintType>());
  EXPECT_FALSE(type_factory.GetUintType().Is<DoubleType>());
  EXPECT_FALSE(type_factory.GetUintType().Is<StringType>());
  EXPECT_FALSE(type_factory.GetUintType().Is<BytesType>());
  EXPECT_FALSE(type_factory.GetUintType().Is<DurationType>());
  EXPECT_FALSE(type_factory.GetUintType().Is<TimestampType>());
}

TEST(Type, Double) {
  TestTypeFactory type_factory;
  EXPECT_EQ(type_factory.GetDoubleType()->kind(), Kind::kDouble);
  EXPECT_EQ(type_factory.GetDoubleType()->name(), "double");
  EXPECT_THAT(type_factory.GetDoubleType()->parameters(), SizeIs(0));
  EXPECT_FALSE(type_factory.GetDoubleType().Is<NullType>());
  EXPECT_FALSE(type_factory.GetDoubleType().Is<DynType>());
  EXPECT_FALSE(type_factory.GetDoubleType().Is<AnyType>());
  EXPECT_FALSE(type_factory.GetDoubleType().Is<BoolType>());
  EXPECT_FALSE(type_factory.GetDoubleType().Is<IntType>());
  EXPECT_FALSE(type_factory.GetDoubleType().Is<UintType>());
  EXPECT_TRUE(type_factory.GetDoubleType().Is<DoubleType>());
  EXPECT_FALSE(type_factory.GetDoubleType().Is<StringType>());
  EXPECT_FALSE(type_factory.GetDoubleType().Is<BytesType>());
  EXPECT_FALSE(type_factory.GetDoubleType().Is<DurationType>());
  EXPECT_FALSE(type_factory.GetDoubleType().Is<TimestampType>());
}

TEST(Type, String) {
  TestTypeFactory type_factory;
  EXPECT_EQ(type_factory.GetStringType()->kind(), Kind::kString);
  EXPECT_EQ(type_factory.GetStringType()->name(), "string");
  EXPECT_THAT(type_factory.GetStringType()->parameters(), SizeIs(0));
  EXPECT_FALSE(type_factory.GetStringType().Is<NullType>());
  EXPECT_FALSE(type_factory.GetStringType().Is<DynType>());
  EXPECT_FALSE(type_factory.GetStringType().Is<AnyType>());
  EXPECT_FALSE(type_factory.GetStringType().Is<BoolType>());
  EXPECT_FALSE(type_factory.GetStringType().Is<IntType>());
  EXPECT_FALSE(type_factory.GetStringType().Is<UintType>());
  EXPECT_FALSE(type_factory.GetStringType().Is<DoubleType>());
  EXPECT_TRUE(type_factory.GetStringType().Is<StringType>());
  EXPECT_FALSE(type_factory.GetStringType().Is<BytesType>());
  EXPECT_FALSE(type_factory.GetStringType().Is<DurationType>());
  EXPECT_FALSE(type_factory.GetStringType().Is<TimestampType>());
}

TEST(Type, Bytes) {
  TestTypeFactory type_factory;
  EXPECT_EQ(type_factory.GetBytesType()->kind(), Kind::kBytes);
  EXPECT_EQ(type_factory.GetBytesType()->name(), "bytes");
  EXPECT_THAT(type_factory.GetBytesType()->parameters(), SizeIs(0));
  EXPECT_FALSE(type_factory.GetBytesType().Is<NullType>());
  EXPECT_FALSE(type_factory.GetBytesType().Is<DynType>());
  EXPECT_FALSE(type_factory.GetBytesType().Is<AnyType>());
  EXPECT_FALSE(type_factory.GetBytesType().Is<BoolType>());
  EXPECT_FALSE(type_factory.GetBytesType().Is<IntType>());
  EXPECT_FALSE(type_factory.GetBytesType().Is<UintType>());
  EXPECT_FALSE(type_factory.GetBytesType().Is<DoubleType>());
  EXPECT_FALSE(type_factory.GetBytesType().Is<StringType>());
  EXPECT_TRUE(type_factory.GetBytesType().Is<BytesType>());
  EXPECT_FALSE(type_factory.GetBytesType().Is<DurationType>());
  EXPECT_FALSE(type_factory.GetBytesType().Is<TimestampType>());
}

TEST(Type, Duration) {
  TestTypeFactory type_factory;
  EXPECT_EQ(type_factory.GetDurationType()->kind(), Kind::kDuration);
  EXPECT_EQ(type_factory.GetDurationType()->name(), "google.protobuf.Duration");
  EXPECT_THAT(type_factory.GetDurationType()->parameters(), SizeIs(0));
  EXPECT_FALSE(type_factory.GetDurationType().Is<NullType>());
  EXPECT_FALSE(type_factory.GetDurationType().Is<DynType>());
  EXPECT_FALSE(type_factory.GetDurationType().Is<AnyType>());
  EXPECT_FALSE(type_factory.GetDurationType().Is<BoolType>());
  EXPECT_FALSE(type_factory.GetDurationType().Is<IntType>());
  EXPECT_FALSE(type_factory.GetDurationType().Is<UintType>());
  EXPECT_FALSE(type_factory.GetDurationType().Is<DoubleType>());
  EXPECT_FALSE(type_factory.GetDurationType().Is<StringType>());
  EXPECT_FALSE(type_factory.GetDurationType().Is<BytesType>());
  EXPECT_TRUE(type_factory.GetDurationType().Is<DurationType>());
  EXPECT_FALSE(type_factory.GetDurationType().Is<TimestampType>());
}

TEST(Type, Timestamp) {
  TestTypeFactory type_factory;
  EXPECT_EQ(type_factory.GetTimestampType()->kind(), Kind::kTimestamp);
  EXPECT_EQ(type_factory.GetTimestampType()->name(),
            "google.protobuf.Timestamp");
  EXPECT_THAT(type_factory.GetTimestampType()->parameters(), SizeIs(0));
  EXPECT_FALSE(type_factory.GetTimestampType().Is<NullType>());
  EXPECT_FALSE(type_factory.GetTimestampType().Is<DynType>());
  EXPECT_FALSE(type_factory.GetTimestampType().Is<AnyType>());
  EXPECT_FALSE(type_factory.GetTimestampType().Is<BoolType>());
  EXPECT_FALSE(type_factory.GetTimestampType().Is<IntType>());
  EXPECT_FALSE(type_factory.GetTimestampType().Is<UintType>());
  EXPECT_FALSE(type_factory.GetTimestampType().Is<DoubleType>());
  EXPECT_FALSE(type_factory.GetTimestampType().Is<StringType>());
  EXPECT_FALSE(type_factory.GetTimestampType().Is<BytesType>());
  EXPECT_FALSE(type_factory.GetTimestampType().Is<DurationType>());
  EXPECT_TRUE(type_factory.GetTimestampType().Is<TimestampType>());
}

TEST(Type, SupportsAbslHash) {
  TestTypeFactory type_factory;
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      Persistent<const Type>(type_factory.GetNullType()),
      Persistent<const Type>(type_factory.GetErrorType()),
      Persistent<const Type>(type_factory.GetDynType()),
      Persistent<const Type>(type_factory.GetAnyType()),
      Persistent<const Type>(type_factory.GetBoolType()),
      Persistent<const Type>(type_factory.GetIntType()),
      Persistent<const Type>(type_factory.GetUintType()),
      Persistent<const Type>(type_factory.GetDoubleType()),
      Persistent<const Type>(type_factory.GetStringType()),
      Persistent<const Type>(type_factory.GetBytesType()),
      Persistent<const Type>(type_factory.GetDurationType()),
      Persistent<const Type>(type_factory.GetTimestampType()),
  }));
}

}  // namespace
}  // namespace cel
