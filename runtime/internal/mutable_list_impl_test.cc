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
#include "base/value_manager.h"
#include "common/type.h"
#include "common/value.h"
#include "common/values/legacy_value_manager.h"
#include "internal/testing.h"

namespace cel::runtime_internal {
namespace {

TEST(MutableListImplValue, Creation) {
  common_internal::LegacyValueManager value_factory(
      MemoryManagerRef::ReferenceCounting(), TypeProvider::Builtin());

  ASSERT_OK_AND_ASSIGN(auto builder, value_factory.NewListValueBuilder(
                                         value_factory.GetDynListType()));

  auto mutable_list_value =
      value_factory.GetMemoryManager().MakeShared<MutableListValue>(
          std::move(builder));

  OpaqueValue opaque_handle = mutable_list_value;

  EXPECT_EQ(NativeTypeId::Of(opaque_handle),
            NativeTypeId::For<MutableListValue>());

  // Check that after casting back the handle still points to the same object.
  EXPECT_EQ(opaque_handle.operator->(), mutable_list_value.operator->());
}

TEST(MutableListImplValue, ListBuilding) {
  common_internal::LegacyValueManager value_factory(
      MemoryManagerRef::ReferenceCounting(), TypeProvider::Builtin());

  ASSERT_OK_AND_ASSIGN(auto builder, value_factory.NewListValueBuilder(
                                         value_factory.GetDynListType()));

  auto mutable_list_value =
      value_factory.GetMemoryManager().MakeShared<MutableListValue>(
          std::move(builder));

  // This type should only exist in the context of a comprehension so const
  // casting should be safe.
  MutableListValue& mutable_ref =
      const_cast<MutableListValue&>(*mutable_list_value);

  ASSERT_OK(mutable_ref.Append(value_factory.CreateIntValue(1)));

  ASSERT_OK_AND_ASSIGN(ListValue list_value, std::move(mutable_ref).Build());

  EXPECT_EQ(list_value.Size(), 1);

  cel::Value scratch;

  ASSERT_OK_AND_ASSIGN(auto element, list_value.Get(value_factory, 0, scratch));

  ASSERT_TRUE(InstanceOf<IntValueView>(element));

  EXPECT_EQ(Cast<IntValueView>(element).NativeValue(), 1);
}

}  // namespace
}  // namespace cel::runtime_internal
