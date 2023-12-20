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
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "common/any.h"
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

using testing::ElementsAreArray;
using testing::TestParamInfo;
using cel::internal::IsOkAndHolds;
using cel::internal::StatusIs;

class ListValueTest : public common_internal::ThreadCompatibleValueTest<> {
 public:
  template <typename... Args>
  absl::StatusOr<ListValue> NewIntListValue(Args&&... args) {
    CEL_ASSIGN_OR_RETURN(auto builder,
                         value_manager().NewListValueBuilder(GetIntListType()));
    (static_cast<void>(builder->Add(std::forward<Args>(args))), ...);
    return std::move(*builder).Build();
  }

  ListType GetIntListType() {
    return type_factory().CreateListType(IntTypeView());
  }
};

TEST(ListValue, Default) {
  ListValue value;
  EXPECT_TRUE(value.IsEmpty());
  EXPECT_EQ(value.Size(), 0);
  EXPECT_EQ(value.DebugString(), "[]");
  EXPECT_EQ(value.type().element(), DynTypeView());
}

TEST_P(ListValueTest, Kind) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  EXPECT_EQ(value.kind(), ListValue::kKind);
  EXPECT_EQ(Value(value).kind(), ListValue::kKind);
}

TEST_P(ListValueTest, Type) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  EXPECT_EQ(value.type(), GetIntListType());
  EXPECT_EQ(Value(value).type(), GetIntListType());
}

TEST_P(ListValueTest, DebugString) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  {
    std::ostringstream out;
    out << value;
    EXPECT_EQ(out.str(), "[0, 1, 2]");
  }
  {
    std::ostringstream out;
    out << Value(value);
    EXPECT_EQ(out.str(), "[0, 1, 2]");
  }
}

TEST_P(ListValueTest, IsEmpty) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  EXPECT_FALSE(value.IsEmpty());
}

TEST_P(ListValueTest, Size) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  EXPECT_EQ(value.Size(), 3);
}

TEST_P(ListValueTest, Get) {
  Value scratch;
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  ASSERT_OK_AND_ASSIGN(auto element, value.Get(value_factory(), 0, scratch));
  ASSERT_TRUE(InstanceOf<IntValueView>(element));
  ASSERT_EQ(Cast<IntValueView>(element).NativeValue(), 0);
  ASSERT_OK_AND_ASSIGN(element, value.Get(value_factory(), 1, scratch));
  ASSERT_TRUE(InstanceOf<IntValueView>(element));
  ASSERT_EQ(Cast<IntValueView>(element).NativeValue(), 1);
  ASSERT_OK_AND_ASSIGN(element, value.Get(value_factory(), 2, scratch));
  ASSERT_TRUE(InstanceOf<IntValueView>(element));
  ASSERT_EQ(Cast<IntValueView>(element).NativeValue(), 2);
  EXPECT_THAT(value.Get(value_factory(), 3, scratch),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(ListValueTest, ForEach) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  std::vector<int64_t> elements;
  EXPECT_OK(value.ForEach(value_factory(), [&elements](ValueView element) {
    elements.push_back(Cast<IntValueView>(element).NativeValue());
    return true;
  }));
  EXPECT_THAT(elements, ElementsAreArray({0, 1, 2}));
}

TEST_P(ListValueTest, NewIterator) {
  Value scratch;
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  ASSERT_OK_AND_ASSIGN(auto iterator, value.NewIterator(value_factory()));
  std::vector<int64_t> elements;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(auto element, iterator->Next(scratch));
    ASSERT_TRUE(InstanceOf<IntValueView>(element));
    elements.push_back(Cast<IntValueView>(element).NativeValue());
  }
  EXPECT_EQ(iterator->HasNext(), false);
  EXPECT_THAT(iterator->Next(scratch),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(elements, ElementsAreArray({0, 1, 2}));
}

TEST_P(ListValueTest, GetSerializedSize) {
  ASSERT_OK_AND_ASSIGN(auto value, NewIntListValue());
  EXPECT_THAT(value.GetSerializedSize(),
              StatusIs(absl::StatusCode::kUnimplemented));
}

TEST_P(ListValueTest, ConvertToAny) {
  ASSERT_OK_AND_ASSIGN(auto value, NewIntListValue());
  EXPECT_THAT(value.ConvertToAny(),
              IsOkAndHolds(MakeAny(MakeTypeUrl("google.protobuf.ListValue"),
                                   absl::Cord())));
}

TEST_P(ListValueTest, ConvertToJson) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  EXPECT_THAT(value.ConvertToJson(),
              IsOkAndHolds(Json(MakeJsonArray({0.0, 1.0, 2.0}))));
}

INSTANTIATE_TEST_SUITE_P(
    ListValueTest, ListValueTest,
    ::testing::Combine(::testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting)),
    ListValueTest::ToString);

class ListValueViewTest : public common_internal::ThreadCompatibleValueTest<> {
 public:
  template <typename... Args>
  absl::StatusOr<ListValue> NewIntListValue(Args&&... args) {
    CEL_ASSIGN_OR_RETURN(auto builder,
                         value_manager().NewListValueBuilder(GetIntListType()));
    (static_cast<void>(builder->Add(std::forward<Args>(args))), ...);
    return std::move(*builder).Build();
  }

  ListType GetIntListType() {
    return type_factory().CreateListType(IntTypeView());
  }
};

TEST(ListValueView, Default) {
  ListValueView value;
  EXPECT_TRUE(value.IsEmpty());
  EXPECT_EQ(value.Size(), 0);
  EXPECT_EQ(value.DebugString(), "[]");
  EXPECT_EQ(value.type().element(), DynTypeView());
}

TEST_P(ListValueViewTest, Kind) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  EXPECT_EQ(ListValueView(value).kind(), ListValueView::kKind);
  EXPECT_EQ(ValueView(ListValueView(value)).kind(), ListValueView::kKind);
}

TEST_P(ListValueViewTest, Type) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  EXPECT_EQ(ListValueView(value).type(), GetIntListType());
  EXPECT_EQ(ListValue(ListValueView(value)).type(), GetIntListType());
}

TEST_P(ListValueViewTest, DebugString) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  {
    std::ostringstream out;
    out << ListValueView(value);
    EXPECT_EQ(out.str(), "[0, 1, 2]");
  }
  {
    std::ostringstream out;
    out << ValueView(ListValueView(value));
    EXPECT_EQ(out.str(), "[0, 1, 2]");
  }
}

TEST_P(ListValueViewTest, IsEmpty) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  EXPECT_FALSE(ListValueView(value).IsEmpty());
}

TEST_P(ListValueViewTest, Size) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  EXPECT_EQ(ListValueView(value).Size(), 3);
}

TEST_P(ListValueViewTest, Get) {
  Value scratch;
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  ASSERT_OK_AND_ASSIGN(auto element,
                       ListValueView(value).Get(value_factory(), 0, scratch));
  ASSERT_TRUE(InstanceOf<IntValueView>(element));
  ASSERT_EQ(Cast<IntValueView>(element).NativeValue(), 0);
  ASSERT_OK_AND_ASSIGN(element,
                       ListValueView(value).Get(value_factory(), 1, scratch));
  ASSERT_TRUE(InstanceOf<IntValueView>(element));
  ASSERT_EQ(Cast<IntValueView>(element).NativeValue(), 1);
  ASSERT_OK_AND_ASSIGN(element,
                       ListValueView(value).Get(value_factory(), 2, scratch));
  ASSERT_TRUE(InstanceOf<IntValueView>(element));
  ASSERT_EQ(Cast<IntValueView>(element).NativeValue(), 2);
  EXPECT_THAT(ListValueView(value).Get(value_factory(), 3, scratch),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(ListValueViewTest, ForEach) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  std::vector<int64_t> elements;
  EXPECT_OK(ListValueView(value).ForEach(
      value_factory(), [&elements](ValueView element) {
        elements.push_back(Cast<IntValueView>(element).NativeValue());
        return true;
      }));
  EXPECT_THAT(elements, ElementsAreArray({0, 1, 2}));
}

TEST_P(ListValueViewTest, NewIterator) {
  Value scratch;
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  ASSERT_OK_AND_ASSIGN(auto iterator,
                       ListValueView(value).NewIterator(value_factory()));
  std::vector<int64_t> elements;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(auto element, iterator->Next(scratch));
    ASSERT_TRUE(InstanceOf<IntValueView>(element));
    elements.push_back(Cast<IntValueView>(element).NativeValue());
  }
  EXPECT_EQ(iterator->HasNext(), false);
  EXPECT_THAT(iterator->Next(scratch),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  EXPECT_THAT(elements, ElementsAreArray({0, 1, 2}));
}

TEST_P(ListValueViewTest, GetSerializedSize) {
  ASSERT_OK_AND_ASSIGN(auto value, NewIntListValue());
  EXPECT_THAT(ListValueView(value).GetSerializedSize(),
              StatusIs(absl::StatusCode::kUnimplemented));
}

TEST_P(ListValueViewTest, ConvertToAny) {
  ASSERT_OK_AND_ASSIGN(auto value, NewIntListValue());
  EXPECT_THAT(ListValueView(value).ConvertToAny(),
              IsOkAndHolds(MakeAny(MakeTypeUrl("google.protobuf.ListValue"),
                                   absl::Cord())));
}

TEST_P(ListValueViewTest, ConvertToJson) {
  ASSERT_OK_AND_ASSIGN(auto value,
                       NewIntListValue(IntValue(0), IntValue(1), IntValue(2)));
  EXPECT_THAT(ListValueView(value).ConvertToJson(),
              IsOkAndHolds(Json(MakeJsonArray({0.0, 1.0, 2.0}))));
}

INSTANTIATE_TEST_SUITE_P(
    ListValueViewTest, ListValueViewTest,
    ::testing::Combine(::testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting)),
    ListValueViewTest::ToString);

}  // namespace
}  // namespace cel
