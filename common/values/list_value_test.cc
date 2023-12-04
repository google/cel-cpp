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
#include <ostream>
#include <sstream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "absl/status/status.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/type.h"
#include "common/type_factory.h"
#include "common/value.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::An;
using testing::ElementsAreArray;
using testing::Ne;
using testing::TestParamInfo;
using testing::TestWithParam;
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

class ListValueBuilderTest
    : public TestWithParam<std::tuple<MemoryManagement, Typing>> {
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

  std::unique_ptr<ListValueBuilderInterface> NewIntListValueBuilder() {
    switch (typing()) {
      case Typing::kStatic:
        return std::make_unique<ListValueBuilder<IntValue>>(**type_factory_,
                                                            IntTypeView());
      case Typing::kDynamic:
        return std::make_unique<ListValueBuilder<Value>>(**type_factory_,
                                                         IntTypeView());
    }
  }

  void TearDown() override { Finish(); }

  void Finish() {
    type_factory_.reset();
    memory_manager_.reset();
  }

  MemoryManagerRef memory_manager() { return *memory_manager_; }

  MemoryManagement memory_management() const { return std::get<0>(GetParam()); }

  Typing typing() const { return std::get<1>(GetParam()); }

  static std::string ToString(
      TestParamInfo<std::tuple<MemoryManagement, Typing>> param) {
    std::ostringstream out;
    out << std::get<0>(param.param) << "_" << std::get<1>(param.param);
    return out.str();
  }

 private:
  absl::optional<MemoryManager> memory_manager_;
  absl::optional<Shared<TypeFactory>> type_factory_;
};

TEST_P(ListValueBuilderTest, Coverage) {
  constexpr size_t kNumValues = 64;
  auto builder = NewIntListValueBuilder();
  EXPECT_TRUE(builder->IsEmpty());
  EXPECT_EQ(builder->Size(), 0);
  builder->Reserve(kNumValues);
  for (size_t index = 0; index < kNumValues; ++index) {
    builder->Add(IntValue(index));
  }
  EXPECT_FALSE(builder->IsEmpty());
  EXPECT_EQ(builder->Size(), kNumValues);
  auto value = std::move(*builder).Build();
  EXPECT_FALSE(value.IsEmpty());
  EXPECT_EQ(value.Size(), kNumValues);
}

INSTANTIATE_TEST_SUITE_P(
    ListValueBuilderTest, ListValueBuilderTest,
    ::testing::Combine(::testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting),
                       ::testing::Values(Typing::kStatic, Typing::kDynamic)),
    ListValueBuilderTest::ToString);

class ListValueTest : public TestWithParam<MemoryManagement> {
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
  ListValue NewIntListValue(Args&&... args) {
    ListValueBuilder<IntValue> builder(**type_factory_, IntTypeView());
    (builder.Add(std::forward<Args>(args)), ...);
    return std::move(builder).Build();
  }

  ListType GetIntListType() {
    return (*type_factory_)->CreateListType(IntTypeView());
  }

  void TearDown() override { Finish(); }

  void Finish() {
    type_factory_.reset();
    memory_manager_.reset();
  }

  MemoryManagerRef memory_manager() { return *memory_manager_; }

  MemoryManagement memory_management() const { return GetParam(); }

  static std::string ToString(TestParamInfo<MemoryManagement> param) {
    std::ostringstream out;
    out << param.param;
    return out.str();
  }

 private:
  absl::optional<MemoryManager> memory_manager_;
  absl::optional<Shared<TypeFactory>> type_factory_;
};

TEST_P(ListValueTest, Kind) {
  auto value = NewIntListValue(IntValue(0), IntValue(1), IntValue(2));
  EXPECT_EQ(value.kind(), ListValue::kKind);
  EXPECT_EQ(Value(value).kind(), ListValue::kKind);
}

TEST_P(ListValueTest, Type) {
  auto value = NewIntListValue(IntValue(0), IntValue(1), IntValue(2));
  EXPECT_EQ(value.type(), GetIntListType());
  EXPECT_EQ(Value(value).type(), GetIntListType());
}

TEST_P(ListValueTest, DebugString) {
  auto value = NewIntListValue(IntValue(0), IntValue(1), IntValue(2));
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
  auto value = NewIntListValue(IntValue(0), IntValue(1), IntValue(2));
  EXPECT_FALSE(value.IsEmpty());
}

TEST_P(ListValueTest, Size) {
  auto value = NewIntListValue(IntValue(0), IntValue(1), IntValue(2));
  EXPECT_EQ(value.Size(), 3);
}

TEST_P(ListValueTest, Get) {
  Value scratch;
  auto value = NewIntListValue(IntValue(0), IntValue(1), IntValue(2));
  ASSERT_OK_AND_ASSIGN(auto element, value.Get(0, scratch));
  ASSERT_TRUE(InstanceOf<IntValueView>(element));
  ASSERT_EQ(Cast<IntValueView>(element).NativeValue(), 0);
  ASSERT_OK_AND_ASSIGN(element, value.Get(1, scratch));
  ASSERT_TRUE(InstanceOf<IntValueView>(element));
  ASSERT_EQ(Cast<IntValueView>(element).NativeValue(), 1);
  ASSERT_OK_AND_ASSIGN(element, value.Get(2, scratch));
  ASSERT_TRUE(InstanceOf<IntValueView>(element));
  ASSERT_EQ(Cast<IntValueView>(element).NativeValue(), 2);
  EXPECT_THAT(value.Get(3, scratch),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(ListValueTest, ForEach) {
  auto value = NewIntListValue(IntValue(0), IntValue(1), IntValue(2));
  std::vector<int64_t> elements;
  EXPECT_OK(value.ForEach([&elements](ValueView element) {
    elements.push_back(Cast<IntValueView>(element).NativeValue());
    return true;
  }));
  EXPECT_THAT(elements, ElementsAreArray({0, 1, 2}));
}

TEST_P(ListValueTest, NewIterator) {
  Value scratch;
  auto value = NewIntListValue(IntValue(0), IntValue(1), IntValue(2));
  ASSERT_OK_AND_ASSIGN(auto iterator, value.NewIterator());
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

INSTANTIATE_TEST_SUITE_P(
    ListValueTest, ListValueTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    ListValueTest::ToString);

class ListValueViewTest : public TestWithParam<MemoryManagement> {
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
  ListValue NewIntListValue(Args&&... args) {
    ListValueBuilder<IntValue> builder(**type_factory_, IntTypeView());
    (builder.Add(std::forward<Args>(args)), ...);
    return std::move(builder).Build();
  }

  ListType GetIntListType() {
    return (*type_factory_)->CreateListType(IntTypeView());
  }

  void TearDown() override { Finish(); }

  void Finish() {
    type_factory_.reset();
    memory_manager_.reset();
  }

  MemoryManagerRef memory_manager() { return *memory_manager_; }

  MemoryManagement memory_management() const { return GetParam(); }

  static std::string ToString(TestParamInfo<MemoryManagement> param) {
    std::ostringstream out;
    out << param.param;
    return out.str();
  }

 private:
  absl::optional<MemoryManager> memory_manager_;
  absl::optional<Shared<TypeFactory>> type_factory_;
};

TEST_P(ListValueViewTest, Kind) {
  auto value = NewIntListValue(IntValue(0), IntValue(1), IntValue(2));
  EXPECT_EQ(ListValueView(value).kind(), ListValueView::kKind);
  EXPECT_EQ(ValueView(ListValueView(value)).kind(), ListValueView::kKind);
}

TEST_P(ListValueViewTest, Type) {
  auto value = NewIntListValue(IntValue(0), IntValue(1), IntValue(2));
  EXPECT_EQ(ListValueView(value).type(), GetIntListType());
  EXPECT_EQ(ListValue(ListValueView(value)).type(), GetIntListType());
}

TEST_P(ListValueViewTest, DebugString) {
  auto value = NewIntListValue(IntValue(0), IntValue(1), IntValue(2));
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
  auto value = NewIntListValue(IntValue(0), IntValue(1), IntValue(2));
  EXPECT_FALSE(ListValueView(value).IsEmpty());
}

TEST_P(ListValueViewTest, Size) {
  auto value = NewIntListValue(IntValue(0), IntValue(1), IntValue(2));
  EXPECT_EQ(ListValueView(value).Size(), 3);
}

TEST_P(ListValueViewTest, Get) {
  Value scratch;
  auto value = NewIntListValue(IntValue(0), IntValue(1), IntValue(2));
  ASSERT_OK_AND_ASSIGN(auto element, ListValueView(value).Get(0, scratch));
  ASSERT_TRUE(InstanceOf<IntValueView>(element));
  ASSERT_EQ(Cast<IntValueView>(element).NativeValue(), 0);
  ASSERT_OK_AND_ASSIGN(element, ListValueView(value).Get(1, scratch));
  ASSERT_TRUE(InstanceOf<IntValueView>(element));
  ASSERT_EQ(Cast<IntValueView>(element).NativeValue(), 1);
  ASSERT_OK_AND_ASSIGN(element, ListValueView(value).Get(2, scratch));
  ASSERT_TRUE(InstanceOf<IntValueView>(element));
  ASSERT_EQ(Cast<IntValueView>(element).NativeValue(), 2);
  EXPECT_THAT(ListValueView(value).Get(3, scratch),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(ListValueViewTest, ForEach) {
  auto value = NewIntListValue(IntValue(0), IntValue(1), IntValue(2));
  std::vector<int64_t> elements;
  EXPECT_OK(ListValueView(value).ForEach([&elements](ValueView element) {
    elements.push_back(Cast<IntValueView>(element).NativeValue());
    return true;
  }));
  EXPECT_THAT(elements, ElementsAreArray({0, 1, 2}));
}

TEST_P(ListValueViewTest, NewIterator) {
  Value scratch;
  auto value = NewIntListValue(IntValue(0), IntValue(1), IntValue(2));
  ASSERT_OK_AND_ASSIGN(auto iterator, ListValueView(value).NewIterator());
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

INSTANTIATE_TEST_SUITE_P(
    ListValueViewTest, ListValueViewTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    ListValueViewTest::ToString);

}  // namespace
}  // namespace cel
