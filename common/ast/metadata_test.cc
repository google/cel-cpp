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

#include "common/ast/metadata.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/types/variant.h"
#include "common/expr.h"
#include "internal/testing.h"

namespace cel {
namespace {

TEST(AstTest, ListTypeSpecifierMutableConstruction) {
  ListTypeSpecifier type;
  type.mutable_elem_type() = TypeSpecifier(PrimitiveType::kBool);
  EXPECT_EQ(absl::get<PrimitiveType>(type.elem_type().type_kind()),
            PrimitiveType::kBool);
}

TEST(AstTest, MapTypeSpecifierMutableConstruction) {
  MapTypeSpecifier type;
  type.mutable_key_type() = TypeSpecifier(PrimitiveType::kBool);
  type.mutable_value_type() = TypeSpecifier(PrimitiveType::kBool);
  EXPECT_EQ(absl::get<PrimitiveType>(type.key_type().type_kind()),
            PrimitiveType::kBool);
  EXPECT_EQ(absl::get<PrimitiveType>(type.value_type().type_kind()),
            PrimitiveType::kBool);
}

TEST(AstTest, MapTypeSpecifierComparatorKeyType) {
  MapTypeSpecifier type;
  type.mutable_key_type() = TypeSpecifier(PrimitiveType::kBool);
  EXPECT_FALSE(type == MapTypeSpecifier());
}

TEST(AstTest, MapTypeSpecifierComparatorValueType) {
  MapTypeSpecifier type;
  type.mutable_value_type() = TypeSpecifier(PrimitiveType::kBool);
  EXPECT_FALSE(type == MapTypeSpecifier());
}

TEST(AstTest, FunctionTypeSpecifierMutableConstruction) {
  FunctionTypeSpecifier type;
  type.mutable_result_type() = TypeSpecifier(PrimitiveType::kBool);
  EXPECT_EQ(absl::get<PrimitiveType>(type.result_type().type_kind()),
            PrimitiveType::kBool);
}

TEST(AstTest, FunctionTypeSpecifierComparatorArgTypes) {
  FunctionTypeSpecifier type;
  type.mutable_arg_types().emplace_back(TypeSpecifier());
  EXPECT_FALSE(type == FunctionTypeSpecifier());
}

TEST(AstTest, ListTypeSpecifierDefaults) {
  EXPECT_EQ(ListTypeSpecifier().elem_type(), TypeSpecifier());
}

TEST(AstTest, MapTypeSpecifierDefaults) {
  EXPECT_EQ(MapTypeSpecifier().key_type(), TypeSpecifier());
  EXPECT_EQ(MapTypeSpecifier().value_type(), TypeSpecifier());
}

TEST(AstTest, FunctionTypeSpecifierDefaults) {
  EXPECT_EQ(FunctionTypeSpecifier().result_type(), TypeSpecifier());
}

TEST(AstTest, TypeDefaults) {
  EXPECT_EQ(TypeSpecifier().null(), NullTypeSpecifier());
  EXPECT_EQ(TypeSpecifier().primitive(),
            PrimitiveType::kPrimitiveTypeUnspecified);
  EXPECT_EQ(TypeSpecifier().wrapper(),
            PrimitiveType::kPrimitiveTypeUnspecified);
  EXPECT_EQ(TypeSpecifier().well_known(),
            WellKnownTypeSpecifier::kWellKnownTypeUnspecified);
  EXPECT_EQ(TypeSpecifier().list_type(), ListTypeSpecifier());
  EXPECT_EQ(TypeSpecifier().map_type(), MapTypeSpecifier());
  EXPECT_EQ(TypeSpecifier().function(), FunctionTypeSpecifier());
  EXPECT_EQ(TypeSpecifier().message_type(), MessageTypeSpecifier());
  EXPECT_EQ(TypeSpecifier().type_param(), ParamTypeSpecifier());
  EXPECT_EQ(TypeSpecifier().type(), TypeSpecifier());
  EXPECT_EQ(TypeSpecifier().error_type(), ErrorTypeSpecifier());
  EXPECT_EQ(TypeSpecifier().abstract_type(), AbstractType());
}

TEST(AstTest, TypeComparatorTest) {
  TypeSpecifier type;
  type.set_type_kind(std::make_unique<TypeSpecifier>(PrimitiveType::kBool));

  EXPECT_TRUE(type == TypeSpecifier(std::make_unique<TypeSpecifier>(
                          PrimitiveType::kBool)));
  EXPECT_FALSE(type == TypeSpecifier(PrimitiveType::kBool));
  EXPECT_FALSE(type == TypeSpecifier(std::unique_ptr<TypeSpecifier>()));
  EXPECT_FALSE(type == TypeSpecifier(std::make_unique<TypeSpecifier>(
                           PrimitiveType::kInt64)));
}

TEST(AstTest, ExprMutableConstruction) {
  Expr expr;
  expr.mutable_const_expr().set_bool_value(true);
  ASSERT_TRUE(expr.has_const_expr());
  EXPECT_TRUE(expr.const_expr().bool_value());
  expr.mutable_ident_expr().set_name("expr");
  ASSERT_TRUE(expr.has_ident_expr());
  EXPECT_FALSE(expr.has_const_expr());
  EXPECT_EQ(expr.ident_expr().name(), "expr");
  expr.mutable_select_expr().set_field("field");
  ASSERT_TRUE(expr.has_select_expr());
  EXPECT_FALSE(expr.has_ident_expr());
  EXPECT_EQ(expr.select_expr().field(), "field");
  expr.mutable_call_expr().set_function("function");
  ASSERT_TRUE(expr.has_call_expr());
  EXPECT_FALSE(expr.has_select_expr());
  EXPECT_EQ(expr.call_expr().function(), "function");
  expr.mutable_list_expr();
  EXPECT_TRUE(expr.has_list_expr());
  EXPECT_FALSE(expr.has_call_expr());
  expr.mutable_struct_expr().set_name("name");
  ASSERT_TRUE(expr.has_struct_expr());
  EXPECT_EQ(expr.struct_expr().name(), "name");
  EXPECT_FALSE(expr.has_list_expr());
  expr.mutable_comprehension_expr().set_accu_var("accu_var");
  ASSERT_TRUE(expr.has_comprehension_expr());
  EXPECT_FALSE(expr.has_list_expr());
  EXPECT_EQ(expr.comprehension_expr().accu_var(), "accu_var");
}

TEST(AstTest, ReferenceConstantDefaultValue) {
  Reference reference;
  EXPECT_EQ(reference.value(), Constant());
}

TEST(AstTest, TypeCopyable) {
  TypeSpecifier type = TypeSpecifier(PrimitiveType::kBool);
  TypeSpecifier type2 = type;
  EXPECT_TRUE(type2.has_primitive());
  EXPECT_EQ(type2, type);

  type = TypeSpecifier(
      ListTypeSpecifier(std::make_unique<TypeSpecifier>(PrimitiveType::kBool)));
  type2 = type;
  EXPECT_TRUE(type2.has_list_type());
  EXPECT_EQ(type2, type);

  type = TypeSpecifier(
      MapTypeSpecifier(std::make_unique<TypeSpecifier>(PrimitiveType::kBool),
                       std::make_unique<TypeSpecifier>(PrimitiveType::kBool)));
  type2 = type;
  EXPECT_TRUE(type2.has_map_type());
  EXPECT_EQ(type2, type);

  type = TypeSpecifier(FunctionTypeSpecifier(
      std::make_unique<TypeSpecifier>(PrimitiveType::kBool), {}));
  type2 = type;
  EXPECT_TRUE(type2.has_function());
  EXPECT_EQ(type2, type);

  type = TypeSpecifier(
      AbstractType("optional", {TypeSpecifier(PrimitiveType::kBool)}));
  type2 = type;
  EXPECT_TRUE(type2.has_abstract_type());
  EXPECT_EQ(type2, type);
}

TEST(AstTest, TypeMoveable) {
  TypeSpecifier type = TypeSpecifier(PrimitiveType::kBool);
  TypeSpecifier type2 = type;
  TypeSpecifier type3 = std::move(type);
  EXPECT_TRUE(type2.has_primitive());
  EXPECT_EQ(type2, type3);

  type = TypeSpecifier(
      ListTypeSpecifier(std::make_unique<TypeSpecifier>(PrimitiveType::kBool)));
  type2 = type;
  type3 = std::move(type);
  EXPECT_TRUE(type2.has_list_type());
  EXPECT_EQ(type2, type3);

  type = TypeSpecifier(
      MapTypeSpecifier(std::make_unique<TypeSpecifier>(PrimitiveType::kBool),
                       std::make_unique<TypeSpecifier>(PrimitiveType::kBool)));
  type2 = type;
  type3 = std::move(type);
  EXPECT_TRUE(type2.has_map_type());
  EXPECT_EQ(type2, type3);

  type = TypeSpecifier(FunctionTypeSpecifier(
      std::make_unique<TypeSpecifier>(PrimitiveType::kBool), {}));
  type2 = type;
  type3 = std::move(type);
  EXPECT_TRUE(type2.has_function());
  EXPECT_EQ(type2, type3);

  type = TypeSpecifier(
      AbstractType("optional", {TypeSpecifier(PrimitiveType::kBool)}));
  type2 = type;
  type3 = std::move(type);
  EXPECT_TRUE(type2.has_abstract_type());
  EXPECT_EQ(type2, type3);
}

TEST(AstTest, NestedTypeKindCopyAssignable) {
  ListTypeSpecifier list_type(
      std::make_unique<TypeSpecifier>(PrimitiveType::kBool));
  ListTypeSpecifier list_type2;
  list_type2 = list_type;

  EXPECT_EQ(list_type2, list_type);

  MapTypeSpecifier map_type(
      std::make_unique<TypeSpecifier>(PrimitiveType::kBool),
      std::make_unique<TypeSpecifier>(PrimitiveType::kBool));
  MapTypeSpecifier map_type2;
  map_type2 = map_type;

  AbstractType abstract_type("abstract", {TypeSpecifier(PrimitiveType::kBool),
                                          TypeSpecifier(PrimitiveType::kBool)});
  AbstractType abstract_type2;
  abstract_type2 = abstract_type;

  EXPECT_EQ(abstract_type2, abstract_type);

  FunctionTypeSpecifier function_type(
      std::make_unique<TypeSpecifier>(PrimitiveType::kBool),
      {TypeSpecifier(PrimitiveType::kBool),
       TypeSpecifier(PrimitiveType::kBool)});
  FunctionTypeSpecifier function_type2;
  function_type2 = function_type;

  EXPECT_EQ(function_type2, function_type);
}

TEST(AstTest, ExtensionSupported) {
  SourceInfo source_info;

  source_info.mutable_extensions().push_back(
      ExtensionSpecifier("constant_folding", nullptr, {}));

  EXPECT_EQ(source_info.extensions()[0],
            ExtensionSpecifier("constant_folding", nullptr, {}));
}

TEST(AstTest, ExtensionSpecifierEquality) {
  ExtensionSpecifier extension1("constant_folding", nullptr, {});

  EXPECT_EQ(extension1, ExtensionSpecifier("constant_folding", nullptr, {}));

  EXPECT_NE(extension1,
            ExtensionSpecifier(
                "constant_folding",
                std::make_unique<ExtensionSpecifier::Version>(1, 0), {}));
  EXPECT_NE(extension1,
            ExtensionSpecifier("constant_folding", nullptr,
                               {ExtensionSpecifier::Component::kRuntime}));

  EXPECT_EQ(extension1,
            ExtensionSpecifier(
                "constant_folding",
                std::make_unique<ExtensionSpecifier::Version>(0, 0), {}));
}

}  // namespace
}  // namespace cel
