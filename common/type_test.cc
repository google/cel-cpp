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

#include "common/type.h"

#include <sstream>
#include <utility>

#include "absl/hash/hash.h"
#include "absl/hash/hash_testing.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::_;

template <typename T>
void IS_INITIALIZED(T&) {}

TEST(Type, KindDebugDeath) {
  Type moved_from_type = AnyType();
  Type type = std::move(moved_from_type);
  IS_INITIALIZED(moved_from_type);
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(static_cast<void>(moved_from_type.kind()), _);
}

TEST(Type, NameDebugDeath) {
  Type moved_from_type = AnyType();
  Type type = std::move(moved_from_type);
  IS_INITIALIZED(moved_from_type);
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(static_cast<void>(moved_from_type.name()), _);
}

TEST(Type, DebugStringDebugDeath) {
  Type moved_from_type = AnyType();
  Type type = std::move(moved_from_type);
  IS_INITIALIZED(moved_from_type);
  static_cast<void>(type);
  std::ostringstream out;
  EXPECT_DEBUG_DEATH(static_cast<void>(out << moved_from_type), _);
}

TEST(Type, HashDebugDeath) {
  Type moved_from_type = AnyType();
  Type type = std::move(moved_from_type);
  IS_INITIALIZED(moved_from_type);
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(static_cast<void>(absl::HashOf(moved_from_type)), _);
}

TEST(Type, EqualDebugDeath) {
  Type moved_from_type = AnyType();
  Type type = std::move(moved_from_type);
  IS_INITIALIZED(moved_from_type);
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(static_cast<void>(type == moved_from_type), _);
  EXPECT_DEBUG_DEATH(static_cast<void>(moved_from_type == type), _);
}

TEST(Type, NativeTypeIdDebugDeath) {
  Type moved_from_type = AnyType();
  Type type = std::move(moved_from_type);
  IS_INITIALIZED(moved_from_type);
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(static_cast<void>(NativeTypeId::Of(moved_from_type)), _);
}

TEST(Type, VerifyTypeImplementsAbslHashCorrectly) {
  MemoryManagerRef memory_manager = MemoryManagerRef::ReferenceCounting();
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      {Type(AnyType()),
       Type(BoolType()),
       Type(BoolWrapperType()),
       Type(BytesType()),
       Type(BytesWrapperType()),
       Type(DoubleType()),
       Type(DoubleWrapperType()),
       Type(DurationType()),
       Type(DynType()),
       Type(ErrorType()),
       Type(IntType()),
       Type(IntWrapperType()),
       Type(ListType(memory_manager, DynType())),
       Type(MapType(memory_manager, DynType(), DynType())),
       Type(NullType()),
       Type(OptionalType(memory_manager, DynType())),
       Type(StringType()),
       Type(StringWrapperType()),
       Type(StructType(memory_manager, "test.Struct")),
       Type(TimestampType()),
       Type(TypeType()),
       Type(UintType()),
       Type(UintWrapperType()),
       Type(UnknownType())}));
}

TEST(TypeView, KindDebugDeath) {
  Type moved_from_type = AnyType();
  Type type = std::move(moved_from_type);
  IS_INITIALIZED(moved_from_type);
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(static_cast<void>(TypeView(moved_from_type).kind()), _);
}

TEST(TypeView, NameDebugDeath) {
  Type moved_from_type = AnyType();
  Type type = std::move(moved_from_type);
  IS_INITIALIZED(moved_from_type);
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(static_cast<void>(TypeView(moved_from_type).name()), _);
}

TEST(TypeView, DebugStringDebugDeath) {
  Type moved_from_type = AnyType();
  Type type = std::move(moved_from_type);
  IS_INITIALIZED(moved_from_type);
  static_cast<void>(type);
  std::ostringstream out;
  EXPECT_DEBUG_DEATH(static_cast<void>(out << TypeView(moved_from_type)), _);
}

TEST(TypeView, HashDebugDeath) {
  Type moved_from_type = AnyType();
  Type type = std::move(moved_from_type);
  IS_INITIALIZED(moved_from_type);
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(static_cast<void>(absl::HashOf(TypeView(moved_from_type))),
                     _);
}

TEST(TypeView, EqualDebugDeath) {
  Type moved_from_type = AnyType();
  Type type = std::move(moved_from_type);
  IS_INITIALIZED(moved_from_type);
  EXPECT_DEBUG_DEATH(
      static_cast<void>(TypeView(type) == TypeView(moved_from_type)), _);
  EXPECT_DEBUG_DEATH(
      static_cast<void>(TypeView(moved_from_type) == TypeView(type)), _);
}

TEST(TypeView, NativeTypeIdDebugDeath) {
  Type moved_from_type = AnyType();
  Type type = std::move(moved_from_type);
  IS_INITIALIZED(moved_from_type);
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(
      static_cast<void>(NativeTypeId::Of(TypeView(moved_from_type))), _);
}

TEST(TypeView, VerifyTypeImplementsAbslHashCorrectly) {
  MemoryManagerRef memory_manager = MemoryManagerRef::ReferenceCounting();
  auto list_type = ListType(memory_manager, DynType());
  auto map_type = MapType(memory_manager, DynType(), DynType());
  auto optional_type = OptionalType(memory_manager, DynType());
  auto struct_type = StructType(memory_manager, "test.Struct");
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly(
      {TypeView(AnyTypeView()),
       TypeView(BoolTypeView()),
       TypeView(BoolWrapperTypeView()),
       TypeView(BytesTypeView()),
       TypeView(BytesWrapperTypeView()),
       TypeView(DoubleTypeView()),
       TypeView(DoubleWrapperTypeView()),
       TypeView(DurationTypeView()),
       TypeView(DynTypeView()),
       TypeView(ErrorTypeView()),
       TypeView(IntTypeView()),
       TypeView(IntWrapperTypeView()),
       TypeView(ListTypeView(list_type)),
       TypeView(MapTypeView(map_type)),
       TypeView(NullTypeView()),
       TypeView(OptionalTypeView(optional_type)),
       TypeView(StringTypeView()),
       TypeView(StringWrapperTypeView()),
       TypeView(StructTypeView(struct_type)),
       TypeView(TimestampTypeView()),
       TypeView(TypeTypeView()),
       TypeView(UintTypeView()),
       TypeView(UintWrapperTypeView()),
       TypeView(UnknownTypeView())}));
}

TEST(Type, HashIsTransparent) {
  MemoryManagerRef memory_manager = MemoryManagerRef::ReferenceCounting();
  auto list_type = ListType(memory_manager, DynType());
  auto map_type = MapType(memory_manager, DynType(), DynType());
  auto optional_type = OptionalType(memory_manager, DynType());
  auto struct_type = StructType(memory_manager, "test.Struct");
  EXPECT_EQ(absl::HashOf(Type(AnyType())),
            absl::HashOf(TypeView(AnyTypeView())));
  EXPECT_EQ(absl::HashOf(Type(BoolType())),
            absl::HashOf(TypeView(BoolTypeView())));
  EXPECT_EQ(absl::HashOf(Type(BoolWrapperType())),
            absl::HashOf(TypeView(BoolWrapperTypeView())));
  EXPECT_EQ(absl::HashOf(Type(BytesType())),
            absl::HashOf(TypeView(BytesTypeView())));
  EXPECT_EQ(absl::HashOf(Type(BytesWrapperType())),
            absl::HashOf(TypeView(BytesWrapperTypeView())));
  EXPECT_EQ(absl::HashOf(Type(DoubleType())),
            absl::HashOf(TypeView(DoubleTypeView())));
  EXPECT_EQ(absl::HashOf(Type(DoubleWrapperType())),
            absl::HashOf(TypeView(DoubleWrapperTypeView())));
  EXPECT_EQ(absl::HashOf(Type(DurationType())),
            absl::HashOf(TypeView(DurationTypeView())));
  EXPECT_EQ(absl::HashOf(Type(DynType())),
            absl::HashOf(TypeView(DynTypeView())));
  EXPECT_EQ(absl::HashOf(Type(ErrorType())),
            absl::HashOf(TypeView(ErrorTypeView())));
  EXPECT_EQ(absl::HashOf(Type(IntType())),
            absl::HashOf(TypeView(IntTypeView())));
  EXPECT_EQ(absl::HashOf(Type(IntWrapperType())),
            absl::HashOf(TypeView(IntWrapperTypeView())));
  EXPECT_EQ(absl::HashOf(Type(ListType(list_type))),
            absl::HashOf(TypeView(ListTypeView(list_type))));
  EXPECT_EQ(absl::HashOf(Type(MapType(map_type))),
            absl::HashOf(TypeView(MapTypeView(map_type))));
  EXPECT_EQ(absl::HashOf(Type(NullType())),
            absl::HashOf(TypeView(NullTypeView())));
  EXPECT_EQ(absl::HashOf(Type(OptionalType(optional_type))),
            absl::HashOf(TypeView(OptionalTypeView(optional_type))));
  EXPECT_EQ(absl::HashOf(Type(StringType())),
            absl::HashOf(TypeView(StringTypeView())));
  EXPECT_EQ(absl::HashOf(Type(StringWrapperType())),
            absl::HashOf(TypeView(StringWrapperTypeView())));
  EXPECT_EQ(absl::HashOf(Type(StructType(struct_type))),
            absl::HashOf(TypeView(StructTypeView(struct_type))));
  EXPECT_EQ(absl::HashOf(Type(TimestampType())),
            absl::HashOf(TypeView(TimestampTypeView())));
  EXPECT_EQ(absl::HashOf(Type(TypeType())),
            absl::HashOf(TypeView(TypeTypeView())));
  EXPECT_EQ(absl::HashOf(Type(UintType())),
            absl::HashOf(TypeView(UintTypeView())));
  EXPECT_EQ(absl::HashOf(Type(UintWrapperType())),
            absl::HashOf(TypeView(UintWrapperTypeView())));
  EXPECT_EQ(absl::HashOf(Type(UnknownType())),
            absl::HashOf(TypeView(UnknownTypeView())));
}

}  // namespace
}  // namespace cel
