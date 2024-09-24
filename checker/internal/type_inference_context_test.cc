// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "checker/internal/type_inference_context.h"

#include <vector>

#include "common/decl.h"
#include "common/type.h"
#include "common/type_kind.h"
#include "internal/testing.h"
#include "google/protobuf/arena.h"

namespace cel::checker_internal {
namespace {

using ::testing::ElementsAre;
using ::testing::IsEmpty;
using ::testing::SafeMatcherCast;
using ::testing::SizeIs;

MATCHER_P(IsTypeParam, param, "") {
  const Type& got = arg;
  if (got.kind() != TypeKind::kTypeParam) {
    return false;
  }
  TypeParamType type = got.GetTypeParam();

  return type.name() == param;
}

MATCHER_P(IsListType, elems_matcher, "") {
  const Type& got = arg;
  if (got.kind() != TypeKind::kList) {
    return false;
  }
  ListType type = got.GetList();

  Type elem = type.element();
  return SafeMatcherCast<Type>(elems_matcher)
      .MatchAndExplain(elem, result_listener);
}

MATCHER_P2(IsMapType, key_matcher, value_matcher, "") {
  const Type& got = arg;
  if (got.kind() != TypeKind::kMap) {
    return false;
  }
  MapType type = got.GetMap();

  Type key = type.key();
  Type value = type.value();
  return SafeMatcherCast<Type>(key_matcher)
             .MatchAndExplain(key, result_listener) &&
         SafeMatcherCast<Type>(value_matcher)
             .MatchAndExplain(value, result_listener);
}

MATCHER_P(IsTypeKind, kind, "") {
  const Type& got = arg;
  TypeKind want_kind = kind;
  if (got.kind() == want_kind) {
    return true;
  }
  *result_listener << "got: " << TypeKindToString(got.kind());
  *result_listener << "\n";
  *result_listener << "wanted: " << TypeKindToString(want_kind);
  return false;
}

MATCHER_P(IsTypeType, matcher, "") {
  const Type& got = arg;

  if (got.kind() != TypeKind::kType) {
    return false;
  }

  TypeType type_type = got.GetType();
  if (type_type.GetParameters().size() != 1) {
    return false;
  }

  return SafeMatcherCast<Type>(matcher).MatchAndExplain(got.GetParameters()[0],
                                                        result_listener);
}

TEST(TypeInferenceContextTest, InstantiateTypeParams) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);
  Type type = context.InstantiateTypeParams(TypeParamType("MyType"));
  EXPECT_THAT(type, IsTypeParam("T%1"));
  Type type2 = context.InstantiateTypeParams(TypeParamType("MyType"));
  EXPECT_THAT(type2, IsTypeParam("T%2"));
}

TEST(TypeInferenceContextTest, InstantiateTypeParamsWithSubstitutions) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);
  TypeInferenceContext::InstanceMap instance_map;
  Type type =
      context.InstantiateTypeParams(TypeParamType("MyType"), instance_map);
  EXPECT_THAT(type, IsTypeParam("T%1"));
  Type type2 =
      context.InstantiateTypeParams(TypeParamType("MyType"), instance_map);
  EXPECT_THAT(type2, IsTypeParam("T%1"));
}

TEST(TypeInferenceContextTest, InstantiateTypeParamsUnparameterized) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  Type type = context.InstantiateTypeParams(IntType());
  EXPECT_TRUE(type.IsInt());
}

TEST(TypeInferenceContextTest, InstantiateTypeParamsList) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);
  Type list_type = ListType(&arena, TypeParamType("MyType"));

  Type type = context.InstantiateTypeParams(list_type);
  EXPECT_THAT(type, IsListType(IsTypeParam("T%1")));
}

TEST(TypeInferenceContextTest, InstantiateTypeParamsListPrimitive) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);
  Type list_type = ListType(&arena, IntType());

  Type type = context.InstantiateTypeParams(list_type);
  EXPECT_THAT(type, IsListType(IsTypeKind(TypeKind::kInt)));
}

TEST(TypeInferenceContextTest, InstantiateTypeParamsMap) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);
  Type map_type = MapType(&arena, TypeParamType("K"), TypeParamType("V"));

  Type type = context.InstantiateTypeParams(map_type);
  EXPECT_THAT(type, IsMapType(IsTypeParam("T%1"), IsTypeParam("T%2")));
}

TEST(TypeInferenceContextTest, InstantiateTypeParamsMapSameParam) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);
  Type map_type = MapType(&arena, TypeParamType("E"), TypeParamType("E"));

  Type type = context.InstantiateTypeParams(map_type);
  EXPECT_THAT(type, IsMapType(IsTypeParam("T%1"), IsTypeParam("T%1")));
}

TEST(TypeInferenceContextTest, InstantiateTypeParamsMapPrimitive) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);
  Type map_type = MapType(&arena, StringType(), IntType());

  Type type = context.InstantiateTypeParams(map_type);
  EXPECT_THAT(type, IsMapType(IsTypeKind(TypeKind::kString),
                              IsTypeKind(TypeKind::kInt)));
}

TEST(TypeInferenceContextTest, InstantiateTypeParamsType) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);
  Type type_type = TypeType(&arena, TypeParamType("T"));

  Type type = context.InstantiateTypeParams(type_type);
  EXPECT_THAT(type, IsTypeType(IsTypeParam("T%1")));
}

TEST(TypeInferenceContextTest, InstantiateTypeParamsTypeEmpty) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);
  Type type_type = TypeType();

  Type type = context.InstantiateTypeParams(type_type);
  EXPECT_THAT(type, IsTypeKind(TypeKind::kType));
  EXPECT_THAT(type.AsType()->GetParameters(), IsEmpty());
}

TEST(TypeInferenceContextTest, InstantiateTypeParamsOpaque) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);
  std::vector<Type> parameters = {TypeParamType("T"), IntType(),
                                  TypeParamType("U"), TypeParamType("T")};

  Type type_type = OpaqueType(&arena, "MyTuple", parameters);

  Type type = context.InstantiateTypeParams(type_type);
  ASSERT_THAT(type, IsTypeKind(TypeKind::kOpaque));
  EXPECT_EQ(type.AsOpaque()->name(), "MyTuple");
  EXPECT_THAT(type.AsOpaque()->GetParameters(),
              ElementsAre(IsTypeParam("T%1"), IsTypeKind(TypeKind::kInt),
                          IsTypeParam("T%2"), IsTypeParam("T%1")));
}

// TODO: Does not consider any substitutions based on type
// inferences yet.
TEST(TypeInferenceContextTest, OpaqueTypeAssignable) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);
  std::vector<Type> parameters = {TypeParamType("T"), IntType()};

  Type type_type = OpaqueType(&arena, "MyTuple", parameters);

  Type type = context.InstantiateTypeParams(type_type);
  ASSERT_THAT(type, IsTypeKind(TypeKind::kOpaque));
  EXPECT_TRUE(context.IsAssignable(type, type));
}

TEST(TypeInferenceContextTest, WrapperTypeAssignable) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  EXPECT_TRUE(context.IsAssignable(StringWrapperType(), StringType()));
  EXPECT_TRUE(context.IsAssignable(StringWrapperType(), NullType()));
}

TEST(TypeInferenceContextTest, MismatchedTypeNotAssignable) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  EXPECT_FALSE(context.IsAssignable(StringWrapperType(), IntType()));
}

TEST(TypeInferenceContextTest, OverloadResolution) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  ASSERT_OK_AND_ASSIGN(
      auto decl,
      MakeFunctionDecl(
          "foo",
          MakeOverloadDecl("foo_int_int", IntType(), IntType(), IntType()),
          MakeOverloadDecl("foo_double_double", DoubleType(), DoubleType(),
                           DoubleType())));

  auto resolution = context.ResolveOverload(decl, {IntType(), IntType()},
                                            /*is_receiver=*/false);
  ASSERT_TRUE(resolution.has_value());
  EXPECT_THAT(resolution->result_type, IsTypeKind(TypeKind::kInt));
  EXPECT_THAT(resolution->overloads, SizeIs(1));
}

TEST(TypeInferenceContextTest, MultipleOverloadsResultTypeDyn) {
  google::protobuf::Arena arena;
  TypeInferenceContext context(&arena);

  ASSERT_OK_AND_ASSIGN(
      auto decl,
      MakeFunctionDecl(
          "foo",
          MakeOverloadDecl("foo_int_int", IntType(), IntType(), IntType()),
          MakeOverloadDecl("foo_double_double", DoubleType(), DoubleType(),
                           DoubleType())));

  auto resolution = context.ResolveOverload(decl, {DynType(), DynType()},
                                            /*is_receiver=*/false);
  ASSERT_TRUE(resolution.has_value());
  EXPECT_THAT(resolution->result_type, IsTypeKind(TypeKind::kDyn));
  EXPECT_THAT(resolution->overloads, SizeIs(2));
}

}  // namespace
}  // namespace cel::checker_internal
