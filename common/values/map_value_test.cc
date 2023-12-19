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

#include <cstdint>
#include <memory>
#include <sstream>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_factory.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/status_macros.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::TestParamInfo;
using testing::UnorderedElementsAreArray;
using cel::internal::IsOk;
using cel::internal::IsOkAndHolds;
using cel::internal::StatusIs;

TEST(MapValue, CheckKey) {
  EXPECT_THAT(MapValueView::CheckKey(BoolValueView()), IsOk());
  EXPECT_THAT(MapValueView::CheckKey(IntValueView()), IsOk());
  EXPECT_THAT(MapValueView::CheckKey(UintValueView()), IsOk());
  EXPECT_THAT(MapValueView::CheckKey(StringValueView()), IsOk());
  EXPECT_THAT(MapValueView::CheckKey(BytesValueView()),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

class MapValueTest : public common_internal::ThreadCompatibleValueTest<> {
 public:
  template <typename... Args>
  absl::StatusOr<MapValue> NewIntDoubleMapValue(Args&&... args) {
    CEL_ASSIGN_OR_RETURN(auto builder,
                         value_provider().NewMapValueBuilder(
                             value_factory(), GetIntDoubleMapType()));
    (static_cast<void>(builder->Put(std::forward<Args>(args).first,
                                    std::forward<Args>(args).second)),
     ...);
    return std::move(*builder).Build();
  }

  ListType GetIntListType() {
    return type_factory().CreateListType(IntTypeView());
  }

  MapType GetIntDoubleMapType() {
    return type_factory().CreateMapType(IntTypeView(), DoubleTypeView());
  }
};

TEST_P(MapValueTest, Default) {
  Value scratch;
  MapValue map_value;
  ListValue map_keys_scratch;
  EXPECT_TRUE(map_value.IsEmpty());
  EXPECT_EQ(map_value.Size(), 0);
  EXPECT_EQ(map_value.DebugString(), "{}");
  EXPECT_EQ(map_value.type().key(), DynTypeView());
  EXPECT_EQ(map_value.type().value(), DynTypeView());
  ASSERT_OK_AND_ASSIGN(
      auto list_value,
      map_value.ListKeys(type_factory(), value_factory(), map_keys_scratch));
  EXPECT_TRUE(list_value.IsEmpty());
  EXPECT_EQ(list_value.Size(), 0);
  EXPECT_EQ(list_value.DebugString(), "[]");
  EXPECT_EQ(list_value.type().element(), DynTypeView());
  ASSERT_OK_AND_ASSIGN(auto iterator, map_value.NewIterator(value_factory()));
  EXPECT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(scratch),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(MapValueTest, Kind) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  EXPECT_EQ(value.kind(), MapValue::kKind);
  EXPECT_EQ(Value(value).kind(), MapValue::kKind);
}

TEST_P(MapValueTest, Type) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  EXPECT_EQ(value.type(), GetIntDoubleMapType());
  EXPECT_EQ(Value(value).type(), GetIntDoubleMapType());
}

TEST_P(MapValueTest, DebugString) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
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
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  EXPECT_FALSE(value.IsEmpty());
}

TEST_P(MapValueTest, Size) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  EXPECT_EQ(value.Size(), 3);
}

TEST_P(MapValueTest, Get) {
  Value scratch;
  ASSERT_OK_AND_ASSIGN(
      auto map_value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  ASSERT_OK_AND_ASSIGN(
      auto value, map_value.Get(value_factory(), IntValueView(0), scratch));
  ASSERT_TRUE(InstanceOf<DoubleValueView>(value));
  ASSERT_EQ(Cast<DoubleValueView>(value).NativeValue(), 3.0);
  ASSERT_OK_AND_ASSIGN(
      value, map_value.Get(value_factory(), IntValueView(1), scratch));
  ASSERT_TRUE(InstanceOf<DoubleValueView>(value));
  ASSERT_EQ(Cast<DoubleValueView>(value).NativeValue(), 4.0);
  ASSERT_OK_AND_ASSIGN(
      value, map_value.Get(value_factory(), IntValueView(2), scratch));
  ASSERT_TRUE(InstanceOf<DoubleValueView>(value));
  ASSERT_EQ(Cast<DoubleValueView>(value).NativeValue(), 5.0);
  EXPECT_THAT(map_value.Get(value_factory(), IntValueView(3), scratch),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_P(MapValueTest, Find) {
  Value scratch;
  ASSERT_OK_AND_ASSIGN(
      auto map_value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  ValueView value;
  bool ok;
  ASSERT_OK_AND_ASSIGN(
      std::tie(value, ok),
      map_value.Find(value_factory(), IntValueView(0), scratch));
  ASSERT_TRUE(ok);
  ASSERT_TRUE(InstanceOf<DoubleValueView>(value));
  ASSERT_EQ(Cast<DoubleValueView>(value).NativeValue(), 3.0);
  ASSERT_OK_AND_ASSIGN(
      std::tie(value, ok),
      map_value.Find(value_factory(), IntValueView(1), scratch));
  ASSERT_TRUE(ok);
  ASSERT_TRUE(InstanceOf<DoubleValueView>(value));
  ASSERT_EQ(Cast<DoubleValueView>(value).NativeValue(), 4.0);
  ASSERT_OK_AND_ASSIGN(
      std::tie(value, ok),
      map_value.Find(value_factory(), IntValueView(2), scratch));
  ASSERT_TRUE(ok);
  ASSERT_TRUE(InstanceOf<DoubleValueView>(value));
  ASSERT_EQ(Cast<DoubleValueView>(value).NativeValue(), 5.0);
  ASSERT_OK_AND_ASSIGN(
      std::tie(value, ok),
      map_value.Find(value_factory(), IntValueView(3), scratch));
  ASSERT_FALSE(ok);
}

TEST_P(MapValueTest, Has) {
  ASSERT_OK_AND_ASSIGN(
      auto map_value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
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
  ASSERT_OK_AND_ASSIGN(
      auto map_value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  ListValue map_keys_scratch;
  ASSERT_OK_AND_ASSIGN(
      auto list_keys,
      map_value.ListKeys(type_factory(), value_factory(), map_keys_scratch));
  std::vector<int64_t> keys;
  ASSERT_OK(
      list_keys.ForEach(value_factory(), [&keys](ValueView element) -> bool {
        keys.push_back(Cast<IntValueView>(element).NativeValue());
        return true;
      }));
  EXPECT_THAT(keys, UnorderedElementsAreArray({0, 1, 2}));
}

TEST_P(MapValueTest, ForEach) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  std::vector<std::pair<int64_t, double>> entries;
  EXPECT_OK(value.ForEach(value_factory(), [&entries](ValueView key,
                                                      ValueView value) {
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
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  ASSERT_OK_AND_ASSIGN(auto iterator, value.NewIterator(value_factory()));
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

TEST_P(MapValueTest, ConvertToJson) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  EXPECT_THAT(value.ConvertToJson(),
              IsOkAndHolds(Json(MakeJsonObject({{JsonString("0"), 3.0},
                                                {JsonString("1"), 4.0},
                                                {JsonString("2"), 5.0}}))));
}

INSTANTIATE_TEST_SUITE_P(
    MapValueTest, MapValueTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    MapValueTest::ToString);

class MapValueViewTest : public common_internal::ThreadCompatibleValueTest<> {
 public:
  template <typename... Args>
  absl::StatusOr<MapValue> NewIntDoubleMapValue(Args&&... args) {
    CEL_ASSIGN_OR_RETURN(auto builder,
                         value_provider().NewMapValueBuilder(
                             value_factory(), GetIntDoubleMapType()));
    (static_cast<void>(builder->Put(std::forward<Args>(args).first,
                                    std::forward<Args>(args).second)),
     ...);
    return std::move(*builder).Build();
  }

  ListType GetIntListType() {
    return type_factory().CreateListType(IntTypeView());
  }

  MapType GetIntDoubleMapType() {
    return type_factory().CreateMapType(IntTypeView(), DoubleTypeView());
  }
};

TEST_P(MapValueViewTest, Default) {
  Value scratch;
  MapValueView map_value;
  ListValue map_keys_scratch;
  EXPECT_TRUE(map_value.IsEmpty());
  EXPECT_EQ(map_value.Size(), 0);
  EXPECT_EQ(map_value.DebugString(), "{}");
  EXPECT_EQ(map_value.type().key(), DynTypeView());
  EXPECT_EQ(map_value.type().value(), DynTypeView());
  ASSERT_OK_AND_ASSIGN(
      auto list_value,
      map_value.ListKeys(type_factory(), value_factory(), map_keys_scratch));
  EXPECT_TRUE(list_value.IsEmpty());
  EXPECT_EQ(list_value.Size(), 0);
  EXPECT_EQ(list_value.DebugString(), "[]");
  EXPECT_EQ(list_value.type().element(), DynTypeView());
  ASSERT_OK_AND_ASSIGN(auto iterator, map_value.NewIterator(value_factory()));
  EXPECT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(scratch),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(MapValueViewTest, Kind) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  EXPECT_EQ(MapValueView(value).kind(), MapValue::kKind);
  EXPECT_EQ(ValueView(MapValueView(value)).kind(), MapValue::kKind);
}

TEST_P(MapValueViewTest, Type) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  EXPECT_EQ(MapValueView(value).type(), GetIntDoubleMapType());
  EXPECT_EQ(ValueView(MapValueView(value)).type(), GetIntDoubleMapType());
}

TEST_P(MapValueViewTest, DebugString) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
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
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  EXPECT_FALSE(MapValueView(value).IsEmpty());
}

TEST_P(MapValueViewTest, Size) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  EXPECT_EQ(MapValueView(value).Size(), 3);
}

TEST_P(MapValueViewTest, Get) {
  Value scratch;
  ASSERT_OK_AND_ASSIGN(
      auto map_value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  ASSERT_OK_AND_ASSIGN(
      auto value,
      MapValueView(map_value).Get(value_factory(), IntValueView(0), scratch));
  ASSERT_TRUE(InstanceOf<DoubleValueView>(value));
  ASSERT_EQ(Cast<DoubleValueView>(value).NativeValue(), 3.0);
  ASSERT_OK_AND_ASSIGN(value, MapValueView(map_value).Get(
                                  value_factory(), IntValueView(1), scratch));
  ASSERT_TRUE(InstanceOf<DoubleValueView>(value));
  ASSERT_EQ(Cast<DoubleValueView>(value).NativeValue(), 4.0);
  ASSERT_OK_AND_ASSIGN(value, MapValueView(map_value).Get(
                                  value_factory(), IntValueView(2), scratch));
  ASSERT_TRUE(InstanceOf<DoubleValueView>(value));
  ASSERT_EQ(Cast<DoubleValueView>(value).NativeValue(), 5.0);
  EXPECT_THAT(
      MapValueView(map_value).Get(value_factory(), IntValueView(3), scratch),
      StatusIs(absl::StatusCode::kNotFound));
}

TEST_P(MapValueViewTest, Find) {
  Value scratch;
  ASSERT_OK_AND_ASSIGN(
      auto map_value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  ValueView value;
  bool ok;
  ASSERT_OK_AND_ASSIGN(
      std::tie(value, ok),
      MapValueView(map_value).Find(value_factory(), IntValueView(0), scratch));
  ASSERT_TRUE(ok);
  ASSERT_TRUE(InstanceOf<DoubleValueView>(value));
  ASSERT_EQ(Cast<DoubleValueView>(value).NativeValue(), 3.0);
  ASSERT_OK_AND_ASSIGN(
      std::tie(value, ok),
      MapValueView(map_value).Find(value_factory(), IntValueView(1), scratch));
  ASSERT_TRUE(ok);
  ASSERT_TRUE(InstanceOf<DoubleValueView>(value));
  ASSERT_EQ(Cast<DoubleValueView>(value).NativeValue(), 4.0);
  ASSERT_OK_AND_ASSIGN(
      std::tie(value, ok),
      MapValueView(map_value).Find(value_factory(), IntValueView(2), scratch));
  ASSERT_TRUE(ok);
  ASSERT_TRUE(InstanceOf<DoubleValueView>(value));
  ASSERT_EQ(Cast<DoubleValueView>(value).NativeValue(), 5.0);
  ASSERT_OK_AND_ASSIGN(
      std::tie(value, ok),
      MapValueView(map_value).Find(value_factory(), IntValueView(3), scratch));
  ASSERT_FALSE(ok);
}

TEST_P(MapValueViewTest, Has) {
  ASSERT_OK_AND_ASSIGN(
      auto map_value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
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
  ASSERT_OK_AND_ASSIGN(
      auto map_value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  ListValue map_keys_scratch;
  ASSERT_OK_AND_ASSIGN(auto list_keys,
                       MapValueView(map_value).ListKeys(
                           type_factory(), value_factory(), map_keys_scratch));
  std::vector<int64_t> keys;
  ASSERT_OK(
      list_keys.ForEach(value_factory(), [&keys](ValueView element) -> bool {
        keys.push_back(Cast<IntValueView>(element).NativeValue());
        return true;
      }));
  EXPECT_THAT(keys, UnorderedElementsAreArray({0, 1, 2}));
}

TEST_P(MapValueViewTest, ForEach) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  std::vector<std::pair<int64_t, double>> entries;
  EXPECT_OK(MapValueView(value).ForEach(
      value_factory(), [&entries](ValueView key, ValueView value) {
        entries.push_back(
            std::pair{Cast<IntValueView>(key).NativeValue(),
                      Cast<DoubleValueView>(value).NativeValue()});
        return true;
      }));
  EXPECT_THAT(entries,
              UnorderedElementsAreArray(
                  {std::pair{0, 3.0}, std::pair{1, 4.0}, std::pair{2, 5.0}}));
}

TEST_P(MapValueViewTest, NewIterator) {
  Value scratch;
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  ASSERT_OK_AND_ASSIGN(auto iterator,
                       MapValueView(value).NewIterator(value_factory()));
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

TEST_P(MapValueViewTest, ConvertToJson) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      NewIntDoubleMapValue(std::pair{IntValue(0), DoubleValue(3.0)},
                           std::pair{IntValue(1), DoubleValue(4.0)},
                           std::pair{IntValue(2), DoubleValue(5.0)}));
  EXPECT_THAT(MapValueView(value).ConvertToJson(),
              IsOkAndHolds(Json(MakeJsonObject({{JsonString("0"), 3.0},
                                                {JsonString("1"), 4.0},
                                                {JsonString("2"), 5.0}}))));
}

INSTANTIATE_TEST_SUITE_P(
    MapValueViewTest, MapValueViewTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    MapValueViewTest::ToString);

}  // namespace
}  // namespace cel
