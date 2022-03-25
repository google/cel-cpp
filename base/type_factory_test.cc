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

#include "base/type_factory.h"

#include "absl/status/status.h"
#include "base/memory_manager.h"
#include "internal/testing.h"

namespace cel {
namespace {

TEST(TypeFactory, CreateListTypeCaches) {
  TypeFactory type_factory(MemoryManager::Global());
  ASSERT_OK_AND_ASSIGN(auto list_type_1,
                       type_factory.CreateListType(type_factory.GetBoolType()));
  ASSERT_OK_AND_ASSIGN(auto list_type_2,
                       type_factory.CreateListType(type_factory.GetBoolType()));
  EXPECT_EQ(list_type_1.operator->(), list_type_2.operator->());
}

TEST(TypeFactory, CreateMapTypeCaches) {
  TypeFactory type_factory(MemoryManager::Global());
  ASSERT_OK_AND_ASSIGN(auto map_type_1,
                       type_factory.CreateMapType(type_factory.GetStringType(),
                                                  type_factory.GetBoolType()));
  ASSERT_OK_AND_ASSIGN(auto map_type_2,
                       type_factory.CreateMapType(type_factory.GetStringType(),
                                                  type_factory.GetBoolType()));
  EXPECT_EQ(map_type_1.operator->(), map_type_2.operator->());
}

}  // namespace
}  // namespace cel
