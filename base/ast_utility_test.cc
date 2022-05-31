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

#include "base/ast_utility.h"

#include <cstdint>
#include <memory>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/text_format.h"
#include "absl/status/status.h"
#include "absl/time/time.h"
#include "absl/types/variant.h"
#include "base/ast.h"
#include "internal/testing.h"

namespace cel {
namespace ast {
namespace internal {
namespace {

TEST(AstUtilityTest, IdentToNative) {
  google::api::expr::v1alpha1::Expr expr;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        ident_expr { name: "name" }
      )pb",
      &expr));

  auto native_expr = ToNative(expr);

  ASSERT_TRUE(absl::holds_alternative<Ident>(native_expr->expr_kind()));
  EXPECT_EQ(absl::get<Ident>(native_expr->expr_kind()).name(), "name");
}

TEST(AstUtilityTest, SelectToNative) {
  google::api::expr::v1alpha1::Expr expr;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        select_expr {
          operand { ident_expr { name: "name" } }
          field: "field"
          test_only: true
        }
      )pb",
      &expr));

  auto native_expr = ToNative(expr);

  ASSERT_TRUE(absl::holds_alternative<Select>(native_expr->expr_kind()));
  auto& native_select = absl::get<Select>(native_expr->expr_kind());
  ASSERT_TRUE(
      absl::holds_alternative<Ident>(native_select.operand()->expr_kind()));
  EXPECT_EQ(absl::get<Ident>(native_select.operand()->expr_kind()).name(),
            "name");
  EXPECT_EQ(native_select.field(), "field");
  EXPECT_TRUE(native_select.test_only());
}

TEST(AstUtilityTest, CallToNative) {
  google::api::expr::v1alpha1::Expr expr;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        call_expr {
          target { ident_expr { name: "name" } }
          function: "function"
          args { ident_expr { name: "arg1" } }
          args { ident_expr { name: "arg2" } }
        }
      )pb",
      &expr));

  auto native_expr = ToNative(expr);

  ASSERT_TRUE(absl::holds_alternative<Call>(native_expr->expr_kind()));
  auto& native_call = absl::get<Call>(native_expr->expr_kind());
  ASSERT_TRUE(
      absl::holds_alternative<Ident>(native_call.target()->expr_kind()));
  EXPECT_EQ(absl::get<Ident>(native_call.target()->expr_kind()).name(), "name");
  EXPECT_EQ(native_call.function(), "function");
  auto& native_arg1 = native_call.args()[0];
  ASSERT_TRUE(absl::holds_alternative<Ident>(native_arg1.expr_kind()));
  EXPECT_EQ(absl::get<Ident>(native_arg1.expr_kind()).name(), "arg1");
  auto& native_arg2 = native_call.args()[1];
  ASSERT_TRUE(absl::holds_alternative<Ident>(native_arg2.expr_kind()));
  ASSERT_EQ(absl::get<Ident>(native_arg2.expr_kind()).name(), "arg2");
}

TEST(AstUtilityTest, CreateListToNative) {
  google::api::expr::v1alpha1::Expr expr;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        list_expr {
          elements { ident_expr { name: "elem1" } }
          elements { ident_expr { name: "elem2" } }
        }
      )pb",
      &expr));

  auto native_expr = ToNative(expr);

  ASSERT_TRUE(absl::holds_alternative<CreateList>(native_expr->expr_kind()));
  auto& native_create_list = absl::get<CreateList>(native_expr->expr_kind());
  auto& native_elem1 = native_create_list.elements()[0];
  ASSERT_TRUE(absl::holds_alternative<Ident>(native_elem1.expr_kind()));
  ASSERT_EQ(absl::get<Ident>(native_elem1.expr_kind()).name(), "elem1");
  auto& native_elem2 = native_create_list.elements()[1];
  ASSERT_TRUE(absl::holds_alternative<Ident>(native_elem2.expr_kind()));
  ASSERT_EQ(absl::get<Ident>(native_elem2.expr_kind()).name(), "elem2");
}

TEST(AstUtilityTest, CreateStructToNative) {
  google::api::expr::v1alpha1::Expr expr;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        struct_expr {
          entries {
            id: 1
            field_key: "key1"
            value { ident_expr { name: "value1" } }
          }
          entries {
            id: 2
            map_key { ident_expr { name: "key2" } }
            value { ident_expr { name: "value2" } }
          }
        }
      )pb",
      &expr));

  auto native_expr = ToNative(expr);

  ASSERT_TRUE(absl::holds_alternative<CreateStruct>(native_expr->expr_kind()));
  auto& native_struct = absl::get<CreateStruct>(native_expr->expr_kind());
  auto& native_entry1 = native_struct.entries()[0];
  EXPECT_EQ(native_entry1.id(), 1);
  ASSERT_TRUE(absl::holds_alternative<std::string>(native_entry1.key_kind()));
  ASSERT_EQ(absl::get<std::string>(native_entry1.key_kind()), "key1");
  ASSERT_TRUE(
      absl::holds_alternative<Ident>(native_entry1.value()->expr_kind()));
  ASSERT_EQ(absl::get<Ident>(native_entry1.value()->expr_kind()).name(),
            "value1");
  auto& native_entry2 = native_struct.entries()[1];
  EXPECT_EQ(native_entry2.id(), 2);
  ASSERT_TRUE(
      absl::holds_alternative<std::unique_ptr<Expr>>(native_entry2.key_kind()));
  ASSERT_TRUE(absl::holds_alternative<Ident>(
      absl::get<std::unique_ptr<Expr>>(native_entry2.key_kind())->expr_kind()));
  EXPECT_EQ(absl::get<Ident>(
                absl::get<std::unique_ptr<Expr>>(native_entry2.key_kind())
                    ->expr_kind())
                .name(),
            "key2");
  ASSERT_EQ(absl::get<Ident>(native_entry2.value()->expr_kind()).name(),
            "value2");
}

TEST(AstUtilityTest, CreateStructError) {
  google::api::expr::v1alpha1::Expr expr;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        struct_expr {
          entries {
            id: 1
            value { ident_expr { name: "value" } }
          }
        }
      )pb",
      &expr));

  auto native_expr = ToNative(expr);

  EXPECT_EQ(native_expr.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(native_expr.status().message(),
              ::testing::HasSubstr(
                  "Illegal type provided for "
                  "google::api::expr::v1alpha1::Expr::CreateStruct::Entry::key_kind."));
}

TEST(AstUtilityTest, ComprehensionToNative) {
  google::api::expr::v1alpha1::Expr expr;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        comprehension_expr {
          iter_var: "iter_var"
          iter_range { ident_expr { name: "iter_range" } }
          accu_var: "accu_var"
          accu_init { ident_expr { name: "accu_init" } }
          loop_condition { ident_expr { name: "loop_condition" } }
          loop_step { ident_expr { name: "loop_step" } }
          result { ident_expr { name: "result" } }
        }
      )pb",
      &expr));

  auto native_expr = ToNative(expr);

  ASSERT_TRUE(absl::holds_alternative<Comprehension>(native_expr->expr_kind()));
  auto& native_comprehension =
      absl::get<Comprehension>(native_expr->expr_kind());
  EXPECT_EQ(native_comprehension.iter_var(), "iter_var");
  ASSERT_TRUE(absl::holds_alternative<Ident>(
      native_comprehension.iter_range()->expr_kind()));
  EXPECT_EQ(
      absl::get<Ident>(native_comprehension.iter_range()->expr_kind()).name(),
      "iter_range");
  EXPECT_EQ(native_comprehension.accu_var(), "accu_var");
  ASSERT_TRUE(absl::holds_alternative<Ident>(
      native_comprehension.accu_init()->expr_kind()));
  EXPECT_EQ(
      absl::get<Ident>(native_comprehension.accu_init()->expr_kind()).name(),
      "accu_init");
  ASSERT_TRUE(absl::holds_alternative<Ident>(
      native_comprehension.loop_condition()->expr_kind()));
  EXPECT_EQ(absl::get<Ident>(native_comprehension.loop_condition()->expr_kind())
                .name(),
            "loop_condition");
  ASSERT_TRUE(absl::holds_alternative<Ident>(
      native_comprehension.loop_step()->expr_kind()));
  EXPECT_EQ(
      absl::get<Ident>(native_comprehension.loop_step()->expr_kind()).name(),
      "loop_step");
  ASSERT_TRUE(absl::holds_alternative<Ident>(
      native_comprehension.result()->expr_kind()));
  EXPECT_EQ(absl::get<Ident>(native_comprehension.result()->expr_kind()).name(),
            "result");
}

TEST(AstUtilityTest, ConstantToNative) {
  google::api::expr::v1alpha1::Expr expr;
  auto* constant = expr.mutable_const_expr();
  constant->set_null_value(google::protobuf::NULL_VALUE);

  auto native_expr = ToNative(expr);

  ASSERT_TRUE(absl::holds_alternative<Constant>(native_expr->expr_kind()));
  auto& native_constant = absl::get<Constant>(native_expr->expr_kind());
  ASSERT_TRUE(absl::holds_alternative<NullValue>(native_constant));
  EXPECT_EQ(absl::get<NullValue>(native_constant), NullValue::kNullValue);
}

TEST(AstUtilityTest, ConstantBoolTrueToNative) {
  google::api::expr::v1alpha1::Constant constant;
  constant.set_bool_value(true);

  auto native_constant = ToNative(constant);

  ASSERT_TRUE(absl::holds_alternative<bool>(*native_constant));
  EXPECT_TRUE(absl::get<bool>(*native_constant));
}

TEST(AstUtilityTest, ConstantBoolFalseToNative) {
  google::api::expr::v1alpha1::Constant constant;
  constant.set_bool_value(false);

  auto native_constant = ToNative(constant);

  ASSERT_TRUE(absl::holds_alternative<bool>(*native_constant));
  EXPECT_FALSE(absl::get<bool>(*native_constant));
}

TEST(AstUtilityTest, ConstantInt64ToNative) {
  google::api::expr::v1alpha1::Constant constant;
  constant.set_int64_value(-23);

  auto native_constant = ToNative(constant);

  ASSERT_TRUE(absl::holds_alternative<int64_t>(*native_constant));
  ASSERT_FALSE(absl::holds_alternative<uint64_t>(*native_constant));
  EXPECT_EQ(absl::get<int64_t>(*native_constant), -23);
}

TEST(AstUtilityTest, ConstantUint64ToNative) {
  google::api::expr::v1alpha1::Constant constant;
  constant.set_uint64_value(23);

  auto native_constant = ToNative(constant);

  ASSERT_TRUE(absl::holds_alternative<uint64_t>(*native_constant));
  ASSERT_FALSE(absl::holds_alternative<int64_t>(*native_constant));
  EXPECT_EQ(absl::get<uint64_t>(*native_constant), 23);
}

TEST(AstUtilityTest, ConstantDoubleToNative) {
  google::api::expr::v1alpha1::Constant constant;
  constant.set_double_value(12.34);

  auto native_constant = ToNative(constant);

  ASSERT_TRUE(absl::holds_alternative<double>(*native_constant));
  EXPECT_EQ(absl::get<double>(*native_constant), 12.34);
}

TEST(AstUtilityTest, ConstantStringToNative) {
  google::api::expr::v1alpha1::Constant constant;
  constant.set_string_value("string");

  auto native_constant = ToNative(constant);

  ASSERT_TRUE(absl::holds_alternative<std::string>(*native_constant));
  EXPECT_EQ(absl::get<std::string>(*native_constant), "string");
}

TEST(AstUtilityTest, ConstantBytesToNative) {
  google::api::expr::v1alpha1::Constant constant;
  constant.set_bytes_value("bytes");

  auto native_constant = ToNative(constant);

  ASSERT_TRUE(absl::holds_alternative<std::string>(*native_constant));
  EXPECT_EQ(absl::get<std::string>(*native_constant), "bytes");
}

TEST(AstUtilityTest, ConstantDurationToNative) {
  google::api::expr::v1alpha1::Constant constant;
  constant.mutable_duration_value()->set_seconds(123);
  constant.mutable_duration_value()->set_nanos(456);

  auto native_constant = ToNative(constant);

  ASSERT_TRUE(absl::holds_alternative<absl::Duration>(*native_constant));
  EXPECT_EQ(absl::get<absl::Duration>(*native_constant),
            absl::Seconds(123) + absl::Nanoseconds(456));
}

TEST(AstUtilityTest, ConstantTimestampToNative) {
  google::api::expr::v1alpha1::Constant constant;
  constant.mutable_timestamp_value()->set_seconds(123);
  constant.mutable_timestamp_value()->set_nanos(456);

  auto native_constant = ToNative(constant);

  ASSERT_TRUE(absl::holds_alternative<absl::Time>(*native_constant));
  EXPECT_EQ(absl::get<absl::Time>(*native_constant),
            absl::FromUnixSeconds(123) + absl::Nanoseconds(456));
}

TEST(AstUtilityTest, ConstantError) {
  auto native_constant = ToNative(google::api::expr::v1alpha1::Constant());

  EXPECT_EQ(native_constant.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(native_constant.status().message(),
              ::testing::HasSubstr(
                  "Illegal type supplied for google::api::expr::v1alpha1::Constant."));
}

TEST(AstUtilityTest, ExprError) {
  auto native_constant = ToNative(google::api::expr::v1alpha1::Expr());

  EXPECT_EQ(native_constant.status().code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(
      native_constant.status().message(),
      ::testing::HasSubstr(
          "Illegal type supplied for google::api::expr::v1alpha1::Expr::expr_kind."));
}

TEST(AstUtilityTest, SourceInfoToNative) {
  google::api::expr::v1alpha1::SourceInfo source_info;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        syntax_version: "version"
        location: "location"
        line_offsets: 1
        line_offsets: 2
        positions { key: 1 value: 2 }
        positions { key: 3 value: 4 }
        macro_calls {
          key: 1
          value { ident_expr { name: "name" } }
        }
      )pb",
      &source_info));

  auto native_source_info = ToNative(source_info);

  EXPECT_EQ(native_source_info->syntax_version(), "version");
  EXPECT_EQ(native_source_info->location(), "location");
  EXPECT_EQ(native_source_info->line_offsets(), std::vector<int32_t>({1, 2}));
  EXPECT_EQ(native_source_info->positions().at(1), 2);
  EXPECT_EQ(native_source_info->positions().at(3), 4);
  ASSERT_TRUE(absl::holds_alternative<Ident>(
      native_source_info->macro_calls().at(1).expr_kind()));
  ASSERT_EQ(
      absl::get<Ident>(native_source_info->macro_calls().at(1).expr_kind())
          .name(),
      "name");
}

TEST(AstUtilityTest, ParsedExprToNative) {
  google::api::expr::v1alpha1::ParsedExpr parsed_expr;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        expr { ident_expr { name: "name" } }
        source_info {
          syntax_version: "version"
          location: "location"
          line_offsets: 1
          line_offsets: 2
          positions { key: 1 value: 2 }
          positions { key: 3 value: 4 }
          macro_calls {
            key: 1
            value { ident_expr { name: "name" } }
          }
        }
      )pb",
      &parsed_expr));

  auto native_parsed_expr = ToNative(parsed_expr);

  ASSERT_TRUE(
      absl::holds_alternative<Ident>(native_parsed_expr->expr().expr_kind()));
  ASSERT_EQ(absl::get<Ident>(native_parsed_expr->expr().expr_kind()).name(),
            "name");
  auto& native_source_info = native_parsed_expr->source_info();
  EXPECT_EQ(native_source_info.syntax_version(), "version");
  EXPECT_EQ(native_source_info.location(), "location");
  EXPECT_EQ(native_source_info.line_offsets(), std::vector<int32_t>({1, 2}));
  EXPECT_EQ(native_source_info.positions().at(1), 2);
  EXPECT_EQ(native_source_info.positions().at(3), 4);
  ASSERT_TRUE(absl::holds_alternative<Ident>(
      native_source_info.macro_calls().at(1).expr_kind()));
  ASSERT_EQ(absl::get<Ident>(native_source_info.macro_calls().at(1).expr_kind())
                .name(),
            "name");
}

TEST(AstUtilityTest, PrimitiveTypeUnspecifiedToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_primitive(google::api::expr::v1alpha1::Type::PRIMITIVE_TYPE_UNSPECIFIED);

  auto native_type = ToNative(type);

  ASSERT_TRUE(absl::holds_alternative<PrimitiveType>(native_type->type_kind()));
  EXPECT_EQ(absl::get<PrimitiveType>(native_type->type_kind()),
            PrimitiveType::kPrimitiveTypeUnspecified);
}

TEST(AstUtilityTest, PrimitiveTypeBoolToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_primitive(google::api::expr::v1alpha1::Type::BOOL);

  auto native_type = ToNative(type);

  ASSERT_TRUE(absl::holds_alternative<PrimitiveType>(native_type->type_kind()));
  EXPECT_EQ(absl::get<PrimitiveType>(native_type->type_kind()),
            PrimitiveType::kBool);
}

TEST(AstUtilityTest, PrimitiveTypeInt64ToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_primitive(google::api::expr::v1alpha1::Type::INT64);

  auto native_type = ToNative(type);

  ASSERT_TRUE(absl::holds_alternative<PrimitiveType>(native_type->type_kind()));
  EXPECT_EQ(absl::get<PrimitiveType>(native_type->type_kind()),
            PrimitiveType::kInt64);
}

TEST(AstUtilityTest, PrimitiveTypeUint64ToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_primitive(google::api::expr::v1alpha1::Type::UINT64);

  auto native_type = ToNative(type);

  ASSERT_TRUE(absl::holds_alternative<PrimitiveType>(native_type->type_kind()));
  EXPECT_EQ(absl::get<PrimitiveType>(native_type->type_kind()),
            PrimitiveType::kUint64);
}

TEST(AstUtilityTest, PrimitiveTypeDoubleToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_primitive(google::api::expr::v1alpha1::Type::DOUBLE);

  auto native_type = ToNative(type);

  ASSERT_TRUE(absl::holds_alternative<PrimitiveType>(native_type->type_kind()));
  EXPECT_EQ(absl::get<PrimitiveType>(native_type->type_kind()),
            PrimitiveType::kDouble);
}

TEST(AstUtilityTest, PrimitiveTypeStringToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_primitive(google::api::expr::v1alpha1::Type::STRING);

  auto native_type = ToNative(type);

  ASSERT_TRUE(absl::holds_alternative<PrimitiveType>(native_type->type_kind()));
  EXPECT_EQ(absl::get<PrimitiveType>(native_type->type_kind()),
            PrimitiveType::kString);
}

TEST(AstUtilityTest, PrimitiveTypeBytesToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_primitive(google::api::expr::v1alpha1::Type::BYTES);

  auto native_type = ToNative(type);

  ASSERT_TRUE(absl::holds_alternative<PrimitiveType>(native_type->type_kind()));
  EXPECT_EQ(absl::get<PrimitiveType>(native_type->type_kind()),
            PrimitiveType::kBytes);
}

TEST(AstUtilityTest, PrimitiveTypeError) {
  google::api::expr::v1alpha1::Type type;
  type.set_primitive(::google::api::expr::v1alpha1::Type_PrimitiveType(7));

  auto native_type = ToNative(type);

  EXPECT_EQ(native_type.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(native_type.status().message(),
              ::testing::HasSubstr("Illegal type specified for "
                                   "google::api::expr::v1alpha1::Type::PrimitiveType."));
}

TEST(AstUtilityTest, WellKnownTypeUnspecifiedToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_well_known(google::api::expr::v1alpha1::Type::WELL_KNOWN_TYPE_UNSPECIFIED);

  auto native_type = ToNative(type);

  ASSERT_TRUE(absl::holds_alternative<WellKnownType>(native_type->type_kind()));
  EXPECT_EQ(absl::get<WellKnownType>(native_type->type_kind()),
            WellKnownType::kWellKnownTypeUnspecified);
}

TEST(AstUtilityTest, WellKnownTypeAnyToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_well_known(google::api::expr::v1alpha1::Type::ANY);

  auto native_type = ToNative(type);

  ASSERT_TRUE(absl::holds_alternative<WellKnownType>(native_type->type_kind()));
  EXPECT_EQ(absl::get<WellKnownType>(native_type->type_kind()),
            WellKnownType::kAny);
}

TEST(AstUtilityTest, WellKnownTypeTimestampToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_well_known(google::api::expr::v1alpha1::Type::TIMESTAMP);

  auto native_type = ToNative(type);

  ASSERT_TRUE(absl::holds_alternative<WellKnownType>(native_type->type_kind()));
  EXPECT_EQ(absl::get<WellKnownType>(native_type->type_kind()),
            WellKnownType::kTimestamp);
}

TEST(AstUtilityTest, WellKnownTypeDuraionToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_well_known(google::api::expr::v1alpha1::Type::DURATION);

  auto native_type = ToNative(type);

  ASSERT_TRUE(absl::holds_alternative<WellKnownType>(native_type->type_kind()));
  EXPECT_EQ(absl::get<WellKnownType>(native_type->type_kind()),
            WellKnownType::kDuration);
}

TEST(AstUtilityTest, WellKnownTypeError) {
  google::api::expr::v1alpha1::Type type;
  type.set_well_known(::google::api::expr::v1alpha1::Type_WellKnownType(4));

  auto native_type = ToNative(type);

  EXPECT_EQ(native_type.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(native_type.status().message(),
              ::testing::HasSubstr("Illegal type specified for "
                                   "google::api::expr::v1alpha1::Type::WellKnownType."));
}

TEST(AstUtilityTest, ListTypeToNative) {
  google::api::expr::v1alpha1::Type type;
  type.mutable_list_type()->mutable_elem_type()->set_primitive(
      google::api::expr::v1alpha1::Type::BOOL);

  auto native_type = ToNative(type);

  ASSERT_TRUE(absl::holds_alternative<ListType>(native_type->type_kind()));
  auto& native_list_type = absl::get<ListType>(native_type->type_kind());
  ASSERT_TRUE(absl::holds_alternative<PrimitiveType>(
      native_list_type.elem_type()->type_kind()));
  EXPECT_EQ(absl::get<PrimitiveType>(native_list_type.elem_type()->type_kind()),
            PrimitiveType::kBool);
}

TEST(AstUtilityTest, MapTypeToNative) {
  google::api::expr::v1alpha1::Type type;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        map_type {
          key_type { primitive: BOOL }
          value_type { primitive: DOUBLE }
        }
      )pb",
      &type));

  auto native_type = ToNative(type);

  ASSERT_TRUE(absl::holds_alternative<MapType>(native_type->type_kind()));
  auto& native_map_type = absl::get<MapType>(native_type->type_kind());
  ASSERT_TRUE(absl::holds_alternative<PrimitiveType>(
      native_map_type.key_type()->type_kind()));
  EXPECT_EQ(absl::get<PrimitiveType>(native_map_type.key_type()->type_kind()),
            PrimitiveType::kBool);
  ASSERT_TRUE(absl::holds_alternative<PrimitiveType>(
      native_map_type.value_type()->type_kind()));
  EXPECT_EQ(absl::get<PrimitiveType>(native_map_type.value_type()->type_kind()),
            PrimitiveType::kDouble);
}

TEST(AstUtilityTest, FunctionTypeToNative) {
  google::api::expr::v1alpha1::Type type;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        function {
          result_type { primitive: BOOL }
          arg_types { primitive: DOUBLE }
          arg_types { primitive: STRING }
        }
      )pb",
      &type));

  auto native_type = ToNative(type);

  ASSERT_TRUE(absl::holds_alternative<FunctionType>(native_type->type_kind()));
  auto& native_function_type =
      absl::get<FunctionType>(native_type->type_kind());
  ASSERT_TRUE(absl::holds_alternative<PrimitiveType>(
      native_function_type.result_type()->type_kind()));
  EXPECT_EQ(
      absl::get<PrimitiveType>(native_function_type.result_type()->type_kind()),
      PrimitiveType::kBool);
  ASSERT_TRUE(absl::holds_alternative<PrimitiveType>(
      native_function_type.arg_types().at(0).type_kind()));
  EXPECT_EQ(absl::get<PrimitiveType>(
                native_function_type.arg_types().at(0).type_kind()),
            PrimitiveType::kDouble);
  ASSERT_TRUE(absl::holds_alternative<PrimitiveType>(
      native_function_type.arg_types().at(1).type_kind()));
  EXPECT_EQ(absl::get<PrimitiveType>(
                native_function_type.arg_types().at(1).type_kind()),
            PrimitiveType::kString);
}

TEST(AstUtilityTest, AbstractTypeToNative) {
  google::api::expr::v1alpha1::Type type;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        abstract_type {
          name: "name"
          parameter_types { primitive: DOUBLE }
          parameter_types { primitive: STRING }
        }
      )pb",
      &type));

  auto native_type = ToNative(type);

  ASSERT_TRUE(absl::holds_alternative<AbstractType>(native_type->type_kind()));
  auto& native_abstract_type =
      absl::get<AbstractType>(native_type->type_kind());
  EXPECT_EQ(native_abstract_type.name(), "name");
  ASSERT_TRUE(absl::holds_alternative<PrimitiveType>(
      native_abstract_type.parameter_types().at(0).type_kind()));
  EXPECT_EQ(absl::get<PrimitiveType>(
                native_abstract_type.parameter_types().at(0).type_kind()),
            PrimitiveType::kDouble);
  ASSERT_TRUE(absl::holds_alternative<PrimitiveType>(
      native_abstract_type.parameter_types().at(1).type_kind()));
  EXPECT_EQ(absl::get<PrimitiveType>(
                native_abstract_type.parameter_types().at(1).type_kind()),
            PrimitiveType::kString);
}

TEST(AstUtilityTest, DynamicTypeToNative) {
  google::api::expr::v1alpha1::Type type;
  type.mutable_dyn();

  auto native_type = ToNative(type);

  ASSERT_TRUE(absl::holds_alternative<DynamicType>(native_type->type_kind()));
}

TEST(AstUtilityTest, NullTypeToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_null(google::protobuf::NULL_VALUE);

  auto native_type = ToNative(type);

  ASSERT_TRUE(absl::holds_alternative<NullValue>(native_type->type_kind()));
  EXPECT_EQ(absl::get<NullValue>(native_type->type_kind()),
            NullValue::kNullValue);
}

TEST(AstUtilityTest, PrimitiveTypeWrapperToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_wrapper(google::api::expr::v1alpha1::Type::BOOL);

  auto native_type = ToNative(type);

  ASSERT_TRUE(
      absl::holds_alternative<PrimitiveTypeWrapper>(native_type->type_kind()));
  EXPECT_EQ(absl::get<PrimitiveTypeWrapper>(native_type->type_kind()).type(),
            PrimitiveType::kBool);
}

TEST(AstUtilityTest, MessageTypeToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_message_type("message");

  auto native_type = ToNative(type);

  ASSERT_TRUE(absl::holds_alternative<MessageType>(native_type->type_kind()));
  EXPECT_EQ(absl::get<MessageType>(native_type->type_kind()).type(), "message");
}

TEST(AstUtilityTest, ParamTypeToNative) {
  google::api::expr::v1alpha1::Type type;
  type.set_type_param("param");

  auto native_type = ToNative(type);

  ASSERT_TRUE(absl::holds_alternative<ParamType>(native_type->type_kind()));
  EXPECT_EQ(absl::get<ParamType>(native_type->type_kind()).type(), "param");
}

TEST(AstUtilityTest, NestedTypeToNative) {
  google::api::expr::v1alpha1::Type type;
  type.mutable_type()->mutable_dyn();

  auto native_type = ToNative(type);

  ASSERT_TRUE(
      absl::holds_alternative<std::unique_ptr<Type>>(native_type->type_kind()));
  EXPECT_TRUE(absl::holds_alternative<DynamicType>(
      absl::get<std::unique_ptr<Type>>(native_type->type_kind())->type_kind()));
}

TEST(AstUtilityTest, TypeError) {
  auto native_type = ToNative(google::api::expr::v1alpha1::Type());

  EXPECT_EQ(native_type.status().code(), absl::StatusCode::kInvalidArgument);
  EXPECT_THAT(native_type.status().message(),
              ::testing::HasSubstr(
                  "Illegal type specified for google::api::expr::v1alpha1::Type."));
}

TEST(AstUtilityTest, ReferenceToNative) {
  google::api::expr::v1alpha1::Reference reference;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        name: "name"
        overload_id: "id1"
        overload_id: "id2"
        value { bool_value: true }
      )pb",
      &reference));

  auto native_reference = ToNative(reference);

  EXPECT_EQ(native_reference->name(), "name");
  EXPECT_EQ(native_reference->overload_id(),
            std::vector<std::string>({"id1", "id2"}));
  EXPECT_TRUE(absl::get<bool>(native_reference->value()));
}

TEST(AstUtilityTest, CheckedExprToNative) {
  google::api::expr::v1alpha1::CheckedExpr checked_expr;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(
        reference_map {
          key: 1
          value {
            name: "name"
            overload_id: "id1"
            overload_id: "id2"
            value { bool_value: true }
          }
        }
        type_map {
          key: 1
          value { dyn {} }
        }
        source_info {
          syntax_version: "version"
          location: "location"
          line_offsets: 1
          line_offsets: 2
          positions { key: 1 value: 2 }
          positions { key: 3 value: 4 }
          macro_calls {
            key: 1
            value { ident_expr { name: "name" } }
          }
        }
        expr_version: "version"
        expr { ident_expr { name: "expr" } }
      )pb",
      &checked_expr));

  auto native_checked_expr = ToNative(checked_expr);

  EXPECT_EQ(native_checked_expr->reference_map().at(1).name(), "name");
  EXPECT_EQ(native_checked_expr->reference_map().at(1).overload_id(),
            std::vector<std::string>({"id1", "id2"}));
  EXPECT_TRUE(
      absl::get<bool>(native_checked_expr->reference_map().at(1).value()));
  auto& native_source_info = native_checked_expr->source_info();
  EXPECT_EQ(native_source_info.syntax_version(), "version");
  EXPECT_EQ(native_source_info.location(), "location");
  EXPECT_EQ(native_source_info.line_offsets(), std::vector<int32_t>({1, 2}));
  EXPECT_EQ(native_source_info.positions().at(1), 2);
  EXPECT_EQ(native_source_info.positions().at(3), 4);
  ASSERT_TRUE(absl::holds_alternative<Ident>(
      native_source_info.macro_calls().at(1).expr_kind()));
  ASSERT_EQ(absl::get<Ident>(native_source_info.macro_calls().at(1).expr_kind())
                .name(),
            "name");
  EXPECT_EQ(native_checked_expr->expr_version(), "version");
  ASSERT_TRUE(
      absl::holds_alternative<Ident>(native_checked_expr->expr().expr_kind()));
  EXPECT_EQ(absl::get<Ident>(native_checked_expr->expr().expr_kind()).name(),
            "expr");
}

}  // namespace
}  // namespace internal
}  // namespace ast
}  // namespace cel
