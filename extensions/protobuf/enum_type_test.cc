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

#include "google/protobuf/type.pb.h"
#include "google/protobuf/generated_enum_reflection.h"
#include "base/kind.h"
#include "base/memory_manager.h"
#include "base/type_factory.h"
#include "internal/testing.h"

namespace cel::extensions {
namespace {

TEST(ProtoEnumType, CreateStatically) {
  TypeFactory type_factory(MemoryManager::Global());
  ASSERT_OK_AND_ASSIGN(
      auto type,
      ProtoEnumType::Create<google::protobuf::Field::Kind>(type_factory));
  EXPECT_TRUE(type.Is<EnumType>());
  EXPECT_TRUE(type.Is<ProtoEnumType>());
  EXPECT_EQ(type->kind(), Kind::kEnum);
  EXPECT_EQ(type->name(), "google.protobuf.Field.Kind");
  EXPECT_EQ(&type->descriptor(),
            google::protobuf::GetEnumDescriptor<google::protobuf::Field::Kind>());
}

TEST(ProtoEnumType, CreateDynamically) {
  TypeFactory type_factory(MemoryManager::Global());
  ASSERT_OK_AND_ASSIGN(
      auto type,
      ProtoEnumType::Create(
          type_factory,
          google::protobuf::GetEnumDescriptor<google::protobuf::Field::Kind>()));
  EXPECT_TRUE(type.Is<EnumType>());
  EXPECT_TRUE(type.Is<ProtoEnumType>());
  EXPECT_EQ(type->kind(), Kind::kEnum);
  EXPECT_EQ(type->name(), "google.protobuf.Field.Kind");
  EXPECT_EQ(&type->descriptor(),
            google::protobuf::GetEnumDescriptor<google::protobuf::Field::Kind>());
}

TEST(ProtoEnumType, FindConstantByName) {
  TypeFactory type_factory(MemoryManager::Global());
  ASSERT_OK_AND_ASSIGN(
      auto type,
      ProtoEnumType::Create<google::protobuf::Field::Kind>(type_factory));
  ASSERT_OK_AND_ASSIGN(auto constant,
                       type->FindConstant(EnumType::ConstantId("TYPE_STRING")));
  ASSERT_TRUE(constant.has_value());
  EXPECT_EQ(constant->number, 9);
  EXPECT_EQ(constant->name, "TYPE_STRING");
}

TEST(ProtoEnumType, FindConstantByNumber) {
  TypeFactory type_factory(MemoryManager::Global());
  ASSERT_OK_AND_ASSIGN(
      auto type,
      ProtoEnumType::Create<google::protobuf::Field::Kind>(type_factory));
  ASSERT_OK_AND_ASSIGN(auto constant,
                       type->FindConstant(EnumType::ConstantId(9)));
  ASSERT_TRUE(constant.has_value());
  EXPECT_EQ(constant->number, 9);
  EXPECT_EQ(constant->name, "TYPE_STRING");
}

}  // namespace
}  // namespace cel::extensions
