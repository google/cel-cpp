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

#include <cstddef>
#include <cstdint>
#include <memory>
#include <ostream>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/hash/hash.h"
#include "absl/status/status.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_factory.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::_;
using testing::TestParamInfo;
using testing::TestWithParam;
using testing::UnorderedElementsAreArray;
using cel::internal::IsOk;
using cel::internal::StatusIs;

enum class Typing {
  kStatic,
  kDynamic,
};

std::ostream& operator<<(std::ostream& out, Typing typing) {
  switch (typing) {
    case Typing::kStatic:
      return out << "STATIC";
    case Typing::kDynamic:
      return out << "DYNAMIC";
  }
}

class MapValueBuilderTest
    : public TestWithParam<std::tuple<MemoryManagement, Typing, Typing>> {
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
    type_factory_ = NewThreadCompatibleTypeFactory(memory_manager());
  }

  std::unique_ptr<MapValueBuilderInterface> NewIntDoubleMapValueBuilder() {
    switch (key_typing()) {
      case Typing::kStatic:
        switch (value_typing()) {
          case Typing::kStatic:
            return std::make_unique<MapValueBuilder<IntValue, DoubleValue>>(
                **type_factory_, IntTypeView(), DoubleTypeView());
          case Typing::kDynamic:
            return std::make_unique<MapValueBuilder<IntValue, Value>>(
                **type_factory_, IntTypeView(), DoubleTypeView());
        }
      case Typing::kDynamic:
        switch (value_typing()) {
          case Typing::kStatic:
            return std::make_unique<MapValueBuilder<Value, DoubleValue>>(
                **type_factory_, IntTypeView(), DoubleTypeView());
          case Typing::kDynamic:
            return std::make_unique<MapValueBuilder<Value, Value>>(
                **type_factory_, IntTypeView(), DoubleTypeView());
        }
    }
  }

  void TearDown() override { Finish(); }

  void Finish() {
    type_factory_.reset();
    memory_manager_.reset();
  }

  MemoryManagerRef memory_manager() { return *memory_manager_; }

  MemoryManagement memory_management() const { return std::get<0>(GetParam()); }

  Typing key_typing() const { return std::get<1>(GetParam()); }

  Typing value_typing() const { return std::get<2>(GetParam()); }

  static std::string ToString(
      TestParamInfo<std::tuple<MemoryManagement, Typing, Typing>> param) {
    std::ostringstream out;
    out << std::get<0>(param.param) << "_" << std::get<1>(param.param) << "_"
        << std::get<2>(param.param);
    return out.str();
  }

 private:
  absl::optional<MemoryManager> memory_manager_;
  absl::optional<Shared<TypeFactory>> type_factory_;
};

TEST_P(MapValueBuilderTest, Coverage) {
  constexpr size_t kNumValues = 64;
  auto builder = NewIntDoubleMapValueBuilder();
  EXPECT_TRUE(builder->IsEmpty());
  EXPECT_EQ(builder->Size(), 0);
  builder->Reserve(kNumValues);
  for (size_t index = 0; index < kNumValues; ++index) {
    builder->Put(IntValue(index), DoubleValue(index));
  }
  EXPECT_FALSE(builder->IsEmpty());
  EXPECT_EQ(builder->Size(), kNumValues);
  auto value = std::move(*builder).Build();
  EXPECT_FALSE(value.IsEmpty());
  EXPECT_EQ(value.Size(), kNumValues);
}

INSTANTIATE_TEST_SUITE_P(
    MapValueBuilderTest, MapValueBuilderTest,
    ::testing::Combine(::testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting),
                       ::testing::Values(Typing::kStatic, Typing::kDynamic),
                       ::testing::Values(Typing::kStatic, Typing::kDynamic)),
    MapValueBuilderTest::ToString);

TEST(MapValue, CheckKey) {
  EXPECT_THAT(MapValueView::CheckKey(BoolValueView()), IsOk());
  EXPECT_THAT(MapValueView::CheckKey(IntValueView()), IsOk());
  EXPECT_THAT(MapValueView::CheckKey(UintValueView()), IsOk());
  EXPECT_THAT(MapValueView::CheckKey(StringValueView()), IsOk());
  EXPECT_THAT(MapValueView::CheckKey(BytesValueView()),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(MapValueKeyHash, Value) {
  using Hash = common_internal::MapValueKeyHash<Value>;
  EXPECT_EQ(Hash{}(BoolValueView()),
            absl::HashOf(ValueKind::kBool, BoolValueView()));
  EXPECT_EQ(Hash{}(IntValueView()),
            absl::HashOf(ValueKind::kInt, IntValueView()));
  EXPECT_EQ(Hash{}(UintValueView()),
            absl::HashOf(ValueKind::kUint, UintValueView()));
  EXPECT_EQ(Hash{}(StringValueView()),
            absl::HashOf(ValueKind::kString, StringValueView()));
  EXPECT_DEBUG_DEATH(Hash{}(BytesValueView()), _);
}

TEST(MapValueKeyEqualTo, Value) {
  using EqualTo = common_internal::MapValueKeyEqualTo<Value>;
  EXPECT_TRUE(EqualTo{}(BoolValueView(), BoolValueView()));
  EXPECT_FALSE(EqualTo{}(BoolValueView(), IntValueView()));
  EXPECT_DEBUG_DEATH(EqualTo{}(BoolValueView(), BytesValueView()), _);
  EXPECT_FALSE(EqualTo{}(IntValueView(), BoolValueView()));
  EXPECT_TRUE(EqualTo{}(IntValueView(), IntValueView()));
  EXPECT_DEBUG_DEATH(EqualTo{}(IntValueView(), BytesValueView()), _);
  EXPECT_TRUE(EqualTo{}(UintValueView(), UintValueView()));
  EXPECT_FALSE(EqualTo{}(UintValueView(), BoolValueView()));
  EXPECT_DEBUG_DEATH(EqualTo{}(UintValueView(), BytesValueView()), _);
  EXPECT_TRUE(EqualTo{}(StringValueView(), StringValueView()));
  EXPECT_FALSE(EqualTo{}(StringValueView(), BoolValueView()));
  EXPECT_DEBUG_DEATH(EqualTo{}(StringValueView(), BytesValueView()), _);
  EXPECT_DEBUG_DEATH(EqualTo{}(BytesValueView(), BytesValueView()), _);
}

TEST(MapValueKeyLess, Value) {
  using Less = common_internal::MapValueKeyLess<Value>;
  EXPECT_TRUE(Less{}(BoolValueView(false), BoolValueView(true)));
  EXPECT_TRUE(Less{}(BoolValueView(), IntValueView()));
  EXPECT_DEBUG_DEATH(Less{}(BoolValueView(), BytesValueView()), _);
  EXPECT_TRUE(Less{}(IntValueView(), IntValueView(1)));
  EXPECT_FALSE(Less{}(IntValueView(), BoolValueView()));
  EXPECT_TRUE(Less{}(IntValueView(), UintValueView()));
  EXPECT_DEBUG_DEATH(Less{}(IntValueView(), BytesValueView()), _);
  EXPECT_TRUE(Less{}(UintValueView(), UintValueView(1)));
  EXPECT_FALSE(Less{}(UintValueView(), BoolValueView()));
  EXPECT_TRUE(Less{}(UintValueView(), StringValueView()));
  EXPECT_DEBUG_DEATH(Less{}(UintValueView(), BytesValueView()), _);
  EXPECT_TRUE(Less{}(StringValueView("bar"), StringValueView("foo")));
  EXPECT_FALSE(Less{}(StringValueView(), BoolValueView()));
  EXPECT_DEBUG_DEATH(Less{}(StringValueView(), BytesValueView()), _);
  EXPECT_DEBUG_DEATH(Less{}(BytesValueView(), BytesValueView()), _);
}

class MapValueTest : public TestWithParam<MemoryManagement> {
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
    type_factory_ = NewThreadCompatibleTypeFactory(memory_manager());
  }

  template <typename... Args>
  MapValue NewIntDoubleMapValue(Args&&... args) {
    MapValueBuilder<IntValue, DoubleValue> builder(
        **type_factory_, IntTypeView(), DoubleTypeView());
    (builder.Put(std::forward<Args>(args).first,
                 std::forward<Args>(args).second),
     ...);
    return std::move(builder).Build();
  }

  ListType GetIntListType() {
    return (*type_factory_)->CreateListType(IntTypeView());
  }

  MapType GetIntDoubleMapType() {
    return (*type_factory_)->CreateMapType(IntTypeView(), DoubleTypeView());
  }

  void TearDown() override { Finish(); }

  void Finish() {
    type_factory_.reset();
    memory_manager_.reset();
  }

  MemoryManagerRef memory_manager() { return *memory_manager_; }

  MemoryManagement memory_management() const { return GetParam(); }

  TypeFactory& type_factory() const { return **type_factory_; }

  static std::string ToString(TestParamInfo<MemoryManagement> param) {
    std::ostringstream out;
    out << param.param;
    return out.str();
  }

 private:
  absl::optional<MemoryManager> memory_manager_;
  absl::optional<Shared<TypeFactory>> type_factory_;
};

TEST_P(MapValueTest, Default) {
  Value scratch;
  MapValue map_value;
  EXPECT_TRUE(map_value.IsEmpty());
  EXPECT_EQ(map_value.Size(), 0);
  EXPECT_EQ(map_value.DebugString(), "{}");
  EXPECT_EQ(map_value.type().key(), DynTypeView());
  EXPECT_EQ(map_value.type().value(), DynTypeView());
  ASSERT_OK_AND_ASSIGN(auto list_value, map_value.ListKeys(type_factory()));
  EXPECT_TRUE(list_value.IsEmpty());
  EXPECT_EQ(list_value.Size(), 0);
  EXPECT_EQ(list_value.DebugString(), "[]");
  EXPECT_EQ(list_value.type().element(), DynTypeView());
  ASSERT_OK_AND_ASSIGN(auto iterator, map_value.NewIterator());
  EXPECT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(scratch),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(MapValueTest, Kind) {
  auto value = NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                                    std::pair{IntValue(1), DoubleValue(4.0)},
                                    std::pair{IntValue(2), DoubleValue(5.0)});
  EXPECT_EQ(value.kind(), MapValue::kKind);
  EXPECT_EQ(Value(value).kind(), MapValue::kKind);
}

TEST_P(MapValueTest, Type) {
  auto value = NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                                    std::pair{IntValue(1), DoubleValue(4.0)},
                                    std::pair{IntValue(2), DoubleValue(5.0)});
  EXPECT_EQ(value.type(), GetIntDoubleMapType());
  EXPECT_EQ(Value(value).type(), GetIntDoubleMapType());
}

TEST_P(MapValueTest, DebugString) {
  auto value = NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                                    std::pair{IntValue(1), DoubleValue(4.0)},
                                    std::pair{IntValue(2), DoubleValue(5.0)});
  {
    std::ostringstream out;
    out << value;
    EXPECT_EQ(out.str(), "{0: 3.0, 1: 4.0, 2: 5.0}");
  }
  {
    std::ostringstream out;
    out << Value(value);
    EXPECT_EQ(out.str(), "{0: 3.0, 1: 4.0, 2: 5.0}");
  }
}

TEST_P(MapValueTest, IsEmpty) {
  auto value = NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                                    std::pair{IntValue(1), DoubleValue(4.0)},
                                    std::pair{IntValue(2), DoubleValue(5.0)});
  EXPECT_FALSE(value.IsEmpty());
}

TEST_P(MapValueTest, Size) {
  auto value = NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                                    std::pair{IntValue(1), DoubleValue(4.0)},
                                    std::pair{IntValue(2), DoubleValue(5.0)});
  EXPECT_EQ(value.Size(), 3);
}

TEST_P(MapValueTest, Get) {
  Value scratch;
  auto map_value =
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)});
  ASSERT_OK_AND_ASSIGN(auto value, map_value.Get(IntValueView(0), scratch));
  ASSERT_TRUE(InstanceOf<DoubleValueView>(value));
  ASSERT_EQ(Cast<DoubleValueView>(value).NativeValue(), 3.0);
  ASSERT_OK_AND_ASSIGN(value, map_value.Get(IntValueView(1), scratch));
  ASSERT_TRUE(InstanceOf<DoubleValueView>(value));
  ASSERT_EQ(Cast<DoubleValueView>(value).NativeValue(), 4.0);
  ASSERT_OK_AND_ASSIGN(value, map_value.Get(IntValueView(2), scratch));
  ASSERT_TRUE(InstanceOf<DoubleValueView>(value));
  ASSERT_EQ(Cast<DoubleValueView>(value).NativeValue(), 5.0);
  EXPECT_THAT(map_value.Get(IntValueView(3), scratch),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_P(MapValueTest, Find) {
  Value scratch;
  auto map_value =
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)});
  ValueView value;
  bool ok;
  ASSERT_OK_AND_ASSIGN(std::tie(value, ok),
                       map_value.Find(IntValueView(0), scratch));
  ASSERT_TRUE(ok);
  ASSERT_TRUE(InstanceOf<DoubleValueView>(value));
  ASSERT_EQ(Cast<DoubleValueView>(value).NativeValue(), 3.0);
  ASSERT_OK_AND_ASSIGN(std::tie(value, ok),
                       map_value.Find(IntValueView(1), scratch));
  ASSERT_TRUE(ok);
  ASSERT_TRUE(InstanceOf<DoubleValueView>(value));
  ASSERT_EQ(Cast<DoubleValueView>(value).NativeValue(), 4.0);
  ASSERT_OK_AND_ASSIGN(std::tie(value, ok),
                       map_value.Find(IntValueView(2), scratch));
  ASSERT_TRUE(ok);
  ASSERT_TRUE(InstanceOf<DoubleValueView>(value));
  ASSERT_EQ(Cast<DoubleValueView>(value).NativeValue(), 5.0);
  ASSERT_OK_AND_ASSIGN(std::tie(value, ok),
                       map_value.Find(IntValueView(3), scratch));
  ASSERT_FALSE(ok);
}

TEST_P(MapValueTest, Has) {
  auto map_value =
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)});
  ASSERT_OK_AND_ASSIGN(auto value, map_value.Has(IntValueView(0)));
  ASSERT_TRUE(InstanceOf<BoolValueView>(value));
  ASSERT_TRUE(Cast<BoolValueView>(value).NativeValue());
  ASSERT_OK_AND_ASSIGN(value, map_value.Has(IntValueView(1)));
  ASSERT_TRUE(InstanceOf<BoolValueView>(value));
  ASSERT_TRUE(Cast<BoolValueView>(value).NativeValue());
  ASSERT_OK_AND_ASSIGN(value, map_value.Has(IntValueView(2)));
  ASSERT_TRUE(InstanceOf<BoolValueView>(value));
  ASSERT_TRUE(Cast<BoolValueView>(value).NativeValue());
  ASSERT_OK_AND_ASSIGN(value, map_value.Has(IntValueView(3)));
  ASSERT_TRUE(InstanceOf<BoolValueView>(value));
  ASSERT_FALSE(Cast<BoolValueView>(value).NativeValue());
}

TEST_P(MapValueTest, ListKeys) {
  auto map_value =
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)});
  ASSERT_OK_AND_ASSIGN(auto list_keys, map_value.ListKeys(type_factory()));
  std::vector<int64_t> keys;
  ASSERT_OK(list_keys.ForEach([&keys](ValueView element) -> bool {
    keys.push_back(Cast<IntValueView>(element).NativeValue());
    return true;
  }));
  EXPECT_THAT(keys, UnorderedElementsAreArray({0, 1, 2}));
}

TEST_P(MapValueTest, ForEach) {
  auto value = NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                                    std::pair{IntValue(1), DoubleValue(4.0)},
                                    std::pair{IntValue(2), DoubleValue(5.0)});
  std::vector<std::pair<int64_t, double>> entries;
  EXPECT_OK(value.ForEach([&entries](ValueView key, ValueView value) {
    entries.push_back(std::pair{Cast<IntValueView>(key).NativeValue(),
                                Cast<DoubleValueView>(value).NativeValue()});
    return true;
  }));
  EXPECT_THAT(entries,
              UnorderedElementsAreArray(
                  {std::pair{0, 3.0}, std::pair{1, 4.0}, std::pair{2, 5.0}}));
}

TEST_P(MapValueTest, NewIterator) {
  Value scratch;
  auto value = NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                                    std::pair{IntValue(1), DoubleValue(4.0)},
                                    std::pair{IntValue(2), DoubleValue(5.0)});
  ASSERT_OK_AND_ASSIGN(auto iterator, value.NewIterator());
  std::vector<int64_t> keys;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(auto element, iterator->Next(scratch));
    ASSERT_TRUE(InstanceOf<IntValueView>(element));
    keys.push_back(Cast<IntValueView>(element).NativeValue());
  }
  EXPECT_EQ(iterator->HasNext(), false);
  EXPECT_THAT(iterator->Next(scratch),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(keys, UnorderedElementsAreArray({0, 1, 2}));
}

INSTANTIATE_TEST_SUITE_P(
    MapValueTest, MapValueTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    MapValueTest::ToString);

class MapValueViewTest : public TestWithParam<MemoryManagement> {
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
    type_factory_ = NewThreadCompatibleTypeFactory(memory_manager());
  }

  template <typename... Args>
  MapValue NewIntDoubleMapValue(Args&&... args) {
    MapValueBuilder<IntValue, DoubleValue> builder(
        **type_factory_, IntTypeView(), DoubleTypeView());
    (builder.Put(std::forward<Args>(args).first,
                 std::forward<Args>(args).second),
     ...);
    return std::move(builder).Build();
  }

  ListType GetIntListType() {
    return (*type_factory_)->CreateListType(IntTypeView());
  }

  MapType GetIntDoubleMapType() {
    return (*type_factory_)->CreateMapType(IntTypeView(), DoubleTypeView());
  }

  void TearDown() override { Finish(); }

  void Finish() {
    type_factory_.reset();
    memory_manager_.reset();
  }

  MemoryManagerRef memory_manager() { return *memory_manager_; }

  MemoryManagement memory_management() const { return GetParam(); }

  TypeFactory& type_factory() const { return **type_factory_; }

  static std::string ToString(TestParamInfo<MemoryManagement> param) {
    std::ostringstream out;
    out << param.param;
    return out.str();
  }

 private:
  absl::optional<MemoryManager> memory_manager_;
  absl::optional<Shared<TypeFactory>> type_factory_;
};

TEST_P(MapValueViewTest, Default) {
  Value scratch;
  MapValueView map_value;
  EXPECT_TRUE(map_value.IsEmpty());
  EXPECT_EQ(map_value.Size(), 0);
  EXPECT_EQ(map_value.DebugString(), "{}");
  EXPECT_EQ(map_value.type().key(), DynTypeView());
  EXPECT_EQ(map_value.type().value(), DynTypeView());
  ASSERT_OK_AND_ASSIGN(auto list_value, map_value.ListKeys(type_factory()));
  EXPECT_TRUE(list_value.IsEmpty());
  EXPECT_EQ(list_value.Size(), 0);
  EXPECT_EQ(list_value.DebugString(), "[]");
  EXPECT_EQ(list_value.type().element(), DynTypeView());
  ASSERT_OK_AND_ASSIGN(auto iterator, map_value.NewIterator());
  EXPECT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(scratch),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(MapValueViewTest, Kind) {
  auto value = NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                                    std::pair{IntValue(1), DoubleValue(4.0)},
                                    std::pair{IntValue(2), DoubleValue(5.0)});
  EXPECT_EQ(MapValueView(value).kind(), MapValue::kKind);
  EXPECT_EQ(ValueView(MapValueView(value)).kind(), MapValue::kKind);
}

TEST_P(MapValueViewTest, Type) {
  auto value = NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                                    std::pair{IntValue(1), DoubleValue(4.0)},
                                    std::pair{IntValue(2), DoubleValue(5.0)});
  EXPECT_EQ(MapValueView(value).type(), GetIntDoubleMapType());
  EXPECT_EQ(ValueView(MapValueView(value)).type(), GetIntDoubleMapType());
}

TEST_P(MapValueViewTest, DebugString) {
  auto value = NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                                    std::pair{IntValue(1), DoubleValue(4.0)},
                                    std::pair{IntValue(2), DoubleValue(5.0)});
  {
    std::ostringstream out;
    out << MapValueView(value);
    EXPECT_EQ(out.str(), "{0: 3.0, 1: 4.0, 2: 5.0}");
  }
  {
    std::ostringstream out;
    out << ValueView(MapValueView(value));
    EXPECT_EQ(out.str(), "{0: 3.0, 1: 4.0, 2: 5.0}");
  }
}

TEST_P(MapValueViewTest, IsEmpty) {
  auto value = NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                                    std::pair{IntValue(1), DoubleValue(4.0)},
                                    std::pair{IntValue(2), DoubleValue(5.0)});
  EXPECT_FALSE(MapValueView(value).IsEmpty());
}

TEST_P(MapValueViewTest, Size) {
  auto value = NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                                    std::pair{IntValue(1), DoubleValue(4.0)},
                                    std::pair{IntValue(2), DoubleValue(5.0)});
  EXPECT_EQ(MapValueView(value).Size(), 3);
}

TEST_P(MapValueViewTest, Get) {
  Value scratch;
  auto map_value =
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)});
  ASSERT_OK_AND_ASSIGN(auto value,
                       MapValueView(map_value).Get(IntValueView(0), scratch));
  ASSERT_TRUE(InstanceOf<DoubleValueView>(value));
  ASSERT_EQ(Cast<DoubleValueView>(value).NativeValue(), 3.0);
  ASSERT_OK_AND_ASSIGN(value,
                       MapValueView(map_value).Get(IntValueView(1), scratch));
  ASSERT_TRUE(InstanceOf<DoubleValueView>(value));
  ASSERT_EQ(Cast<DoubleValueView>(value).NativeValue(), 4.0);
  ASSERT_OK_AND_ASSIGN(value,
                       MapValueView(map_value).Get(IntValueView(2), scratch));
  ASSERT_TRUE(InstanceOf<DoubleValueView>(value));
  ASSERT_EQ(Cast<DoubleValueView>(value).NativeValue(), 5.0);
  EXPECT_THAT(MapValueView(map_value).Get(IntValueView(3), scratch),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_P(MapValueViewTest, Find) {
  Value scratch;
  auto map_value =
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)});
  ValueView value;
  bool ok;
  ASSERT_OK_AND_ASSIGN(std::tie(value, ok),
                       MapValueView(map_value).Find(IntValueView(0), scratch));
  ASSERT_TRUE(ok);
  ASSERT_TRUE(InstanceOf<DoubleValueView>(value));
  ASSERT_EQ(Cast<DoubleValueView>(value).NativeValue(), 3.0);
  ASSERT_OK_AND_ASSIGN(std::tie(value, ok),
                       MapValueView(map_value).Find(IntValueView(1), scratch));
  ASSERT_TRUE(ok);
  ASSERT_TRUE(InstanceOf<DoubleValueView>(value));
  ASSERT_EQ(Cast<DoubleValueView>(value).NativeValue(), 4.0);
  ASSERT_OK_AND_ASSIGN(std::tie(value, ok),
                       MapValueView(map_value).Find(IntValueView(2), scratch));
  ASSERT_TRUE(ok);
  ASSERT_TRUE(InstanceOf<DoubleValueView>(value));
  ASSERT_EQ(Cast<DoubleValueView>(value).NativeValue(), 5.0);
  ASSERT_OK_AND_ASSIGN(std::tie(value, ok),
                       MapValueView(map_value).Find(IntValueView(3), scratch));
  ASSERT_FALSE(ok);
}

TEST_P(MapValueViewTest, Has) {
  auto map_value =
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)});
  ASSERT_OK_AND_ASSIGN(auto value,
                       MapValueView(map_value).Has(IntValueView(0)));
  ASSERT_TRUE(InstanceOf<BoolValueView>(value));
  ASSERT_TRUE(Cast<BoolValueView>(value).NativeValue());
  ASSERT_OK_AND_ASSIGN(value, MapValueView(map_value).Has(IntValueView(1)));
  ASSERT_TRUE(InstanceOf<BoolValueView>(value));
  ASSERT_TRUE(Cast<BoolValueView>(value).NativeValue());
  ASSERT_OK_AND_ASSIGN(value, MapValueView(map_value).Has(IntValueView(2)));
  ASSERT_TRUE(InstanceOf<BoolValueView>(value));
  ASSERT_TRUE(Cast<BoolValueView>(value).NativeValue());
  ASSERT_OK_AND_ASSIGN(value, MapValueView(map_value).Has(IntValueView(3)));
  ASSERT_TRUE(InstanceOf<BoolValueView>(value));
  ASSERT_FALSE(Cast<BoolValueView>(value).NativeValue());
}

TEST_P(MapValueViewTest, ListKeys) {
  auto map_value =
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)});
  ASSERT_OK_AND_ASSIGN(auto list_keys,
                       MapValueView(map_value).ListKeys(type_factory()));
  std::vector<int64_t> keys;
  ASSERT_OK(list_keys.ForEach([&keys](ValueView element) -> bool {
    keys.push_back(Cast<IntValueView>(element).NativeValue());
    return true;
  }));
  EXPECT_THAT(keys, UnorderedElementsAreArray({0, 1, 2}));
}

TEST_P(MapValueViewTest, ForEach) {
  auto value = NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                                    std::pair{IntValue(1), DoubleValue(4.0)},
                                    std::pair{IntValue(2), DoubleValue(5.0)});
  std::vector<std::pair<int64_t, double>> entries;
  EXPECT_OK(MapValueView(value).ForEach([&entries](ValueView key,
                                                   ValueView value) {
    entries.push_back(std::pair{Cast<IntValueView>(key).NativeValue(),
                                Cast<DoubleValueView>(value).NativeValue()});
    return true;
  }));
  EXPECT_THAT(entries,
              UnorderedElementsAreArray(
                  {std::pair{0, 3.0}, std::pair{1, 4.0}, std::pair{2, 5.0}}));
}

TEST_P(MapValueViewTest, NewIterator) {
  Value scratch;
  auto value = NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                                    std::pair{IntValue(1), DoubleValue(4.0)},
                                    std::pair{IntValue(2), DoubleValue(5.0)});
  ASSERT_OK_AND_ASSIGN(auto iterator, MapValueView(value).NewIterator());
  std::vector<int64_t> keys;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(auto element, iterator->Next(scratch));
    ASSERT_TRUE(InstanceOf<IntValueView>(element));
    keys.push_back(Cast<IntValueView>(element).NativeValue());
  }
  EXPECT_EQ(iterator->HasNext(), false);
  EXPECT_THAT(iterator->Next(scratch),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(keys, UnorderedElementsAreArray({0, 1, 2}));
}

INSTANTIATE_TEST_SUITE_P(
    MapValueViewTest, MapValueViewTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    MapValueViewTest::ToString);

}  // namespace
}  // namespace cel
