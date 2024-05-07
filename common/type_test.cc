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

#include "absl/hash/hash.h"
#include "absl/hash/hash_testing.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::_;

TEST(Type, KindDebugDeath) {
  Type type;
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(static_cast<void>(type.kind()), _);
}

TEST(Type, NameDebugDeath) {
  Type type;
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(static_cast<void>(type.name()), _);
}

TEST(Type, DebugStringDebugDeath) {
  Type type;
  std::ostringstream out;
  EXPECT_DEBUG_DEATH(static_cast<void>(out << type), _);
}

TEST(Type, HashDebugDeath) {
  Type type;
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(static_cast<void>(absl::HashOf(type)), _);
}

TEST(Type, EqualDebugDeath) {
  Type type;
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(static_cast<void>(type == type), _);
}

TEST(Type, NativeTypeIdDebugDeath) {
  Type type;
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(static_cast<void>(NativeTypeId::Of(type)), _);
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
       Type(FunctionType(memory_manager, DynType(), {DynType()})),
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
       Type(TypeParamType(memory_manager, "T")),
       Type(TypeType()),
       Type(UintType()),
       Type(UintWrapperType()),
       Type(UnknownType())}));
}

TEST(TypeView, KindDebugDeath) {
  TypeView type;
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(static_cast<void>(type.kind()), _);
}

TEST(TypeView, NameDebugDeath) {
  TypeView type;
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(static_cast<void>(type.name()), _);
}

TEST(TypeView, DebugStringDebugDeath) {
  TypeView type;
  static_cast<void>(type);
  std::ostringstream out;
  EXPECT_DEBUG_DEATH(static_cast<void>(out << type), _);
}

TEST(TypeView, HashDebugDeath) {
  TypeView type;
  static_cast<void>(type);
  EXPECT_DEBUG_DEATH(static_cast<void>(absl::HashOf(type)), _);
}

TEST(TypeView, EqualDebugDeath) {
  TypeView type;
  EXPECT_DEBUG_DEATH(static_cast<void>(TypeView(type) == type), _);
  EXPECT_DEBUG_DEATH(static_cast<void>(type == TypeView(type)), _);
}

TEST(TypeView, NativeTypeIdDebugDeath) {
  TypeView type;
  EXPECT_DEBUG_DEATH(static_cast<void>(NativeTypeId::Of(type)), _);
}

TEST(TypeView, VerifyTypeImplementsAbslHashCorrectly) {
  MemoryManagerRef memory_manager = MemoryManagerRef::ReferenceCounting();
  auto function_type = FunctionType(memory_manager, DynType(), {DynType()});
  auto list_type = ListType(memory_manager, DynType());
  auto map_type = MapType(memory_manager, DynType(), DynType());
  auto optional_type = OptionalType(memory_manager, DynType());
  auto struct_type = StructType(memory_manager, "test.Struct");
  auto type_param_type = TypeParamType(memory_manager, "T");
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
       TypeView(FunctionTypeView(function_type)),
       TypeView(IntTypeView()),
       TypeView(IntWrapperTypeView()),
       TypeView(ListTypeView(list_type)),
       TypeView(MapTypeView(map_type)),
       TypeView(NullTypeView()),
       TypeView(OptionalTypeView(optional_type)),
       TypeView(StringTypeView()),
       TypeView(StringWrapperTypeView()),
       TypeView(StructTypeView(struct_type)),
       TypeView(TypeParamTypeView(type_param_type)),
       TypeView(TimestampTypeView()),
       TypeView(TypeTypeView()),
       TypeView(UintTypeView()),
       TypeView(UintWrapperTypeView()),
       TypeView(UnknownTypeView())}));
}

TEST(Type, HashIsTransparent) {
  MemoryManagerRef memory_manager = MemoryManagerRef::ReferenceCounting();
  auto function_type = FunctionType(memory_manager, DynType(), {DynType()});
  auto list_type = ListType(memory_manager, DynType());
  auto map_type = MapType(memory_manager, DynType(), DynType());
  auto optional_type = OptionalType(memory_manager, DynType());
  auto struct_type = StructType(memory_manager, "test.Struct");
  auto type_param_type = TypeParamType(memory_manager, "T");
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
  EXPECT_EQ(absl::HashOf(Type(FunctionType(function_type))),
            absl::HashOf(TypeView(FunctionTypeView(function_type))));
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
  EXPECT_EQ(absl::HashOf(Type(TypeParamType(type_param_type))),
            absl::HashOf(TypeView(TypeParamTypeView(type_param_type))));
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
