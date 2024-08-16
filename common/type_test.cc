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

#include "absl/hash/hash.h"
#include "absl/hash/hash_testing.h"
#include "absl/log/die_if_null.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "google/protobuf/arena.h"

namespace cel {
namespace {

using ::cel::internal::GetTestingDescriptorPool;
using testing::_;
using testing::An;
using testing::Optional;

TEST(Type, Enum) {
  EXPECT_EQ(
      Type::Enum(
          ABSL_DIE_IF_NULL(GetTestingDescriptorPool()->FindEnumTypeByName(
              "google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum"))),
      EnumType(ABSL_DIE_IF_NULL(GetTestingDescriptorPool()->FindEnumTypeByName(
          "google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum"))));
  EXPECT_EQ(Type::Enum(
                ABSL_DIE_IF_NULL(GetTestingDescriptorPool()->FindEnumTypeByName(
                    "google.protobuf.NullValue"))),
            NullType());
}

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

TEST(Type, Is) {
  google::protobuf::Arena arena;

  EXPECT_TRUE(Type(AnyType()).Is<AnyType>());

  EXPECT_TRUE(Type(BoolType()).Is<BoolType>());

  EXPECT_TRUE(Type(BoolWrapperType()).Is<BoolWrapperType>());
  EXPECT_TRUE(Type(BoolWrapperType()).IsWrapper());

  EXPECT_TRUE(Type(BytesType()).Is<BytesType>());

  EXPECT_TRUE(Type(BytesWrapperType()).Is<BytesWrapperType>());
  EXPECT_TRUE(Type(BytesWrapperType()).IsWrapper());

  EXPECT_TRUE(Type(DoubleType()).Is<DoubleType>());

  EXPECT_TRUE(Type(DoubleWrapperType()).Is<DoubleWrapperType>());
  EXPECT_TRUE(Type(DoubleWrapperType()).IsWrapper());

  EXPECT_TRUE(Type(DurationType()).Is<DurationType>());

  EXPECT_TRUE(Type(DynType()).Is<DynType>());

  EXPECT_TRUE(
      Type(EnumType(
               ABSL_DIE_IF_NULL(GetTestingDescriptorPool()->FindEnumTypeByName(
                   "google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum"))))
          .Is<EnumType>());

  EXPECT_TRUE(Type(ErrorType()).Is<ErrorType>());

  EXPECT_TRUE(Type(FunctionType(&arena, DynType(), {})).Is<FunctionType>());

  EXPECT_TRUE(Type(IntType()).Is<IntType>());

  EXPECT_TRUE(Type(IntWrapperType()).Is<IntWrapperType>());
  EXPECT_TRUE(Type(IntWrapperType()).IsWrapper());

  EXPECT_TRUE(Type(ListType()).Is<ListType>());

  EXPECT_TRUE(Type(MapType()).Is<MapType>());

  EXPECT_TRUE(Type(MessageType(ABSL_DIE_IF_NULL(
                       GetTestingDescriptorPool()->FindMessageTypeByName(
                           "google.api.expr.test.v1.proto3.TestAllTypes"))))
                  .IsStruct());
  EXPECT_TRUE(Type(MessageType(ABSL_DIE_IF_NULL(
                       GetTestingDescriptorPool()->FindMessageTypeByName(
                           "google.api.expr.test.v1.proto3.TestAllTypes"))))
                  .IsMessage());

  EXPECT_TRUE(Type(NullType()).Is<NullType>());

  EXPECT_TRUE(Type(OptionalType()).Is<OpaqueType>());
  EXPECT_TRUE(Type(OptionalType()).Is<OptionalType>());

  EXPECT_TRUE(Type(StringType()).Is<StringType>());

  EXPECT_TRUE(Type(StringWrapperType()).Is<StringWrapperType>());
  EXPECT_TRUE(Type(StringWrapperType()).IsWrapper());

  EXPECT_TRUE(Type(TimestampType()).Is<TimestampType>());

  EXPECT_TRUE(Type(UintType()).Is<UintType>());

  EXPECT_TRUE(Type(UintWrapperType()).Is<UintWrapperType>());
  EXPECT_TRUE(Type(UintWrapperType()).IsWrapper());

  EXPECT_TRUE(Type(UnknownType()).Is<UnknownType>());
}

TEST(Type, As) {
  google::protobuf::Arena arena;

  EXPECT_THAT(Type(AnyType()).As<AnyType>(), Optional(An<AnyType>()));

  EXPECT_THAT(Type(BoolType()).As<BoolType>(), Optional(An<BoolType>()));

  EXPECT_THAT(Type(BoolWrapperType()).As<BoolWrapperType>(),
              Optional(An<BoolWrapperType>()));

  EXPECT_THAT(Type(BytesType()).As<BytesType>(), Optional(An<BytesType>()));

  EXPECT_THAT(Type(BytesWrapperType()).As<BytesWrapperType>(),
              Optional(An<BytesWrapperType>()));

  EXPECT_THAT(Type(DoubleType()).As<DoubleType>(), Optional(An<DoubleType>()));

  EXPECT_THAT(Type(DoubleWrapperType()).As<DoubleWrapperType>(),
              Optional(An<DoubleWrapperType>()));

  EXPECT_THAT(Type(DurationType()).As<DurationType>(),
              Optional(An<DurationType>()));

  EXPECT_THAT(Type(DynType()).As<DynType>(), Optional(An<DynType>()));

  EXPECT_THAT(
      Type(EnumType(
               ABSL_DIE_IF_NULL(GetTestingDescriptorPool()->FindEnumTypeByName(
                   "google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum"))))
          .As<EnumType>(),
      Optional(An<EnumType>()));

  EXPECT_THAT(Type(ErrorType()).As<ErrorType>(), Optional(An<ErrorType>()));

  EXPECT_TRUE(Type(FunctionType(&arena, DynType(), {})).Is<FunctionType>());

  EXPECT_THAT(Type(IntType()).As<IntType>(), Optional(An<IntType>()));

  EXPECT_THAT(Type(IntWrapperType()).As<IntWrapperType>(),
              Optional(An<IntWrapperType>()));

  EXPECT_THAT(Type(ListType()).As<ListType>(), Optional(An<ListType>()));

  EXPECT_THAT(Type(MapType()).As<MapType>(), Optional(An<MapType>()));

  EXPECT_THAT(Type(MessageType(ABSL_DIE_IF_NULL(
                       GetTestingDescriptorPool()->FindMessageTypeByName(
                           "google.api.expr.test.v1.proto3.TestAllTypes"))))
                  .As<StructType>(),
              Optional(An<StructType>()));
  EXPECT_THAT(Type(MessageType(ABSL_DIE_IF_NULL(
                       GetTestingDescriptorPool()->FindMessageTypeByName(
                           "google.api.expr.test.v1.proto3.TestAllTypes"))))
                  .As<MessageType>(),
              Optional(An<MessageType>()));

  EXPECT_THAT(Type(NullType()).As<NullType>(), Optional(An<NullType>()));

  EXPECT_THAT(Type(OptionalType()).As<OptionalType>(),
              Optional(An<OptionalType>()));
  EXPECT_THAT(Type(OptionalType()).As<OptionalType>(),
              Optional(An<OptionalType>()));

  EXPECT_THAT(Type(StringType()).As<StringType>(), Optional(An<StringType>()));

  EXPECT_THAT(Type(StringWrapperType()).As<StringWrapperType>(),
              Optional(An<StringWrapperType>()));

  EXPECT_THAT(Type(TimestampType()).As<TimestampType>(),
              Optional(An<TimestampType>()));

  EXPECT_THAT(Type(UintType()).As<UintType>(), Optional(An<UintType>()));

  EXPECT_THAT(Type(UintWrapperType()).As<UintWrapperType>(),
              Optional(An<UintWrapperType>()));

  EXPECT_THAT(Type(UnknownType()).As<UnknownType>(),
              Optional(An<UnknownType>()));
}

TEST(Type, Cast) {
  google::protobuf::Arena arena;

  EXPECT_THAT(static_cast<AnyType>(Type(AnyType())), An<AnyType>());

  EXPECT_THAT(static_cast<BoolType>(Type(BoolType())), An<BoolType>());

  EXPECT_THAT(static_cast<BoolWrapperType>(Type(BoolWrapperType())),
              An<BoolWrapperType>());
  EXPECT_THAT(static_cast<BoolWrapperType>(Type(BoolWrapperType())),
              An<BoolWrapperType>());

  EXPECT_THAT(static_cast<BytesType>(Type(BytesType())), An<BytesType>());

  EXPECT_THAT(static_cast<BytesWrapperType>(Type(BytesWrapperType())),
              An<BytesWrapperType>());
  EXPECT_THAT(static_cast<BytesWrapperType>(Type(BytesWrapperType())),
              An<BytesWrapperType>());

  EXPECT_THAT(static_cast<DoubleType>(Type(DoubleType())), An<DoubleType>());

  EXPECT_THAT(static_cast<DoubleWrapperType>(Type(DoubleWrapperType())),
              An<DoubleWrapperType>());
  EXPECT_THAT(static_cast<DoubleWrapperType>(Type(DoubleWrapperType())),
              An<DoubleWrapperType>());

  EXPECT_THAT(static_cast<DurationType>(Type(DurationType())),
              An<DurationType>());

  EXPECT_THAT(static_cast<DynType>(Type(DynType())), An<DynType>());

  EXPECT_THAT(
      static_cast<EnumType>(Type(EnumType(
          ABSL_DIE_IF_NULL(GetTestingDescriptorPool()->FindEnumTypeByName(
              "google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum"))))),
      An<EnumType>());

  EXPECT_THAT(static_cast<ErrorType>(Type(ErrorType())), An<ErrorType>());

  EXPECT_TRUE(Type(FunctionType(&arena, DynType(), {})).Is<FunctionType>());

  EXPECT_THAT(static_cast<IntType>(Type(IntType())), An<IntType>());

  EXPECT_THAT(static_cast<IntWrapperType>(Type(IntWrapperType())),
              An<IntWrapperType>());
  EXPECT_THAT(static_cast<IntWrapperType>(Type(IntWrapperType())),
              An<IntWrapperType>());

  EXPECT_THAT(static_cast<ListType>(Type(ListType())), An<ListType>());

  EXPECT_THAT(static_cast<MapType>(Type(MapType())), An<MapType>());

  EXPECT_THAT(static_cast<StructType>(Type(MessageType(ABSL_DIE_IF_NULL(
                  GetTestingDescriptorPool()->FindMessageTypeByName(
                      "google.api.expr.test.v1.proto3.TestAllTypes"))))),
              An<StructType>());
  EXPECT_THAT(static_cast<MessageType>(Type(MessageType(ABSL_DIE_IF_NULL(
                  GetTestingDescriptorPool()->FindMessageTypeByName(
                      "google.api.expr.test.v1.proto3.TestAllTypes"))))),
              An<MessageType>());

  EXPECT_THAT(static_cast<NullType>(Type(NullType())), An<NullType>());

  EXPECT_THAT(static_cast<OptionalType>(Type(OptionalType())),
              An<OptionalType>());
  EXPECT_THAT(static_cast<OptionalType>(Type(OptionalType())),
              An<OptionalType>());

  EXPECT_THAT(static_cast<StringType>(Type(StringType())), An<StringType>());

  EXPECT_THAT(static_cast<StringWrapperType>(Type(StringWrapperType())),
              An<StringWrapperType>());
  EXPECT_THAT(static_cast<StringWrapperType>(Type(StringWrapperType())),
              An<StringWrapperType>());

  EXPECT_THAT(static_cast<TimestampType>(Type(TimestampType())),
              An<TimestampType>());

  EXPECT_THAT(static_cast<UintType>(Type(UintType())), An<UintType>());

  EXPECT_THAT(static_cast<UintWrapperType>(Type(UintWrapperType())),
              An<UintWrapperType>());
  EXPECT_THAT(static_cast<UintWrapperType>(Type(UintWrapperType())),
              An<UintWrapperType>());

  EXPECT_THAT(static_cast<UnknownType>(Type(UnknownType())), An<UnknownType>());
}

TEST(Type, VerifyTypeImplementsAbslHashCorrectly) {
  google::protobuf::Arena arena;
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
       Type(FunctionType(&arena, DynType(), {DynType()})),
       Type(IntType()),
       Type(IntWrapperType()),
       Type(ListType(&arena, DynType())),
       Type(MapType(&arena, DynType(), DynType())),
       Type(NullType()),
       Type(OptionalType(&arena, DynType())),
       Type(StringType()),
       Type(StringWrapperType()),
       Type(StructType(common_internal::MakeBasicStructType("test.Struct"))),
       Type(TimestampType()),
       Type(TypeParamType("T")),
       Type(TypeType()),
       Type(UintType()),
       Type(UintWrapperType()),
       Type(UnknownType())}));
}

}  // namespace
}  // namespace cel
