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

#include "extensions/protobuf/type_provider.h"

#include "google/protobuf/type.pb.h"
#include "base/kind.h"
#include "base/memory.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "extensions/protobuf/struct_type.h"
#include "internal/testing.h"
#include "google/protobuf/generated_enum_reflection.h"

namespace cel::extensions {
namespace {

TEST(ProtoTypeProvider, Struct) {
  TypeFactory type_factory(MemoryManagerRef::ReferenceCounting());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(auto type,
                       type_manager.ResolveType("google.protobuf.Field"));
  ASSERT_TRUE(type);
  EXPECT_TRUE((*type)->Is<StructType>());
  EXPECT_TRUE((*type)->Is<ProtoStructType>());
  EXPECT_EQ((*type)->kind(), Kind::kStruct);
  EXPECT_EQ(&((*type).As<ProtoStructType>()->descriptor()),
            google::protobuf::Field::descriptor());
}

}  // namespace
}  // namespace cel::extensions
