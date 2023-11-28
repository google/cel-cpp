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
using cel::internal::IsOkAndHolds;
using cel::internal::StatusIs;

class CustomStructTypeInterface;

class CustomStructTypeFieldIterator final : public StructTypeFieldIterator {
 public:
  explicit CustomStructTypeFieldIterator(const CustomStructTypeInterface& type)
      : type_(type) {}

  bool HasNext() override;

  absl::StatusOr<StructTypeFieldId> Next() override;

 private:
  const CustomStructTypeInterface& type_;
  size_t field_index_ = 0;
};

class CustomStructTypeInterface final
    : public ExtendStructTypeInterface<CustomStructTypeInterface> {
 public:
  absl::string_view name() const override { return "test.Struct"; }

  size_t field_count() const override { return 3; }

  absl::StatusOr<absl::optional<StructTypeFieldId>> FindFieldByName(
      absl::string_view name) const override {
    if (name == "FOO") {
      return StructTypeFieldId(absl::in_place_type<size_t>, 0);
    }
    if (name == "BAR") {
      return StructTypeFieldId(absl::in_place_type<size_t>, 1);
    }
    if (name == "BAZ") {
      return StructTypeFieldId(absl::in_place_type<size_t>, 2);
    }
    return absl::nullopt;
  }

  absl::StatusOr<absl::optional<StructTypeFieldId>> FindFieldByNumber(
      int64_t number) const override {
    switch (number) {
      case 1:
        return StructTypeFieldId(absl::in_place_type<size_t>, 0);
      case 2:
        return StructTypeFieldId(absl::in_place_type<size_t>, 1);
      case 3:
        return StructTypeFieldId(absl::in_place_type<size_t>, 2);
      default:
        return absl::nullopt;
    }
  }

  absl::string_view GetFieldName(StructTypeFieldId id) const override {
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

  int64_t GetFieldNumber(StructTypeFieldId id) const override {
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

  absl::StatusOr<Type> GetFieldType(StructTypeFieldId id) const override {
    switch (id.Get<size_t>()) {
      case 0:
        return BoolType();
      case 1:
        return StringType();
      case 2:
        return BytesType();
      default:
        ABSL_UNREACHABLE();
    }
  }

  absl::StatusOr<absl::Nonnull<StructTypeFieldIteratorPtr>> NewFieldIterator()
      const override {
    return std::make_unique<CustomStructTypeFieldIterator>(*this);
  }

 private:
  friend class CustomStructTypeFieldIterator;
};

bool CustomStructTypeFieldIterator::HasNext() { return field_index_ < 4; }

absl::StatusOr<StructTypeFieldId> CustomStructTypeFieldIterator::Next() {
  if (ABSL_PREDICT_FALSE(field_index_ >= 4)) {
    return absl::FailedPreconditionError(
        "StructTypeFieldIterator::Next() called when "
        "StructTypeFieldIterator::HasNext() returns false");
  }
  return StructTypeFieldId(absl::in_place_type<size_t>, field_index_++);
}

using CustomStructType = StructTypeFor<CustomStructTypeInterface>;

using CustomStructTypeView = StructTypeViewFor<CustomStructTypeInterface>;

class StructTypeTest : public TestWithParam<MemoryManagement> {
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

  absl::StatusOr<StructType> MakeTestStructType() {
    return StructType::Create(memory_manager(), "test.Struct",
                              {StructTypeFieldView{"foo", 1, BoolType()},
                               StructTypeFieldView{"bar", 2, StringType()},
                               StructTypeFieldView{"baz", 3, BytesType()}});
  }

  CustomStructType MakeCustomStructType() {
    return CustomStructType(
        memory_manager().MakeShared<CustomStructTypeInterface>());
  }

  static std::string ToString(TestParamInfo<MemoryManagement> param) {
    std::ostringstream out;
    out << param.param;
    return out.str();
  }

 private:
  absl::optional<MemoryManager> memory_manager_;
};

TEST_P(StructTypeTest, FieldCount) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestStructType());
  EXPECT_EQ(type.field_count(), 3);
}

TEST_P(StructTypeTest, FindFieldByName) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestStructType());
  ASSERT_OK_AND_ASSIGN(auto field, type.FindFieldByName("foo"));
  ASSERT_TRUE(field.has_value());
  EXPECT_EQ(type.GetFieldName(*field), "foo");
  EXPECT_EQ(type.GetFieldNumber(*field), 1);
  EXPECT_THAT(type.GetFieldType(*field), IsOkAndHolds(BoolType()));
}

TEST_P(StructTypeTest, FindFieldByNameNotFound) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestStructType());
  ASSERT_OK_AND_ASSIGN(auto field, type.FindFieldByName("qux"));
  EXPECT_FALSE(field.has_value());
}

TEST_P(StructTypeTest, FindFieldByNumber) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestStructType());
  ASSERT_OK_AND_ASSIGN(auto field, type.FindFieldByNumber(1));
  ASSERT_TRUE(field.has_value());
  EXPECT_EQ(type.GetFieldName(*field), "foo");
  EXPECT_EQ(type.GetFieldNumber(*field), 1);
  EXPECT_THAT(type.GetFieldType(*field), IsOkAndHolds(BoolType()));
}

TEST_P(StructTypeTest, FindFieldByNumberNotFound) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestStructType());
  ASSERT_OK_AND_ASSIGN(auto field, type.FindFieldByNumber(4));
  EXPECT_FALSE(field.has_value());
}

TEST_P(StructTypeTest, NewFieldIterator) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestStructType());
  ASSERT_OK_AND_ASSIGN(auto iterator, type.NewFieldIterator());
  ASSERT_TRUE(iterator->HasNext());
  ASSERT_OK_AND_ASSIGN(auto field, iterator->Next());
  EXPECT_EQ(type.GetFieldName(field), "foo");
  EXPECT_EQ(type.GetFieldNumber(field), 1);
  EXPECT_THAT(type.GetFieldType(field), IsOkAndHolds(BoolType()));
  ASSERT_TRUE(iterator->HasNext());
  ASSERT_OK_AND_ASSIGN(field, iterator->Next());
  EXPECT_EQ(type.GetFieldName(field), "bar");
  EXPECT_EQ(type.GetFieldNumber(field), 2);
  EXPECT_THAT(type.GetFieldType(field), IsOkAndHolds(StringType()));
  ASSERT_TRUE(iterator->HasNext());
  ASSERT_OK_AND_ASSIGN(field, iterator->Next());
  EXPECT_EQ(type.GetFieldName(field), "baz");
  EXPECT_EQ(type.GetFieldNumber(field), 3);
  EXPECT_THAT(type.GetFieldType(field), IsOkAndHolds(BytesType()));
  EXPECT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(StructTypeTest, Kind) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestStructType());
  EXPECT_EQ(type.kind(), StructType::kKind);
  EXPECT_EQ(Type(type).kind(), StructType::kKind);
}

TEST_P(StructTypeTest, Name) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestStructType());
  EXPECT_EQ(type.name(), "test.Struct");
  EXPECT_EQ(Type(type).name(), "test.Struct");
}

TEST_P(StructTypeTest, DebugString) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestStructType());
  {
    std::ostringstream out;
    out << type;
    EXPECT_EQ(out.str(), "test.Struct");
  }
  {
    std::ostringstream out;
    out << Type(type);
    EXPECT_EQ(out.str(), "test.Struct");
  }
}

TEST_P(StructTypeTest, Hash) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestStructType());
  const auto expected_hash = absl::HashOf(type);
  EXPECT_EQ(absl::HashOf(type), expected_hash);
  EXPECT_EQ(absl::HashOf(Type(type)), expected_hash);
}

TEST_P(StructTypeTest, Equal) {
  ASSERT_OK_AND_ASSIGN(auto type1, MakeTestStructType());
  ASSERT_OK_AND_ASSIGN(auto type2, MakeTestStructType());
  EXPECT_EQ(type1, type1);
  EXPECT_EQ(type1, type2);
  EXPECT_EQ(type2, type1);
  EXPECT_EQ(type2, type2);
  EXPECT_EQ(type1, Type(type2));
  EXPECT_EQ(Type(type1), type2);
  EXPECT_EQ(Type(type1), Type(type2));
}

TEST_P(StructTypeTest, NativeTypeId) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestStructType());
  EXPECT_NE(NativeTypeId::Of(type), NativeTypeId());
  EXPECT_EQ(NativeTypeId::Of(Type(type)), NativeTypeId::Of(type));
}

TEST_P(StructTypeTest, CreateWithInvalidArguments) {
  EXPECT_THAT(StructType::Create(memory_manager(), "test.Struct",
                                 {StructTypeFieldView{"foo", 1, BoolType()},
                                  StructTypeFieldView{"bar", 2, StringType()},
                                  StructTypeFieldView{"baz", -1, BytesType()}}),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(StructType::Create(memory_manager(), "test.Struct",
                                 {StructTypeFieldView{"foo", 1, BoolType()},
                                  StructTypeFieldView{"bar", 2, StringType()},
                                  StructTypeFieldView{"baz", 1, BytesType()}}),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(StructType::Create(memory_manager(), "test.Struct",
                                 {StructTypeFieldView{"foo", 1, BoolType()},
                                  StructTypeFieldView{"bar", 2, StringType()},
                                  StructTypeFieldView{"foo", 3, BytesType()}}),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_P(StructTypeTest, InstanceOf) {
  EXPECT_TRUE(InstanceOf<StructType>(MakeCustomStructType()));
  EXPECT_TRUE(InstanceOf<StructType>(StructType(MakeCustomStructType())));
  EXPECT_TRUE(InstanceOf<StructType>(Type(StructType(MakeCustomStructType()))));
  EXPECT_TRUE(InstanceOf<CustomStructType>(MakeCustomStructType()));
  EXPECT_TRUE(InstanceOf<CustomStructType>(StructType(MakeCustomStructType())));
  EXPECT_TRUE(
      InstanceOf<CustomStructType>(Type(StructType(MakeCustomStructType()))));
}

TEST_P(StructTypeTest, Cast) {
  EXPECT_THAT(Cast<CustomStructType>(MakeCustomStructType()),
              An<CustomStructType>());
  EXPECT_THAT(Cast<CustomStructType>(StructType(MakeCustomStructType())),
              An<CustomStructType>());
  EXPECT_THAT(Cast<CustomStructType>(Type(StructType(MakeCustomStructType()))),
              An<CustomStructType>());
}

TEST_P(StructTypeTest, As) {
  EXPECT_THAT(As<CustomStructType>(MakeCustomStructType()), Ne(absl::nullopt));
  EXPECT_THAT(As<CustomStructType>(StructType(MakeCustomStructType())),
              Ne(absl::nullopt));
  EXPECT_THAT(As<CustomStructType>(Type(StructType(MakeCustomStructType()))),
              Ne(absl::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    StructTypeTest, StructTypeTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    StructTypeTest::ToString);

class StructTypeViewTest : public TestWithParam<MemoryManagement> {
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

  absl::StatusOr<StructType> MakeTestStructType() {
    return StructType::Create(memory_manager(), "test.Struct",
                              {StructTypeFieldView{"foo", 1, BoolType()},
                               StructTypeFieldView{"bar", 2, StringType()},
                               StructTypeFieldView{"baz", 3, BytesType()}});
  }

  CustomStructType MakeCustomStructType() {
    return CustomStructType(
        memory_manager().MakeShared<CustomStructTypeInterface>());
  }

  static std::string ToString(TestParamInfo<MemoryManagement> param) {
    std::ostringstream out;
    out << param.param;
    return out.str();
  }

 private:
  absl::optional<MemoryManager> memory_manager_;
};

TEST_P(StructTypeViewTest, FieldCount) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestStructType());
  EXPECT_EQ(StructTypeView(type).field_count(), 3);
}

TEST_P(StructTypeViewTest, FindFieldByName) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestStructType());
  ASSERT_OK_AND_ASSIGN(auto field, StructTypeView(type).FindFieldByName("foo"));
  ASSERT_TRUE(field.has_value());
  EXPECT_EQ(StructTypeView(type).GetFieldName(*field), "foo");
  EXPECT_EQ(StructTypeView(type).GetFieldNumber(*field), 1);
  EXPECT_THAT(StructTypeView(type).GetFieldType(*field),
              IsOkAndHolds(BoolType()));
}

TEST_P(StructTypeViewTest, FindFieldByNameNotFound) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestStructType());
  ASSERT_OK_AND_ASSIGN(auto field, StructTypeView(type).FindFieldByName("qux"));
  EXPECT_FALSE(field.has_value());
}

TEST_P(StructTypeViewTest, FindFieldByNumber) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestStructType());
  ASSERT_OK_AND_ASSIGN(auto field, StructTypeView(type).FindFieldByNumber(1));
  ASSERT_TRUE(field.has_value());
  EXPECT_EQ(StructTypeView(type).GetFieldName(*field), "foo");
  EXPECT_EQ(StructTypeView(type).GetFieldNumber(*field), 1);
  EXPECT_THAT(StructTypeView(type).GetFieldType(*field),
              IsOkAndHolds(BoolType()));
}

TEST_P(StructTypeViewTest, FindFieldByNumberNotFound) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestStructType());
  ASSERT_OK_AND_ASSIGN(auto field, StructTypeView(type).FindFieldByNumber(4));
  EXPECT_FALSE(field.has_value());
}

TEST_P(StructTypeViewTest, NewFieldIterator) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestStructType());
  ASSERT_OK_AND_ASSIGN(auto iterator, StructTypeView(type).NewFieldIterator());
  ASSERT_TRUE(iterator->HasNext());
  ASSERT_OK_AND_ASSIGN(auto field, iterator->Next());
  EXPECT_EQ(StructTypeView(type).GetFieldName(field), "foo");
  EXPECT_EQ(StructTypeView(type).GetFieldNumber(field), 1);
  EXPECT_THAT(StructTypeView(type).GetFieldType(field),
              IsOkAndHolds(BoolType()));
  ASSERT_TRUE(iterator->HasNext());
  ASSERT_OK_AND_ASSIGN(field, iterator->Next());
  EXPECT_EQ(StructTypeView(type).GetFieldName(field), "bar");
  EXPECT_EQ(StructTypeView(type).GetFieldNumber(field), 2);
  EXPECT_THAT(StructTypeView(type).GetFieldType(field),
              IsOkAndHolds(StringType()));
  ASSERT_TRUE(iterator->HasNext());
  ASSERT_OK_AND_ASSIGN(field, iterator->Next());
  EXPECT_EQ(StructTypeView(type).GetFieldName(field), "baz");
  EXPECT_EQ(StructTypeView(type).GetFieldNumber(field), 3);
  EXPECT_THAT(StructTypeView(type).GetFieldType(field),
              IsOkAndHolds(BytesType()));
  EXPECT_FALSE(iterator->HasNext());
  EXPECT_THAT(iterator->Next(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(StructTypeViewTest, Kind) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestStructType());
  EXPECT_EQ(StructTypeView(type).kind(), StructType::kKind);
  EXPECT_EQ(TypeView(StructTypeView(type)).kind(), StructType::kKind);
}

TEST_P(StructTypeViewTest, Name) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestStructType());
  EXPECT_EQ(StructTypeView(type).name(), "test.Struct");
  EXPECT_EQ(TypeView(StructTypeView(type)).name(), "test.Struct");
}

TEST_P(StructTypeViewTest, DebugString) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestStructType());
  {
    std::ostringstream out;
    out << StructTypeView(type);
    EXPECT_EQ(out.str(), "test.Struct");
  }
  {
    std::ostringstream out;
    out << TypeView(StructTypeView(type));
    EXPECT_EQ(out.str(), "test.Struct");
  }
}

TEST_P(StructTypeViewTest, Hash) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestStructType());
  const auto expected_hash = absl::HashOf(StructTypeView(type));
  EXPECT_EQ(absl::HashOf(StructTypeView(type)), expected_hash);
  EXPECT_EQ(absl::HashOf(TypeView(StructTypeView(type))), expected_hash);
}

TEST_P(StructTypeViewTest, Equal) {
  ASSERT_OK_AND_ASSIGN(auto type1, MakeTestStructType());
  ASSERT_OK_AND_ASSIGN(auto type2, MakeTestStructType());
  EXPECT_EQ(StructTypeView(type1), StructTypeView(type1));
  EXPECT_EQ(StructTypeView(type1), StructTypeView(type2));
  EXPECT_EQ(StructTypeView(type2), StructTypeView(type1));
  EXPECT_EQ(StructTypeView(type2), StructTypeView(type2));
  EXPECT_EQ(StructTypeView(type1), TypeView(StructTypeView(type2)));
  EXPECT_EQ(TypeView(StructTypeView(type1)), StructTypeView(type2));
  EXPECT_EQ(TypeView(StructTypeView(type1)), TypeView(StructTypeView(type2)));
}

TEST_P(StructTypeViewTest, NativeTypeId) {
  ASSERT_OK_AND_ASSIGN(auto type, MakeTestStructType());
  EXPECT_NE(NativeTypeId::Of(StructTypeView(type)), NativeTypeId());
  EXPECT_EQ(NativeTypeId::Of(TypeView(StructTypeView(type))),
            NativeTypeId::Of(StructTypeView(type)));
}

TEST_P(StructTypeViewTest, InstanceOf) {
  auto type = MakeCustomStructType();
  EXPECT_TRUE(InstanceOf<StructTypeView>(CustomStructTypeView(type)));
  EXPECT_TRUE(
      InstanceOf<StructTypeView>(StructTypeView(CustomStructTypeView(type))));
  EXPECT_TRUE(InstanceOf<StructTypeView>(
      TypeView(StructTypeView(CustomStructTypeView(type)))));
  EXPECT_TRUE(InstanceOf<CustomStructTypeView>(CustomStructTypeView(type)));
  EXPECT_TRUE(InstanceOf<CustomStructTypeView>(
      StructTypeView(CustomStructTypeView(type))));
  EXPECT_TRUE(InstanceOf<CustomStructTypeView>(
      TypeView(StructTypeView(CustomStructTypeView(type)))));
}

TEST_P(StructTypeViewTest, Cast) {
  auto type = MakeCustomStructType();
  EXPECT_THAT(Cast<CustomStructTypeView>(CustomStructTypeView(type)),
              An<CustomStructTypeView>());
  EXPECT_THAT(
      Cast<CustomStructTypeView>(StructTypeView(CustomStructTypeView(type))),
      An<CustomStructTypeView>());
  EXPECT_THAT(Cast<CustomStructTypeView>(
                  TypeView(StructTypeView(CustomStructTypeView(type)))),
              An<CustomStructTypeView>());
}

TEST_P(StructTypeViewTest, As) {
  auto type = MakeCustomStructType();
  EXPECT_THAT(As<CustomStructTypeView>(CustomStructTypeView(type)),
              Ne(absl::nullopt));
  EXPECT_THAT(
      As<CustomStructTypeView>(StructTypeView(CustomStructTypeView(type))),
      Ne(absl::nullopt));
  EXPECT_THAT(As<CustomStructTypeView>(
                  TypeView(StructTypeView(CustomStructTypeView(type)))),
              Ne(absl::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    StructTypeViewTest, StructTypeViewTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    StructTypeViewTest::ToString);

}  // namespace
}  // namespace cel
