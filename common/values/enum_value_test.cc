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
#include <sstream>
#include <string>

#include "absl/container/flat_hash_set.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::TestParamInfo;
using testing::TestWithParam;
using testing::UnorderedElementsAre;
using cel::internal::StatusIs;

class EnumValueTest : public TestWithParam<MemoryManagement> {
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
  }

  void TearDown() override { Finish(); }

  void Finish() { memory_manager_.reset(); }

  MemoryManagerRef memory_manager() { return *memory_manager_; }

  MemoryManagement memory_management() const { return GetParam(); }

  absl::StatusOr<EnumType> MakeTestEnumType() {
    return EnumType::Create(
        memory_manager(), "test.Enum",
        {EnumTypeValueView{"FOO", 1}, EnumTypeValueView{"BAR", 2},
         EnumTypeValueView{"BAZ", 3}});
  }

  static std::string ToString(TestParamInfo<MemoryManagement> param) {
    std::ostringstream out;
    out << param.param;
    return out.str();
  }

 private:
  absl::optional<MemoryManager> memory_manager_;
};

TEST_P(EnumValueTest, Name) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestEnumType());
  ASSERT_OK_AND_ASSIGN(auto value, type.FindValueByName("FOO"));
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(value->kind(), EnumValue::kKind);
  EXPECT_EQ(value->name(), "FOO");
  EXPECT_EQ(value->number(), 1);
}

TEST_P(EnumValueTest, NameNotFound) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestEnumType());
  ASSERT_OK_AND_ASSIGN(auto value, type.FindValueByName("QUX"));
  EXPECT_EQ(value, absl::nullopt);
}

TEST_P(EnumValueTest, Number) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestEnumType());
  ASSERT_OK_AND_ASSIGN(auto value, type.FindValueByNumber(1));
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(value->kind(), EnumValue::kKind);
  EXPECT_EQ(value->name(), "FOO");
  EXPECT_EQ(value->number(), 1);
}

TEST_P(EnumValueTest, NumberNotFound) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestEnumType());
  ASSERT_OK_AND_ASSIGN(auto value, type.FindValueByNumber(4));
  EXPECT_EQ(value, absl::nullopt);
}

TEST_P(EnumValueTest, DebugString) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestEnumType());
  ASSERT_OK_AND_ASSIGN(auto value, type.FindValueByName("FOO"));
  ASSERT_TRUE(value.has_value());
  std::ostringstream out;
  out << *value;
  EXPECT_EQ(out.str(), "test.Enum.FOO");
}

TEST_P(EnumValueTest, NewValueIterator) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestEnumType());
  ASSERT_OK_AND_ASSIGN(auto it, type.NewValueIterator());
  absl::flat_hash_set<int64_t> numbers;
  absl::flat_hash_set<absl::string_view> names;
  while (it->HasNext()) {
    ASSERT_OK_AND_ASSIGN(auto value, it->Next());
    EXPECT_TRUE(numbers.insert(value.number()).second);
    EXPECT_TRUE(names.insert(value.name()).second);
  }
  EXPECT_THAT(numbers, UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(names, UnorderedElementsAre("FOO", "BAR", "BAZ"));
  EXPECT_THAT(it->Next(), StatusIs(absl::StatusCode::kFailedPrecondition));
}

INSTANTIATE_TEST_SUITE_P(
    EnumValueTest, EnumValueTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    EnumValueTest::ToString);

class EnumValueViewTest : public TestWithParam<MemoryManagement> {
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
  }

  void TearDown() override { Finish(); }

  void Finish() { memory_manager_.reset(); }

  MemoryManagerRef memory_manager() { return *memory_manager_; }

  MemoryManagement memory_management() const { return GetParam(); }

  absl::StatusOr<EnumType> MakeTestEnumType() {
    return EnumType::Create(
        memory_manager(), "test.Enum",
        {EnumTypeValueView{"FOO", 1}, EnumTypeValueView{"BAR", 2},
         EnumTypeValueView{"BAZ", 3}});
  }

  static std::string ToString(TestParamInfo<MemoryManagement> param) {
    std::ostringstream out;
    out << param.param;
    return out.str();
  }

 private:
  absl::optional<MemoryManager> memory_manager_;
};

TEST_P(EnumValueViewTest, Kind) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestEnumType());
  ASSERT_OK_AND_ASSIGN(auto value, type.FindValueByName("FOO"));
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(EnumValueView(*value).kind(), EnumValue::kKind);
}

TEST_P(EnumValueViewTest, Name) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestEnumType());
  ASSERT_OK_AND_ASSIGN(auto value, type.FindValueByName("FOO"));
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(EnumValueView(*value).name(), "FOO");
  EXPECT_EQ(EnumValueView(*value).number(), 1);
}

TEST_P(EnumValueViewTest, NameNotFound) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestEnumType());
  ASSERT_OK_AND_ASSIGN(auto value, type.FindValueByName("QUX"));
  EXPECT_EQ(value, absl::nullopt);
}

TEST_P(EnumValueViewTest, Number) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestEnumType());
  ASSERT_OK_AND_ASSIGN(auto value, type.FindValueByNumber(1));
  ASSERT_TRUE(value.has_value());
  EXPECT_EQ(EnumValueView(*value).name(), "FOO");
  EXPECT_EQ(EnumValueView(*value).number(), 1);
}

TEST_P(EnumValueViewTest, NumberNotFound) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestEnumType());
  ASSERT_OK_AND_ASSIGN(auto value, type.FindValueByNumber(4));
  EXPECT_EQ(value, absl::nullopt);
}

TEST_P(EnumValueViewTest, DebugString) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestEnumType());
  ASSERT_OK_AND_ASSIGN(auto value, type.FindValueByName("FOO"));
  ASSERT_TRUE(value.has_value());
  std::ostringstream out;
  out << EnumValueView(*value);
  EXPECT_EQ(out.str(), "test.Enum.FOO");
}

TEST_P(EnumValueViewTest, NewValueIterator) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestEnumType());
  ASSERT_OK_AND_ASSIGN(auto it, type.NewValueIterator());
  absl::flat_hash_set<int64_t> numbers;
  absl::flat_hash_set<absl::string_view> names;
  while (it->HasNext()) {
    ASSERT_OK_AND_ASSIGN(auto value, it->Next());
    EXPECT_TRUE(numbers.insert(EnumValueView(value).number()).second);
    EXPECT_TRUE(names.insert(EnumValueView(value).name()).second);
  }
  EXPECT_THAT(numbers, UnorderedElementsAre(1, 2, 3));
  EXPECT_THAT(names, UnorderedElementsAre("FOO", "BAR", "BAZ"));
  EXPECT_THAT(it->Next(), StatusIs(absl::StatusCode::kFailedPrecondition));
}

INSTANTIATE_TEST_SUITE_P(
    EnumValueViewTest, EnumValueViewTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    EnumValueViewTest::ToString);

}  // namespace
}  // namespace cel
