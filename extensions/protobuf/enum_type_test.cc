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

#include "extensions/protobuf/enum_type.h"

#include <set>

#include "google/protobuf/type.pb.h"
#include "absl/status/status.h"
#include "base/internal/memory_manager_testing.h"
#include "base/kind.h"
#include "base/memory.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "extensions/protobuf/internal/testing.h"
#include "extensions/protobuf/type.h"
#include "extensions/protobuf/type_provider.h"
#include "internal/testing.h"
#include "google/protobuf/generated_enum_reflection.h"

namespace cel::extensions {
namespace {

using cel::internal::StatusIs;

using ProtoEnumTypeTest = ProtoTest<>;

TEST_P(ProtoEnumTypeTest, CreateStatically) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type,
      ProtoType::Resolve<google::protobuf::Field::Kind>(type_manager));
  EXPECT_TRUE(type->Is<EnumType>());
  EXPECT_TRUE(type->Is<ProtoEnumType>());
  EXPECT_EQ(type->kind(), Kind::kEnum);
  EXPECT_EQ(type->name(), "google.protobuf.Field.Kind");
  EXPECT_EQ(&type->descriptor(),
            google::protobuf::GetEnumDescriptor<google::protobuf::Field::Kind>());
}

TEST_P(ProtoEnumTypeTest, CreateDynamically) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type,
      ProtoType::Resolve(
          type_manager,
          *google::protobuf::GetEnumDescriptor<google::protobuf::Field::Kind>()));
  EXPECT_TRUE(type->Is<EnumType>());
  EXPECT_TRUE(type->Is<ProtoEnumType>());
  EXPECT_EQ(type->kind(), Kind::kEnum);
  EXPECT_EQ(type->name(), "google.protobuf.Field.Kind");
  EXPECT_EQ(&type.As<ProtoEnumType>()->descriptor(),
            google::protobuf::GetEnumDescriptor<google::protobuf::Field::Kind>());
}

TEST_P(ProtoEnumTypeTest, FindConstantByName) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type,
      ProtoType::Resolve<google::protobuf::Field::Kind>(type_manager));
  ASSERT_OK_AND_ASSIGN(auto constant, type->FindConstantByName("TYPE_STRING"));
  ASSERT_TRUE(constant.has_value());
  EXPECT_EQ(constant->number, 9);
  EXPECT_EQ(constant->name, "TYPE_STRING");
}

TEST_P(ProtoEnumTypeTest, FindConstantByNumber) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type,
      ProtoType::Resolve<google::protobuf::Field::Kind>(type_manager));
  ASSERT_OK_AND_ASSIGN(auto constant, type->FindConstantByNumber(9));
  ASSERT_TRUE(constant.has_value());
  EXPECT_EQ(constant->number, 9);
  EXPECT_EQ(constant->name, "TYPE_STRING");
}

TEST_P(ProtoEnumTypeTest, ConstantCount) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type,
      ProtoType::Resolve<google::protobuf::Field::Kind>(type_manager));
  EXPECT_EQ(type->constant_count(),
            google::protobuf::GetEnumDescriptor<google::protobuf::Field::Kind>()
                ->value_count());
}

TEST_P(ProtoEnumTypeTest, NewConstantIteratorNames) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type,
      ProtoType::Resolve<google::protobuf::Field::Kind>(type_manager));
  ASSERT_OK_AND_ASSIGN(auto iterator,
                       type->NewConstantIterator(memory_manager()));
  std::set<absl::string_view> actual_names;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(auto name, iterator->NextName());
    actual_names.insert(name);
  }
  EXPECT_THAT(iterator->Next(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  std::set<absl::string_view> expected_names;
  const auto* const descriptor =
      google::protobuf::GetEnumDescriptor<google::protobuf::Field::Kind>();
  for (int index = 0; index < descriptor->value_count(); index++) {
    expected_names.insert(descriptor->value(index)->name());
  }
  EXPECT_EQ(actual_names, expected_names);
}

TEST_P(ProtoEnumTypeTest, NewConstantIteratorNumbers) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type,
      ProtoType::Resolve<google::protobuf::Field::Kind>(type_manager));
  ASSERT_OK_AND_ASSIGN(auto iterator,
                       type->NewConstantIterator(memory_manager()));
  std::set<int64_t> actual_names;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(auto number, iterator->NextNumber());
    actual_names.insert(number);
  }
  EXPECT_THAT(iterator->Next(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  std::set<int64_t> expected_names;
  const auto* const descriptor =
      google::protobuf::GetEnumDescriptor<google::protobuf::Field::Kind>();
  for (int index = 0; index < descriptor->value_count(); index++) {
    expected_names.insert(descriptor->value(index)->number());
  }
  EXPECT_EQ(actual_names, expected_names);
}

INSTANTIATE_TEST_SUITE_P(ProtoEnumTypeTest, ProtoEnumTypeTest,
                         cel::base_internal::MemoryManagerTestModeAll(),
                         cel::base_internal::MemoryManagerTestModeTupleName);

}  // namespace
}  // namespace cel::extensions
