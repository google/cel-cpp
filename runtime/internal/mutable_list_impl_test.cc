//
// Copyright 2023 Google LLC
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

#include "runtime/internal/mutable_list_impl.h"

#include "base/memory.h"
#include "base/type_factory.h"
#include "base/type_provider.h"
#include "base/types/opaque_type.h"
#include "base/value_factory.h"
#include "base/values/list_value.h"
#include "base/values/opaque_value.h"
#include "internal/testing.h"

namespace cel::runtime_internal {
namespace {

TEST(MutableListImplType, Creation) {
  TypeFactory type_factory(MemoryManager::Global());

  ASSERT_OK_AND_ASSIGN(Handle<MutableListType> list_type,
                       type_factory.CreateOpaqueType<MutableListType>());

  Handle<OpaqueType> opaque_type = list_type;
  EXPECT_THAT(opaque_type->name(), kMutableListTypeName);
  EXPECT_TRUE(opaque_type->Is<OpaqueType>());
  EXPECT_TRUE(opaque_type->Is<MutableListType>());

  EXPECT_EQ(&opaque_type->As<MutableListType>(), &(*list_type));
}

TEST(MutableListImplValue, Creation) {
  TypeFactory type_factory(MemoryManager::Global());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);

  ASSERT_OK_AND_ASSIGN(Handle<MutableListType> mutable_list_type,
                       type_factory.CreateOpaqueType<MutableListType>());
  ASSERT_OK_AND_ASSIGN(auto list_type,
                       type_factory.CreateListType(type_factory.GetDynType()));
  ASSERT_OK_AND_ASSIGN(auto list_builder,
                       list_type->NewValueBuilder(value_factory));

  ASSERT_OK_AND_ASSIGN(Handle<MutableListValue> mutable_list_value,
                       value_factory.CreateOpaqueValue<MutableListValue>(
                           mutable_list_type, std::move(list_builder)));

  Handle<OpaqueValue> opaque_handle = mutable_list_value;

  EXPECT_TRUE(opaque_handle->Is<MutableListValue>());

  // Check that after casting back the handle still points to the same object.
  EXPECT_EQ(&opaque_handle->As<MutableListValue>(), &(*mutable_list_value));
}

TEST(MutableListImplValue, ListBuilding) {
  TypeFactory type_factory(MemoryManager::Global());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);

  ASSERT_OK_AND_ASSIGN(Handle<MutableListType> mutable_list_type,
                       type_factory.CreateOpaqueType<MutableListType>());
  ASSERT_OK_AND_ASSIGN(auto list_type,
                       type_factory.CreateListType(type_factory.GetDynType()));
  ASSERT_OK_AND_ASSIGN(auto list_builder,
                       list_type->NewValueBuilder(value_factory));

  ASSERT_OK_AND_ASSIGN(Handle<MutableListValue> mutable_list_value,
                       value_factory.CreateOpaqueValue<MutableListValue>(
                           mutable_list_type, std::move(list_builder)));

  // This type should only exist in the context of a comprehension so const
  // casting should be safe.
  MutableListValue& mutable_ref =
      const_cast<MutableListValue&>(*mutable_list_value);

  ASSERT_OK(mutable_ref.Append(value_factory.CreateIntValue(1)));
  EXPECT_EQ(mutable_ref.DebugString(), "[1]");

  ASSERT_OK_AND_ASSIGN(Handle<ListValue> list_value,
                       std::move(mutable_ref).Build());

  EXPECT_EQ(list_value->Size(), 1);

  ASSERT_OK_AND_ASSIGN(auto element, list_value->Get(value_factory, 0));

  ASSERT_TRUE(element->Is<IntValue>());

  EXPECT_EQ(element->As<IntValue>().NativeValue(), 1);
}

}  // namespace
}  // namespace cel::runtime_internal
