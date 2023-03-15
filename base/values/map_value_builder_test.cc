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
#include "base/memory_manager.h"
#include "base/type_factory.h"
#include "base/type_provider.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::IsFalse;
using testing::IsTrue;
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
  EXPECT_THAT(map_builder.InsertOrUpdate(key, value),
              IsOkAndHolds(IsFalse()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrUpdate(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.InsertOrUpdate(make_key("bar"), value),
              IsOkAndHolds(IsTrue()));  // rvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrUpdate(make_key("bar"), make_value("baz")),
              IsOkAndHolds(IsFalse()));  // rvalue, rvalue
  EXPECT_TRUE(map_builder.Has(key));
  EXPECT_FALSE(map_builder.empty());
  EXPECT_EQ(map_builder.size(), 3);
  EXPECT_EQ(map_builder.DebugString(),
            "{\"\": b\"\", \"foo\": b\"\", \"bar\": b\"baz\"}");
  ASSERT_OK_AND_ASSIGN(auto map, std::move(map_builder).Build());
  EXPECT_FALSE(map->empty());
  EXPECT_EQ(map->size(), 3);
  ASSERT_OK_AND_ASSIGN(auto entry, map->Get(value_factory, key));
  EXPECT_TRUE((*entry)->Is<BytesValue>());
  EXPECT_TRUE((*entry).As<BytesValue>()->empty());
  ASSERT_OK_AND_ASSIGN(entry, map->Get(value_factory, make_key("foo")));
  EXPECT_TRUE((*entry)->Is<BytesValue>());
  EXPECT_TRUE((*entry).As<BytesValue>()->empty());
  ASSERT_OK_AND_ASSIGN(entry, map->Get(value_factory, make_key("bar")));
  EXPECT_TRUE((*entry)->Is<BytesValue>());
  EXPECT_EQ((*entry).As<BytesValue>()->ToString(), "baz");
  EXPECT_EQ(map->DebugString(),
            "{\"\": b\"\", \"foo\": b\"\", \"bar\": b\"baz\"}");
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
  EXPECT_THAT(map_builder.InsertOrUpdate(key, value),
              IsOkAndHolds(IsFalse()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrUpdate(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.InsertOrUpdate(make_key("bar"), value),
              IsOkAndHolds(IsTrue()));  // rvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrUpdate(make_key("bar"), make_value("baz")),
              IsOkAndHolds(IsFalse()));  // rvalue, rvalue
  EXPECT_TRUE(map_builder.Has(key));
  EXPECT_FALSE(map_builder.empty());
  EXPECT_EQ(map_builder.size(), 3);
  EXPECT_EQ(map_builder.DebugString(),
            "{\"\": b\"\", \"foo\": b\"\", \"bar\": b\"baz\"}");
  ASSERT_OK_AND_ASSIGN(auto map, std::move(map_builder).Build());
  EXPECT_FALSE(map->empty());
  EXPECT_EQ(map->size(), 3);
  ASSERT_OK_AND_ASSIGN(auto entry, map->Get(value_factory, key));
  EXPECT_TRUE((*entry)->Is<BytesValue>());
  EXPECT_TRUE((*entry).As<BytesValue>()->empty());
  ASSERT_OK_AND_ASSIGN(entry, map->Get(value_factory, make_key("foo")));
  EXPECT_TRUE((*entry)->Is<BytesValue>());
  EXPECT_TRUE((*entry).As<BytesValue>()->empty());
  ASSERT_OK_AND_ASSIGN(entry, map->Get(value_factory, make_key("bar")));
  EXPECT_TRUE((*entry)->Is<BytesValue>());
  EXPECT_EQ((*entry).As<BytesValue>()->ToString(), "baz");
  EXPECT_EQ(map->DebugString(),
            "{\"\": b\"\", \"foo\": b\"\", \"bar\": b\"baz\"}");
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
  EXPECT_THAT(map_builder.InsertOrUpdate(key, value),
              IsOkAndHolds(IsFalse()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrUpdate(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.InsertOrUpdate(make_key("bar"), value),
              IsOkAndHolds(IsTrue()));  // rvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrUpdate(make_key("bar"), make_value("baz")),
              IsOkAndHolds(IsFalse()));  // rvalue, rvalue
  EXPECT_TRUE(map_builder.Has(key));
  EXPECT_FALSE(map_builder.empty());
  EXPECT_EQ(map_builder.size(), 3);
  EXPECT_EQ(map_builder.DebugString(),
            "{\"\": b\"\", \"foo\": b\"\", \"bar\": b\"baz\"}");
  ASSERT_OK_AND_ASSIGN(auto map, std::move(map_builder).Build());
  EXPECT_FALSE(map->empty());
  EXPECT_EQ(map->size(), 3);
  ASSERT_OK_AND_ASSIGN(auto entry, map->Get(value_factory, key));
  EXPECT_TRUE((*entry)->Is<BytesValue>());
  EXPECT_TRUE((*entry).As<BytesValue>()->empty());
  ASSERT_OK_AND_ASSIGN(entry, map->Get(value_factory, make_key("foo")));
  EXPECT_TRUE((*entry)->Is<BytesValue>());
  EXPECT_TRUE((*entry).As<BytesValue>()->empty());
  ASSERT_OK_AND_ASSIGN(entry, map->Get(value_factory, make_key("bar")));
  EXPECT_TRUE((*entry)->Is<BytesValue>());
  EXPECT_EQ((*entry).As<BytesValue>()->ToString(), "baz");
  EXPECT_EQ(map->DebugString(),
            "{\"\": b\"\", \"foo\": b\"\", \"bar\": b\"baz\"}");
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
  EXPECT_THAT(map_builder.InsertOrUpdate(key, value),
              IsOkAndHolds(IsFalse()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrUpdate(key, make_value()),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.InsertOrUpdate(make_key("bar"), value),
              IsOkAndHolds(IsTrue()));  // rvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrUpdate(make_key("bar"), make_value("baz")),
              IsOkAndHolds(IsFalse()));  // rvalue, rvalue
  EXPECT_TRUE(map_builder.Has(key));
  EXPECT_FALSE(map_builder.empty());
  EXPECT_EQ(map_builder.size(), 3);
  EXPECT_EQ(map_builder.DebugString(),
            "{\"\": b\"\", \"foo\": b\"\", \"bar\": b\"baz\"}");
  ASSERT_OK_AND_ASSIGN(auto map, std::move(map_builder).Build());
  EXPECT_FALSE(map->empty());
  EXPECT_EQ(map->size(), 3);
  ASSERT_OK_AND_ASSIGN(auto entry, map->Get(value_factory, key));
  EXPECT_TRUE((*entry)->Is<BytesValue>());
  EXPECT_TRUE((*entry).As<BytesValue>()->empty());
  ASSERT_OK_AND_ASSIGN(entry, map->Get(value_factory, make_key("foo")));
  EXPECT_TRUE((*entry)->Is<BytesValue>());
  EXPECT_TRUE((*entry).As<BytesValue>()->empty());
  ASSERT_OK_AND_ASSIGN(entry, map->Get(value_factory, make_key("bar")));
  EXPECT_TRUE((*entry)->Is<BytesValue>());
  EXPECT_EQ((*entry).As<BytesValue>()->ToString(), "baz");
  EXPECT_EQ(map->DebugString(),
            "{\"\": b\"\", \"foo\": b\"\", \"bar\": b\"baz\"}");
}

template <typename Key, typename Value, typename GetKey, typename GetValue,
          typename MakeKey, typename MakeValue>
void TestMapBuilder(GetKey get_key, GetValue get_value, MakeKey make_key1,
                    MakeKey make_key2, MakeKey make_key3, MakeValue make_value1,
                    MakeValue make_value2, absl::string_view debug_string) {
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
  EXPECT_THAT(map_builder.InsertOrUpdate(key, value),
              IsOkAndHolds(IsFalse()));  // lvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrUpdate(key, make_value1(value_factory)),
              IsOkAndHolds(IsFalse()));  // lvalue, rvalue
  EXPECT_THAT(map_builder.InsertOrUpdate(make_key3(value_factory), value),
              IsOkAndHolds(IsTrue()));  // rvalue, lvalue
  EXPECT_THAT(map_builder.InsertOrUpdate(make_key3(value_factory),
                                         make_value2(value_factory)),
              IsOkAndHolds(IsFalse()));  // rvalue, rvalue
  EXPECT_TRUE(map_builder.Has(key));
  EXPECT_FALSE(map_builder.empty());
  EXPECT_EQ(map_builder.size(), 3);
  EXPECT_EQ(map_builder.DebugString(), debug_string);
  ASSERT_OK_AND_ASSIGN(auto map, std::move(map_builder).Build());
  EXPECT_FALSE(map->empty());
  EXPECT_EQ(map->size(), 3);
  ASSERT_OK_AND_ASSIGN(auto entry, map->Get(value_factory, key));
  EXPECT_TRUE((*entry)->template Is<Value>());
  EXPECT_EQ(*((*entry).template As<Value>()),
            *((make_value1(value_factory)).template As<Value>()));
  ASSERT_OK_AND_ASSIGN(entry,
                       map->Get(value_factory, make_key2(value_factory)));
  EXPECT_TRUE((*entry)->template Is<Value>());
  EXPECT_EQ(*((*entry).template As<Value>()),
            *((make_value1(value_factory)).template As<Value>()));
  ASSERT_OK_AND_ASSIGN(entry,
                       map->Get(value_factory, make_key3(value_factory)));
  EXPECT_TRUE((*entry)->template Is<Value>());
  EXPECT_EQ(*((*entry).template As<Value>()),
            *((make_value2(value_factory)).template As<Value>()));
  EXPECT_EQ(map->DebugString(), debug_string);
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
      MakeBoolValue<true>, "{0: false, 1: false, 2: true}");
}

TEST(MapValueBuilder, IntInt) {
  TestMapBuilder<IntValue, IntValue>(
      &TypeFactory::GetIntType, &TypeFactory::GetIntType, MakeIntValue<0>,
      MakeIntValue<1>, MakeIntValue<2>, MakeIntValue<0>, MakeIntValue<1>,
      "{0: 0, 1: 0, 2: 1}");
}

TEST(MapValueBuilder, IntUint) {
  TestMapBuilder<IntValue, UintValue>(
      &TypeFactory::GetIntType, &TypeFactory::GetUintType, MakeIntValue<0>,
      MakeIntValue<1>, MakeIntValue<2>, MakeUintValue<0>, MakeUintValue<1>,
      "{0: 0u, 1: 0u, 2: 1u}");
}

TEST(MapValueBuilder, IntDouble) {
  TestMapBuilder<IntValue, DoubleValue>(
      &TypeFactory::GetIntType, &TypeFactory::GetDoubleType, MakeIntValue<0>,
      MakeIntValue<1>, MakeIntValue<2>, MakeDoubleValue(0.0),
      MakeDoubleValue(1.0), "{0: 0.0, 1: 0.0, 2: 1.0}");
}

TEST(MapValueBuilder, IntDuration) {
  TestMapBuilder<IntValue, DurationValue>(
      &TypeFactory::GetIntType, &TypeFactory::GetDurationType, MakeIntValue<0>,
      MakeIntValue<1>, MakeIntValue<2>, MakeDurationValue(absl::ZeroDuration()),
      MakeDurationValue(absl::Seconds(1)), "{0: 0, 1: 0, 2: 1s}");
}

TEST(MapValueBuilder, IntTimestamp) {
  TestMapBuilder<IntValue, TimestampValue>(
      &TypeFactory::GetIntType, &TypeFactory::GetTimestampType, MakeIntValue<0>,
      MakeIntValue<1>, MakeIntValue<2>, MakeTimestampValue(absl::UnixEpoch()),
      MakeTimestampValue(absl::UnixEpoch() + absl::Seconds(1)),
      "{0: 1970-01-01T00:00:00Z, 1: 1970-01-01T00:00:00Z, 2: "
      "1970-01-01T00:00:01Z}");
}

TEST(MapValueBuilder, UintBool) {
  TestMapBuilder<UintValue, BoolValue>(
      &TypeFactory::GetUintType, &TypeFactory::GetBoolType, MakeUintValue<0>,
      MakeUintValue<1>, MakeUintValue<2>, MakeBoolValue<false>,
      MakeBoolValue<true>, "{0u: false, 1u: false, 2u: true}");
}

TEST(MapValueBuilder, UintInt) {
  TestMapBuilder<UintValue, IntValue>(
      &TypeFactory::GetUintType, &TypeFactory::GetIntType, MakeUintValue<0>,
      MakeUintValue<1>, MakeUintValue<2>, MakeIntValue<0>, MakeIntValue<1>,
      "{0u: 0, 1u: 0, 2u: 1}");
}

TEST(MapValueBuilder, UintUint) {
  TestMapBuilder<UintValue, UintValue>(
      &TypeFactory::GetUintType, &TypeFactory::GetUintType, MakeUintValue<0>,
      MakeUintValue<1>, MakeUintValue<2>, MakeUintValue<0>, MakeUintValue<1>,
      "{0u: 0u, 1u: 0u, 2u: 1u}");
}

TEST(MapValueBuilder, UintDouble) {
  TestMapBuilder<UintValue, DoubleValue>(
      &TypeFactory::GetUintType, &TypeFactory::GetDoubleType, MakeUintValue<0>,
      MakeUintValue<1>, MakeUintValue<2>, MakeDoubleValue(0.0),
      MakeDoubleValue(1.0), "{0u: 0.0, 1u: 0.0, 2u: 1.0}");
}

TEST(MapValueBuilder, UintDuration) {
  TestMapBuilder<UintValue, DurationValue>(
      &TypeFactory::GetUintType, &TypeFactory::GetDurationType,
      MakeUintValue<0>, MakeUintValue<1>, MakeUintValue<2>,
      MakeDurationValue(absl::ZeroDuration()),
      MakeDurationValue(absl::Seconds(1)), "{0u: 0, 1u: 0, 2u: 1s}");
}

TEST(MapValueBuilder, UintTimestamp) {
  TestMapBuilder<UintValue, TimestampValue>(
      &TypeFactory::GetUintType, &TypeFactory::GetTimestampType,
      MakeUintValue<0>, MakeUintValue<1>, MakeUintValue<2>,
      MakeTimestampValue(absl::UnixEpoch()),
      MakeTimestampValue(absl::UnixEpoch() + absl::Seconds(1)),
      "{0u: 1970-01-01T00:00:00Z, 1u: 1970-01-01T00:00:00Z, 2u: "
      "1970-01-01T00:00:01Z}");
}

// TODO(issues/5): add <Primitive>Generic, Generic<Primitive>, and friends

}  // namespace
}  // namespace cel
