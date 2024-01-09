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

#include "common/value_factory.h"

#include <ostream>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/strings/cord.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_factory.h"
#include "common/types/type_cache.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "common/value_provider.h"
#include "internal/testing.h"

namespace cel {
namespace {

using common_internal::ProcessLocalTypeCache;
using testing::TestParamInfo;
using testing::TestWithParam;
using testing::UnorderedElementsAreArray;

enum class ThreadSafety {
  kCompatible,
  kSafe,
};

std::ostream& operator<<(std::ostream& out, ThreadSafety thread_safety) {
  switch (thread_safety) {
    case ThreadSafety::kCompatible:
      return out << "THREAD_SAFE";
    case ThreadSafety::kSafe:
      return out << "THREAD_COMPATIBLE";
  }
}

class ValueFactoryTest
    : public TestWithParam<std::tuple<MemoryManagement, ThreadSafety>> {
 public:
  void SetUp() override {
    switch (memory_management()) {
      case MemoryManagement::kPooling:
        memory_manager_ =
            MemoryManager::Pooling(NewThreadCompatiblePoolingMemoryManager());
        break;
      case MemoryManagement::kReferenceCounting:
        memory_manager_ = MemoryManager::ReferenceCounting();
        break;
    }
    switch (thread_safety()) {
      case ThreadSafety::kCompatible:
        value_manager_ = NewThreadCompatibleValueManager(
            memory_manager(),
            NewThreadCompatibleValueProvider(memory_manager()));
        break;
      case ThreadSafety::kSafe:
        value_manager_ = NewThreadSafeValueManager(
            memory_manager(), NewThreadSafeValueProvider(memory_manager()));
        break;
    }
  }

  void TearDown() override { Finish(); }

  void Finish() {
    value_manager_.reset();
    memory_manager_.reset();
  }

  MemoryManagerRef memory_manager() { return *memory_manager_; }

  MemoryManagement memory_management() const { return std::get<0>(GetParam()); }

  TypeFactory& type_factory() const { return value_manager(); }

  TypeManager& type_manager() const { return value_manager(); }

  ValueFactory& value_factory() const { return value_manager(); }

  ValueManager& value_manager() const { return **value_manager_; }

  ThreadSafety thread_safety() const { return std::get<1>(GetParam()); }

  static std::string ToString(
      TestParamInfo<std::tuple<MemoryManagement, ThreadSafety>> param) {
    std::ostringstream out;
    out << std::get<0>(param.param) << "_" << std::get<1>(param.param);
    return out.str();
  }

 private:
  absl::optional<MemoryManager> memory_manager_;
  absl::optional<Shared<ValueManager>> value_manager_;
};

TEST_P(ValueFactoryTest, JsonValueNull) {
  auto value = value_factory().CreateValueFromJson(kJsonNull);
  EXPECT_TRUE(InstanceOf<NullValue>(value));
}

TEST_P(ValueFactoryTest, JsonValueBool) {
  auto value = value_factory().CreateValueFromJson(true);
  ASSERT_TRUE(InstanceOf<BoolValue>(value));
  EXPECT_TRUE(Cast<BoolValue>(value).NativeValue());
}

TEST_P(ValueFactoryTest, JsonValueNumber) {
  auto value = value_factory().CreateValueFromJson(1.0);
  ASSERT_TRUE(InstanceOf<DoubleValue>(value));
  EXPECT_EQ(Cast<DoubleValue>(value).NativeValue(), 1.0);
}

TEST_P(ValueFactoryTest, JsonValueString) {
  auto value = value_factory().CreateValueFromJson(absl::Cord("foo"));
  ASSERT_TRUE(InstanceOf<StringValue>(value));
  EXPECT_EQ(Cast<StringValue>(value).NativeString(), "foo");
}

JsonObject NewJsonObjectForTesting(bool with_array = true,
                                   bool with_nested_object = true);

JsonArray NewJsonArrayForTesting(bool with_nested_array = true,
                                 bool with_object = true) {
  JsonArrayBuilder builder;
  builder.push_back(kJsonNull);
  builder.push_back(true);
  builder.push_back(1.0);
  builder.push_back(absl::Cord("foo"));
  if (with_nested_array) {
    builder.push_back(NewJsonArrayForTesting(false, false));
  }
  if (with_object) {
    builder.push_back(NewJsonObjectForTesting(false, false));
  }
  return std::move(builder).Build();
}

JsonObject NewJsonObjectForTesting(bool with_array, bool with_nested_object) {
  JsonObjectBuilder builder;
  builder.insert_or_assign(absl::Cord("a"), kJsonNull);
  builder.insert_or_assign(absl::Cord("b"), true);
  builder.insert_or_assign(absl::Cord("c"), 1.0);
  builder.insert_or_assign(absl::Cord("d"), absl::Cord("foo"));
  if (with_array) {
    builder.insert_or_assign(absl::Cord("e"),
                             NewJsonArrayForTesting(false, false));
  }
  if (with_nested_object) {
    builder.insert_or_assign(absl::Cord("f"),
                             NewJsonObjectForTesting(false, false));
  }
  return std::move(builder).Build();
}

TEST_P(ValueFactoryTest, JsonValueArray) {
  auto value = value_factory().CreateValueFromJson(NewJsonArrayForTesting());
  ASSERT_TRUE(InstanceOf<ListValue>(value));
  EXPECT_EQ(TypeView(value.GetType(type_manager())),
            type_factory().GetDynListType());
  auto list_value = Cast<ListValue>(value);
  EXPECT_FALSE(list_value.IsEmpty());
  EXPECT_EQ(list_value.Size(), 6);
  EXPECT_EQ(list_value.DebugString(),
            "[null, true, 1.0, \"foo\", [null, true, 1.0, \"foo\"], {\"a\": "
            "null, \"b\": true, \"c\": 1.0, \"d\": \"foo\"}]");
  Value element;
  ASSERT_OK_AND_ASSIGN(auto element_view,
                       list_value.Get(value_manager(), 0, element));
  EXPECT_TRUE(InstanceOf<NullValueView>(element_view));
}

TEST_P(ValueFactoryTest, JsonValueObject) {
  auto value = value_factory().CreateValueFromJson(NewJsonObjectForTesting());
  ASSERT_TRUE(InstanceOf<MapValue>(value));
  EXPECT_EQ(TypeView(value.GetType(type_manager())),
            type_factory().GetStringDynMapType());
  auto map_value = Cast<MapValue>(value);
  EXPECT_FALSE(map_value.IsEmpty());
  EXPECT_EQ(map_value.Size(), 6);
  EXPECT_EQ(map_value.DebugString(),
            "{\"a\": null, \"b\": true, \"c\": 1.0, \"d\": \"foo\", \"e\": "
            "[null, true, 1.0, \"foo\"], \"f\": {\"a\": null, \"b\": true, "
            "\"c\": 1.0, \"d\": \"foo\"}}");

  ListValue keys;
  ASSERT_OK_AND_ASSIGN(auto keys_view,
                       map_value.ListKeys(value_manager(), keys));
  EXPECT_EQ(keys_view.Size(), 6);

  ASSERT_OK_AND_ASSIGN(auto keys_iterator,
                       map_value.NewIterator(value_manager()));
  std::vector<StringValue> string_keys;
  while (keys_iterator->HasNext()) {
    Value key;
    ASSERT_OK_AND_ASSIGN(auto key_view, keys_iterator->Next(key));
    string_keys.push_back(StringValue(Cast<StringValueView>(key_view)));
  }
  EXPECT_THAT(string_keys, UnorderedElementsAreArray(
                               {StringValueView("a"), StringValueView("b"),
                                StringValueView("c"), StringValueView("d"),
                                StringValueView("e"), StringValueView("f")}));
  Value has;
  ASSERT_OK_AND_ASSIGN(
      auto has_view, map_value.Has(value_manager(), StringValueView("a"), has));
  ASSERT_TRUE(InstanceOf<BoolValueView>(has_view));
  EXPECT_TRUE(Cast<BoolValueView>(has_view).NativeValue());
  ASSERT_OK_AND_ASSIGN(
      has_view,
      map_value.Has(value_manager(), StringValueView(absl::Cord("a")), has));
  ASSERT_TRUE(InstanceOf<BoolValueView>(has_view));
  EXPECT_TRUE(Cast<BoolValueView>(has_view).NativeValue());

  Value get;
  ASSERT_OK_AND_ASSIGN(
      auto get_view, map_value.Get(value_manager(), StringValueView("a"), get));
  ASSERT_TRUE(InstanceOf<NullValueView>(get_view));
  ASSERT_OK_AND_ASSIGN(
      get_view,
      map_value.Get(value_manager(), StringValueView(absl::Cord("a")), get));
  ASSERT_TRUE(InstanceOf<NullValueView>(get_view));
}

TEST_P(ValueFactoryTest, ListValue) {
  // Primitive zero value types are cached.
  auto list_value1 = value_factory().CreateZeroListValue(
      type_factory().CreateListType(StringTypeView()));
  EXPECT_TRUE(
      Is(list_value1, value_factory().CreateZeroListValue(
                          type_factory().CreateListType(StringTypeView()))));
  EXPECT_FALSE(
      Is(list_value1, value_factory().CreateZeroListValue(
                          type_factory().CreateListType(BoolTypeView()))));
  // Try types which are not cached to exercise other codepath.
  auto struct_type1 = type_factory().CreateStructType("test.Struct1");
  auto struct_type2 = type_factory().CreateStructType("test.Struct2");
  auto list_value2 = value_factory().CreateZeroListValue(
      type_factory().CreateListType(struct_type1));
  EXPECT_TRUE(
      Is(list_value2, value_factory().CreateZeroListValue(
                          type_factory().CreateListType(struct_type1))));
  EXPECT_FALSE(
      Is(list_value2, value_factory().CreateZeroListValue(
                          type_factory().CreateListType(struct_type2))));

  auto zero_list_value = value_factory().GetZeroDynListValue();
  EXPECT_TRUE(zero_list_value.IsEmpty());
  EXPECT_EQ(zero_list_value.Size(), 0);
  EXPECT_EQ(zero_list_value.GetType(type_manager()),
            ProcessLocalTypeCache::Get()->GetDynListType());
}

TEST_P(ValueFactoryTest, MapValue) {
  // Primitive zero value types are cached.
  auto map_value1 = value_factory().CreateZeroMapValue(
      type_factory().CreateMapType(StringTypeView(), IntTypeView()));
  EXPECT_TRUE(Is(map_value1, value_factory().CreateZeroMapValue(
                                 type_factory().CreateMapType(StringTypeView(),
                                                              IntTypeView()))));
  EXPECT_FALSE(Is(map_value1, value_factory().CreateZeroMapValue(
                                  type_factory().CreateMapType(
                                      StringTypeView(), BoolTypeView()))));
  // Try types which are not cached to exercise other codepath.
  auto struct_type1 = type_factory().CreateStructType("test.Struct1");
  auto struct_type2 = type_factory().CreateStructType("test.Struct2");
  auto map_value2 = value_factory().CreateZeroMapValue(
      type_factory().CreateMapType(StringTypeView(), struct_type1));
  EXPECT_TRUE(Is(map_value2, value_factory().CreateZeroMapValue(
                                 type_factory().CreateMapType(StringTypeView(),
                                                              struct_type1))));
  EXPECT_FALSE(Is(map_value2, value_factory().CreateZeroMapValue(
                                  type_factory().CreateMapType(StringTypeView(),
                                                               struct_type2))));

  auto zero_map_value = value_factory().GetZeroDynDynMapValue();
  EXPECT_TRUE(zero_map_value.IsEmpty());
  EXPECT_EQ(zero_map_value.Size(), 0);
  EXPECT_EQ(zero_map_value.GetType(type_manager()),
            ProcessLocalTypeCache::Get()->GetDynDynMapType());
  zero_map_value = value_factory().GetZeroStringDynMapValue();
  EXPECT_TRUE(zero_map_value.IsEmpty());
  EXPECT_EQ(zero_map_value.Size(), 0);
  EXPECT_EQ(zero_map_value.GetType(type_manager()),
            ProcessLocalTypeCache::Get()->GetStringDynMapType());
}

TEST_P(ValueFactoryTest, OptionalType) {
  // Primitive zero value types are cached.
  auto optional_value1 = value_factory().CreateZeroOptionalValue(
      type_factory().CreateOptionalType(StringTypeView()));
  EXPECT_TRUE(Is(optional_value1,
                 value_factory().CreateZeroOptionalValue(
                     type_factory().CreateOptionalType(StringTypeView()))));
  EXPECT_FALSE(Is(optional_value1,
                  value_factory().CreateZeroOptionalValue(
                      type_factory().CreateOptionalType(BoolTypeView()))));
  // Try types which are not cached to exercise other codepath.
  auto struct_type1 = type_factory().CreateStructType("test.Struct1");
  auto struct_type2 = type_factory().CreateStructType("test.Struct2");
  auto optional_value2 = value_factory().CreateZeroOptionalValue(
      type_factory().CreateOptionalType(struct_type1));
  EXPECT_TRUE(Is(optional_value2,
                 value_factory().CreateZeroOptionalValue(
                     type_factory().CreateOptionalType(struct_type1))));
  EXPECT_FALSE(Is(optional_value2,
                  value_factory().CreateZeroOptionalValue(
                      type_factory().CreateOptionalType(struct_type2))));

  auto zero_optional_value = value_factory().GetZeroDynOptionalValue();
  EXPECT_FALSE(zero_optional_value.HasValue());
  EXPECT_EQ(zero_optional_value.GetType(type_manager()),
            ProcessLocalTypeCache::Get()->GetDynOptionalType());
}

INSTANTIATE_TEST_SUITE_P(
    ValueFactoryTest, ValueFactoryTest,
    ::testing::Combine(::testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting),
                       ::testing::Values(ThreadSafety::kCompatible,
                                         ThreadSafety::kSafe)),
    ValueFactoryTest::ToString);

}  // namespace
}  // namespace cel
