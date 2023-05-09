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

#include <set>
#include <utility>

#include "google/protobuf/type.pb.h"
#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "base/internal/memory_manager_testing.h"
#include "base/memory.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "base/types/list_type.h"
#include "base/types/map_type.h"
#include "extensions/protobuf/internal/testing.h"
#include "extensions/protobuf/type.h"
#include "extensions/protobuf/type_provider.h"
#include "internal/testing.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"

namespace cel::extensions {
namespace {

using cel::internal::StatusIs;

using TestAllTypes = google::api::expr::test::v1::proto3::TestAllTypes;

using ProtoStructTypeTest = ProtoTest<>;

TEST_P(ProtoStructTypeTest, CreateStatically) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type, ProtoType::Resolve<google::protobuf::Field>(type_manager));
  EXPECT_TRUE(type->Is<StructType>());
  EXPECT_TRUE(type->Is<ProtoStructType>());
  EXPECT_EQ(type->kind(), Kind::kStruct);
  EXPECT_EQ(type->name(), "google.protobuf.Field");
  EXPECT_EQ(&type->descriptor(), google::protobuf::Field::descriptor());
}

TEST_P(ProtoStructTypeTest, CreateDynamically) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type,
      ProtoType::Resolve(type_manager, *google::protobuf::Field::descriptor()));
  EXPECT_TRUE(type->Is<StructType>());
  EXPECT_TRUE(type->Is<ProtoStructType>());
  EXPECT_EQ(type->kind(), Kind::kStruct);
  EXPECT_EQ(type->name(), "google.protobuf.Field");
  EXPECT_EQ(&type.As<ProtoStructType>()->descriptor(),
            google::protobuf::Field::descriptor());
}

TEST_P(ProtoStructTypeTest, FindFieldByName) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type, ProtoType::Resolve<google::protobuf::Field>(type_manager));
  ASSERT_OK_AND_ASSIGN(
      auto field,
      type->FindField(type_manager, StructType::FieldId("default_value")));
  ASSERT_TRUE(field.has_value());
  EXPECT_EQ(field->number, 11);
  EXPECT_EQ(field->name, "default_value");
  EXPECT_EQ(field->type, type_factory.GetStringType());
}

TEST_P(ProtoStructTypeTest, FindFieldByNumber) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type, ProtoType::Resolve<google::protobuf::Field>(type_manager));
  ASSERT_OK_AND_ASSIGN(auto field,
                       type->FindField(type_manager, StructType::FieldId(11)));
  ASSERT_TRUE(field.has_value());
  EXPECT_EQ(field->number, 11);
  EXPECT_EQ(field->name, "default_value");
  EXPECT_EQ(field->type, type_factory.GetStringType());
}

TEST_P(ProtoStructTypeTest, EnumField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type, ProtoType::Resolve<google::protobuf::Field>(type_manager));
  ASSERT_OK_AND_ASSIGN(
      auto field,
      type->FindField(type_manager, StructType::FieldId("cardinality")));
  ASSERT_TRUE(field.has_value());
  EXPECT_TRUE(field->type->Is<EnumType>());
  EXPECT_EQ(field->type->name(), "google.protobuf.Field.Cardinality");
}

TEST_P(ProtoStructTypeTest, BoolField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type, ProtoType::Resolve<google::protobuf::Field>(type_manager));
  ASSERT_OK_AND_ASSIGN(
      auto field, type->FindField(type_manager, StructType::FieldId("packed")));
  ASSERT_TRUE(field.has_value());
  EXPECT_EQ(field->type, type_factory.GetBoolType());
}

TEST_P(ProtoStructTypeTest, IntField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type, ProtoType::Resolve<google::protobuf::Field>(type_manager));
  ASSERT_OK_AND_ASSIGN(
      auto field,
      type->FindField(type_manager, StructType::FieldId("oneof_index")));
  ASSERT_TRUE(field.has_value());
  EXPECT_EQ(field->type, type_factory.GetIntType());
}

TEST_P(ProtoStructTypeTest, StringListField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type, ProtoType::Resolve<google::protobuf::Type>(type_manager));
  ASSERT_OK_AND_ASSIGN(
      auto field, type->FindField(type_manager, StructType::FieldId("oneofs")));
  ASSERT_TRUE(field.has_value());
  EXPECT_TRUE(field->type->Is<ListType>());
  EXPECT_EQ(field->type.As<ListType>()->element(),
            type_factory.GetStringType());
}

TEST_P(ProtoStructTypeTest, StructListField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type, ProtoType::Resolve<google::protobuf::Field>(type_manager));
  ASSERT_OK_AND_ASSIGN(
      auto field,
      type->FindField(type_manager, StructType::FieldId("options")));
  ASSERT_TRUE(field.has_value());
  EXPECT_TRUE(field->type->Is<ListType>());
  EXPECT_EQ(field->type.As<ListType>()->element()->name(),
            "google.protobuf.Option");
}

TEST_P(ProtoStructTypeTest, MapField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(auto type,
                       ProtoType::Resolve<TestAllTypes>(type_manager));
  ASSERT_OK_AND_ASSIGN(
      auto field,
      type->FindField(type_manager, StructType::FieldId("map_string_string")));
  ASSERT_TRUE(field.has_value());
  EXPECT_TRUE(field->type->Is<MapType>());
  EXPECT_EQ(field->type.As<MapType>()->key(), type_factory.GetStringType());
  EXPECT_EQ(field->type.As<MapType>()->value(), type_factory.GetStringType());
}

TEST_P(ProtoStructTypeTest, NewFieldIteratorIds) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(auto type,
                       ProtoType::Resolve<TestAllTypes>(type_manager));
  ASSERT_OK_AND_ASSIGN(auto iterator, type->NewFieldIterator(memory_manager()));
  std::set<StructType::FieldId> actual_ids;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(auto id, iterator->NextId(type_manager));
    actual_ids.insert(id);
  }
  EXPECT_THAT(iterator->NextId(type_manager),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  std::set<StructType::FieldId> expected_ids;
  const auto* const descriptor = TestAllTypes::descriptor();
  for (int index = 0; index < descriptor->field_count(); ++index) {
    expected_ids.insert(StructType::FieldId(descriptor->field(index)->name()));
  }
  EXPECT_EQ(actual_ids, expected_ids);
}

TEST_P(ProtoStructTypeTest, NewFieldIteratorName) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(auto type,
                       ProtoType::Resolve<TestAllTypes>(type_manager));
  ASSERT_OK_AND_ASSIGN(auto iterator, type->NewFieldIterator(memory_manager()));
  std::set<absl::string_view> actual_names;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(auto name, iterator->NextName(type_manager));
    actual_names.insert(name);
  }
  EXPECT_THAT(iterator->NextName(type_manager),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  std::set<absl::string_view> expected_names;
  const auto* const descriptor = TestAllTypes::descriptor();
  for (int index = 0; index < descriptor->field_count(); ++index) {
    expected_names.insert(descriptor->field(index)->name());
  }
  EXPECT_EQ(actual_names, expected_names);
}

TEST_P(ProtoStructTypeTest, NewFieldIteratorNumbers) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(auto type,
                       ProtoType::Resolve<TestAllTypes>(type_manager));
  ASSERT_OK_AND_ASSIGN(auto iterator, type->NewFieldIterator(memory_manager()));
  std::set<int64_t> actual_numbers;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(auto number, iterator->NextNumber(type_manager));
    actual_numbers.insert(number);
  }
  EXPECT_THAT(iterator->NextNumber(type_manager),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  std::set<int64_t> expected_numbers;
  const auto* const descriptor = TestAllTypes::descriptor();
  for (int index = 0; index < descriptor->field_count(); ++index) {
    expected_numbers.insert(descriptor->field(index)->number());
  }
  EXPECT_EQ(actual_numbers, expected_numbers);
}

TEST_P(ProtoStructTypeTest, NewFieldIteratorTypes) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(auto type,
                       ProtoType::Resolve<TestAllTypes>(type_manager));
  ASSERT_OK_AND_ASSIGN(auto iterator, type->NewFieldIterator(memory_manager()));
  absl::flat_hash_set<Handle<Type>> actual_types;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(auto type, iterator->NextType(type_manager));
    actual_types.insert(std::move(type));
  }
  EXPECT_THAT(iterator->NextType(type_manager),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  // We cannot really test actual_types, as hand translating TestAllTypes would
  // be obnoxious. Otherwise we would simply be testing the same logic against
  // itself, which would not be useful.
}

INSTANTIATE_TEST_SUITE_P(ProtoStructTypeTest, ProtoStructTypeTest,
                         cel::base_internal::MemoryManagerTestModeAll(),
                         cel::base_internal::MemoryManagerTestModeTupleName);

}  // namespace
}  // namespace cel::extensions
