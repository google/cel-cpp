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
#include <sstream>
#include <string>
#include <utility>

#include "absl/base/optimization.h"
#include "absl/hash/hash.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/utility/utility.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/sized_input_view.h"
#include "common/type.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::An;
using testing::Ne;
using testing::TestParamInfo;
using testing::TestWithParam;
using cel::internal::StatusIs;

class CustomEnumTypeInterface;

class CustomEnumTypeValueIterator final : public EnumTypeValueIterator {
 public:
  explicit CustomEnumTypeValueIterator(const CustomEnumTypeInterface& type)
      : type_(type) {}

  bool HasNext() override { return value_index_ < 3; }

 private:
  absl::StatusOr<EnumTypeValueId> NextId() override;

  EnumType GetType() const override;

  const CustomEnumTypeInterface& type_;
  size_t value_index_ = 0;
};

class CustomEnumTypeInterface final
    : public ExtendEnumTypeInterface<CustomEnumTypeInterface> {
 public:
  absl::string_view name() const override { return "test.Enum"; }

  size_t value_count() const override { return 3; }

  absl::StatusOr<absl::Nonnull<EnumTypeValueIteratorPtr>> NewValueIterator()
      const override {
    return std::make_unique<CustomEnumTypeValueIterator>(*this);
  }

 private:
  friend class CustomEnumTypeValueIterator;

  absl::StatusOr<absl::optional<EnumTypeValueId>> FindIdByName(
      absl::string_view name) const override {
    if (name == "FOO") {
      return EnumTypeValueId(absl::in_place_type<size_t>, 0);
    }
    if (name == "BAR") {
      return EnumTypeValueId(absl::in_place_type<size_t>, 1);
    }
    if (name == "BAZ") {
      return EnumTypeValueId(absl::in_place_type<size_t>, 2);
    }
    return absl::nullopt;
  }

  absl::StatusOr<absl::optional<EnumTypeValueId>> FindIdByNumber(
      int64_t number) const override {
    if (number == 1) {
      return EnumTypeValueId(absl::in_place_type<size_t>, 0);
    }
    if (number == 2) {
      return EnumTypeValueId(absl::in_place_type<size_t>, 1);
    }
    if (number == 3) {
      return EnumTypeValueId(absl::in_place_type<size_t>, 2);
    }
    return absl::nullopt;
  }

  absl::string_view GetNameForId(EnumTypeValueId id) const override {
    switch (id.Get<size_t>()) {
      case 0:
        return "FOO";
      case 1:
        return "BAR";
      case 2:
        return "BAZ";
      default:
        ABSL_UNREACHABLE();
    }
  }

  int64_t GetNumberForId(EnumTypeValueId id) const override {
    switch (id.Get<size_t>()) {
      case 0:
        return 1;
      case 1:
        return 2;
      case 2:
        return 3;
      default:
        ABSL_UNREACHABLE();
    }
  }
};

absl::StatusOr<EnumTypeValueId> CustomEnumTypeValueIterator::NextId() {
  auto value_index = value_index_++;
  return EnumTypeValueId(absl::in_place_type<size_t>, value_index);
}

EnumType CustomEnumTypeValueIterator::GetType() const {
  return type_.shared_from_this();
}

using CustomEnumType = EnumTypeFor<CustomEnumTypeInterface>;

using CustomEnumTypeView = EnumTypeViewFor<CustomEnumTypeInterface>;

class EnumTypeTest : public TestWithParam<MemoryManagement> {
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

  CustomEnumType MakeCustomEnumType() {
    return CustomEnumType(
        memory_manager().MakeShared<CustomEnumTypeInterface>());
  }

  static std::string ToString(TestParamInfo<MemoryManagement> param) {
    std::ostringstream out;
    out << param.param;
    return out.str();
  }

 private:
  absl::optional<MemoryManager> memory_manager_;
};

TEST_P(EnumTypeTest, ValueCount) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestEnumType());
  EXPECT_EQ(type.value_count(), 3);
}

TEST_P(EnumTypeTest, Kind) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestEnumType());
  EXPECT_EQ(type.kind(), EnumType::kKind);
  EXPECT_EQ(Type(type).kind(), EnumType::kKind);
}

TEST_P(EnumTypeTest, Name) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestEnumType());
  EXPECT_EQ(type.name(), "test.Enum");
  EXPECT_EQ(Type(type).name(), "test.Enum");
}

TEST_P(EnumTypeTest, DebugString) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestEnumType());
  {
    std::ostringstream out;
    out << type;
    EXPECT_EQ(out.str(), "test.Enum");
  }
  {
    std::ostringstream out;
    out << Type(type);
    EXPECT_EQ(out.str(), "test.Enum");
  }
}

TEST_P(EnumTypeTest, Hash) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestEnumType());
  const auto expected_hash = absl::HashOf(type);
  EXPECT_EQ(absl::HashOf(type), expected_hash);
  EXPECT_EQ(absl::HashOf(Type(type)), expected_hash);
}

TEST_P(EnumTypeTest, Equal) {
  ASSERT_OK_AND_ASSIGN(auto type1, MakeTestEnumType());
  ASSERT_OK_AND_ASSIGN(auto type2, MakeTestEnumType());
  EXPECT_EQ(type1, type1);
  EXPECT_EQ(type1, type2);
  EXPECT_EQ(type2, type1);
  EXPECT_EQ(type2, type2);
  EXPECT_EQ(type1, Type(type2));
  EXPECT_EQ(Type(type1), type2);
  EXPECT_EQ(Type(type1), Type(type2));
}

TEST_P(EnumTypeTest, NativeTypeId) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestEnumType());
  EXPECT_NE(NativeTypeId::Of(type), NativeTypeId());
  EXPECT_EQ(NativeTypeId::Of(Type(type)), NativeTypeId::Of(type));
}

TEST_P(EnumTypeTest, CreateWithInvalidArguments) {
  EXPECT_THAT(EnumType::Create(
                  memory_manager(), "test.Enum",
                  {EnumTypeValueView{"FOO", 1}, EnumTypeValueView{"BAR", 2},
                   EnumTypeValueView{"BAZ", -1}}),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(EnumType::Create(
                  memory_manager(), "test.Enum",
                  {EnumTypeValueView{"FOO", 1}, EnumTypeValueView{"BAR", 2},
                   EnumTypeValueView{"BAZ", 1}}),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(EnumType::Create(
                  memory_manager(), "test.Enum",
                  {EnumTypeValueView{"FOO", 1}, EnumTypeValueView{"BAR", 2},
                   EnumTypeValueView{"FOO", 3}}),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(EnumTypeTest, InstanceOf) {
  EXPECT_TRUE(InstanceOf<EnumType>(MakeCustomEnumType()));
  EXPECT_TRUE(InstanceOf<EnumType>(EnumType(MakeCustomEnumType())));
  EXPECT_TRUE(InstanceOf<EnumType>(Type(EnumType(MakeCustomEnumType()))));
  EXPECT_TRUE(InstanceOf<CustomEnumType>(MakeCustomEnumType()));
  EXPECT_TRUE(InstanceOf<CustomEnumType>(EnumType(MakeCustomEnumType())));
  EXPECT_TRUE(InstanceOf<CustomEnumType>(Type(EnumType(MakeCustomEnumType()))));
}

TEST_P(EnumTypeTest, Cast) {
  EXPECT_THAT(Cast<CustomEnumType>(MakeCustomEnumType()), An<CustomEnumType>());
  EXPECT_THAT(Cast<CustomEnumType>(EnumType(MakeCustomEnumType())),
              An<CustomEnumType>());
  EXPECT_THAT(Cast<CustomEnumType>(Type(EnumType(MakeCustomEnumType()))),
              An<CustomEnumType>());
}

TEST_P(EnumTypeTest, As) {
  EXPECT_THAT(As<CustomEnumType>(MakeCustomEnumType()), Ne(absl::nullopt));
  EXPECT_THAT(As<CustomEnumType>(EnumType(MakeCustomEnumType())),
              Ne(absl::nullopt));
  EXPECT_THAT(As<CustomEnumType>(Type(EnumType(MakeCustomEnumType()))),
              Ne(absl::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    EnumTypeTest, EnumTypeTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    EnumTypeTest::ToString);

class EnumTypeViewTest : public TestWithParam<MemoryManagement> {
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

  CustomEnumType MakeCustomEnumType() {
    return CustomEnumType(
        memory_manager().MakeShared<CustomEnumTypeInterface>());
  }

  static std::string ToString(TestParamInfo<MemoryManagement> param) {
    std::ostringstream out;
    out << param.param;
    return out.str();
  }

 private:
  absl::optional<MemoryManager> memory_manager_;
};

TEST_P(EnumTypeViewTest, ValueCount) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestEnumType());
  EXPECT_EQ(EnumTypeView(type).value_count(), 3);
}

TEST_P(EnumTypeViewTest, Kind) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestEnumType());
  EXPECT_EQ(EnumTypeView(type).kind(), EnumType::kKind);
  EXPECT_EQ(TypeView(EnumTypeView(type)).kind(), EnumType::kKind);
}

TEST_P(EnumTypeViewTest, Name) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestEnumType());
  EXPECT_EQ(EnumTypeView(type).name(), "test.Enum");
  EXPECT_EQ(TypeView(EnumTypeView(type)).name(), "test.Enum");
}

TEST_P(EnumTypeViewTest, DebugString) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestEnumType());
  {
    std::ostringstream out;
    out << EnumTypeView(type);
    EXPECT_EQ(out.str(), "test.Enum");
  }
  {
    std::ostringstream out;
    out << TypeView(EnumTypeView(type));
    EXPECT_EQ(out.str(), "test.Enum");
  }
}

TEST_P(EnumTypeViewTest, Hash) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestEnumType());
  const auto expected_hash = absl::HashOf(EnumTypeView(type));
  EXPECT_EQ(absl::HashOf(EnumTypeView(type)), expected_hash);
  EXPECT_EQ(absl::HashOf(TypeView(EnumTypeView(type))), expected_hash);
}

TEST_P(EnumTypeViewTest, Equal) {
  ASSERT_OK_AND_ASSIGN(auto type1, MakeTestEnumType());
  ASSERT_OK_AND_ASSIGN(auto type2, MakeTestEnumType());
  EXPECT_EQ(EnumTypeView(type1), EnumTypeView(type1));
  EXPECT_EQ(EnumTypeView(type1), EnumTypeView(type2));
  EXPECT_EQ(EnumTypeView(type2), EnumTypeView(type1));
  EXPECT_EQ(EnumTypeView(type2), EnumTypeView(type2));
  EXPECT_EQ(EnumTypeView(type1), TypeView(EnumTypeView(type2)));
  EXPECT_EQ(TypeView(EnumTypeView(type1)), EnumTypeView(type2));
  EXPECT_EQ(TypeView(EnumTypeView(type1)), TypeView(EnumTypeView(type2)));
}

TEST_P(EnumTypeViewTest, NativeTypeId) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestEnumType());
  EXPECT_NE(NativeTypeId::Of(EnumTypeView(type)), NativeTypeId());
  EXPECT_EQ(NativeTypeId::Of(TypeView(EnumTypeView(type))),
            NativeTypeId::Of(EnumTypeView(type)));
}

TEST_P(EnumTypeViewTest, InstanceOf) {
  auto type = MakeCustomEnumType();
  EXPECT_TRUE(InstanceOf<EnumTypeView>(CustomEnumTypeView(type)));
  EXPECT_TRUE(InstanceOf<EnumTypeView>(EnumTypeView(CustomEnumTypeView(type))));
  EXPECT_TRUE(InstanceOf<EnumTypeView>(
      TypeView(EnumTypeView(CustomEnumTypeView(type)))));
  EXPECT_TRUE(InstanceOf<CustomEnumTypeView>(CustomEnumTypeView(type)));
  EXPECT_TRUE(
      InstanceOf<CustomEnumTypeView>(EnumTypeView(CustomEnumTypeView(type))));
  EXPECT_TRUE(InstanceOf<CustomEnumTypeView>(
      TypeView(EnumTypeView(CustomEnumTypeView(type)))));
}

TEST_P(EnumTypeViewTest, Cast) {
  auto type = MakeCustomEnumType();
  EXPECT_THAT(Cast<CustomEnumTypeView>(CustomEnumTypeView(type)),
              An<CustomEnumTypeView>());
  EXPECT_THAT(Cast<CustomEnumTypeView>(EnumTypeView(CustomEnumTypeView(type))),
              An<CustomEnumTypeView>());
  EXPECT_THAT(Cast<CustomEnumTypeView>(
                  TypeView(EnumTypeView(CustomEnumTypeView(type)))),
              An<CustomEnumTypeView>());
}

TEST_P(EnumTypeViewTest, As) {
  auto type = MakeCustomEnumType();
  EXPECT_THAT(As<CustomEnumTypeView>(CustomEnumTypeView(type)),
              Ne(absl::nullopt));
  EXPECT_THAT(As<CustomEnumTypeView>(EnumTypeView(CustomEnumTypeView(type))),
              Ne(absl::nullopt));
  EXPECT_THAT(
      As<CustomEnumTypeView>(TypeView(EnumTypeView(CustomEnumTypeView(type)))),
      Ne(absl::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    EnumTypeViewTest, EnumTypeViewTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    EnumTypeViewTest::ToString);

}  // namespace
}  // namespace cel
