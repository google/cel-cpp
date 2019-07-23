#include "common/converters.h"

#include <utility>

#include "gtest/gtest.h"
#include "absl/memory/memory.h"
#include "common/value.h"

namespace google {
namespace api {
namespace expr {
namespace common {
namespace {

void TestListImpl(const Value& list, const Value& first, const Value& second,
                  const Value& missing) {
  EXPECT_EQ(2, list.list_value().size());
  EXPECT_EQ(second, list.list_value().Get(1));
  EXPECT_EQ(Value::FalseValue(), list.list_value().Contains(missing));
  EXPECT_EQ(Value::FalseValue(),
            list.list_value().Contains(Value::NullValue()));
  EXPECT_EQ(Value::TrueValue(), list.list_value().Contains(first));
}

template <Value::Kind ValueKind, typename T>
void TestList(T&& first, T&& second, T&& missing) {
  std::vector<T> list({first, second});
  std::vector<Value> values = {
      ValueFromList<ValueKind>(list),  // Copy
      ValueFromList<ValueKind>(
          absl::make_unique<std::vector<T>>(list)),  // OwnedPtr
      ValueForList<ValueKind>(&list),                // Ptr
      ValueForList<ValueKind>(&list, absl::nullopt)  // Copy
  };
  EXPECT_TRUE(values[0].owns_value());
  EXPECT_TRUE(values[1].owns_value());
  EXPECT_FALSE(values[2].owns_value());
  EXPECT_TRUE(values[3].owns_value());

  for (const auto& value : values) {
    TestListImpl(value, Value::From<ValueKind>(first),
                 Value::From<ValueKind>(second),
                 Value::From<ValueKind>(missing));
  }
}

TEST(ConverterTest, List_Int) {
  TestList<Value::Kind::kInt, int32_t>(1, 3, 5);
  TestList<Value::Kind::kInt, uint64_t>(1, 3, 5);
  TestList<Value::Kind::kInt, int16_t>(1, 3, 5);
}

TEST(ConverterTest, List_Bool) {
  TestList<Value::Kind::kBool, bool>(true, true, false);
}

void TestMapImpl(const Value& map, const Value& k1, const Value& v1,
                 const Value& k2, const Value& v2) {
  EXPECT_EQ(2, map.map_value().size());

  EXPECT_EQ(v1, map.map_value().Get(k1));
  EXPECT_NE(v1, map.map_value().Get(k2));
  EXPECT_EQ(v2, map.map_value().Get(k2));
  EXPECT_NE(v2, map.map_value().Get(k1));

  EXPECT_EQ(Value::TrueValue(), map.map_value().ContainsKey(k1));
  EXPECT_EQ(Value::TrueValue(), map.map_value().ContainsKey(k2));
  EXPECT_EQ(Value::FalseValue(),
            map.map_value().ContainsKey(Value::NullValue()));

  EXPECT_EQ(Value::TrueValue(), map.map_value().ContainsValue(v1));
  EXPECT_EQ(Value::TrueValue(), map.map_value().ContainsValue(v2));
  EXPECT_EQ(Value::FalseValue(),
            map.map_value().ContainsValue(Value::NullValue()));
}

template <Value::Kind KeyKind, Value::Kind ValueKind, typename K, typename V>
void TestMap(K&& k1, V&& v1, K&& k2, V&& v2) {
  std::map<K, V> map({{k1, v1}, {k2, v2}});
  std::vector<Value> values = {
      ValueFromMap<KeyKind, ValueKind>(map),  // Copy
      ValueFromMap<KeyKind, ValueKind>(
          absl::make_unique<std::map<K, V>>(map)),          // OwnedPtr
      ValueForMap<KeyKind, ValueKind>(&map),                // Ptr
      ValueForMap<KeyKind, ValueKind>(&map, absl::nullopt)  // Copy
  };
  EXPECT_TRUE(values[0].owns_value());
  EXPECT_TRUE(values[1].owns_value());
  EXPECT_FALSE(values[2].owns_value());
  EXPECT_TRUE(values[3].owns_value());

  for (const auto& value : values) {
    TestMapImpl(value, Value::From<KeyKind>(k1), Value::From<ValueKind>(v1),
                Value::From<KeyKind>(k2), Value::From<ValueKind>(v2));
  }
}

TEST(ConverterTest, Map_Int) {
  TestMap<Value::Kind::kInt, Value::Kind::kBool, int32_t>(1, true, 3, false);
  TestMap<Value::Kind::kInt, Value::Kind::kUInt, uint64_t, int16_t>(1, 7, 3, 8);
  TestMap<Value::Kind::kInt, Value::Kind::kInt, int16_t>(1, 7, 3, 5);
}

TEST(ConverterTest, Map_Bool) {
  TestMap<Value::Kind::kBool, Value::Kind::kBool>(true, false, false, true);
}

}  // namespace
}  // namespace common
}  // namespace expr
}  // namespace api
}  // namespace google
