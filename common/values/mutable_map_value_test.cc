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
#include <vector>

#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "common/memory.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "common/values/map_value_builder.h"
#include "internal/testing.h"

namespace cel::common_internal {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::cel::test::BoolValueIs;
using ::cel::test::IntValueIs;
using ::cel::test::IsNullValue;
using ::cel::test::ListValueElements;
using ::cel::test::ListValueIs;
using ::cel::test::StringValueIs;
using ::testing::IsEmpty;
using ::testing::IsFalse;
using ::testing::IsTrue;
using ::testing::Pair;
using ::testing::UnorderedElementsAre;

using MutableMapValueTest = common_internal::ValueTest<>;

TEST_F(MutableMapValueTest, DebugString) {
  auto mutable_map_value = NewMutableMapValue(arena());
  EXPECT_THAT(mutable_map_value->DebugString(), "{}");
}

TEST_F(MutableMapValueTest, IsEmpty) {
  auto mutable_map_value = NewMutableMapValue(arena());
  mutable_map_value->Reserve(1);
  EXPECT_TRUE(mutable_map_value->IsEmpty());
  EXPECT_THAT(mutable_map_value->Put(StringValue("foo"), IntValue(1)), IsOk());
  EXPECT_FALSE(mutable_map_value->IsEmpty());
}

TEST_F(MutableMapValueTest, Size) {
  auto mutable_map_value = NewMutableMapValue(arena());
  mutable_map_value->Reserve(1);
  EXPECT_THAT(mutable_map_value->Size(), 0);
  EXPECT_THAT(mutable_map_value->Put(StringValue("foo"), IntValue(1)), IsOk());
  EXPECT_THAT(mutable_map_value->Size(), 1);
}

TEST_F(MutableMapValueTest, ListKeys) {
  auto mutable_map_value = NewMutableMapValue(arena());
  mutable_map_value->Reserve(1);
  ListValue keys;
  EXPECT_THAT(mutable_map_value->Put(StringValue("foo"), IntValue(1)), IsOk());
  EXPECT_THAT(mutable_map_value->ListKeys(descriptor_pool(), message_factory(),
                                          arena(), &keys),
              IsOk());
  EXPECT_THAT(keys, ListValueIs(ListValueElements(
                        UnorderedElementsAre(StringValueIs("foo")),
                        descriptor_pool(), message_factory(), arena())));
}

TEST_F(MutableMapValueTest, ForEach) {
  auto mutable_map_value = NewMutableMapValue(arena());
  mutable_map_value->Reserve(1);
  std::vector<std::pair<Value, Value>> entries;
  auto for_each_callback = [&](const Value& key,
                               const Value& value) -> absl::StatusOr<bool> {
    entries.push_back(std::pair{key, value});
    return true;
  };
  EXPECT_THAT(mutable_map_value->ForEach(for_each_callback, descriptor_pool(),
                                         message_factory(), arena()),
              IsOk());
  EXPECT_THAT(entries, IsEmpty());
  EXPECT_THAT(mutable_map_value->Put(StringValue("foo"), IntValue(1)), IsOk());
  EXPECT_THAT(mutable_map_value->ForEach(for_each_callback, descriptor_pool(),
                                         message_factory(), arena()),
              IsOk());
  EXPECT_THAT(entries,
              UnorderedElementsAre(Pair(StringValueIs("foo"), IntValueIs(1))));
}

TEST_F(MutableMapValueTest, NewIterator) {
  auto mutable_map_value = NewMutableMapValue(arena());
  mutable_map_value->Reserve(1);
  ASSERT_OK_AND_ASSIGN(auto iterator, mutable_map_value->NewIterator());
  EXPECT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(mutable_map_value->Put(StringValue("foo"), IntValue(1)), IsOk());
  ASSERT_OK_AND_ASSIGN(iterator, mutable_map_value->NewIterator());
  EXPECT_TRUE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              IsOkAndHolds(StringValueIs("foo")));
  EXPECT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(descriptor_pool(), message_factory(), arena()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_F(MutableMapValueTest, FindHas) {
  auto mutable_map_value = NewMutableMapValue(arena());
  mutable_map_value->Reserve(1);
  Value value;
  EXPECT_THAT(mutable_map_value->Find(StringValue("foo"), descriptor_pool(),
                                      message_factory(), arena(), &value),
              IsOkAndHolds(IsFalse()));
  EXPECT_THAT(value, IsNullValue());
  EXPECT_THAT(mutable_map_value->Has(StringValue("foo"), descriptor_pool(),
                                     message_factory(), arena(), &value),
              IsOk());
  EXPECT_THAT(value, BoolValueIs(false));
  EXPECT_THAT(mutable_map_value->Put(StringValue("foo"), IntValue(1)), IsOk());
  EXPECT_THAT(mutable_map_value->Find(StringValue("foo"), descriptor_pool(),
                                      message_factory(), arena(), &value),
              IsOkAndHolds(IsTrue()));
  EXPECT_THAT(value, IntValueIs(1));
  EXPECT_THAT(mutable_map_value->Has(StringValue("foo"), descriptor_pool(),
                                     message_factory(), arena(), &value),
              IsOk());
  EXPECT_THAT(value, BoolValueIs(true));
}

TEST_F(MutableMapValueTest, IsMutableMapValue) {
  auto mutable_map_value = NewMutableMapValue(arena());
  EXPECT_TRUE(IsMutableMapValue(Value(CustomMapValue(mutable_map_value))));
  EXPECT_TRUE(IsMutableMapValue(MapValue(CustomMapValue(mutable_map_value))));
}

TEST_F(MutableMapValueTest, AsMutableMapValue) {
  auto mutable_map_value = NewMutableMapValue(arena());
  EXPECT_EQ(AsMutableMapValue(Value(CustomMapValue(mutable_map_value))),
            mutable_map_value.operator->());
  EXPECT_EQ(AsMutableMapValue(MapValue(CustomMapValue(mutable_map_value))),
            mutable_map_value.operator->());
}

TEST_F(MutableMapValueTest, GetMutableMapValue) {
  auto mutable_map_value = NewMutableMapValue(arena());
  EXPECT_EQ(&GetMutableMapValue(Value(CustomMapValue(mutable_map_value))),
            mutable_map_value.operator->());
  EXPECT_EQ(&GetMutableMapValue(MapValue(CustomMapValue(mutable_map_value))),
            mutable_map_value.operator->());
}

}  // namespace
}  // namespace cel::common_internal
