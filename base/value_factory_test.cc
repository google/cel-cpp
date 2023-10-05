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

#include "base/value_factory.h"

#include "absl/status/status.h"
#include "base/memory.h"
#include "base/testing/value_matchers.h"
#include "internal/testing.h"

namespace cel {
namespace {

using ::cel_testing::ValueOf;
using testing::IsTrue;
using cel::internal::IsOkAndHolds;
using cel::internal::StatusIs;

TEST(ValueFactory, CreateErrorValueReplacesOk) {
  TypeFactory type_factory(MemoryManager::Global());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_THAT(value_factory.CreateErrorValue(absl::OkStatus())->value(),
              StatusIs(absl::StatusCode::kUnknown));
}

TEST(ValueFactory, CreateStringValueIllegalByteSequence) {
  TypeFactory type_factory(MemoryManager::Global());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  EXPECT_THAT(value_factory.CreateStringValue("\xff"),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(value_factory.CreateStringValue(absl::Cord("\xff")),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ValueFactory, JsonNull) {
  TypeFactory type_factory(MemoryManager::Global());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto value = value_factory.CreateValueFromJson(kJsonNull);
  EXPECT_TRUE(value->Is<NullValue>());
}

TEST(ValueFactory, JsonBool) {
  TypeFactory type_factory(MemoryManager::Global());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto value = value_factory.CreateValueFromJson(true);
  ASSERT_TRUE(value->Is<BoolValue>());
  EXPECT_TRUE(value->As<BoolValue>().value());
}

TEST(ValueFactory, JsonNumber) {
  TypeFactory type_factory(MemoryManager::Global());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto value = value_factory.CreateValueFromJson(1.0);
  ASSERT_TRUE(value->Is<DoubleValue>());
  EXPECT_EQ(value->As<DoubleValue>().value(), 1.0);
}

TEST(ValueFactory, JsonString) {
  TypeFactory type_factory(MemoryManager::Global());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto value = value_factory.CreateValueFromJson(JsonString("foo"));
  ASSERT_TRUE(value->Is<StringValue>());
  EXPECT_EQ(value->As<StringValue>().ToCord(), "foo");
}

TEST(ValueFactory, JsonArray) {
  TypeFactory type_factory(MemoryManager::Global());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto array = MakeJsonArray({JsonString("foo")});
  auto value = value_factory.CreateValueFromJson(array);
  ASSERT_TRUE(value->Is<ListValue>());
  EXPECT_FALSE(value->As<ListValue>().empty());
  EXPECT_EQ(value->As<ListValue>().size(), 1);
  EXPECT_EQ(value->DebugString(), "[\"foo\"]");
  ASSERT_OK_AND_ASSIGN(auto element,
                       value->As<ListValue>().Get(value_factory, 0));
  ASSERT_TRUE(element->Is<StringValue>());
  EXPECT_EQ(element->As<StringValue>().ToCord(), "foo");
  ASSERT_OK_AND_ASSIGN(
      auto json, value->As<ListValue>().ConvertToJsonArray(value_factory));
  EXPECT_EQ(json, array);
}

TEST(ValueFactory, JsonObject) {
  TypeFactory type_factory(MemoryManager::Global());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto object = MakeJsonObject({{JsonString("foo"), true}});
  auto value = value_factory.CreateValueFromJson(object);
  ASSERT_TRUE(value->Is<MapValue>());
  EXPECT_FALSE(value->As<MapValue>().empty());
  EXPECT_EQ(value->As<MapValue>().size(), 1);
  EXPECT_EQ(value->DebugString(), "{\"foo\": true}");
  ASSERT_OK_AND_ASSIGN(
      auto entry,
      value->As<MapValue>().Get(
          value_factory, value_factory.CreateUncheckedStringValue("foo")));
  ASSERT_TRUE(entry->Is<BoolValue>());
  EXPECT_TRUE(entry->As<BoolValue>().value());
  EXPECT_THAT(
      value->As<MapValue>().Has(
          value_factory, value_factory.CreateUncheckedStringValue("foo")),
      IsOkAndHolds(ValueOf<BoolValue>(value_factory, true)));
  ASSERT_OK_AND_ASSIGN(
      auto json, value->As<MapValue>().ConvertToJsonObject(value_factory));
  EXPECT_EQ(json, object);
}

}  // namespace
}  // namespace cel
