// Copyright 2022 Google LLC
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

#include "base/type.h"

#include <cstddef>
#include <memory>
#include <type_traits>
#include <utility>

#include "absl/base/nullability.h"
#include "absl/hash/hash_testing.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "base/handle.h"
#include "base/internal/data.h"
#include "base/internal/memory_manager_testing.h"
#include "base/memory.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "base/type_provider.h"
#include "base/types/enum_type.h"
#include "base/value.h"
#include "base/value_factory.h"
#include "common/type_kind.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::ElementsAre;
using testing::Eq;
using cel::internal::IsOk;
using cel::internal::IsOkAndHolds;
using cel::internal::StatusIs;

enum class TestEnum {
  kValue1 = 1,
  kValue2 = 2,
};

class TestEnumType final : public EnumType {
 public:
  using EnumType::EnumType;

  absl::string_view name() const override { return "test_enum.TestEnum"; }

  size_t constant_count() const override { return 2; }

  absl::StatusOr<absl::Nonnull<std::unique_ptr<ConstantIterator>>>
  NewConstantIterator(MemoryManager& memory_manager) const override {
    return absl::UnimplementedError(
        "EnumType::NewConstantIterator is unimplemented");
  }

  absl::StatusOr<absl::optional<Constant>> FindConstantByName(
      absl::string_view name) const override {
    if (name == "VALUE1") {
      return Constant(MakeConstantId(TestEnum::kValue1), "VALUE1",
                      static_cast<int64_t>(TestEnum::kValue1));
    } else if (name == "VALUE2") {
      return Constant(MakeConstantId(TestEnum::kValue2), "VALUE2",
                      static_cast<int64_t>(TestEnum::kValue2));
    }
    return absl::nullopt;
  }

  absl::StatusOr<absl::optional<Constant>> FindConstantByNumber(
      int64_t number) const override {
    switch (number) {
      case 1:
        return Constant(MakeConstantId(TestEnum::kValue1), "VALUE1",
                        static_cast<int64_t>(TestEnum::kValue1));
      case 2:
        return Constant(MakeConstantId(TestEnum::kValue2), "VALUE2",
                        static_cast<int64_t>(TestEnum::kValue2));
      default:
        return absl::nullopt;
    }
  }

 private:
  CEL_DECLARE_ENUM_TYPE(TestEnumType);
};

CEL_IMPLEMENT_ENUM_TYPE(TestEnumType);

// struct TestStruct {
//   bool bool_field;
//   int64_t int_field;
//   uint64_t uint_field;
//   double double_field;
// };

class TestStructType final : public CEL_STRUCT_TYPE_CLASS {
 public:
  absl::string_view name() const override { return "test_struct.TestStruct"; }

  size_t field_count() const override { return 4; }

  absl::StatusOr<absl::Nonnull<std::unique_ptr<FieldIterator>>>
  NewFieldIterator(TypeManager& type_manager) const override {
    return absl::UnimplementedError(
        "StructType::NewFieldIterator() is unimplemented");
  }

  absl::StatusOr<absl::optional<Field>> FindFieldByName(
      TypeManager& type_manager, absl::string_view name) const override {
    if (name == "bool_field") {
      return Field(MakeFieldId(0), "bool_field", 0,
                   type_manager.type_factory().GetBoolType());
    } else if (name == "int_field") {
      return Field(MakeFieldId(1), "int_field", 1,
                   type_manager.type_factory().GetIntType());
    } else if (name == "uint_field") {
      return Field(MakeFieldId(2), "uint_field", 2,
                   type_manager.type_factory().GetUintType());
    } else if (name == "double_field") {
      return Field(MakeFieldId(3), "double_field", 3,
                   type_manager.type_factory().GetDoubleType());
    }
    return absl::nullopt;
  }

  absl::StatusOr<absl::optional<Field>> FindFieldByNumber(
      TypeManager& type_manager, int64_t number) const override {
    switch (number) {
      case 0:
        return Field(MakeFieldId(0), "bool_field", 0,
                     type_manager.type_factory().GetBoolType());
      case 1:
        return Field(MakeFieldId(1), "int_field", 1,
                     type_manager.type_factory().GetIntType());
      case 2:
        return Field(MakeFieldId(2), "uint_field", 2,
                     type_manager.type_factory().GetUintType());
      case 3:
        return Field(MakeFieldId(3), "double_field", 3,
                     type_manager.type_factory().GetDoubleType());
      default:
        return absl::nullopt;
    }
  }

 private:
  CEL_DECLARE_STRUCT_TYPE(TestStructType);
};

CEL_IMPLEMENT_STRUCT_TYPE(TestStructType);

template <typename T>
Handle<T> Must(absl::StatusOr<Handle<T>> status_or_handle) {
  return std::move(status_or_handle).value();
}

template <class T>
constexpr void IS_INITIALIZED(T&) {}

class TypeTest
    : public testing::TestWithParam<base_internal::MemoryManagerTestMode> {
 protected:
  void SetUp() override {
    if (GetParam() == base_internal::MemoryManagerTestMode::kArena) {
      memory_manager_ = ArenaMemoryManager::Default();
    }
  }

  void TearDown() override {
    if (GetParam() == base_internal::MemoryManagerTestMode::kArena) {
      memory_manager_.reset();
    }
  }

  MemoryManager& memory_manager() const {
    switch (GetParam()) {
      case base_internal::MemoryManagerTestMode::kGlobal:
        return MemoryManager::Global();
      case base_internal::MemoryManagerTestMode::kArena:
        return *memory_manager_;
    }
  }

 private:
  std::unique_ptr<ArenaMemoryManager> memory_manager_;
};

TEST(Type, HandleTypeTraits) {
  EXPECT_TRUE(std::is_default_constructible_v<Handle<Type>>);
  EXPECT_TRUE(std::is_copy_constructible_v<Handle<Type>>);
  EXPECT_TRUE(std::is_move_constructible_v<Handle<Type>>);
  EXPECT_TRUE(std::is_copy_assignable_v<Handle<Type>>);
  EXPECT_TRUE(std::is_move_assignable_v<Handle<Type>>);
  EXPECT_TRUE(std::is_swappable_v<Handle<Type>>);
  EXPECT_TRUE(std::is_default_constructible_v<Handle<Type>>);
  EXPECT_TRUE(std::is_copy_constructible_v<Handle<Type>>);
  EXPECT_TRUE(std::is_move_constructible_v<Handle<Type>>);
  EXPECT_TRUE(std::is_copy_assignable_v<Handle<Type>>);
  EXPECT_TRUE(std::is_move_assignable_v<Handle<Type>>);
  EXPECT_TRUE(std::is_swappable_v<Handle<Type>>);
}

TEST_P(TypeTest, CopyConstructor) {
  TypeFactory type_factory(memory_manager());
  Handle<Type> type(type_factory.GetIntType());
  EXPECT_EQ(type, type_factory.GetIntType());
}

TEST_P(TypeTest, MoveConstructor) {
  TypeFactory type_factory(memory_manager());
  Handle<Type> from(type_factory.GetIntType());
  Handle<Type> to(std::move(from));
  IS_INITIALIZED(from);
  EXPECT_FALSE(from);
  EXPECT_EQ(to, type_factory.GetIntType());
}

TEST_P(TypeTest, CopyAssignment) {
  TypeFactory type_factory(memory_manager());
  Handle<Type> type(type_factory.GetNullType());
  type = type_factory.GetIntType();
  EXPECT_EQ(type, type_factory.GetIntType());
}

TEST_P(TypeTest, MoveAssignment) {
  TypeFactory type_factory(memory_manager());
  Handle<Type> from(type_factory.GetIntType());
  Handle<Type> to(type_factory.GetNullType());
  to = std::move(from);
  IS_INITIALIZED(from);
  EXPECT_FALSE(from);
  EXPECT_EQ(to, type_factory.GetIntType());
}

TEST_P(TypeTest, Swap) {
  TypeFactory type_factory(memory_manager());
  Handle<Type> lhs = type_factory.GetIntType();
  Handle<Type> rhs = type_factory.GetUintType();
  std::swap(lhs, rhs);
  EXPECT_EQ(lhs, type_factory.GetUintType());
  EXPECT_EQ(rhs, type_factory.GetIntType());
}

// The below tests could be made parameterized but doing so requires the
// extension for struct member initiation by name for it to be worth it. That
// feature is not available in C++17.

template <typename T>
void TestTypeIs(const Handle<T>& type) {
  EXPECT_EQ(type->template Is<NullType>(), (std::is_same<T, NullType>::value));
  EXPECT_EQ(type->template Is<DynType>(), (std::is_same<T, DynType>::value));
  EXPECT_EQ(type->template Is<AnyType>(), (std::is_same<T, AnyType>::value));
  EXPECT_EQ(type->template Is<BoolType>(), (std::is_same<T, BoolType>::value));
  EXPECT_EQ(type->template Is<IntType>(), (std::is_same<T, IntType>::value));
  EXPECT_EQ(type->template Is<UintType>(), (std::is_same<T, UintType>::value));
  EXPECT_EQ(type->template Is<DoubleType>(),
            (std::is_same<T, DoubleType>::value));
  EXPECT_EQ(type->template Is<StringType>(),
            (std::is_same<T, StringType>::value));
  EXPECT_EQ(type->template Is<BytesType>(),
            (std::is_same<T, BytesType>::value));
  EXPECT_EQ(type->template Is<DurationType>(),
            (std::is_same<T, DurationType>::value));
  EXPECT_EQ(type->template Is<TimestampType>(),
            (std::is_same<T, TimestampType>::value));
  EXPECT_EQ(type->template Is<EnumType>(),
            (std::is_base_of<EnumType, T>::value));
  EXPECT_EQ(type->template Is<StructType>(),
            (std::is_base_of<StructType, T>::value));
  EXPECT_EQ(type->template Is<ListType>(), (std::is_same<T, ListType>::value));
  EXPECT_EQ(type->template Is<MapType>(), (std::is_same<T, MapType>::value));
  EXPECT_EQ(type->template Is<TypeType>(), (std::is_same<T, TypeType>::value));
  EXPECT_EQ(type->template Is<UnknownType>(),
            (std::is_same<T, UnknownType>::value));
  EXPECT_EQ(type->template Is<WrapperType>(),
            (std::is_base_of<WrapperType, T>::value));
  EXPECT_EQ(type->template Is<BoolWrapperType>(),
            (std::is_same<T, BoolWrapperType>::value));
  EXPECT_EQ(type->template Is<BytesWrapperType>(),
            (std::is_same<T, BytesWrapperType>::value));
  EXPECT_EQ(type->template Is<DoubleWrapperType>(),
            (std::is_same<T, DoubleWrapperType>::value));
  EXPECT_EQ(type->template Is<IntWrapperType>(),
            (std::is_same<T, IntWrapperType>::value));
  EXPECT_EQ(type->template Is<StringWrapperType>(),
            (std::is_same<T, StringWrapperType>::value));
  EXPECT_EQ(type->template Is<UintWrapperType>(),
            (std::is_same<T, UintWrapperType>::value));
  EXPECT_EQ(type->template Is<OpaqueType>(),
            (std::is_base_of<OpaqueType, T>::value));
  EXPECT_EQ(type->template Is<OptionalType>(),
            (std::is_same<T, OptionalType>::value));
}

TEST_P(TypeTest, Null) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetNullType()->kind(), TypeKind::kNullType);
  EXPECT_EQ(type_factory.GetNullType()->name(), "null_type");
  TestTypeIs(type_factory.GetNullType());
}

TEST_P(TypeTest, Error) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetErrorType()->kind(), TypeKind::kError);
  EXPECT_EQ(type_factory.GetErrorType()->name(), "*error*");
  TestTypeIs(type_factory.GetErrorType());
}

TEST_P(TypeTest, Dyn) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetDynType()->kind(), TypeKind::kDyn);
  EXPECT_EQ(type_factory.GetDynType()->name(), "dyn");
  TestTypeIs(type_factory.GetDynType());
}

TEST_P(TypeTest, Any) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetAnyType()->kind(), TypeKind::kAny);
  EXPECT_EQ(type_factory.GetAnyType()->name(), "google.protobuf.Any");
  TestTypeIs(type_factory.GetAnyType());
}

TEST_P(TypeTest, Bool) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetBoolType()->kind(), TypeKind::kBool);
  EXPECT_EQ(type_factory.GetBoolType()->name(), "bool");
  TestTypeIs(type_factory.GetBoolType());
}

TEST_P(TypeTest, Int) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetIntType()->kind(), TypeKind::kInt);
  EXPECT_EQ(type_factory.GetIntType()->name(), "int");
  TestTypeIs(type_factory.GetIntType());
}

TEST_P(TypeTest, Uint) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetUintType()->kind(), TypeKind::kUint);
  EXPECT_EQ(type_factory.GetUintType()->name(), "uint");
  TestTypeIs(type_factory.GetUintType());
}

TEST_P(TypeTest, Double) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetDoubleType()->kind(), TypeKind::kDouble);
  EXPECT_EQ(type_factory.GetDoubleType()->name(), "double");
  TestTypeIs(type_factory.GetDoubleType());
}

TEST_P(TypeTest, String) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetStringType()->kind(), TypeKind::kString);
  EXPECT_EQ(type_factory.GetStringType()->name(), "string");
  TestTypeIs(type_factory.GetStringType());
}

TEST_P(TypeTest, Bytes) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetBytesType()->kind(), TypeKind::kBytes);
  EXPECT_EQ(type_factory.GetBytesType()->name(), "bytes");
  TestTypeIs(type_factory.GetBytesType());
}

TEST_P(TypeTest, Duration) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetDurationType()->kind(), TypeKind::kDuration);
  EXPECT_EQ(type_factory.GetDurationType()->name(), "google.protobuf.Duration");
  TestTypeIs(type_factory.GetDurationType());
}

TEST_P(TypeTest, Timestamp) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetTimestampType()->kind(), TypeKind::kTimestamp);
  EXPECT_EQ(type_factory.GetTimestampType()->name(),
            "google.protobuf.Timestamp");
  TestTypeIs(type_factory.GetTimestampType());
}

TEST_P(TypeTest, Enum) {
  TypeFactory type_factory(memory_manager());
  ASSERT_OK_AND_ASSIGN(auto enum_type,
                       type_factory.CreateEnumType<TestEnumType>());
  EXPECT_EQ(enum_type->kind(), TypeKind::kEnum);
  EXPECT_EQ(enum_type->name(), "test_enum.TestEnum");
  TestTypeIs(enum_type);
}

TEST_P(TypeTest, Struct) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ASSERT_OK_AND_ASSIGN(
      auto struct_type,
      type_manager.type_factory().CreateStructType<TestStructType>());
  EXPECT_EQ(struct_type->kind(), TypeKind::kStruct);
  EXPECT_EQ(struct_type->name(), "test_struct.TestStruct");
  TestTypeIs(struct_type);
}

TEST_P(TypeTest, List) {
  TypeFactory type_factory(memory_manager());
  ASSERT_OK_AND_ASSIGN(auto list_type,
                       type_factory.CreateListType(type_factory.GetBoolType()));
  EXPECT_EQ(list_type,
            Must(type_factory.CreateListType(type_factory.GetBoolType())));
  EXPECT_EQ(list_type->kind(), TypeKind::kList);
  EXPECT_EQ(list_type->name(), "list");
  EXPECT_EQ(list_type->element(), type_factory.GetBoolType());
  TestTypeIs(list_type);
}

TEST_P(TypeTest, Map) {
  TypeFactory type_factory(memory_manager());
  ASSERT_OK_AND_ASSIGN(auto map_type,
                       type_factory.CreateMapType(type_factory.GetStringType(),
                                                  type_factory.GetBoolType()));
  EXPECT_EQ(map_type,
            Must(type_factory.CreateMapType(type_factory.GetStringType(),
                                            type_factory.GetBoolType())));
  EXPECT_NE(map_type,
            Must(type_factory.CreateMapType(type_factory.GetBoolType(),
                                            type_factory.GetStringType())));
  EXPECT_EQ(map_type->kind(), TypeKind::kMap);
  EXPECT_EQ(map_type->name(), "map");
  EXPECT_EQ(map_type->key(), type_factory.GetStringType());
  EXPECT_EQ(map_type->value(), type_factory.GetBoolType());
  TestTypeIs(map_type);
}

TEST_P(TypeTest, TypeType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetTypeType()->kind(), TypeKind::kType);
  EXPECT_EQ(type_factory.GetTypeType()->name(), "type");
  TestTypeIs(type_factory.GetTypeType());
}

TEST_P(TypeTest, UnknownType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetUnknownType()->kind(), TypeKind::kUnknown);
  EXPECT_EQ(type_factory.GetUnknownType()->name(), "*unknown*");
  TestTypeIs(type_factory.GetUnknownType());
}

TEST_P(TypeTest, OptionalType) {
  TypeFactory type_factory(memory_manager());
  ASSERT_OK_AND_ASSIGN(auto optional_type, type_factory.CreateOptionalType(
                                               type_factory.GetStringType()));
  EXPECT_EQ(optional_type->kind(), TypeKind::kOpaque);
  EXPECT_EQ(optional_type->name(), "optional");
  TestTypeIs(optional_type);
  TestTypeIs<StringType>(optional_type->type().As<StringType>());
}

TEST_P(TypeTest, BoolWrapperType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetBoolWrapperType()->kind(), TypeKind::kWrapper);
  EXPECT_EQ(type_factory.GetBoolWrapperType()->name(),
            "google.protobuf.BoolValue");
  TestTypeIs(type_factory.GetBoolWrapperType());
}

TEST_P(TypeTest, ByteWrapperType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetBytesWrapperType()->kind(), TypeKind::kWrapper);
  EXPECT_EQ(type_factory.GetBytesWrapperType()->name(),
            "google.protobuf.BytesValue");
  TestTypeIs(type_factory.GetBytesWrapperType());
}

TEST_P(TypeTest, DoubleWrapperType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetDoubleWrapperType()->kind(), TypeKind::kWrapper);
  EXPECT_EQ(type_factory.GetDoubleWrapperType()->name(),
            "google.protobuf.DoubleValue");
  TestTypeIs(type_factory.GetDoubleWrapperType());
}

TEST_P(TypeTest, IntWrapperType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetIntWrapperType()->kind(), TypeKind::kWrapper);
  EXPECT_EQ(type_factory.GetIntWrapperType()->name(),
            "google.protobuf.Int64Value");
  TestTypeIs(type_factory.GetIntWrapperType());
}

TEST_P(TypeTest, StringWrapperType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetStringWrapperType()->kind(), TypeKind::kWrapper);
  EXPECT_EQ(type_factory.GetStringWrapperType()->name(),
            "google.protobuf.StringValue");
  TestTypeIs(type_factory.GetStringWrapperType());
}

TEST_P(TypeTest, UintWrapperType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetUintWrapperType()->kind(), TypeKind::kWrapper);
  EXPECT_EQ(type_factory.GetUintWrapperType()->name(),
            "google.protobuf.UInt64Value");
  TestTypeIs(type_factory.GetUintWrapperType());
}

using EnumTypeTest = TypeTest;

TEST_P(EnumTypeTest, FindConstant) {
  TypeFactory type_factory(memory_manager());
  ASSERT_OK_AND_ASSIGN(auto enum_type,
                       type_factory.CreateEnumType<TestEnumType>());

  ASSERT_OK_AND_ASSIGN(auto value1, enum_type->FindConstantByName("VALUE1"));
  EXPECT_EQ(value1->name, "VALUE1");
  EXPECT_EQ(value1->number, 1);

  ASSERT_OK_AND_ASSIGN(value1, enum_type->FindConstantByNumber(1));
  EXPECT_EQ(value1->name, "VALUE1");
  EXPECT_EQ(value1->number, 1);

  ASSERT_OK_AND_ASSIGN(auto value2, enum_type->FindConstantByName("VALUE2"));
  EXPECT_EQ(value2->name, "VALUE2");
  EXPECT_EQ(value2->number, 2);

  ASSERT_OK_AND_ASSIGN(value2, enum_type->FindConstantByNumber(2));
  EXPECT_EQ(value2->name, "VALUE2");
  EXPECT_EQ(value2->number, 2);

  EXPECT_THAT(enum_type->FindConstantByName("VALUE3"),
              IsOkAndHolds(Eq(absl::nullopt)));
  EXPECT_THAT(enum_type->FindConstantByNumber(3),
              IsOkAndHolds(Eq(absl::nullopt)));
}

INSTANTIATE_TEST_SUITE_P(EnumTypeTest, EnumTypeTest,
                         base_internal::MemoryManagerTestModeAll(),
                         base_internal::MemoryManagerTestModeName);

class StructTypeTest : public TypeTest {};

TEST_P(StructTypeTest, FindField) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ASSERT_OK_AND_ASSIGN(
      auto struct_type,
      type_manager.type_factory().CreateStructType<TestStructType>());

  ASSERT_OK_AND_ASSIGN(
      auto field1, struct_type->FindFieldByName(type_manager, "bool_field"));
  EXPECT_EQ(field1->name, "bool_field");
  EXPECT_EQ(field1->number, 0);
  EXPECT_EQ(field1->type, type_manager.type_factory().GetBoolType());

  ASSERT_OK_AND_ASSIGN(field1, struct_type->FindFieldByNumber(type_manager, 0));
  EXPECT_EQ(field1->name, "bool_field");
  EXPECT_EQ(field1->number, 0);
  EXPECT_EQ(field1->type, type_manager.type_factory().GetBoolType());

  ASSERT_OK_AND_ASSIGN(auto field2,
                       struct_type->FindFieldByName(type_manager, "int_field"));
  EXPECT_EQ(field2->name, "int_field");
  EXPECT_EQ(field2->number, 1);
  EXPECT_EQ(field2->type, type_manager.type_factory().GetIntType());

  ASSERT_OK_AND_ASSIGN(field2, struct_type->FindFieldByNumber(type_manager, 1));
  EXPECT_EQ(field2->name, "int_field");
  EXPECT_EQ(field2->number, 1);
  EXPECT_EQ(field2->type, type_manager.type_factory().GetIntType());

  ASSERT_OK_AND_ASSIGN(
      auto field3, struct_type->FindFieldByName(type_manager, "uint_field"));
  EXPECT_EQ(field3->name, "uint_field");
  EXPECT_EQ(field3->number, 2);
  EXPECT_EQ(field3->type, type_manager.type_factory().GetUintType());

  ASSERT_OK_AND_ASSIGN(field3, struct_type->FindFieldByNumber(type_manager, 2));
  EXPECT_EQ(field3->name, "uint_field");
  EXPECT_EQ(field3->number, 2);
  EXPECT_EQ(field3->type, type_manager.type_factory().GetUintType());

  ASSERT_OK_AND_ASSIGN(
      auto field4, struct_type->FindFieldByName(type_manager, "double_field"));
  EXPECT_EQ(field4->name, "double_field");
  EXPECT_EQ(field4->number, 3);
  EXPECT_EQ(field4->type, type_manager.type_factory().GetDoubleType());

  ASSERT_OK_AND_ASSIGN(field4, struct_type->FindFieldByNumber(type_manager, 3));
  EXPECT_EQ(field4->name, "double_field");
  EXPECT_EQ(field4->number, 3);
  EXPECT_EQ(field4->type, type_manager.type_factory().GetDoubleType());

  EXPECT_THAT(struct_type->FindFieldByName(type_manager, "missing_field"),
              IsOkAndHolds(Eq(absl::nullopt)));
  EXPECT_THAT(struct_type->FindFieldByNumber(type_manager, 4),
              IsOkAndHolds(Eq(absl::nullopt)));
}

INSTANTIATE_TEST_SUITE_P(StructTypeTest, StructTypeTest,
                         base_internal::MemoryManagerTestModeAll(),
                         base_internal::MemoryManagerTestModeName);

class MapTypeTest : public TypeTest {};

TEST_P(MapTypeTest, CheckKey) {
  TypeFactory type_factory(memory_manager());
  EXPECT_THAT(MapType::CheckKey(*type_factory.GetDynType()), IsOk());
  EXPECT_THAT(MapType::CheckKey(*type_factory.GetBoolType()), IsOk());
  EXPECT_THAT(MapType::CheckKey(*type_factory.GetIntType()), IsOk());
  EXPECT_THAT(MapType::CheckKey(*type_factory.GetUintType()), IsOk());
  EXPECT_THAT(MapType::CheckKey(*type_factory.GetStringType()), IsOk());
  EXPECT_THAT(MapType::CheckKey(*type_factory.GetDoubleType()),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

INSTANTIATE_TEST_SUITE_P(MapTypeTest, MapTypeTest,
                         base_internal::MemoryManagerTestModeAll(),
                         base_internal::MemoryManagerTestModeName);

class OptionalTypeTest : public TypeTest {};

TEST_P(OptionalTypeTest, Parameters) {
  TypeFactory type_factory(memory_manager());
  ASSERT_OK_AND_ASSIGN(auto optional_type, type_factory.CreateOptionalType(
                                               type_factory.GetStringType()));
  EXPECT_THAT(optional_type->parameters(),
              ElementsAre(type_factory.GetStringType()));
}

INSTANTIATE_TEST_SUITE_P(OptionalTypeTest, OptionalTypeTest,
                         base_internal::MemoryManagerTestModeAll(),
                         base_internal::MemoryManagerTestModeName);

class TypeDebugStringTest : public TypeTest {};

TEST_P(TypeDebugStringTest, NullType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetNullType()->DebugString(), "null_type");
}

TEST_P(TypeDebugStringTest, ErrorType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetErrorType()->DebugString(), "*error*");
}

TEST_P(TypeDebugStringTest, DynType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetDynType()->DebugString(), "dyn");
}

TEST_P(TypeDebugStringTest, AnyType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetAnyType()->DebugString(), "google.protobuf.Any");
}

TEST_P(TypeDebugStringTest, BoolType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetBoolType()->DebugString(), "bool");
}

TEST_P(TypeDebugStringTest, IntType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetIntType()->DebugString(), "int");
}

TEST_P(TypeDebugStringTest, UintType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetUintType()->DebugString(), "uint");
}

TEST_P(TypeDebugStringTest, DoubleType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetDoubleType()->DebugString(), "double");
}

TEST_P(TypeDebugStringTest, StringType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetStringType()->DebugString(), "string");
}

TEST_P(TypeDebugStringTest, BytesType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetBytesType()->DebugString(), "bytes");
}

TEST_P(TypeDebugStringTest, DurationType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetDurationType()->DebugString(),
            "google.protobuf.Duration");
}

TEST_P(TypeDebugStringTest, TimestampType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetTimestampType()->DebugString(),
            "google.protobuf.Timestamp");
}

TEST_P(TypeDebugStringTest, EnumType) {
  TypeFactory type_factory(memory_manager());
  ASSERT_OK_AND_ASSIGN(auto enum_type,
                       type_factory.CreateEnumType<TestEnumType>());
  EXPECT_EQ(enum_type->DebugString(), "test_enum.TestEnum");
}

TEST_P(TypeDebugStringTest, StructType) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ASSERT_OK_AND_ASSIGN(
      auto struct_type,
      type_manager.type_factory().CreateStructType<TestStructType>());
  EXPECT_EQ(struct_type->DebugString(), "test_struct.TestStruct");
}

TEST_P(TypeDebugStringTest, ListType) {
  TypeFactory type_factory(memory_manager());
  ASSERT_OK_AND_ASSIGN(auto list_type,
                       type_factory.CreateListType(type_factory.GetBoolType()));
  EXPECT_EQ(list_type->DebugString(), "list(bool)");
}

TEST_P(TypeDebugStringTest, MapType) {
  TypeFactory type_factory(memory_manager());
  ASSERT_OK_AND_ASSIGN(auto map_type,
                       type_factory.CreateMapType(type_factory.GetStringType(),
                                                  type_factory.GetBoolType()));
  EXPECT_EQ(map_type->DebugString(), "map(string, bool)");
}

TEST_P(TypeDebugStringTest, TypeType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetTypeType()->DebugString(), "type");
}

TEST_P(TypeDebugStringTest, OptionalType) {
  TypeFactory type_factory(memory_manager());
  ASSERT_OK_AND_ASSIGN(auto optional_type, type_factory.CreateOptionalType(
                                               type_factory.GetStringType()));
  EXPECT_EQ(optional_type->DebugString(), "optional<string>");
}

TEST_P(TypeDebugStringTest, BoolWrapperType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetBoolWrapperType()->DebugString(),
            "google.protobuf.BoolValue");
}

TEST_P(TypeDebugStringTest, BytesWrapperType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetBytesWrapperType()->DebugString(),
            "google.protobuf.BytesValue");
}

TEST_P(TypeDebugStringTest, DoubleWrapperType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetDoubleWrapperType()->DebugString(),
            "google.protobuf.DoubleValue");
}

TEST_P(TypeDebugStringTest, IntWrapperType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetIntWrapperType()->DebugString(),
            "google.protobuf.Int64Value");
}

TEST_P(TypeDebugStringTest, StringWrapperType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetStringWrapperType()->DebugString(),
            "google.protobuf.StringValue");
}

TEST_P(TypeDebugStringTest, UintWrapperType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetUintWrapperType()->DebugString(),
            "google.protobuf.UInt64Value");
}

INSTANTIATE_TEST_SUITE_P(TypeDebugStringTest, TypeDebugStringTest,
                         base_internal::MemoryManagerTestModeAll(),
                         base_internal::MemoryManagerTestModeName);

class TypeNewValueFromAnyTest : public TypeTest {
 public:
  TypeFactory& type_factory() { return *type_factory_; }

  TypeManager& type_manager() { return *type_manager_; }

  ValueFactory& value_factory() { return *value_factory_; }

 protected:
  void SetUp() override {
    TypeTest::SetUp();
    type_factory_.emplace(memory_manager());
    type_manager_.emplace(type_factory(), TypeProvider::Builtin());
    value_factory_.emplace(type_manager());
  }

 private:
  absl::optional<TypeFactory> type_factory_;
  absl::optional<TypeManager> type_manager_;
  absl::optional<ValueFactory> value_factory_;
};

TEST_P(TypeNewValueFromAnyTest, BoolType) {
  EXPECT_THAT(type_factory().GetBoolType().As<Type>()->NewValueFromAny(
                  value_factory(), absl::Cord()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(TypeNewValueFromAnyTest, BytesType) {
  EXPECT_THAT(type_factory().GetBytesType().As<Type>()->NewValueFromAny(
                  value_factory(), absl::Cord()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(TypeNewValueFromAnyTest, DoubleType) {
  EXPECT_THAT(type_factory().GetDoubleType().As<Type>()->NewValueFromAny(
                  value_factory(), absl::Cord()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(TypeNewValueFromAnyTest, EnumType) {
  ASSERT_OK_AND_ASSIGN(auto enum_type,
                       type_factory().CreateEnumType<TestEnumType>());
  EXPECT_THAT(
      enum_type.As<Type>()->NewValueFromAny(value_factory(), absl::Cord()),
      StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(TypeNewValueFromAnyTest, ErrorType) {
  EXPECT_THAT(type_factory().GetErrorType().As<Type>()->NewValueFromAny(
                  value_factory(), absl::Cord()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(TypeNewValueFromAnyTest, IntType) {
  EXPECT_THAT(type_factory().GetIntType().As<Type>()->NewValueFromAny(
                  value_factory(), absl::Cord()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(TypeNewValueFromAnyTest, NullType) {
  EXPECT_THAT(type_factory().GetNullType().As<Type>()->NewValueFromAny(
                  value_factory(), absl::Cord()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(TypeNewValueFromAnyTest, OptionalType) {
  ASSERT_OK_AND_ASSIGN(
      auto optional_type,
      type_factory().CreateOptionalType(type_factory().GetDurationType()));
  EXPECT_THAT(
      optional_type.As<Type>()->NewValueFromAny(value_factory(), absl::Cord()),
      StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(TypeNewValueFromAnyTest, StringType) {
  EXPECT_THAT(type_factory().GetStringType().As<Type>()->NewValueFromAny(
                  value_factory(), absl::Cord()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(TypeNewValueFromAnyTest, TypeType) {
  EXPECT_THAT(type_factory().GetTypeType().As<Type>()->NewValueFromAny(
                  value_factory(), absl::Cord()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(TypeNewValueFromAnyTest, UnknownType) {
  EXPECT_THAT(type_factory().GetUnknownType().As<Type>()->NewValueFromAny(
                  value_factory(), absl::Cord()),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(TypeNewValueFromAnyTest, DurationType) {
  ASSERT_OK_AND_ASSIGN(
      auto value, type_factory().GetDurationType().As<Type>()->NewValueFromAny(
                      value_factory(), absl::Cord("\x08\x01\x10\x01")));
  ASSERT_TRUE(value->Is<DurationValue>());
  EXPECT_EQ(value->As<DurationValue>().value(),
            absl::Seconds(1) + absl::Nanoseconds(1));
}

TEST_P(TypeNewValueFromAnyTest, TimestampType) {
  ASSERT_OK_AND_ASSIGN(
      auto value, type_factory().GetTimestampType().As<Type>()->NewValueFromAny(
                      value_factory(), absl::Cord("\x08\x01\x10\x01")));
  ASSERT_TRUE(value->Is<TimestampValue>());
  EXPECT_EQ(value->As<TimestampValue>().value(),
            absl::UnixEpoch() + absl::Seconds(1) + absl::Nanoseconds(1));
}

TEST_P(TypeNewValueFromAnyTest, AnyType) {
  ASSERT_OK_AND_ASSIGN(
      auto value, type_factory().GetAnyType().As<Type>()->NewValueFromAny(
                      value_factory(),
                      absl::Cord("\x0a\x2dtype.googleapis.com/"
                                 "google.protobuf.BoolValue\x12\x02\x08\x01")));
  ASSERT_TRUE(value->Is<BoolValue>());
  EXPECT_TRUE(value->As<BoolValue>().value());
}

TEST_P(TypeNewValueFromAnyTest, BoolWrapperType) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      type_factory().GetBoolWrapperType().As<Type>()->NewValueFromAny(
          value_factory(), absl::Cord("\x08\x01")));
  ASSERT_TRUE(value->Is<BoolValue>());
  EXPECT_TRUE(value->As<BoolValue>().value());
}

TEST_P(TypeNewValueFromAnyTest, IntWrapperType) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      type_factory().GetIntWrapperType().As<Type>()->NewValueFromAny(
          value_factory(), absl::Cord("\x08\x01")));
  ASSERT_TRUE(value->Is<IntValue>());
  EXPECT_EQ(value->As<IntValue>().value(), 1);
}

TEST_P(TypeNewValueFromAnyTest, UintWrapperType) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      type_factory().GetUintWrapperType().As<Type>()->NewValueFromAny(
          value_factory(), absl::Cord("\x08\x01")));
  ASSERT_TRUE(value->Is<UintValue>());
  EXPECT_EQ(value->As<UintValue>().value(), 1);
}

TEST_P(TypeNewValueFromAnyTest, DoubleWrapperType) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      type_factory().GetDoubleWrapperType().As<Type>()->NewValueFromAny(
          value_factory(), absl::Cord(absl::string_view(
                               "\x09\x00\x00\x00\x00\x00\x00\xf0?", 9))));
  ASSERT_TRUE(value->Is<DoubleValue>());
  EXPECT_EQ(value->As<DoubleValue>().value(), 1.0);
  ASSERT_OK_AND_ASSIGN(
      value, type_factory().GetDoubleWrapperType().As<Type>()->NewValueFromAny(
                 value_factory(),
                 absl::Cord(absl::string_view("\x0d\x00\x00\x80?", 5))));
  ASSERT_TRUE(value->Is<DoubleValue>());
  EXPECT_EQ(value->As<DoubleValue>().value(), 1.0);
}

TEST_P(TypeNewValueFromAnyTest, BytesWrapperType) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      type_factory().GetBytesWrapperType().As<Type>()->NewValueFromAny(
          value_factory(), absl::Cord("\x0a\x03"
                                      "foo")));
  ASSERT_TRUE(value->Is<BytesValue>());
  EXPECT_EQ(value->As<BytesValue>().ToString(), "foo");
}

TEST_P(TypeNewValueFromAnyTest, StringWrapperType) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      type_factory().GetStringWrapperType().As<Type>()->NewValueFromAny(
          value_factory(), absl::Cord("\x0a\x03"
                                      "foo")));
  ASSERT_TRUE(value->Is<StringValue>());
  EXPECT_EQ(value->As<StringValue>().ToString(), "foo");
}

TEST_P(TypeNewValueFromAnyTest, DynType) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      type_factory().GetDynType().As<Type>()->NewValueFromAny(
          value_factory(), absl::Cord(absl::string_view("\x08\x00", 2))));
  ASSERT_TRUE(value->Is<NullValue>());

  ASSERT_OK_AND_ASSIGN(
      value,
      type_factory().GetDynType().As<Type>()->NewValueFromAny(
          value_factory(), absl::Cord(absl::string_view(
                               "\x11\x00\x00\x00\x00\x00\x00\xf0?", 9))));
  ASSERT_TRUE(value->Is<DoubleValue>());
  EXPECT_EQ(value->As<DoubleValue>().value(), 1.0);

  ASSERT_OK_AND_ASSIGN(value, type_factory().GetDynType()->NewValueFromAny(
                                  value_factory(), absl::Cord("\x1a\x03"
                                                              "foo")));
  ASSERT_TRUE(value->Is<StringValue>());
  EXPECT_EQ(value->As<StringValue>().ToString(), "foo");

  ASSERT_OK_AND_ASSIGN(value,
                       type_factory().GetDynType().As<Type>()->NewValueFromAny(
                           value_factory(), absl::Cord("\x20\x01")));
  ASSERT_TRUE(value->Is<BoolValue>());
  EXPECT_TRUE(value->As<BoolValue>().value());

  ASSERT_OK_AND_ASSIGN(
      value, type_factory().GetDynType().As<Type>()->NewValueFromAny(
                 value_factory(), absl::Cord("\x2a\x0b\x0a\x09\x0a\x03"
                                             "foo\x12\x02\x20\x01")));
  ASSERT_TRUE(value->Is<MapValue>());
  ASSERT_EQ(value->As<MapValue>().size(), 1);
  ASSERT_OK_AND_ASSIGN(auto key, value_factory().CreateStringValue("foo"));
  ASSERT_OK_AND_ASSIGN(auto entry,
                       value->As<MapValue>().Get(value_factory(), key));
  ASSERT_TRUE(entry->Is<BoolValue>());
  EXPECT_TRUE(entry->As<BoolValue>().value());

  ASSERT_OK_AND_ASSIGN(
      value, type_factory().GetDynType().As<Type>()->NewValueFromAny(
                 value_factory(),
                 absl::Cord(absl::string_view("\x32\x04\x0a\x02\x20\x01", 6))));
  ASSERT_TRUE(value->Is<ListValue>());
  ASSERT_EQ(value->As<ListValue>().size(), 1);
  ASSERT_OK_AND_ASSIGN(auto element,
                       value->As<ListValue>().Get(value_factory(), 0));
  ASSERT_TRUE(element->Is<BoolValue>());
  EXPECT_TRUE(element->As<BoolValue>().value());
}

INSTANTIATE_TEST_SUITE_P(TypeNewValueFromAnyTest, TypeNewValueFromAnyTest,
                         base_internal::MemoryManagerTestModeAll(),
                         base_internal::MemoryManagerTestModeName);

TEST(ListType, DestructorSkippable) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  ASSERT_OK_AND_ASSIGN(auto trivial_list_type,
                       type_factory.CreateListType(type_factory.GetBoolType()));
  EXPECT_TRUE(
      base_internal::Metadata::IsDestructorSkippable(*trivial_list_type));
}

TEST(MapType, DestructorSkippable) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  ASSERT_OK_AND_ASSIGN(auto trivial_map_type,
                       type_factory.CreateMapType(type_factory.GetStringType(),
                                                  type_factory.GetBoolType()));
  EXPECT_TRUE(
      base_internal::Metadata::IsDestructorSkippable(*trivial_map_type));
}

TEST(OptionalType, DestructorSkippable) {
  auto memory_manager = ArenaMemoryManager::Default();
  TypeFactory type_factory(*memory_manager);
  ASSERT_OK_AND_ASSIGN(
      auto trivial_optional_type,
      type_factory.CreateOptionalType(type_factory.GetStringType()));
  EXPECT_TRUE(
      base_internal::Metadata::IsDestructorSkippable(*trivial_optional_type));
}

TEST_P(TypeTest, SupportsAbslHash) {
  TypeFactory type_factory(memory_manager());
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      Handle<Type>(type_factory.GetNullType()),
      Handle<Type>(type_factory.GetErrorType()),
      Handle<Type>(type_factory.GetDynType()),
      Handle<Type>(type_factory.GetAnyType()),
      Handle<Type>(type_factory.GetBoolType()),
      Handle<Type>(type_factory.GetIntType()),
      Handle<Type>(type_factory.GetUintType()),
      Handle<Type>(type_factory.GetDoubleType()),
      Handle<Type>(type_factory.GetStringType()),
      Handle<Type>(type_factory.GetBytesType()),
      Handle<Type>(type_factory.GetDurationType()),
      Handle<Type>(type_factory.GetTimestampType()),
      Handle<Type>(Must(type_factory.CreateEnumType<TestEnumType>())),
      Handle<Type>(Must(type_factory.CreateStructType<TestStructType>())),
      Handle<Type>(
          Must(type_factory.CreateListType(type_factory.GetBoolType()))),
      Handle<Type>(Must(type_factory.CreateMapType(
          type_factory.GetStringType(), type_factory.GetBoolType()))),
      Handle<Type>(type_factory.GetTypeType()),
      Handle<Type>(type_factory.GetUnknownType()),
      Handle<Type>(type_factory.GetBoolWrapperType()),
      Handle<Type>(type_factory.GetBytesWrapperType()),
      Handle<Type>(type_factory.GetDoubleWrapperType()),
      Handle<Type>(type_factory.GetIntWrapperType()),
      Handle<Type>(type_factory.GetStringWrapperType()),
      Handle<Type>(type_factory.GetUintWrapperType()),
      Handle<Type>(
          Must(type_factory.CreateOptionalType(type_factory.GetStringType()))),
  }));
}

INSTANTIATE_TEST_SUITE_P(TypeTest, TypeTest,
                         base_internal::MemoryManagerTestModeAll(),
                         base_internal::MemoryManagerTestModeName);

}  // namespace
}  // namespace cel
