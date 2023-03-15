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

#include "extensions/protobuf/struct_type.h"

#include "google/protobuf/type.pb.h"
#include "base/memory_manager.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "base/types/list_type.h"
#include "base/types/map_type.h"
#include "extensions/protobuf/type_provider.h"
#include "internal/testing.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"

namespace cel::extensions {
namespace {

using TestAllTypes = google::api::expr::test::v1::proto3::TestAllTypes;

TEST(ProtoStructType, CreateStatically) {
  TypeFactory type_factory(MemoryManager::Global());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type,
      ProtoStructType::Resolve<google::protobuf::Field>(type_manager));
  EXPECT_TRUE(type->Is<StructType>());
  EXPECT_TRUE(type->Is<ProtoStructType>());
  EXPECT_EQ(type->kind(), Kind::kStruct);
  EXPECT_EQ(type->name(), "google.protobuf.Field");
  EXPECT_EQ(&type->descriptor(), google::protobuf::Field::descriptor());
}

TEST(ProtoStructType, CreateDynamically) {
  TypeFactory type_factory(MemoryManager::Global());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type, ProtoStructType::Resolve(
                     type_manager, *google::protobuf::Field::descriptor()));
  EXPECT_TRUE(type->Is<StructType>());
  EXPECT_TRUE(type->Is<ProtoStructType>());
  EXPECT_EQ(type->kind(), Kind::kStruct);
  EXPECT_EQ(type->name(), "google.protobuf.Field");
  EXPECT_EQ(&type->descriptor(), google::protobuf::Field::descriptor());
}

TEST(ProtoStructType, FindFieldByName) {
  TypeFactory type_factory(MemoryManager::Global());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type,
      ProtoStructType::Resolve<google::protobuf::Field>(type_manager));
  ASSERT_OK_AND_ASSIGN(
      auto field,
      type->FindField(type_manager, StructType::FieldId("default_value")));
  ASSERT_TRUE(field.has_value());
  EXPECT_EQ(field->number, 11);
  EXPECT_EQ(field->name, "default_value");
  EXPECT_EQ(field->type, type_factory.GetStringType());
}

TEST(ProtoStructType, FindFieldByNumber) {
  TypeFactory type_factory(MemoryManager::Global());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type,
      ProtoStructType::Resolve<google::protobuf::Field>(type_manager));
  ASSERT_OK_AND_ASSIGN(auto field,
                       type->FindField(type_manager, StructType::FieldId(11)));
  ASSERT_TRUE(field.has_value());
  EXPECT_EQ(field->number, 11);
  EXPECT_EQ(field->name, "default_value");
  EXPECT_EQ(field->type, type_factory.GetStringType());
}

TEST(ProtoStructType, EnumField) {
  TypeFactory type_factory(MemoryManager::Global());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type,
      ProtoStructType::Resolve<google::protobuf::Field>(type_manager));
  ASSERT_OK_AND_ASSIGN(
      auto field,
      type->FindField(type_manager, StructType::FieldId("cardinality")));
  ASSERT_TRUE(field.has_value());
  EXPECT_TRUE(field->type->Is<EnumType>());
  EXPECT_EQ(field->type->name(), "google.protobuf.Field.Cardinality");
}

TEST(ProtoStructType, BoolField) {
  TypeFactory type_factory(MemoryManager::Global());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type,
      ProtoStructType::Resolve<google::protobuf::Field>(type_manager));
  ASSERT_OK_AND_ASSIGN(
      auto field, type->FindField(type_manager, StructType::FieldId("packed")));
  ASSERT_TRUE(field.has_value());
  EXPECT_EQ(field->type, type_factory.GetBoolType());
}

TEST(ProtoStructType, IntField) {
  TypeFactory type_factory(MemoryManager::Global());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type,
      ProtoStructType::Resolve<google::protobuf::Field>(type_manager));
  ASSERT_OK_AND_ASSIGN(
      auto field,
      type->FindField(type_manager, StructType::FieldId("oneof_index")));
  ASSERT_TRUE(field.has_value());
  EXPECT_EQ(field->type, type_factory.GetIntType());
}

TEST(ProtoStructType, StringListField) {
  TypeFactory type_factory(MemoryManager::Global());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type,
      ProtoStructType::Resolve<google::protobuf::Type>(type_manager));
  ASSERT_OK_AND_ASSIGN(
      auto field, type->FindField(type_manager, StructType::FieldId("oneofs")));
  ASSERT_TRUE(field.has_value());
  EXPECT_TRUE(field->type->Is<ListType>());
  EXPECT_EQ(field->type.As<ListType>()->element(),
            type_factory.GetStringType());
}

TEST(ProtoStructType, StructListField) {
  TypeFactory type_factory(MemoryManager::Global());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type,
      ProtoStructType::Resolve<google::protobuf::Field>(type_manager));
  ASSERT_OK_AND_ASSIGN(
      auto field,
      type->FindField(type_manager, StructType::FieldId("options")));
  ASSERT_TRUE(field.has_value());
  EXPECT_TRUE(field->type->Is<ListType>());
  EXPECT_EQ(field->type.As<ListType>()->element()->name(),
            "google.protobuf.Option");
}

TEST(ProtoStructType, MapField) {
  TypeFactory type_factory(MemoryManager::Global());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(auto type,
                       ProtoStructType::Resolve<TestAllTypes>(type_manager));
  ASSERT_OK_AND_ASSIGN(
      auto field,
      type->FindField(type_manager, StructType::FieldId("map_string_string")));
  ASSERT_TRUE(field.has_value());
  EXPECT_TRUE(field->type->Is<MapType>());
  EXPECT_EQ(field->type.As<MapType>()->key(), type_factory.GetStringType());
  EXPECT_EQ(field->type.As<MapType>()->value(), type_factory.GetStringType());
}

}  // namespace
}  // namespace cel::extensions
