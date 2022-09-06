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

#include <memory>
#include <type_traits>
#include <utility>

#include "absl/hash/hash_testing.h"
#include "absl/status/status.h"
#include "base/handle.h"
#include "base/internal/memory_manager_testing.h"
#include "base/memory_manager.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "base/value.h"
#include "base/values/enum_value.h"
#include "base/values/struct_value.h"
#include "internal/testing.h"

namespace cel {
namespace {

using cel::internal::StatusIs;

enum class TestEnum {
  kValue1 = 1,
  kValue2 = 2,
};

class TestEnumType final : public EnumType {
 public:
  using EnumType::EnumType;

  absl::string_view name() const override { return "test_enum.TestEnum"; }

 protected:
  absl::StatusOr<Persistent<const EnumValue>> NewInstanceByName(
      TypedEnumValueFactory& factory, absl::string_view name) const override {
    return absl::UnimplementedError("");
  }

  absl::StatusOr<Persistent<const EnumValue>> NewInstanceByNumber(
      TypedEnumValueFactory& factory, int64_t number) const override {
    return absl::UnimplementedError("");
  }

  absl::StatusOr<Constant> FindConstantByName(
      absl::string_view name) const override {
    if (name == "VALUE1") {
      return Constant("VALUE1", static_cast<int64_t>(TestEnum::kValue1));
    } else if (name == "VALUE2") {
      return Constant("VALUE2", static_cast<int64_t>(TestEnum::kValue2));
    }
    return absl::NotFoundError("");
  }

  absl::StatusOr<Constant> FindConstantByNumber(int64_t number) const override {
    switch (number) {
      case 1:
        return Constant("VALUE1", static_cast<int64_t>(TestEnum::kValue1));
      case 2:
        return Constant("VALUE2", static_cast<int64_t>(TestEnum::kValue2));
      default:
        return absl::NotFoundError("");
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

 protected:
  absl::StatusOr<Persistent<StructValue>> NewInstance(
      TypedStructValueFactory& factory) const override {
    return absl::UnimplementedError("");
  }

  absl::StatusOr<Field> FindFieldByName(TypeManager& type_manager,
                                        absl::string_view name) const override {
    if (name == "bool_field") {
      return Field("bool_field", 0, type_manager.type_factory().GetBoolType());
    } else if (name == "int_field") {
      return Field("int_field", 1, type_manager.type_factory().GetIntType());
    } else if (name == "uint_field") {
      return Field("uint_field", 2, type_manager.type_factory().GetUintType());
    } else if (name == "double_field") {
      return Field("double_field", 3,
                   type_manager.type_factory().GetDoubleType());
    }
    return absl::NotFoundError("");
  }

  absl::StatusOr<Field> FindFieldByNumber(TypeManager& type_manager,
                                          int64_t number) const override {
    switch (number) {
      case 0:
        return Field("bool_field", 0,
                     type_manager.type_factory().GetBoolType());
      case 1:
        return Field("int_field", 1, type_manager.type_factory().GetIntType());
      case 2:
        return Field("uint_field", 2,
                     type_manager.type_factory().GetUintType());
      case 3:
        return Field("double_field", 3,
                     type_manager.type_factory().GetDoubleType());
      default:
        return absl::NotFoundError("");
    }
  }

 private:
  CEL_DECLARE_STRUCT_TYPE(TestStructType);
};

CEL_IMPLEMENT_STRUCT_TYPE(TestStructType);

template <typename T>
Persistent<T> Must(absl::StatusOr<Persistent<T>> status_or_handle) {
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

TEST(Type, PersistentHandleTypeTraits) {
  EXPECT_TRUE(std::is_default_constructible_v<Persistent<Type>>);
  EXPECT_TRUE(std::is_copy_constructible_v<Persistent<Type>>);
  EXPECT_TRUE(std::is_move_constructible_v<Persistent<Type>>);
  EXPECT_TRUE(std::is_copy_assignable_v<Persistent<Type>>);
  EXPECT_TRUE(std::is_move_assignable_v<Persistent<Type>>);
  EXPECT_TRUE(std::is_swappable_v<Persistent<Type>>);
  EXPECT_TRUE(std::is_default_constructible_v<Persistent<const Type>>);
  EXPECT_TRUE(std::is_copy_constructible_v<Persistent<const Type>>);
  EXPECT_TRUE(std::is_move_constructible_v<Persistent<const Type>>);
  EXPECT_TRUE(std::is_copy_assignable_v<Persistent<const Type>>);
  EXPECT_TRUE(std::is_move_assignable_v<Persistent<const Type>>);
  EXPECT_TRUE(std::is_swappable_v<Persistent<const Type>>);
}

TEST_P(TypeTest, CopyConstructor) {
  TypeFactory type_factory(memory_manager());
  Persistent<const Type> type(type_factory.GetIntType());
  EXPECT_EQ(type, type_factory.GetIntType());
}

TEST_P(TypeTest, MoveConstructor) {
  TypeFactory type_factory(memory_manager());
  Persistent<const Type> from(type_factory.GetIntType());
  Persistent<const Type> to(std::move(from));
  IS_INITIALIZED(from);
  EXPECT_FALSE(from);
  EXPECT_EQ(to, type_factory.GetIntType());
}

TEST_P(TypeTest, CopyAssignment) {
  TypeFactory type_factory(memory_manager());
  Persistent<const Type> type(type_factory.GetNullType());
  type = type_factory.GetIntType();
  EXPECT_EQ(type, type_factory.GetIntType());
}

TEST_P(TypeTest, MoveAssignment) {
  TypeFactory type_factory(memory_manager());
  Persistent<const Type> from(type_factory.GetIntType());
  Persistent<const Type> to(type_factory.GetNullType());
  to = std::move(from);
  IS_INITIALIZED(from);
  EXPECT_FALSE(from);
  EXPECT_EQ(to, type_factory.GetIntType());
}

TEST_P(TypeTest, Swap) {
  TypeFactory type_factory(memory_manager());
  Persistent<const Type> lhs = type_factory.GetIntType();
  Persistent<const Type> rhs = type_factory.GetUintType();
  std::swap(lhs, rhs);
  EXPECT_EQ(lhs, type_factory.GetUintType());
  EXPECT_EQ(rhs, type_factory.GetIntType());
}

// The below tests could be made parameterized but doing so requires the
// extension for struct member initiation by name for it to be worth it. That
// feature is not available in C++17.

TEST_P(TypeTest, Null) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetNullType()->kind(), Kind::kNullType);
  EXPECT_EQ(type_factory.GetNullType()->name(), "null_type");
  EXPECT_TRUE(type_factory.GetNullType().Is<NullType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<DynType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<AnyType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<BoolType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<IntType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<UintType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<DoubleType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<StringType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<BytesType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<DurationType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<TimestampType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<EnumType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<StructType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<ListType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<MapType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<TypeType>());
  EXPECT_FALSE(type_factory.GetNullType().Is<UnknownType>());
}

TEST_P(TypeTest, Error) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetErrorType()->kind(), Kind::kError);
  EXPECT_EQ(type_factory.GetErrorType()->name(), "*error*");
  EXPECT_FALSE(type_factory.GetErrorType().Is<NullType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<DynType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<AnyType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<BoolType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<IntType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<UintType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<DoubleType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<StringType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<BytesType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<DurationType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<TimestampType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<EnumType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<StructType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<ListType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<MapType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<TypeType>());
  EXPECT_FALSE(type_factory.GetErrorType().Is<UnknownType>());
}

TEST_P(TypeTest, Dyn) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetDynType()->kind(), Kind::kDyn);
  EXPECT_EQ(type_factory.GetDynType()->name(), "dyn");
  EXPECT_FALSE(type_factory.GetDynType().Is<NullType>());
  EXPECT_TRUE(type_factory.GetDynType().Is<DynType>());
  EXPECT_FALSE(type_factory.GetDynType().Is<AnyType>());
  EXPECT_FALSE(type_factory.GetDynType().Is<BoolType>());
  EXPECT_FALSE(type_factory.GetDynType().Is<IntType>());
  EXPECT_FALSE(type_factory.GetDynType().Is<UintType>());
  EXPECT_FALSE(type_factory.GetDynType().Is<DoubleType>());
  EXPECT_FALSE(type_factory.GetDynType().Is<StringType>());
  EXPECT_FALSE(type_factory.GetDynType().Is<BytesType>());
  EXPECT_FALSE(type_factory.GetDynType().Is<DurationType>());
  EXPECT_FALSE(type_factory.GetDynType().Is<TimestampType>());
  EXPECT_FALSE(type_factory.GetDynType().Is<EnumType>());
  EXPECT_FALSE(type_factory.GetDynType().Is<StructType>());
  EXPECT_FALSE(type_factory.GetDynType().Is<ListType>());
  EXPECT_FALSE(type_factory.GetDynType().Is<MapType>());
  EXPECT_FALSE(type_factory.GetDynType().Is<TypeType>());
  EXPECT_FALSE(type_factory.GetDynType().Is<UnknownType>());
}

TEST_P(TypeTest, Any) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetAnyType()->kind(), Kind::kAny);
  EXPECT_EQ(type_factory.GetAnyType()->name(), "google.protobuf.Any");
  EXPECT_FALSE(type_factory.GetAnyType().Is<NullType>());
  EXPECT_FALSE(type_factory.GetAnyType().Is<DynType>());
  EXPECT_TRUE(type_factory.GetAnyType().Is<AnyType>());
  EXPECT_FALSE(type_factory.GetAnyType().Is<BoolType>());
  EXPECT_FALSE(type_factory.GetAnyType().Is<IntType>());
  EXPECT_FALSE(type_factory.GetAnyType().Is<UintType>());
  EXPECT_FALSE(type_factory.GetAnyType().Is<DoubleType>());
  EXPECT_FALSE(type_factory.GetAnyType().Is<StringType>());
  EXPECT_FALSE(type_factory.GetAnyType().Is<BytesType>());
  EXPECT_FALSE(type_factory.GetAnyType().Is<DurationType>());
  EXPECT_FALSE(type_factory.GetAnyType().Is<TimestampType>());
  EXPECT_FALSE(type_factory.GetAnyType().Is<EnumType>());
  EXPECT_FALSE(type_factory.GetAnyType().Is<StructType>());
  EXPECT_FALSE(type_factory.GetAnyType().Is<ListType>());
  EXPECT_FALSE(type_factory.GetAnyType().Is<MapType>());
  EXPECT_FALSE(type_factory.GetAnyType().Is<TypeType>());
  EXPECT_FALSE(type_factory.GetAnyType().Is<UnknownType>());
}

TEST_P(TypeTest, Bool) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetBoolType()->kind(), Kind::kBool);
  EXPECT_EQ(type_factory.GetBoolType()->name(), "bool");
  EXPECT_FALSE(type_factory.GetBoolType().Is<NullType>());
  EXPECT_FALSE(type_factory.GetBoolType().Is<DynType>());
  EXPECT_FALSE(type_factory.GetBoolType().Is<AnyType>());
  EXPECT_TRUE(type_factory.GetBoolType().Is<BoolType>());
  EXPECT_FALSE(type_factory.GetBoolType().Is<IntType>());
  EXPECT_FALSE(type_factory.GetBoolType().Is<UintType>());
  EXPECT_FALSE(type_factory.GetBoolType().Is<DoubleType>());
  EXPECT_FALSE(type_factory.GetBoolType().Is<StringType>());
  EXPECT_FALSE(type_factory.GetBoolType().Is<BytesType>());
  EXPECT_FALSE(type_factory.GetBoolType().Is<DurationType>());
  EXPECT_FALSE(type_factory.GetBoolType().Is<TimestampType>());
  EXPECT_FALSE(type_factory.GetBoolType().Is<EnumType>());
  EXPECT_FALSE(type_factory.GetBoolType().Is<StructType>());
  EXPECT_FALSE(type_factory.GetBoolType().Is<ListType>());
  EXPECT_FALSE(type_factory.GetBoolType().Is<MapType>());
  EXPECT_FALSE(type_factory.GetBoolType().Is<TypeType>());
  EXPECT_FALSE(type_factory.GetBoolType().Is<UnknownType>());
}

TEST_P(TypeTest, Int) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetIntType()->kind(), Kind::kInt);
  EXPECT_EQ(type_factory.GetIntType()->name(), "int");
  EXPECT_FALSE(type_factory.GetIntType().Is<NullType>());
  EXPECT_FALSE(type_factory.GetIntType().Is<DynType>());
  EXPECT_FALSE(type_factory.GetIntType().Is<AnyType>());
  EXPECT_FALSE(type_factory.GetIntType().Is<BoolType>());
  EXPECT_TRUE(type_factory.GetIntType().Is<IntType>());
  EXPECT_FALSE(type_factory.GetIntType().Is<UintType>());
  EXPECT_FALSE(type_factory.GetIntType().Is<DoubleType>());
  EXPECT_FALSE(type_factory.GetIntType().Is<StringType>());
  EXPECT_FALSE(type_factory.GetIntType().Is<BytesType>());
  EXPECT_FALSE(type_factory.GetIntType().Is<DurationType>());
  EXPECT_FALSE(type_factory.GetIntType().Is<TimestampType>());
  EXPECT_FALSE(type_factory.GetIntType().Is<EnumType>());
  EXPECT_FALSE(type_factory.GetIntType().Is<StructType>());
  EXPECT_FALSE(type_factory.GetIntType().Is<ListType>());
  EXPECT_FALSE(type_factory.GetIntType().Is<MapType>());
  EXPECT_FALSE(type_factory.GetIntType().Is<TypeType>());
  EXPECT_FALSE(type_factory.GetIntType().Is<UnknownType>());
}

TEST_P(TypeTest, Uint) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetUintType()->kind(), Kind::kUint);
  EXPECT_EQ(type_factory.GetUintType()->name(), "uint");
  EXPECT_FALSE(type_factory.GetUintType().Is<NullType>());
  EXPECT_FALSE(type_factory.GetUintType().Is<DynType>());
  EXPECT_FALSE(type_factory.GetUintType().Is<AnyType>());
  EXPECT_FALSE(type_factory.GetUintType().Is<BoolType>());
  EXPECT_FALSE(type_factory.GetUintType().Is<IntType>());
  EXPECT_TRUE(type_factory.GetUintType().Is<UintType>());
  EXPECT_FALSE(type_factory.GetUintType().Is<DoubleType>());
  EXPECT_FALSE(type_factory.GetUintType().Is<StringType>());
  EXPECT_FALSE(type_factory.GetUintType().Is<BytesType>());
  EXPECT_FALSE(type_factory.GetUintType().Is<DurationType>());
  EXPECT_FALSE(type_factory.GetUintType().Is<TimestampType>());
  EXPECT_FALSE(type_factory.GetUintType().Is<EnumType>());
  EXPECT_FALSE(type_factory.GetUintType().Is<StructType>());
  EXPECT_FALSE(type_factory.GetUintType().Is<ListType>());
  EXPECT_FALSE(type_factory.GetUintType().Is<MapType>());
  EXPECT_FALSE(type_factory.GetUintType().Is<TypeType>());
  EXPECT_FALSE(type_factory.GetUintType().Is<UnknownType>());
}

TEST_P(TypeTest, Double) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetDoubleType()->kind(), Kind::kDouble);
  EXPECT_EQ(type_factory.GetDoubleType()->name(), "double");
  EXPECT_FALSE(type_factory.GetDoubleType().Is<NullType>());
  EXPECT_FALSE(type_factory.GetDoubleType().Is<DynType>());
  EXPECT_FALSE(type_factory.GetDoubleType().Is<AnyType>());
  EXPECT_FALSE(type_factory.GetDoubleType().Is<BoolType>());
  EXPECT_FALSE(type_factory.GetDoubleType().Is<IntType>());
  EXPECT_FALSE(type_factory.GetDoubleType().Is<UintType>());
  EXPECT_TRUE(type_factory.GetDoubleType().Is<DoubleType>());
  EXPECT_FALSE(type_factory.GetDoubleType().Is<StringType>());
  EXPECT_FALSE(type_factory.GetDoubleType().Is<BytesType>());
  EXPECT_FALSE(type_factory.GetDoubleType().Is<DurationType>());
  EXPECT_FALSE(type_factory.GetDoubleType().Is<TimestampType>());
  EXPECT_FALSE(type_factory.GetDoubleType().Is<EnumType>());
  EXPECT_FALSE(type_factory.GetDoubleType().Is<StructType>());
  EXPECT_FALSE(type_factory.GetDoubleType().Is<ListType>());
  EXPECT_FALSE(type_factory.GetDoubleType().Is<MapType>());
  EXPECT_FALSE(type_factory.GetDoubleType().Is<TypeType>());
  EXPECT_FALSE(type_factory.GetDoubleType().Is<UnknownType>());
}

TEST_P(TypeTest, String) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetStringType()->kind(), Kind::kString);
  EXPECT_EQ(type_factory.GetStringType()->name(), "string");
  EXPECT_FALSE(type_factory.GetStringType().Is<NullType>());
  EXPECT_FALSE(type_factory.GetStringType().Is<DynType>());
  EXPECT_FALSE(type_factory.GetStringType().Is<AnyType>());
  EXPECT_FALSE(type_factory.GetStringType().Is<BoolType>());
  EXPECT_FALSE(type_factory.GetStringType().Is<IntType>());
  EXPECT_FALSE(type_factory.GetStringType().Is<UintType>());
  EXPECT_FALSE(type_factory.GetStringType().Is<DoubleType>());
  EXPECT_TRUE(type_factory.GetStringType().Is<StringType>());
  EXPECT_FALSE(type_factory.GetStringType().Is<BytesType>());
  EXPECT_FALSE(type_factory.GetStringType().Is<DurationType>());
  EXPECT_FALSE(type_factory.GetStringType().Is<TimestampType>());
  EXPECT_FALSE(type_factory.GetStringType().Is<EnumType>());
  EXPECT_FALSE(type_factory.GetStringType().Is<StructType>());
  EXPECT_FALSE(type_factory.GetStringType().Is<ListType>());
  EXPECT_FALSE(type_factory.GetStringType().Is<MapType>());
  EXPECT_FALSE(type_factory.GetStringType().Is<TypeType>());
  EXPECT_FALSE(type_factory.GetStringType().Is<UnknownType>());
}

TEST_P(TypeTest, Bytes) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetBytesType()->kind(), Kind::kBytes);
  EXPECT_EQ(type_factory.GetBytesType()->name(), "bytes");
  EXPECT_FALSE(type_factory.GetBytesType().Is<NullType>());
  EXPECT_FALSE(type_factory.GetBytesType().Is<DynType>());
  EXPECT_FALSE(type_factory.GetBytesType().Is<AnyType>());
  EXPECT_FALSE(type_factory.GetBytesType().Is<BoolType>());
  EXPECT_FALSE(type_factory.GetBytesType().Is<IntType>());
  EXPECT_FALSE(type_factory.GetBytesType().Is<UintType>());
  EXPECT_FALSE(type_factory.GetBytesType().Is<DoubleType>());
  EXPECT_FALSE(type_factory.GetBytesType().Is<StringType>());
  EXPECT_TRUE(type_factory.GetBytesType().Is<BytesType>());
  EXPECT_FALSE(type_factory.GetBytesType().Is<DurationType>());
  EXPECT_FALSE(type_factory.GetBytesType().Is<TimestampType>());
  EXPECT_FALSE(type_factory.GetBytesType().Is<EnumType>());
  EXPECT_FALSE(type_factory.GetBytesType().Is<StructType>());
  EXPECT_FALSE(type_factory.GetBytesType().Is<ListType>());
  EXPECT_FALSE(type_factory.GetBytesType().Is<MapType>());
  EXPECT_FALSE(type_factory.GetBytesType().Is<TypeType>());
  EXPECT_FALSE(type_factory.GetBytesType().Is<UnknownType>());
}

TEST_P(TypeTest, Duration) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetDurationType()->kind(), Kind::kDuration);
  EXPECT_EQ(type_factory.GetDurationType()->name(), "google.protobuf.Duration");
  EXPECT_FALSE(type_factory.GetDurationType().Is<NullType>());
  EXPECT_FALSE(type_factory.GetDurationType().Is<DynType>());
  EXPECT_FALSE(type_factory.GetDurationType().Is<AnyType>());
  EXPECT_FALSE(type_factory.GetDurationType().Is<BoolType>());
  EXPECT_FALSE(type_factory.GetDurationType().Is<IntType>());
  EXPECT_FALSE(type_factory.GetDurationType().Is<UintType>());
  EXPECT_FALSE(type_factory.GetDurationType().Is<DoubleType>());
  EXPECT_FALSE(type_factory.GetDurationType().Is<StringType>());
  EXPECT_FALSE(type_factory.GetDurationType().Is<BytesType>());
  EXPECT_TRUE(type_factory.GetDurationType().Is<DurationType>());
  EXPECT_FALSE(type_factory.GetDurationType().Is<TimestampType>());
  EXPECT_FALSE(type_factory.GetDurationType().Is<EnumType>());
  EXPECT_FALSE(type_factory.GetDurationType().Is<StructType>());
  EXPECT_FALSE(type_factory.GetDurationType().Is<ListType>());
  EXPECT_FALSE(type_factory.GetDurationType().Is<MapType>());
  EXPECT_FALSE(type_factory.GetDurationType().Is<TypeType>());
  EXPECT_FALSE(type_factory.GetDurationType().Is<UnknownType>());
}

TEST_P(TypeTest, Timestamp) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetTimestampType()->kind(), Kind::kTimestamp);
  EXPECT_EQ(type_factory.GetTimestampType()->name(),
            "google.protobuf.Timestamp");
  EXPECT_FALSE(type_factory.GetTimestampType().Is<NullType>());
  EXPECT_FALSE(type_factory.GetTimestampType().Is<DynType>());
  EXPECT_FALSE(type_factory.GetTimestampType().Is<AnyType>());
  EXPECT_FALSE(type_factory.GetTimestampType().Is<BoolType>());
  EXPECT_FALSE(type_factory.GetTimestampType().Is<IntType>());
  EXPECT_FALSE(type_factory.GetTimestampType().Is<UintType>());
  EXPECT_FALSE(type_factory.GetTimestampType().Is<DoubleType>());
  EXPECT_FALSE(type_factory.GetTimestampType().Is<StringType>());
  EXPECT_FALSE(type_factory.GetTimestampType().Is<BytesType>());
  EXPECT_FALSE(type_factory.GetTimestampType().Is<DurationType>());
  EXPECT_TRUE(type_factory.GetTimestampType().Is<TimestampType>());
  EXPECT_FALSE(type_factory.GetTimestampType().Is<EnumType>());
  EXPECT_FALSE(type_factory.GetTimestampType().Is<StructType>());
  EXPECT_FALSE(type_factory.GetTimestampType().Is<ListType>());
  EXPECT_FALSE(type_factory.GetTimestampType().Is<MapType>());
  EXPECT_FALSE(type_factory.GetTimestampType().Is<TypeType>());
  EXPECT_FALSE(type_factory.GetTimestampType().Is<UnknownType>());
}

TEST_P(TypeTest, Enum) {
  TypeFactory type_factory(memory_manager());
  ASSERT_OK_AND_ASSIGN(auto enum_type,
                       type_factory.CreateEnumType<TestEnumType>());
  EXPECT_EQ(enum_type->kind(), Kind::kEnum);
  EXPECT_EQ(enum_type->name(), "test_enum.TestEnum");
  EXPECT_FALSE(enum_type.Is<NullType>());
  EXPECT_FALSE(enum_type.Is<DynType>());
  EXPECT_FALSE(enum_type.Is<AnyType>());
  EXPECT_FALSE(enum_type.Is<BoolType>());
  EXPECT_FALSE(enum_type.Is<IntType>());
  EXPECT_FALSE(enum_type.Is<UintType>());
  EXPECT_FALSE(enum_type.Is<DoubleType>());
  EXPECT_FALSE(enum_type.Is<StringType>());
  EXPECT_FALSE(enum_type.Is<BytesType>());
  EXPECT_FALSE(enum_type.Is<DurationType>());
  EXPECT_FALSE(enum_type.Is<TimestampType>());
  EXPECT_TRUE(enum_type.Is<EnumType>());
  EXPECT_TRUE(enum_type.Is<TestEnumType>());
  EXPECT_FALSE(enum_type.Is<StructType>());
  EXPECT_FALSE(enum_type.Is<ListType>());
  EXPECT_FALSE(enum_type.Is<MapType>());
  EXPECT_FALSE(enum_type.Is<TypeType>());
  EXPECT_FALSE(enum_type.Is<UnknownType>());
}

TEST_P(TypeTest, Struct) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ASSERT_OK_AND_ASSIGN(
      auto struct_type,
      type_manager.type_factory().CreateStructType<TestStructType>());
  EXPECT_EQ(struct_type->kind(), Kind::kStruct);
  EXPECT_EQ(struct_type->name(), "test_struct.TestStruct");
  EXPECT_FALSE(struct_type.Is<NullType>());
  EXPECT_FALSE(struct_type.Is<DynType>());
  EXPECT_FALSE(struct_type.Is<AnyType>());
  EXPECT_FALSE(struct_type.Is<BoolType>());
  EXPECT_FALSE(struct_type.Is<IntType>());
  EXPECT_FALSE(struct_type.Is<UintType>());
  EXPECT_FALSE(struct_type.Is<DoubleType>());
  EXPECT_FALSE(struct_type.Is<StringType>());
  EXPECT_FALSE(struct_type.Is<BytesType>());
  EXPECT_FALSE(struct_type.Is<DurationType>());
  EXPECT_FALSE(struct_type.Is<TimestampType>());
  EXPECT_FALSE(struct_type.Is<EnumType>());
  EXPECT_TRUE(struct_type.Is<StructType>());
  EXPECT_TRUE(struct_type.Is<TestStructType>());
  EXPECT_FALSE(struct_type.Is<ListType>());
  EXPECT_FALSE(struct_type.Is<MapType>());
  EXPECT_FALSE(struct_type.Is<TypeType>());
  EXPECT_FALSE(struct_type.Is<UnknownType>());
}

TEST_P(TypeTest, List) {
  TypeFactory type_factory(memory_manager());
  ASSERT_OK_AND_ASSIGN(auto list_type,
                       type_factory.CreateListType(type_factory.GetBoolType()));
  EXPECT_EQ(list_type,
            Must(type_factory.CreateListType(type_factory.GetBoolType())));
  EXPECT_EQ(list_type->kind(), Kind::kList);
  EXPECT_EQ(list_type->name(), "list");
  EXPECT_EQ(list_type->element(), type_factory.GetBoolType());
  EXPECT_FALSE(list_type.Is<NullType>());
  EXPECT_FALSE(list_type.Is<DynType>());
  EXPECT_FALSE(list_type.Is<AnyType>());
  EXPECT_FALSE(list_type.Is<BoolType>());
  EXPECT_FALSE(list_type.Is<IntType>());
  EXPECT_FALSE(list_type.Is<UintType>());
  EXPECT_FALSE(list_type.Is<DoubleType>());
  EXPECT_FALSE(list_type.Is<StringType>());
  EXPECT_FALSE(list_type.Is<BytesType>());
  EXPECT_FALSE(list_type.Is<DurationType>());
  EXPECT_FALSE(list_type.Is<TimestampType>());
  EXPECT_FALSE(list_type.Is<EnumType>());
  EXPECT_FALSE(list_type.Is<StructType>());
  EXPECT_TRUE(list_type.Is<ListType>());
  EXPECT_FALSE(list_type.Is<MapType>());
  EXPECT_FALSE(list_type.Is<TypeType>());
  EXPECT_FALSE(list_type.Is<UnknownType>());
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
  EXPECT_EQ(map_type->kind(), Kind::kMap);
  EXPECT_EQ(map_type->name(), "map");
  EXPECT_EQ(map_type->key(), type_factory.GetStringType());
  EXPECT_EQ(map_type->value(), type_factory.GetBoolType());
  EXPECT_FALSE(map_type.Is<NullType>());
  EXPECT_FALSE(map_type.Is<DynType>());
  EXPECT_FALSE(map_type.Is<AnyType>());
  EXPECT_FALSE(map_type.Is<BoolType>());
  EXPECT_FALSE(map_type.Is<IntType>());
  EXPECT_FALSE(map_type.Is<UintType>());
  EXPECT_FALSE(map_type.Is<DoubleType>());
  EXPECT_FALSE(map_type.Is<StringType>());
  EXPECT_FALSE(map_type.Is<BytesType>());
  EXPECT_FALSE(map_type.Is<DurationType>());
  EXPECT_FALSE(map_type.Is<TimestampType>());
  EXPECT_FALSE(map_type.Is<EnumType>());
  EXPECT_FALSE(map_type.Is<StructType>());
  EXPECT_FALSE(map_type.Is<ListType>());
  EXPECT_TRUE(map_type.Is<MapType>());
  EXPECT_FALSE(map_type.Is<TypeType>());
  EXPECT_FALSE(map_type.Is<UnknownType>());
}

TEST_P(TypeTest, TypeType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetTypeType()->kind(), Kind::kType);
  EXPECT_EQ(type_factory.GetTypeType()->name(), "type");
  EXPECT_FALSE(type_factory.GetTypeType().Is<NullType>());
  EXPECT_FALSE(type_factory.GetTypeType().Is<DynType>());
  EXPECT_FALSE(type_factory.GetTypeType().Is<AnyType>());
  EXPECT_FALSE(type_factory.GetTypeType().Is<BoolType>());
  EXPECT_FALSE(type_factory.GetTypeType().Is<IntType>());
  EXPECT_FALSE(type_factory.GetTypeType().Is<UintType>());
  EXPECT_FALSE(type_factory.GetTypeType().Is<DoubleType>());
  EXPECT_FALSE(type_factory.GetTypeType().Is<StringType>());
  EXPECT_FALSE(type_factory.GetTypeType().Is<BytesType>());
  EXPECT_FALSE(type_factory.GetTypeType().Is<DurationType>());
  EXPECT_FALSE(type_factory.GetTypeType().Is<TimestampType>());
  EXPECT_FALSE(type_factory.GetTypeType().Is<EnumType>());
  EXPECT_FALSE(type_factory.GetTypeType().Is<StructType>());
  EXPECT_FALSE(type_factory.GetTypeType().Is<ListType>());
  EXPECT_FALSE(type_factory.GetTypeType().Is<MapType>());
  EXPECT_TRUE(type_factory.GetTypeType().Is<TypeType>());
  EXPECT_FALSE(type_factory.GetTypeType().Is<UnknownType>());
}

TEST_P(TypeTest, UnknownType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetUnknownType()->kind(), Kind::kUnknown);
  EXPECT_EQ(type_factory.GetUnknownType()->name(), "*unknown*");
  EXPECT_FALSE(type_factory.GetUnknownType().Is<NullType>());
  EXPECT_FALSE(type_factory.GetUnknownType().Is<DynType>());
  EXPECT_FALSE(type_factory.GetUnknownType().Is<AnyType>());
  EXPECT_FALSE(type_factory.GetUnknownType().Is<BoolType>());
  EXPECT_FALSE(type_factory.GetUnknownType().Is<IntType>());
  EXPECT_FALSE(type_factory.GetUnknownType().Is<UintType>());
  EXPECT_FALSE(type_factory.GetUnknownType().Is<DoubleType>());
  EXPECT_FALSE(type_factory.GetUnknownType().Is<StringType>());
  EXPECT_FALSE(type_factory.GetUnknownType().Is<BytesType>());
  EXPECT_FALSE(type_factory.GetUnknownType().Is<DurationType>());
  EXPECT_FALSE(type_factory.GetUnknownType().Is<TimestampType>());
  EXPECT_FALSE(type_factory.GetUnknownType().Is<EnumType>());
  EXPECT_FALSE(type_factory.GetUnknownType().Is<StructType>());
  EXPECT_FALSE(type_factory.GetUnknownType().Is<ListType>());
  EXPECT_FALSE(type_factory.GetUnknownType().Is<MapType>());
  EXPECT_FALSE(type_factory.GetUnknownType().Is<TypeType>());
  EXPECT_TRUE(type_factory.GetUnknownType().Is<UnknownType>());
}

using EnumTypeTest = TypeTest;

TEST_P(EnumTypeTest, FindConstant) {
  TypeFactory type_factory(memory_manager());
  ASSERT_OK_AND_ASSIGN(auto enum_type,
                       type_factory.CreateEnumType<TestEnumType>());

  ASSERT_OK_AND_ASSIGN(auto value1,
                       enum_type->FindConstant(EnumType::ConstantId("VALUE1")));
  EXPECT_EQ(value1.name, "VALUE1");
  EXPECT_EQ(value1.number, 1);

  ASSERT_OK_AND_ASSIGN(value1,
                       enum_type->FindConstant(EnumType::ConstantId(1)));
  EXPECT_EQ(value1.name, "VALUE1");
  EXPECT_EQ(value1.number, 1);

  ASSERT_OK_AND_ASSIGN(auto value2,
                       enum_type->FindConstant(EnumType::ConstantId("VALUE2")));
  EXPECT_EQ(value2.name, "VALUE2");
  EXPECT_EQ(value2.number, 2);

  ASSERT_OK_AND_ASSIGN(value2,
                       enum_type->FindConstant(EnumType::ConstantId(2)));
  EXPECT_EQ(value2.name, "VALUE2");
  EXPECT_EQ(value2.number, 2);

  EXPECT_THAT(enum_type->FindConstant(EnumType::ConstantId("VALUE3")),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(enum_type->FindConstant(EnumType::ConstantId(3)),
              StatusIs(absl::StatusCode::kNotFound));
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
      auto field1,
      struct_type->FindField(type_manager, StructType::FieldId("bool_field")));
  EXPECT_EQ(field1.name, "bool_field");
  EXPECT_EQ(field1.number, 0);
  EXPECT_EQ(field1.type, type_manager.type_factory().GetBoolType());

  ASSERT_OK_AND_ASSIGN(
      field1, struct_type->FindField(type_manager, StructType::FieldId(0)));
  EXPECT_EQ(field1.name, "bool_field");
  EXPECT_EQ(field1.number, 0);
  EXPECT_EQ(field1.type, type_manager.type_factory().GetBoolType());

  ASSERT_OK_AND_ASSIGN(
      auto field2,
      struct_type->FindField(type_manager, StructType::FieldId("int_field")));
  EXPECT_EQ(field2.name, "int_field");
  EXPECT_EQ(field2.number, 1);
  EXPECT_EQ(field2.type, type_manager.type_factory().GetIntType());

  ASSERT_OK_AND_ASSIGN(
      field2, struct_type->FindField(type_manager, StructType::FieldId(1)));
  EXPECT_EQ(field2.name, "int_field");
  EXPECT_EQ(field2.number, 1);
  EXPECT_EQ(field2.type, type_manager.type_factory().GetIntType());

  ASSERT_OK_AND_ASSIGN(
      auto field3,
      struct_type->FindField(type_manager, StructType::FieldId("uint_field")));
  EXPECT_EQ(field3.name, "uint_field");
  EXPECT_EQ(field3.number, 2);
  EXPECT_EQ(field3.type, type_manager.type_factory().GetUintType());

  ASSERT_OK_AND_ASSIGN(
      field3, struct_type->FindField(type_manager, StructType::FieldId(2)));
  EXPECT_EQ(field3.name, "uint_field");
  EXPECT_EQ(field3.number, 2);
  EXPECT_EQ(field3.type, type_manager.type_factory().GetUintType());

  ASSERT_OK_AND_ASSIGN(
      auto field4, struct_type->FindField(type_manager,
                                          StructType::FieldId("double_field")));
  EXPECT_EQ(field4.name, "double_field");
  EXPECT_EQ(field4.number, 3);
  EXPECT_EQ(field4.type, type_manager.type_factory().GetDoubleType());

  ASSERT_OK_AND_ASSIGN(
      field4, struct_type->FindField(type_manager, StructType::FieldId(3)));
  EXPECT_EQ(field4.name, "double_field");
  EXPECT_EQ(field4.number, 3);
  EXPECT_EQ(field4.type, type_manager.type_factory().GetDoubleType());

  EXPECT_THAT(struct_type->FindField(type_manager,
                                     StructType::FieldId("missing_field")),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(struct_type->FindField(type_manager, StructType::FieldId(4)),
              StatusIs(absl::StatusCode::kNotFound));
}

INSTANTIATE_TEST_SUITE_P(StructTypeTest, StructTypeTest,
                         base_internal::MemoryManagerTestModeAll(),
                         base_internal::MemoryManagerTestModeName);

class DebugStringTest : public TypeTest {};

TEST_P(DebugStringTest, NullType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetNullType()->DebugString(), "null_type");
}

TEST_P(DebugStringTest, ErrorType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetErrorType()->DebugString(), "*error*");
}

TEST_P(DebugStringTest, DynType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetDynType()->DebugString(), "dyn");
}

TEST_P(DebugStringTest, AnyType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetAnyType()->DebugString(), "google.protobuf.Any");
}

TEST_P(DebugStringTest, BoolType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetBoolType()->DebugString(), "bool");
}

TEST_P(DebugStringTest, IntType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetIntType()->DebugString(), "int");
}

TEST_P(DebugStringTest, UintType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetUintType()->DebugString(), "uint");
}

TEST_P(DebugStringTest, DoubleType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetDoubleType()->DebugString(), "double");
}

TEST_P(DebugStringTest, StringType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetStringType()->DebugString(), "string");
}

TEST_P(DebugStringTest, BytesType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetBytesType()->DebugString(), "bytes");
}

TEST_P(DebugStringTest, DurationType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetDurationType()->DebugString(),
            "google.protobuf.Duration");
}

TEST_P(DebugStringTest, TimestampType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetTimestampType()->DebugString(),
            "google.protobuf.Timestamp");
}

TEST_P(DebugStringTest, EnumType) {
  TypeFactory type_factory(memory_manager());
  ASSERT_OK_AND_ASSIGN(auto enum_type,
                       type_factory.CreateEnumType<TestEnumType>());
  EXPECT_EQ(enum_type->DebugString(), "test_enum.TestEnum");
}

TEST_P(DebugStringTest, StructType) {
  TypeFactory type_factory(memory_manager());
  TypeManager type_manager(type_factory, TypeProvider::Builtin());
  ASSERT_OK_AND_ASSIGN(
      auto struct_type,
      type_manager.type_factory().CreateStructType<TestStructType>());
  EXPECT_EQ(struct_type->DebugString(), "test_struct.TestStruct");
}

TEST_P(DebugStringTest, ListType) {
  TypeFactory type_factory(memory_manager());
  ASSERT_OK_AND_ASSIGN(auto list_type,
                       type_factory.CreateListType(type_factory.GetBoolType()));
  EXPECT_EQ(list_type->DebugString(), "list(bool)");
}

TEST_P(DebugStringTest, MapType) {
  TypeFactory type_factory(memory_manager());
  ASSERT_OK_AND_ASSIGN(auto map_type,
                       type_factory.CreateMapType(type_factory.GetStringType(),
                                                  type_factory.GetBoolType()));
  EXPECT_EQ(map_type->DebugString(), "map(string, bool)");
}

TEST_P(DebugStringTest, TypeType) {
  TypeFactory type_factory(memory_manager());
  EXPECT_EQ(type_factory.GetTypeType()->DebugString(), "type");
}

INSTANTIATE_TEST_SUITE_P(DebugStringTest, DebugStringTest,
                         base_internal::MemoryManagerTestModeAll(),
                         base_internal::MemoryManagerTestModeName);

TEST_P(TypeTest, SupportsAbslHash) {
  TypeFactory type_factory(memory_manager());
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      Persistent<const Type>(type_factory.GetNullType()),
      Persistent<const Type>(type_factory.GetErrorType()),
      Persistent<const Type>(type_factory.GetDynType()),
      Persistent<const Type>(type_factory.GetAnyType()),
      Persistent<const Type>(type_factory.GetBoolType()),
      Persistent<const Type>(type_factory.GetIntType()),
      Persistent<const Type>(type_factory.GetUintType()),
      Persistent<const Type>(type_factory.GetDoubleType()),
      Persistent<const Type>(type_factory.GetStringType()),
      Persistent<const Type>(type_factory.GetBytesType()),
      Persistent<const Type>(type_factory.GetDurationType()),
      Persistent<const Type>(type_factory.GetTimestampType()),
      Persistent<const Type>(Must(type_factory.CreateEnumType<TestEnumType>())),
      Persistent<const Type>(
          Must(type_factory.CreateStructType<TestStructType>())),
      Persistent<const Type>(
          Must(type_factory.CreateListType(type_factory.GetBoolType()))),
      Persistent<const Type>(Must(type_factory.CreateMapType(
          type_factory.GetStringType(), type_factory.GetBoolType()))),
      Persistent<const Type>(type_factory.GetTypeType()),
      Persistent<const Type>(type_factory.GetUnknownType()),
  }));
}

INSTANTIATE_TEST_SUITE_P(TypeTest, TypeTest,
                         base_internal::MemoryManagerTestModeAll(),
                         base_internal::MemoryManagerTestModeName);

}  // namespace
}  // namespace cel
