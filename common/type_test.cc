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

}  // namespace
}  // namespace cel
