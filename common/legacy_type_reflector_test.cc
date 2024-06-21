// Copyright 2024 Google LLC
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

#include <utility>

#include "absl/status/status.h"
#include "common/legacy_value.h"
#include "common/memory.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "common/values/legacy_value_manager.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::cel::common_internal::LegacyValueManager;
using ::cel::interop_internal::TestOnly_IsLegacyListBuilder;
using ::cel::interop_internal::TestOnly_IsLegacyMapBuilder;
using cel::internal::IsOkAndHolds;
using cel::internal::StatusIs;

class TypeReflectorLegacyTest
    : public common_internal::ThreadCompatibleValueTest<> {};

TEST_P(TypeReflectorLegacyTest, NewListValueBuilderLegacyOptimized) {
  LegacyValueManager manager(memory_manager(), TypeReflector::LegacyBuiltin());

  ASSERT_OK_AND_ASSIGN(auto builder,
                       manager.NewListValueBuilder(manager.GetDynListType()));

  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_TRUE(TestOnly_IsLegacyListBuilder(*builder));
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_FALSE(TestOnly_IsLegacyListBuilder(*builder));
      break;
  }
}

TEST_P(TypeReflectorLegacyTest, NewMapValueBuilderLegacyOptimized) {
  LegacyValueManager manager(memory_manager(), TypeReflector::LegacyBuiltin());

  ASSERT_OK_AND_ASSIGN(auto builder,
                       manager.NewMapValueBuilder(manager.GetDynDynMapType()));

  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_TRUE(TestOnly_IsLegacyMapBuilder(*builder));
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_FALSE(TestOnly_IsLegacyMapBuilder(*builder));
      break;
  }
}

TEST_P(TypeReflectorLegacyTest, ListImplementationNext) {
  LegacyValueManager manager(memory_manager(), TypeReflector::LegacyBuiltin());

  ASSERT_OK_AND_ASSIGN(auto builder,
                       manager.NewListValueBuilder(manager.GetDynListType()));

  EXPECT_OK(builder->Add(IntValue(1)));
  EXPECT_OK(builder->Add(IntValue(2)));
  EXPECT_OK(builder->Add(IntValue(3)));
  EXPECT_EQ(builder->Size(), 3);
  EXPECT_FALSE(builder->IsEmpty());
  auto value = std::move(*builder).Build();
  EXPECT_THAT(value.Size(), IsOkAndHolds(3));

  ASSERT_OK_AND_ASSIGN(auto iterator, value.NewIterator(manager));

  while (iterator->HasNext()) {
    EXPECT_OK(iterator->Next(manager));
  }

  EXPECT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(manager),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

INSTANTIATE_TEST_SUITE_P(Default, TypeReflectorLegacyTest,
                         testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting));

}  // namespace
}  // namespace cel
