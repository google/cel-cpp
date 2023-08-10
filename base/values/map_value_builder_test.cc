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

#include "base/values/map_value_builder.h"

#include "absl/time/time.h"
#include "base/memory.h"
#include "base/type_factory.h"
#include "base/type_provider.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::IsFalse;
using testing::IsTrue;
using testing::NotNull;
using testing::WhenDynamicCastTo;
using cel::internal::IsOkAndHolds;

TEST(MapValueBuilder, UnspecializedUnspecialized) {
  TypeFactory type_factory(MemoryManager::Global());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto map_builder = MapValueBuilder<StringValue, BytesValue>(
      value_factory, type_factory.GetStringType(), type_factory.GetBytesType());
  auto make_key = [&](absl::string_view value =
                          absl::string_view()) -> Handle<Value> {
    return value_factory.CreateStringValue(value).value();
  };
  auto key = make_key();
  auto make_value = [&](absl::string_view value =
                            absl::string_view()) -> Handle<Value> {
    return value_factory.CreateBytesValue(value).value();
  };
  auto value = make_value();
  EXPECT_THAT(map_builder.Update(key, value),
              IsOkAndHolds(IsFalse()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Update(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.Insert(key, value),
              IsOkAndHolds(IsTrue()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Insert(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.Insert(make_key("foo"), value),
              IsOkAndHolds(IsTrue()));  // rvalue, lvalue
  EXPECT_THAT(map_builder.Insert(make_key("foo"), make_value()),
              IsOkAndHolds(IsFalse()));  // rvalue, rvalue
  EXPECT_THAT(map_builder.Update(key, value),
              IsOkAndHolds(IsTrue()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Update(key, make_value()),
              IsOkAndHolds(IsTrue()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.InsertOrAssign(key, value),
              IsOkAndHolds(IsFalse()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrAssign(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.InsertOrAssign(make_key("bar"), value),
              IsOkAndHolds(IsTrue()));  // rvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrAssign(make_key("bar"), make_value("baz")),
              IsOkAndHolds(IsFalse()));  // rvalue, rvalue
  EXPECT_TRUE(map_builder.Has(key));
  EXPECT_FALSE(map_builder.empty());
  EXPECT_EQ(map_builder.size(), 3);
  EXPECT_EQ(map_builder.DebugString(),
            "{\"\": b\"\", \"foo\": b\"\", \"bar\": b\"baz\"}");
  ASSERT_OK_AND_ASSIGN(auto map, std::move(map_builder).Build());
  EXPECT_FALSE(map->empty());
  EXPECT_EQ(map->size(), 3);
  ASSERT_OK_AND_ASSIGN(auto entry,
                       map->Get(MapValue::GetContext(value_factory), key));
  EXPECT_TRUE((*entry)->Is<BytesValue>());
  EXPECT_TRUE((*entry).As<BytesValue>()->empty());
  ASSERT_OK_AND_ASSIGN(
      entry, map->Get(MapValue::GetContext(value_factory), make_key("foo")));
  EXPECT_TRUE((*entry)->Is<BytesValue>());
  EXPECT_TRUE((*entry).As<BytesValue>()->empty());
  ASSERT_OK_AND_ASSIGN(
      entry, map->Get(MapValue::GetContext(value_factory), make_key("bar")));
  EXPECT_TRUE((*entry)->Is<BytesValue>());
  EXPECT_EQ((*entry).As<BytesValue>()->ToString(), "baz");
  EXPECT_EQ(map->DebugString(),
            "{\"\": b\"\", \"foo\": b\"\", \"bar\": b\"baz\"}");
  ASSERT_OK_AND_ASSIGN(auto keys,
                       map->ListKeys(MapValue::ListKeysContext(value_factory)));
  EXPECT_FALSE(keys->empty());
  EXPECT_EQ(keys->size(), 3);
  EXPECT_EQ(keys->DebugString(), "[\"\", \"foo\", \"bar\"]");
}

TEST(MapValueBuilder, UnspecializedGeneric) {
  TypeFactory type_factory(MemoryManager::Global());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto map_builder = MapValueBuilder<StringValue, Value>(
      value_factory, type_factory.GetStringType(), type_factory.GetBytesType());
  auto make_key = [&](absl::string_view value =
                          absl::string_view()) -> Handle<Value> {
    return value_factory.CreateStringValue(value).value();
  };
  auto key = make_key();
  auto make_value = [&](absl::string_view value =
                            absl::string_view()) -> Handle<Value> {
    return value_factory.CreateBytesValue(value).value();
  };
  auto value = make_value();
  EXPECT_THAT(map_builder.Update(key, value),
              IsOkAndHolds(IsFalse()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Update(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.Insert(key, value),
              IsOkAndHolds(IsTrue()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Insert(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.Insert(make_key("foo"), value),
              IsOkAndHolds(IsTrue()));  // rvalue, lvalue
  EXPECT_THAT(map_builder.Insert(make_key("foo"), make_value()),
              IsOkAndHolds(IsFalse()));  // rvalue, rvalue
  EXPECT_THAT(map_builder.Update(key, value),
              IsOkAndHolds(IsTrue()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Update(key, make_value()),
              IsOkAndHolds(IsTrue()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.InsertOrAssign(key, value),
              IsOkAndHolds(IsFalse()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrAssign(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.InsertOrAssign(make_key("bar"), value),
              IsOkAndHolds(IsTrue()));  // rvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrAssign(make_key("bar"), make_value("baz")),
              IsOkAndHolds(IsFalse()));  // rvalue, rvalue
  EXPECT_TRUE(map_builder.Has(key));
  EXPECT_FALSE(map_builder.empty());
  EXPECT_EQ(map_builder.size(), 3);
  EXPECT_EQ(map_builder.DebugString(),
            "{\"\": b\"\", \"foo\": b\"\", \"bar\": b\"baz\"}");
  ASSERT_OK_AND_ASSIGN(auto map, std::move(map_builder).Build());
  EXPECT_FALSE(map->empty());
  EXPECT_EQ(map->size(), 3);
  ASSERT_OK_AND_ASSIGN(auto entry,
                       map->Get(MapValue::GetContext(value_factory), key));
  EXPECT_TRUE((*entry)->Is<BytesValue>());
  EXPECT_TRUE((*entry).As<BytesValue>()->empty());
  ASSERT_OK_AND_ASSIGN(
      entry, map->Get(MapValue::GetContext(value_factory), make_key("foo")));
  EXPECT_TRUE((*entry)->Is<BytesValue>());
  EXPECT_TRUE((*entry).As<BytesValue>()->empty());
  ASSERT_OK_AND_ASSIGN(
      entry, map->Get(MapValue::GetContext(value_factory), make_key("bar")));
  EXPECT_TRUE((*entry)->Is<BytesValue>());
  EXPECT_EQ((*entry).As<BytesValue>()->ToString(), "baz");
  EXPECT_EQ(map->DebugString(),
            "{\"\": b\"\", \"foo\": b\"\", \"bar\": b\"baz\"}");
  ASSERT_OK_AND_ASSIGN(auto keys,
                       map->ListKeys(MapValue::ListKeysContext(value_factory)));
  EXPECT_FALSE(keys->empty());
  EXPECT_EQ(keys->size(), 3);
  EXPECT_EQ(keys->DebugString(), "[\"\", \"foo\", \"bar\"]");
}

TEST(MapValueBuilder, GenericUnspecialized) {
  TypeFactory type_factory(MemoryManager::Global());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto map_builder = MapValueBuilder<Value, BytesValue>(
      value_factory, type_factory.GetStringType(), type_factory.GetBytesType());
  auto make_key = [&](absl::string_view value =
                          absl::string_view()) -> Handle<Value> {
    return value_factory.CreateStringValue(value).value();
  };
  auto key = make_key();
  auto make_value = [&](absl::string_view value =
                            absl::string_view()) -> Handle<Value> {
    return value_factory.CreateBytesValue(value).value();
  };
  auto value = make_value();
  EXPECT_THAT(map_builder.Update(key, value),
              IsOkAndHolds(IsFalse()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Update(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.Insert(key, value),
              IsOkAndHolds(IsTrue()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Insert(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.Insert(make_key("foo"), value),
              IsOkAndHolds(IsTrue()));  // rvalue, lvalue
  EXPECT_THAT(map_builder.Insert(make_key("foo"), make_value()),
              IsOkAndHolds(IsFalse()));  // rvalue, rvalue
  EXPECT_THAT(map_builder.Update(key, value),
              IsOkAndHolds(IsTrue()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Update(key, make_value()),
              IsOkAndHolds(IsTrue()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.InsertOrAssign(key, value),
              IsOkAndHolds(IsFalse()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrAssign(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.InsertOrAssign(make_key("bar"), value),
              IsOkAndHolds(IsTrue()));  // rvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrAssign(make_key("bar"), make_value("baz")),
              IsOkAndHolds(IsFalse()));  // rvalue, rvalue
  EXPECT_TRUE(map_builder.Has(key));
  EXPECT_FALSE(map_builder.empty());
  EXPECT_EQ(map_builder.size(), 3);
  EXPECT_EQ(map_builder.DebugString(),
            "{\"\": b\"\", \"foo\": b\"\", \"bar\": b\"baz\"}");
  ASSERT_OK_AND_ASSIGN(auto map, std::move(map_builder).Build());
  EXPECT_FALSE(map->empty());
  EXPECT_EQ(map->size(), 3);
  ASSERT_OK_AND_ASSIGN(auto entry,
                       map->Get(MapValue::GetContext(value_factory), key));
  EXPECT_TRUE((*entry)->Is<BytesValue>());
  EXPECT_TRUE((*entry).As<BytesValue>()->empty());
  ASSERT_OK_AND_ASSIGN(
      entry, map->Get(MapValue::GetContext(value_factory), make_key("foo")));
  EXPECT_TRUE((*entry)->Is<BytesValue>());
  EXPECT_TRUE((*entry).As<BytesValue>()->empty());
  ASSERT_OK_AND_ASSIGN(
      entry, map->Get(MapValue::GetContext(value_factory), make_key("bar")));
  EXPECT_TRUE((*entry)->Is<BytesValue>());
  EXPECT_EQ((*entry).As<BytesValue>()->ToString(), "baz");
  EXPECT_EQ(map->DebugString(),
            "{\"\": b\"\", \"foo\": b\"\", \"bar\": b\"baz\"}");
  ASSERT_OK_AND_ASSIGN(auto keys,
                       map->ListKeys(MapValue::ListKeysContext(value_factory)));
  EXPECT_FALSE(keys->empty());
  EXPECT_EQ(keys->size(), 3);
  EXPECT_EQ(keys->DebugString(), "[\"\", \"foo\", \"bar\"]");
}

TEST(MapValueBuilder, GenericGeneric) {
  TypeFactory type_factory(MemoryManager::Global());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto map_builder = MapValueBuilder<Value, Value>(
      value_factory, type_factory.GetStringType(), type_factory.GetBytesType());
  auto make_key = [&](absl::string_view value =
                          absl::string_view()) -> Handle<Value> {
    return value_factory.CreateStringValue(value).value();
  };
  auto key = make_key();
  auto make_value = [&](absl::string_view value =
                            absl::string_view()) -> Handle<Value> {
    return value_factory.CreateBytesValue(value).value();
  };
  auto value = make_value();
  EXPECT_THAT(map_builder.Update(key, value),
              IsOkAndHolds(IsFalse()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Update(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.Insert(key, value),
              IsOkAndHolds(IsTrue()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Insert(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.Insert(make_key("foo"), value),
              IsOkAndHolds(IsTrue()));  // rvalue, lvalue
  EXPECT_THAT(map_builder.Insert(make_key("foo"), make_value()),
              IsOkAndHolds(IsFalse()));  // rvalue, rvalue
  EXPECT_THAT(map_builder.Update(key, value),
              IsOkAndHolds(IsTrue()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Update(key, make_value()),
              IsOkAndHolds(IsTrue()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.InsertOrAssign(key, value),
              IsOkAndHolds(IsFalse()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrAssign(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.InsertOrAssign(make_key("bar"), value),
              IsOkAndHolds(IsTrue()));  // rvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrAssign(make_key("bar"), make_value("baz")),
              IsOkAndHolds(IsFalse()));  // rvalue, rvalue
  EXPECT_TRUE(map_builder.Has(key));
  EXPECT_FALSE(map_builder.empty());
  EXPECT_EQ(map_builder.size(), 3);
  EXPECT_EQ(map_builder.DebugString(),
            "{\"\": b\"\", \"foo\": b\"\", \"bar\": b\"baz\"}");
  ASSERT_OK_AND_ASSIGN(auto map, std::move(map_builder).Build());
  EXPECT_FALSE(map->empty());
  EXPECT_EQ(map->size(), 3);
  ASSERT_OK_AND_ASSIGN(auto entry,
                       map->Get(MapValue::GetContext(value_factory), key));
  EXPECT_TRUE((*entry)->Is<BytesValue>());
  EXPECT_TRUE((*entry).As<BytesValue>()->empty());
  ASSERT_OK_AND_ASSIGN(
      entry, map->Get(MapValue::GetContext(value_factory), make_key("foo")));
  EXPECT_TRUE((*entry)->Is<BytesValue>());
  EXPECT_TRUE((*entry).As<BytesValue>()->empty());
  ASSERT_OK_AND_ASSIGN(
      entry, map->Get(MapValue::GetContext(value_factory), make_key("bar")));
  EXPECT_TRUE((*entry)->Is<BytesValue>());
  EXPECT_EQ((*entry).As<BytesValue>()->ToString(), "baz");
  EXPECT_EQ(map->DebugString(),
            "{\"\": b\"\", \"foo\": b\"\", \"bar\": b\"baz\"}");
  ASSERT_OK_AND_ASSIGN(auto keys,
                       map->ListKeys(MapValue::ListKeysContext(value_factory)));
  EXPECT_FALSE(keys->empty());
  EXPECT_EQ(keys->size(), 3);
  EXPECT_EQ(keys->DebugString(), "[\"\", \"foo\", \"bar\"]");
}

TEST(MapValueBuilder, UnspecializedSpecialized) {
  TypeFactory type_factory(MemoryManager::Global());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto map_builder = MapValueBuilder<StringValue, IntValue>(
      value_factory, type_factory.GetStringType(), type_factory.GetIntType());
  auto make_key = [&](absl::string_view value =
                          absl::string_view()) -> Handle<StringValue> {
    return value_factory.CreateStringValue(value).value();
  };
  auto key = make_key();
  auto make_value = [&](int64_t value = 0) -> Handle<IntValue> {
    return value_factory.CreateIntValue(value);
  };
  auto value = make_value();
  EXPECT_THAT(map_builder.Update(key, value),
              IsOkAndHolds(IsFalse()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Update(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.Insert(key, value),
              IsOkAndHolds(IsTrue()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Insert(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.Insert(make_key("foo"), value),
              IsOkAndHolds(IsTrue()));  // rvalue, lvalue
  EXPECT_THAT(map_builder.Insert(make_key("foo"), make_value()),
              IsOkAndHolds(IsFalse()));  // rvalue, rvalue
  EXPECT_THAT(map_builder.Update(key, value),
              IsOkAndHolds(IsTrue()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Update(key, make_value()),
              IsOkAndHolds(IsTrue()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.InsertOrAssign(key, value),
              IsOkAndHolds(IsFalse()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrAssign(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.InsertOrAssign(make_key("bar"), value),
              IsOkAndHolds(IsTrue()));  // rvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrAssign(make_key("bar"), make_value(1)),
              IsOkAndHolds(IsFalse()));  // rvalue, rvalue
  EXPECT_TRUE(map_builder.Has(key));
  EXPECT_FALSE(map_builder.empty());
  EXPECT_EQ(map_builder.size(), 3);
  EXPECT_EQ(map_builder.DebugString(), "{\"\": 0, \"foo\": 0, \"bar\": 1}");
  ASSERT_OK_AND_ASSIGN(auto map, std::move(map_builder).Build());
  EXPECT_FALSE(map->empty());
  EXPECT_EQ(map->size(), 3);
  ASSERT_OK_AND_ASSIGN(auto entry,
                       map->Get(MapValue::GetContext(value_factory), key));
  EXPECT_TRUE((*entry)->Is<IntValue>());
  EXPECT_EQ((*entry).As<IntValue>()->value(), 0);
  ASSERT_OK_AND_ASSIGN(
      entry, map->Get(MapValue::GetContext(value_factory), make_key("foo")));
  EXPECT_TRUE((*entry)->Is<IntValue>());
  EXPECT_EQ((*entry).As<IntValue>()->value(), 0);
  ASSERT_OK_AND_ASSIGN(
      entry, map->Get(MapValue::GetContext(value_factory), make_key("bar")));
  EXPECT_TRUE((*entry)->Is<IntValue>());
  EXPECT_EQ((*entry).As<IntValue>()->value(), 1);
  EXPECT_EQ(map->DebugString(), "{\"\": 0, \"foo\": 0, \"bar\": 1}");
  ASSERT_OK_AND_ASSIGN(auto keys,
                       map->ListKeys(MapValue::ListKeysContext(value_factory)));
  EXPECT_FALSE(keys->empty());
  EXPECT_EQ(keys->size(), 3);
  EXPECT_EQ(keys->DebugString(), "[\"\", \"foo\", \"bar\"]");
}

TEST(MapValueBuilder, GenericSpecialized) {
  TypeFactory type_factory(MemoryManager::Global());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto map_builder = MapValueBuilder<Value, IntValue>(
      value_factory, type_factory.GetStringType(), type_factory.GetIntType());
  auto make_key = [&](absl::string_view value =
                          absl::string_view()) -> Handle<Value> {
    return value_factory.CreateStringValue(value).value();
  };
  auto key = make_key();
  auto make_value = [&](int64_t value = 0) -> Handle<IntValue> {
    return value_factory.CreateIntValue(value);
  };
  auto value = make_value();
  EXPECT_THAT(map_builder.Update(key, value),
              IsOkAndHolds(IsFalse()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Update(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.Insert(key, value),
              IsOkAndHolds(IsTrue()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Insert(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.Insert(make_key("foo"), value),
              IsOkAndHolds(IsTrue()));  // rvalue, lvalue
  EXPECT_THAT(map_builder.Insert(make_key("foo"), make_value()),
              IsOkAndHolds(IsFalse()));  // rvalue, rvalue
  EXPECT_THAT(map_builder.Update(key, value),
              IsOkAndHolds(IsTrue()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Update(key, make_value()),
              IsOkAndHolds(IsTrue()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.InsertOrAssign(key, value),
              IsOkAndHolds(IsFalse()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrAssign(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.InsertOrAssign(make_key("bar"), value),
              IsOkAndHolds(IsTrue()));  // rvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrAssign(make_key("bar"), make_value(1)),
              IsOkAndHolds(IsFalse()));  // rvalue, rvalue
  EXPECT_TRUE(map_builder.Has(key));
  EXPECT_FALSE(map_builder.empty());
  EXPECT_EQ(map_builder.size(), 3);
  EXPECT_EQ(map_builder.DebugString(), "{\"\": 0, \"foo\": 0, \"bar\": 1}");
  ASSERT_OK_AND_ASSIGN(auto map, std::move(map_builder).Build());
  EXPECT_FALSE(map->empty());
  EXPECT_EQ(map->size(), 3);
  ASSERT_OK_AND_ASSIGN(auto entry,
                       map->Get(MapValue::GetContext(value_factory), key));
  EXPECT_TRUE((*entry)->Is<IntValue>());
  EXPECT_EQ((*entry).As<IntValue>()->value(), 0);
  ASSERT_OK_AND_ASSIGN(
      entry, map->Get(MapValue::GetContext(value_factory), make_key("foo")));
  EXPECT_TRUE((*entry)->Is<IntValue>());
  EXPECT_EQ((*entry).As<IntValue>()->value(), 0);
  ASSERT_OK_AND_ASSIGN(
      entry, map->Get(MapValue::GetContext(value_factory), make_key("bar")));
  EXPECT_TRUE((*entry)->Is<IntValue>());
  EXPECT_EQ((*entry).As<IntValue>()->value(), 1);
  EXPECT_EQ(map->DebugString(), "{\"\": 0, \"foo\": 0, \"bar\": 1}");
  ASSERT_OK_AND_ASSIGN(auto keys,
                       map->ListKeys(MapValue::ListKeysContext(value_factory)));
  EXPECT_FALSE(keys->empty());
  EXPECT_EQ(keys->size(), 3);
  EXPECT_EQ(keys->DebugString(), "[\"\", \"foo\", \"bar\"]");
}

TEST(MapValueBuilder, SpecializedUnspecialized) {
  TypeFactory type_factory(MemoryManager::Global());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto map_builder = MapValueBuilder<IntValue, StringValue>(
      value_factory, type_factory.GetIntType(), type_factory.GetStringType());
  auto make_key = [&](int64_t value = 0) -> Handle<IntValue> {
    return value_factory.CreateIntValue(value);
  };
  auto key = make_key();
  auto make_value = [&](absl::string_view value =
                            absl::string_view()) -> Handle<StringValue> {
    return value_factory.CreateStringValue(value).value();
  };
  auto value = make_value();
  EXPECT_THAT(map_builder.Update(key, value),
              IsOkAndHolds(IsFalse()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Update(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.Insert(key, value),
              IsOkAndHolds(IsTrue()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Insert(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.Insert(make_key(1), value),
              IsOkAndHolds(IsTrue()));  // rvalue, lvalue
  EXPECT_THAT(map_builder.Insert(make_key(1), make_value()),
              IsOkAndHolds(IsFalse()));  // rvalue, rvalue
  EXPECT_THAT(map_builder.Update(key, value),
              IsOkAndHolds(IsTrue()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Update(key, make_value()),
              IsOkAndHolds(IsTrue()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.InsertOrAssign(key, value),
              IsOkAndHolds(IsFalse()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrAssign(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.InsertOrAssign(make_key(2), value),
              IsOkAndHolds(IsTrue()));  // rvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrAssign(make_key(2), make_value("foo")),
              IsOkAndHolds(IsFalse()));  // rvalue, rvalue
  EXPECT_TRUE(map_builder.Has(key));
  EXPECT_FALSE(map_builder.empty());
  EXPECT_EQ(map_builder.size(), 3);
  EXPECT_EQ(map_builder.DebugString(), "{0: \"\", 1: \"\", 2: \"foo\"}");
  ASSERT_OK_AND_ASSIGN(auto map, std::move(map_builder).Build());
  EXPECT_FALSE(map->empty());
  EXPECT_EQ(map->size(), 3);
  ASSERT_OK_AND_ASSIGN(auto entry,
                       map->Get(MapValue::GetContext(value_factory), key));
  EXPECT_TRUE((*entry)->Is<StringValue>());
  EXPECT_TRUE((*entry).As<StringValue>()->empty());
  ASSERT_OK_AND_ASSIGN(
      entry, map->Get(MapValue::GetContext(value_factory), make_key(1)));
  EXPECT_TRUE((*entry)->Is<StringValue>());
  EXPECT_TRUE((*entry).As<StringValue>()->empty());
  ASSERT_OK_AND_ASSIGN(
      entry, map->Get(MapValue::GetContext(value_factory), make_key(2)));
  EXPECT_TRUE((*entry)->Is<StringValue>());
  EXPECT_EQ((*entry).As<StringValue>()->ToString(), "foo");
  EXPECT_EQ(map->DebugString(), "{0: \"\", 1: \"\", 2: \"foo\"}");
  ASSERT_OK_AND_ASSIGN(auto keys,
                       map->ListKeys(MapValue::ListKeysContext(value_factory)));
  EXPECT_FALSE(keys->empty());
  EXPECT_EQ(keys->size(), 3);
  EXPECT_EQ(keys->DebugString(), "[0, 1, 2]");
}

TEST(MapValueBuilder, SpecializedGeneric) {
  TypeFactory type_factory(MemoryManager::Global());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto map_builder = MapValueBuilder<IntValue, Value>(
      value_factory, type_factory.GetIntType(), type_factory.GetStringType());
  auto make_key = [&](int64_t value = 0) -> Handle<IntValue> {
    return value_factory.CreateIntValue(value);
  };
  auto key = make_key();
  auto make_value = [&](absl::string_view value =
                            absl::string_view()) -> Handle<Value> {
    return value_factory.CreateStringValue(value).value();
  };
  auto value = make_value();
  EXPECT_THAT(map_builder.Update(key, value),
              IsOkAndHolds(IsFalse()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Update(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.Insert(key, value),
              IsOkAndHolds(IsTrue()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Insert(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.Insert(make_key(1), value),
              IsOkAndHolds(IsTrue()));  // rvalue, lvalue
  EXPECT_THAT(map_builder.Insert(make_key(1), make_value()),
              IsOkAndHolds(IsFalse()));  // rvalue, rvalue
  EXPECT_THAT(map_builder.Update(key, value),
              IsOkAndHolds(IsTrue()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Update(key, make_value()),
              IsOkAndHolds(IsTrue()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.InsertOrAssign(key, value),
              IsOkAndHolds(IsFalse()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrAssign(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.InsertOrAssign(make_key(2), value),
              IsOkAndHolds(IsTrue()));  // rvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrAssign(make_key(2), make_value("foo")),
              IsOkAndHolds(IsFalse()));  // rvalue, rvalue
  EXPECT_TRUE(map_builder.Has(key));
  EXPECT_FALSE(map_builder.empty());
  EXPECT_EQ(map_builder.size(), 3);
  EXPECT_EQ(map_builder.DebugString(), "{0: \"\", 1: \"\", 2: \"foo\"}");
  ASSERT_OK_AND_ASSIGN(auto map, std::move(map_builder).Build());
  EXPECT_FALSE(map->empty());
  EXPECT_EQ(map->size(), 3);
  ASSERT_OK_AND_ASSIGN(auto entry,
                       map->Get(MapValue::GetContext(value_factory), key));
  EXPECT_TRUE((*entry)->Is<StringValue>());
  EXPECT_TRUE((*entry).As<StringValue>()->empty());
  ASSERT_OK_AND_ASSIGN(
      entry, map->Get(MapValue::GetContext(value_factory), make_key(1)));
  EXPECT_TRUE((*entry)->Is<StringValue>());
  EXPECT_TRUE((*entry).As<StringValue>()->empty());
  ASSERT_OK_AND_ASSIGN(
      entry, map->Get(MapValue::GetContext(value_factory), make_key(2)));
  EXPECT_TRUE((*entry)->Is<StringValue>());
  EXPECT_EQ((*entry).As<StringValue>()->ToString(), "foo");
  EXPECT_EQ(map->DebugString(), "{0: \"\", 1: \"\", 2: \"foo\"}");
}

template <typename Key, typename Value, typename GetKey, typename GetValue,
          typename MakeKey, typename MakeValue>
void TestMapBuilder(GetKey get_key, GetValue get_value, MakeKey make_key1,
                    MakeKey make_key2, MakeKey make_key3, MakeValue make_value1,
                    MakeValue make_value2, absl::string_view debug_string,
                    absl::string_view keys_debug_string) {
  TypeFactory type_factory(MemoryManager::Global());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
  auto map_builder = MapValueBuilder<Key, Value>(
      value_factory, (type_factory.*get_key)(), (type_factory.*get_value)());
  auto key = make_key1(value_factory);
  auto value = make_value1(value_factory);
  EXPECT_THAT(map_builder.Update(key, value),
              IsOkAndHolds(IsFalse()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Update(key, make_value1(value_factory)),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.Insert(key, value),
              IsOkAndHolds(IsTrue()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Insert(key, make_value1(value_factory)),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.Insert(make_key2(value_factory), value),
              IsOkAndHolds(IsTrue()));  // rvalue, lvalue
  EXPECT_THAT(
      map_builder.Insert(make_key2(value_factory), make_value1(value_factory)),
      IsOkAndHolds(IsFalse()));  // rvalue, rvalue
  EXPECT_THAT(map_builder.Update(key, value),
              IsOkAndHolds(IsTrue()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.Update(key, make_value1(value_factory)),
              IsOkAndHolds(IsTrue()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.InsertOrAssign(key, value),
              IsOkAndHolds(IsFalse()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrAssign(key, make_value1(value_factory)),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.InsertOrAssign(make_key3(value_factory), value),
              IsOkAndHolds(IsTrue()));  // rvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrAssign(make_key3(value_factory),
                                         make_value2(value_factory)),
              IsOkAndHolds(IsFalse()));  // rvalue, rvalue
  EXPECT_TRUE(map_builder.Has(key));
  EXPECT_FALSE(map_builder.empty());
  EXPECT_EQ(map_builder.size(), 3);
  EXPECT_EQ(map_builder.DebugString(), debug_string);
  ASSERT_OK_AND_ASSIGN(auto map, std::move(map_builder).Build());
  EXPECT_FALSE(map->empty());
  EXPECT_EQ(map->size(), 3);
  ASSERT_OK_AND_ASSIGN(auto entry,
                       map->Get(MapValue::GetContext(value_factory), key));
  EXPECT_TRUE((*entry)->template Is<Value>());
  EXPECT_EQ(*((*entry).template As<Value>()),
            *((make_value1(value_factory)).template As<Value>()));
  ASSERT_OK_AND_ASSIGN(entry, map->Get(MapValue::GetContext(value_factory),
                                       make_key2(value_factory)));
  EXPECT_TRUE((*entry)->template Is<Value>());
  EXPECT_EQ(*((*entry).template As<Value>()),
            *((make_value1(value_factory)).template As<Value>()));
  ASSERT_OK_AND_ASSIGN(entry, map->Get(MapValue::GetContext(value_factory),
                                       make_key3(value_factory)));
  EXPECT_TRUE((*entry)->template Is<Value>());
  EXPECT_EQ(*((*entry).template As<Value>()),
            *((make_value2(value_factory)).template As<Value>()));
  EXPECT_EQ(map->DebugString(), debug_string);
  ASSERT_OK_AND_ASSIGN(auto keys,
                       map->ListKeys(MapValue::ListKeysContext(value_factory)));
  EXPECT_FALSE(keys->empty());
  EXPECT_EQ(keys->size(), 3);
  ASSERT_OK_AND_ASSIGN(auto element, keys->Get(value_factory, 0));
  EXPECT_EQ((*element).template As<Key>(), (*key).template As<Key>());
  ASSERT_OK_AND_ASSIGN(element, keys->Get(value_factory, 1));
  EXPECT_EQ((*element).template As<Key>(),
            (*make_key2(value_factory)).template As<Key>());
  ASSERT_OK_AND_ASSIGN(element, keys->Get(value_factory, 2));
  EXPECT_EQ((*element).template As<Key>(),
            (*make_key3(value_factory)).template As<Key>());
  EXPECT_EQ(keys->DebugString(), keys_debug_string);
}

template <bool C>
Handle<Value> MakeBoolValue(ValueFactory& value_factory) {
  return value_factory.CreateBoolValue(C);
}

template <int64_t C>
Handle<Value> MakeIntValue(ValueFactory& value_factory) {
  return value_factory.CreateIntValue(C);
}

template <uint64_t C>
Handle<Value> MakeUintValue(ValueFactory& value_factory) {
  return value_factory.CreateUintValue(C);
}

auto MakeDoubleValue(double value) {
  return [value](ValueFactory& value_factory) -> Handle<Value> {
    return value_factory.CreateDoubleValue(value);
  };
}

auto MakeDurationValue(absl::Duration value) {
  return [value](ValueFactory& value_factory) -> Handle<Value> {
    return value_factory.CreateUncheckedDurationValue(value);
  };
}

auto MakeTimestampValue(absl::Time value) {
  return [value](ValueFactory& value_factory) -> Handle<Value> {
    return value_factory.CreateUncheckedTimestampValue(value);
  };
}

TEST(MapValueBuilder, IntBool) {
  TestMapBuilder<IntValue, BoolValue>(
      &TypeFactory::GetIntType, &TypeFactory::GetBoolType, MakeIntValue<0>,
      MakeIntValue<1>, MakeIntValue<2>, MakeBoolValue<false>,
      MakeBoolValue<true>, "{0: false, 1: false, 2: true}", "[0, 1, 2]");
}

TEST(MapValueBuilder, IntInt) {
  TestMapBuilder<IntValue, IntValue>(
      &TypeFactory::GetIntType, &TypeFactory::GetIntType, MakeIntValue<0>,
      MakeIntValue<1>, MakeIntValue<2>, MakeIntValue<0>, MakeIntValue<1>,
      "{0: 0, 1: 0, 2: 1}", "[0, 1, 2]");
}

TEST(MapValueBuilder, IntUint) {
  TestMapBuilder<IntValue, UintValue>(
      &TypeFactory::GetIntType, &TypeFactory::GetUintType, MakeIntValue<0>,
      MakeIntValue<1>, MakeIntValue<2>, MakeUintValue<0>, MakeUintValue<1>,
      "{0: 0u, 1: 0u, 2: 1u}", "[0, 1, 2]");
}

TEST(MapValueBuilder, IntDouble) {
  TestMapBuilder<IntValue, DoubleValue>(
      &TypeFactory::GetIntType, &TypeFactory::GetDoubleType, MakeIntValue<0>,
      MakeIntValue<1>, MakeIntValue<2>, MakeDoubleValue(0.0),
      MakeDoubleValue(1.0), "{0: 0.0, 1: 0.0, 2: 1.0}", "[0, 1, 2]");
}

TEST(MapValueBuilder, IntDuration) {
  TestMapBuilder<IntValue, DurationValue>(
      &TypeFactory::GetIntType, &TypeFactory::GetDurationType, MakeIntValue<0>,
      MakeIntValue<1>, MakeIntValue<2>, MakeDurationValue(absl::ZeroDuration()),
      MakeDurationValue(absl::Seconds(1)), "{0: 0, 1: 0, 2: 1s}", "[0, 1, 2]");
}

TEST(MapValueBuilder, IntTimestamp) {
  TestMapBuilder<IntValue, TimestampValue>(
      &TypeFactory::GetIntType, &TypeFactory::GetTimestampType, MakeIntValue<0>,
      MakeIntValue<1>, MakeIntValue<2>, MakeTimestampValue(absl::UnixEpoch()),
      MakeTimestampValue(absl::UnixEpoch() + absl::Seconds(1)),
      "{0: 1970-01-01T00:00:00Z, 1: 1970-01-01T00:00:00Z, 2: "
      "1970-01-01T00:00:01Z}",
      "[0, 1, 2]");
}

TEST(MapValueBuilder, UintBool) {
  TestMapBuilder<UintValue, BoolValue>(
      &TypeFactory::GetUintType, &TypeFactory::GetBoolType, MakeUintValue<0>,
      MakeUintValue<1>, MakeUintValue<2>, MakeBoolValue<false>,
      MakeBoolValue<true>, "{0u: false, 1u: false, 2u: true}", "[0u, 1u, 2u]");
}

TEST(MapValueBuilder, UintInt) {
  TestMapBuilder<UintValue, IntValue>(
      &TypeFactory::GetUintType, &TypeFactory::GetIntType, MakeUintValue<0>,
      MakeUintValue<1>, MakeUintValue<2>, MakeIntValue<0>, MakeIntValue<1>,
      "{0u: 0, 1u: 0, 2u: 1}", "[0u, 1u, 2u]");
}

TEST(MapValueBuilder, UintUint) {
  TestMapBuilder<UintValue, UintValue>(
      &TypeFactory::GetUintType, &TypeFactory::GetUintType, MakeUintValue<0>,
      MakeUintValue<1>, MakeUintValue<2>, MakeUintValue<0>, MakeUintValue<1>,
      "{0u: 0u, 1u: 0u, 2u: 1u}", "[0u, 1u, 2u]");
}

TEST(MapValueBuilder, UintDouble) {
  TestMapBuilder<UintValue, DoubleValue>(
      &TypeFactory::GetUintType, &TypeFactory::GetDoubleType, MakeUintValue<0>,
      MakeUintValue<1>, MakeUintValue<2>, MakeDoubleValue(0.0),
      MakeDoubleValue(1.0), "{0u: 0.0, 1u: 0.0, 2u: 1.0}", "[0u, 1u, 2u]");
}

TEST(MapValueBuilder, UintDuration) {
  TestMapBuilder<UintValue, DurationValue>(
      &TypeFactory::GetUintType, &TypeFactory::GetDurationType,
      MakeUintValue<0>, MakeUintValue<1>, MakeUintValue<2>,
      MakeDurationValue(absl::ZeroDuration()),
      MakeDurationValue(absl::Seconds(1)), "{0u: 0, 1u: 0, 2u: 1s}",
      "[0u, 1u, 2u]");
}

TEST(MapValueBuilder, UintTimestamp) {
  TestMapBuilder<UintValue, TimestampValue>(
      &TypeFactory::GetUintType, &TypeFactory::GetTimestampType,
      MakeUintValue<0>, MakeUintValue<1>, MakeUintValue<2>,
      MakeTimestampValue(absl::UnixEpoch()),
      MakeTimestampValue(absl::UnixEpoch() + absl::Seconds(1)),
      "{0u: 1970-01-01T00:00:00Z, 1u: 1970-01-01T00:00:00Z, 2u: "
      "1970-01-01T00:00:01Z}",
      "[0u, 1u, 2u]");
}

template <typename I, typename K, typename V>
void TestMapValueBuilderImpl(ValueFactory& value_factory, const Handle<K>& key,
                             const Handle<V>& value) {
  ASSERT_OK_AND_ASSIGN(auto type,
                       value_factory.type_factory().CreateMapType(key, value));
  ASSERT_OK_AND_ASSIGN(auto builder, type->NewValueBuilder(value_factory));
  EXPECT_THAT((&builder.get()), WhenDynamicCastTo<I*>(NotNull()));
}

TEST(MapValueBuilder, Dynamic) {
  TypeFactory type_factory(MemoryManager::Global());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ValueFactory value_factory(type_manager);
#ifdef ABSL_INTERNAL_HAS_RTTI
  // (BoolValue, ...)
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<BoolValue, BoolValue>>(
          value_factory, type_factory.GetBoolType(),
          type_factory.GetBoolType())));
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<BoolValue, IntValue>>(
          value_factory, type_factory.GetBoolType(),
          type_factory.GetIntType())));
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<BoolValue, UintValue>>(
          value_factory, type_factory.GetBoolType(),
          type_factory.GetUintType())));
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<BoolValue, DoubleValue>>(
          value_factory, type_factory.GetBoolType(),
          type_factory.GetDoubleType())));
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<BoolValue, DurationValue>>(
          value_factory, type_factory.GetBoolType(),
          type_factory.GetDurationType())));
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<BoolValue, TimestampValue>>(
          value_factory, type_factory.GetBoolType(),
          type_factory.GetTimestampType())));
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<BoolValue, Value>>(
          value_factory, type_factory.GetBoolType(),
          type_factory.GetDynType())));
  // (IntValue, ...)
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<IntValue, BoolValue>>(
          value_factory, type_factory.GetIntType(),
          type_factory.GetBoolType())));
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<IntValue, IntValue>>(
          value_factory, type_factory.GetIntType(),
          type_factory.GetIntType())));
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<IntValue, UintValue>>(
          value_factory, type_factory.GetIntType(),
          type_factory.GetUintType())));
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<IntValue, DoubleValue>>(
          value_factory, type_factory.GetIntType(),
          type_factory.GetDoubleType())));
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<IntValue, DurationValue>>(
          value_factory, type_factory.GetIntType(),
          type_factory.GetDurationType())));
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<IntValue, TimestampValue>>(
          value_factory, type_factory.GetIntType(),
          type_factory.GetTimestampType())));
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<IntValue, Value>>(
          value_factory, type_factory.GetIntType(),
          type_factory.GetDynType())));
  // (UintValue, ...)
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<UintValue, BoolValue>>(
          value_factory, type_factory.GetUintType(),
          type_factory.GetBoolType())));
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<UintValue, IntValue>>(
          value_factory, type_factory.GetUintType(),
          type_factory.GetIntType())));
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<UintValue, UintValue>>(
          value_factory, type_factory.GetUintType(),
          type_factory.GetUintType())));
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<UintValue, DoubleValue>>(
          value_factory, type_factory.GetUintType(),
          type_factory.GetDoubleType())));
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<UintValue, DurationValue>>(
          value_factory, type_factory.GetUintType(),
          type_factory.GetDurationType())));
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<UintValue, TimestampValue>>(
          value_factory, type_factory.GetUintType(),
          type_factory.GetTimestampType())));
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<UintValue, Value>>(
          value_factory, type_factory.GetUintType(),
          type_factory.GetDynType())));
  // (StringValue, ...)
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<Value, BoolValue>>(
          value_factory, type_factory.GetStringType(),
          type_factory.GetBoolType())));
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<Value, IntValue>>(
          value_factory, type_factory.GetStringType(),
          type_factory.GetIntType())));
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<Value, UintValue>>(
          value_factory, type_factory.GetStringType(),
          type_factory.GetUintType())));
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<Value, DoubleValue>>(
          value_factory, type_factory.GetStringType(),
          type_factory.GetDoubleType())));
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<Value, DurationValue>>(
          value_factory, type_factory.GetStringType(),
          type_factory.GetDurationType())));
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<Value, TimestampValue>>(
          value_factory, type_factory.GetStringType(),
          type_factory.GetTimestampType())));
  ASSERT_NO_FATAL_FAILURE(
      (TestMapValueBuilderImpl<MapValueBuilder<Value, Value>>(
          value_factory, type_factory.GetStringType(),
          type_factory.GetDynType())));
#else
  GTEST_SKIP() << "RTTI unavailable";
#endif
}

}  // namespace
}  // namespace cel
