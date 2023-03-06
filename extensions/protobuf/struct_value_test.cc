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

#include "extensions/protobuf/struct_value.h"

#include <utility>

#include "absl/types/optional.h"
#include "base/internal/memory_manager_testing.h"
#include "base/memory_manager.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "base/types/struct_type.h"
#include "base/value_factory.h"
#include "extensions/protobuf/type_provider.h"
#include "internal/testing.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include "google/protobuf/dynamic_message.h"

namespace cel::extensions {
namespace {

using testing::Eq;
using testing::EqualsProto;
using cel::internal::IsOkAndHolds;

using TestAllTypes = ::google::api::expr::test::v1::proto3::TestAllTypes;

template <typename... Types>
class BaseProtoStructValueTest
    : public testing::TestWithParam<
          std::tuple<cel::base_internal::MemoryManagerTestMode, Types...>> {
  using Base = testing::TestWithParam<
      std::tuple<base_internal::MemoryManagerTestMode, Types...>>;

 protected:
  void SetUp() override {
    if (std::get<0>(Base::GetParam()) ==
        base_internal::MemoryManagerTestMode::kArena) {
      arena_.emplace();
      proto_memory_manager_.emplace(&arena_.value());
      memory_manager_ = &proto_memory_manager_.value();
    } else {
      memory_manager_ = &MemoryManager::Global();
    }
  }

  void TearDown() override {
    memory_manager_ = nullptr;
    if (std::get<0>(Base::GetParam()) ==
        base_internal::MemoryManagerTestMode::kArena) {
      proto_memory_manager_.reset();
      arena_.reset();
    }
  }

  MemoryManager& memory_manager() const { return *memory_manager_; }

  const auto& test_case() const { return std::get<1>(Base::GetParam()); }

 private:
  absl::optional<google::protobuf::Arena> arena_;
  absl::optional<ProtoMemoryManager> proto_memory_manager_;
  MemoryManager* memory_manager_;
};

using ProtoStructValueTest = BaseProtoStructValueTest<>;

TestAllTypes CreateTestMessage() {
  TestAllTypes message;
  return message;
}

template <typename Func>
TestAllTypes CreateTestMessage(Func&& func) {
  TestAllTypes message;
  std::forward<Func>(func)(message);
  return message;
}

TEST_P(ProtoStructValueTest, BoolHasField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value_without,
      ProtoStructValue::Create(value_factory, CreateTestMessage()));
  EXPECT_THAT(value_without->HasField(type_manager,
                                      ProtoStructType::FieldId("single_bool")),
              IsOkAndHolds(Eq(false)));
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoStructValue::Create(value_factory,
                               CreateTestMessage([](TestAllTypes& message) {
                                 message.set_single_bool(true);
                               })));
  EXPECT_THAT(value_with->HasField(type_manager,
                                   ProtoStructType::FieldId("single_bool")),
              IsOkAndHolds(Eq(true)));
}

TEST_P(ProtoStructValueTest, Int32HasField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value_without,
      ProtoStructValue::Create(value_factory, CreateTestMessage()));
  EXPECT_THAT(value_without->HasField(type_manager,
                                      ProtoStructType::FieldId("single_int32")),
              IsOkAndHolds(Eq(false)));
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoStructValue::Create(value_factory,
                               CreateTestMessage([](TestAllTypes& message) {
                                 message.set_single_int32(1);
                               })));
  EXPECT_THAT(value_with->HasField(type_manager,
                                   ProtoStructType::FieldId("single_int32")),
              IsOkAndHolds(Eq(true)));
}

TEST_P(ProtoStructValueTest, Int64HasField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value_without,
      ProtoStructValue::Create(value_factory, CreateTestMessage()));
  EXPECT_THAT(value_without->HasField(type_manager,
                                      ProtoStructType::FieldId("single_int64")),
              IsOkAndHolds(Eq(false)));
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoStructValue::Create(value_factory,
                               CreateTestMessage([](TestAllTypes& message) {
                                 message.set_single_int64(1);
                               })));
  EXPECT_THAT(value_with->HasField(type_manager,
                                   ProtoStructType::FieldId("single_int64")),
              IsOkAndHolds(Eq(true)));
}

TEST_P(ProtoStructValueTest, Uint32HasField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value_without,
      ProtoStructValue::Create(value_factory, CreateTestMessage()));
  EXPECT_THAT(value_without->HasField(
                  type_manager, ProtoStructType::FieldId("single_uint32")),
              IsOkAndHolds(Eq(false)));
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoStructValue::Create(value_factory,
                               CreateTestMessage([](TestAllTypes& message) {
                                 message.set_single_uint32(1);
                               })));
  EXPECT_THAT(value_with->HasField(type_manager,
                                   ProtoStructType::FieldId("single_uint32")),
              IsOkAndHolds(Eq(true)));
}

TEST_P(ProtoStructValueTest, Uint64HasField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value_without,
      ProtoStructValue::Create(value_factory, CreateTestMessage()));
  EXPECT_THAT(value_without->HasField(
                  type_manager, ProtoStructType::FieldId("single_uint64")),
              IsOkAndHolds(Eq(false)));
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoStructValue::Create(value_factory,
                               CreateTestMessage([](TestAllTypes& message) {
                                 message.set_single_uint64(1);
                               })));
  EXPECT_THAT(value_with->HasField(type_manager,
                                   ProtoStructType::FieldId("single_uint64")),
              IsOkAndHolds(Eq(true)));
}

TEST_P(ProtoStructValueTest, FloatHasField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value_without,
      ProtoStructValue::Create(value_factory, CreateTestMessage()));
  EXPECT_THAT(value_without->HasField(type_manager,
                                      ProtoStructType::FieldId("single_float")),
              IsOkAndHolds(Eq(false)));
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoStructValue::Create(value_factory,
                               CreateTestMessage([](TestAllTypes& message) {
                                 message.set_single_float(1.0);
                               })));
  EXPECT_THAT(value_with->HasField(type_manager,
                                   ProtoStructType::FieldId("single_float")),
              IsOkAndHolds(Eq(true)));
}

TEST_P(ProtoStructValueTest, DoubleHasField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value_without,
      ProtoStructValue::Create(value_factory, CreateTestMessage()));
  EXPECT_THAT(value_without->HasField(
                  type_manager, ProtoStructType::FieldId("single_double")),
              IsOkAndHolds(Eq(false)));
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoStructValue::Create(value_factory,
                               CreateTestMessage([](TestAllTypes& message) {
                                 message.set_single_double(1.0);
                               })));
  EXPECT_THAT(value_with->HasField(type_manager,
                                   ProtoStructType::FieldId("single_double")),
              IsOkAndHolds(Eq(true)));
}

TEST_P(ProtoStructValueTest, BytesHasField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value_without,
      ProtoStructValue::Create(value_factory, CreateTestMessage()));
  EXPECT_THAT(value_without->HasField(type_manager,
                                      ProtoStructType::FieldId("single_bytes")),
              IsOkAndHolds(Eq(false)));
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoStructValue::Create(value_factory,
                               CreateTestMessage([](TestAllTypes& message) {
                                 message.set_single_bytes("foo");
                               })));
  EXPECT_THAT(value_with->HasField(type_manager,
                                   ProtoStructType::FieldId("single_bytes")),
              IsOkAndHolds(Eq(true)));
}

TEST_P(ProtoStructValueTest, StringHasField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value_without,
      ProtoStructValue::Create(value_factory, CreateTestMessage()));
  EXPECT_THAT(value_without->HasField(
                  type_manager, ProtoStructType::FieldId("single_string")),
              IsOkAndHolds(Eq(false)));
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoStructValue::Create(value_factory,
                               CreateTestMessage([](TestAllTypes& message) {
                                 message.set_single_string("foo");
                               })));
  EXPECT_THAT(value_with->HasField(type_manager,
                                   ProtoStructType::FieldId("single_string")),
              IsOkAndHolds(Eq(true)));
}

TEST_P(ProtoStructValueTest, EnumHasField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value_without,
      ProtoStructValue::Create(value_factory, CreateTestMessage()));
  EXPECT_THAT(value_without->HasField(
                  type_manager, ProtoStructType::FieldId("standalone_enum")),
              IsOkAndHolds(Eq(false)));
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoStructValue::Create(value_factory,
                               CreateTestMessage([](TestAllTypes& message) {
                                 message.set_standalone_enum(TestAllTypes::BAR);
                               })));
  EXPECT_THAT(value_with->HasField(type_manager,
                                   ProtoStructType::FieldId("standalone_enum")),
              IsOkAndHolds(Eq(true)));
}

TEST_P(ProtoStructValueTest, StructHasField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value_without,
      ProtoStructValue::Create(value_factory, CreateTestMessage()));
  EXPECT_THAT(value_without->HasField(
                  type_manager, ProtoStructType::FieldId("standalone_message")),
              IsOkAndHolds(Eq(false)));
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoStructValue::Create(value_factory,
                               CreateTestMessage([](TestAllTypes& message) {
                                 message.mutable_standalone_message();
                               })));
  EXPECT_THAT(value_with->HasField(
                  type_manager, ProtoStructType::FieldId("standalone_message")),
              IsOkAndHolds(Eq(true)));
}

TEST_P(ProtoStructValueTest, BoolGetField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value_without,
      ProtoStructValue::Create(value_factory, CreateTestMessage()));
  ASSERT_OK_AND_ASSIGN(
      auto field, value_without->GetField(
                      value_factory, ProtoStructType::FieldId("single_bool")));
  EXPECT_FALSE(field.As<BoolValue>()->value());
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoStructValue::Create(value_factory,
                               CreateTestMessage([](TestAllTypes& message) {
                                 message.set_single_bool(true);
                               })));
  ASSERT_OK_AND_ASSIGN(
      field, value_with->GetField(value_factory,
                                  ProtoStructType::FieldId("single_bool")));
  EXPECT_TRUE(field.As<BoolValue>()->value());
}

TEST_P(ProtoStructValueTest, Int32GetField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value_without,
      ProtoStructValue::Create(value_factory, CreateTestMessage()));
  ASSERT_OK_AND_ASSIGN(
      auto field, value_without->GetField(
                      value_factory, ProtoStructType::FieldId("single_int32")));
  EXPECT_EQ(field.As<IntValue>()->value(), 0);
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoStructValue::Create(value_factory,
                               CreateTestMessage([](TestAllTypes& message) {
                                 message.set_single_int32(1);
                               })));
  ASSERT_OK_AND_ASSIGN(
      field, value_with->GetField(value_factory,
                                  ProtoStructType::FieldId("single_int32")));
  EXPECT_EQ(field.As<IntValue>()->value(), 1);
}

TEST_P(ProtoStructValueTest, Int64GetField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value_without,
      ProtoStructValue::Create(value_factory, CreateTestMessage()));
  ASSERT_OK_AND_ASSIGN(
      auto field, value_without->GetField(
                      value_factory, ProtoStructType::FieldId("single_int64")));
  EXPECT_EQ(field.As<IntValue>()->value(), 0);
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoStructValue::Create(value_factory,
                               CreateTestMessage([](TestAllTypes& message) {
                                 message.set_single_int64(1);
                               })));
  ASSERT_OK_AND_ASSIGN(
      field, value_with->GetField(value_factory,
                                  ProtoStructType::FieldId("single_int64")));
  EXPECT_EQ(field.As<IntValue>()->value(), 1);
}

TEST_P(ProtoStructValueTest, Uint32GetField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value_without,
      ProtoStructValue::Create(value_factory, CreateTestMessage()));
  ASSERT_OK_AND_ASSIGN(
      auto field,
      value_without->GetField(value_factory,
                              ProtoStructType::FieldId("single_uint32")));
  EXPECT_EQ(field.As<UintValue>()->value(), 0);
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoStructValue::Create(value_factory,
                               CreateTestMessage([](TestAllTypes& message) {
                                 message.set_single_uint32(1);
                               })));
  ASSERT_OK_AND_ASSIGN(
      field, value_with->GetField(value_factory,
                                  ProtoStructType::FieldId("single_uint32")));
  EXPECT_EQ(field.As<UintValue>()->value(), 1);
}

TEST_P(ProtoStructValueTest, Uint64GetField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value_without,
      ProtoStructValue::Create(value_factory, CreateTestMessage()));
  ASSERT_OK_AND_ASSIGN(
      auto field,
      value_without->GetField(value_factory,
                              ProtoStructType::FieldId("single_uint64")));
  EXPECT_EQ(field.As<UintValue>()->value(), 0);
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoStructValue::Create(value_factory,
                               CreateTestMessage([](TestAllTypes& message) {
                                 message.set_single_uint64(1);
                               })));
  ASSERT_OK_AND_ASSIGN(
      field, value_with->GetField(value_factory,
                                  ProtoStructType::FieldId("single_uint64")));
  EXPECT_EQ(field.As<UintValue>()->value(), 1);
}

TEST_P(ProtoStructValueTest, FloatGetField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value_without,
      ProtoStructValue::Create(value_factory, CreateTestMessage()));
  ASSERT_OK_AND_ASSIGN(
      auto field, value_without->GetField(
                      value_factory, ProtoStructType::FieldId("single_float")));
  EXPECT_EQ(field.As<DoubleValue>()->value(), 0);
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoStructValue::Create(value_factory,
                               CreateTestMessage([](TestAllTypes& message) {
                                 message.set_single_float(1.0);
                               })));
  ASSERT_OK_AND_ASSIGN(
      field, value_with->GetField(value_factory,
                                  ProtoStructType::FieldId("single_float")));
  EXPECT_EQ(field.As<DoubleValue>()->value(), 1);
}

TEST_P(ProtoStructValueTest, DoubleGetField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value_without,
      ProtoStructValue::Create(value_factory, CreateTestMessage()));
  ASSERT_OK_AND_ASSIGN(
      auto field,
      value_without->GetField(value_factory,
                              ProtoStructType::FieldId("single_double")));
  EXPECT_EQ(field.As<DoubleValue>()->value(), 0);
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoStructValue::Create(value_factory,
                               CreateTestMessage([](TestAllTypes& message) {
                                 message.set_single_double(1.0);
                               })));
  ASSERT_OK_AND_ASSIGN(
      field, value_with->GetField(value_factory,
                                  ProtoStructType::FieldId("single_double")));
  EXPECT_EQ(field.As<DoubleValue>()->value(), 1);
}

TEST_P(ProtoStructValueTest, BytesGetField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value_without,
      ProtoStructValue::Create(value_factory, CreateTestMessage()));
  ASSERT_OK_AND_ASSIGN(
      auto field, value_without->GetField(
                      value_factory, ProtoStructType::FieldId("single_bytes")));
  EXPECT_EQ(field.As<BytesValue>()->ToString(), "");
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoStructValue::Create(value_factory,
                               CreateTestMessage([](TestAllTypes& message) {
                                 message.set_single_bytes("foo");
                               })));
  ASSERT_OK_AND_ASSIGN(
      field, value_with->GetField(value_factory,
                                  ProtoStructType::FieldId("single_bytes")));
  EXPECT_EQ(field.As<BytesValue>()->ToString(), "foo");
}

TEST_P(ProtoStructValueTest, StringGetField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value_without,
      ProtoStructValue::Create(value_factory, CreateTestMessage()));
  ASSERT_OK_AND_ASSIGN(
      auto field,
      value_without->GetField(value_factory,
                              ProtoStructType::FieldId("single_string")));
  EXPECT_EQ(field.As<StringValue>()->ToString(), "");
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoStructValue::Create(value_factory,
                               CreateTestMessage([](TestAllTypes& message) {
                                 message.set_single_string("foo");
                               })));
  ASSERT_OK_AND_ASSIGN(
      field, value_with->GetField(value_factory,
                                  ProtoStructType::FieldId("single_string")));
  EXPECT_EQ(field.As<StringValue>()->ToString(), "foo");
}

TEST_P(ProtoStructValueTest, EnumGetField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value_without,
      ProtoStructValue::Create(value_factory, CreateTestMessage()));
  ASSERT_OK_AND_ASSIGN(
      auto field,
      value_without->GetField(value_factory,
                              ProtoStructType::FieldId("standalone_enum")));
  EXPECT_EQ(field.As<EnumValue>()->number(), 0);
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoStructValue::Create(value_factory,
                               CreateTestMessage([](TestAllTypes& message) {
                                 message.set_standalone_enum(TestAllTypes::BAR);
                               })));
  ASSERT_OK_AND_ASSIGN(
      field, value_with->GetField(value_factory,
                                  ProtoStructType::FieldId("standalone_enum")));
  EXPECT_EQ(field.As<EnumValue>()->number(), 1);
}

TEST_P(ProtoStructValueTest, StructGetField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value_without,
      ProtoStructValue::Create(value_factory, CreateTestMessage()));
  ASSERT_OK_AND_ASSIGN(
      auto field,
      value_without->GetField(value_factory,
                              ProtoStructType::FieldId("standalone_message")));
  EXPECT_THAT(*field.As<ProtoStructValue>()->value(),
              EqualsProto(CreateTestMessage().standalone_message()));
  ASSERT_OK_AND_ASSIGN(
      auto value_with,
      ProtoStructValue::Create(
          value_factory, CreateTestMessage([](TestAllTypes& message) {
            message.mutable_standalone_message()->set_bb(1);
          })));
  ASSERT_OK_AND_ASSIGN(
      field, value_with->GetField(value_factory, ProtoStructType::FieldId(
                                                     "standalone_message")));
  TestAllTypes::NestedMessage expected =
      CreateTestMessage([](TestAllTypes& message) {
        message.mutable_standalone_message()->set_bb(1);
      }).standalone_message();
  TestAllTypes::NestedMessage scratch;
  EXPECT_THAT(*field.As<ProtoStructValue>()->value(), EqualsProto(expected));
  EXPECT_THAT(*field.As<ProtoStructValue>()->value(scratch),
              EqualsProto(expected));
  google::protobuf::Arena arena;
  EXPECT_THAT(*field.As<ProtoStructValue>()->value(arena),
              EqualsProto(expected));
}

TEST_P(ProtoStructValueTest, DebugString) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoStructValue::Create(
          value_factory, CreateTestMessage([](TestAllTypes& message) {
            message.set_single_bool(true);
            message.set_single_int32(1);
            message.set_single_int64(1);
            message.set_single_uint32(1);
            message.set_single_uint64(1);
            message.set_single_float(1.0);
            message.set_single_double(1.0);
            message.set_single_bytes("foo");
            message.set_single_string("foo");
            message.set_standalone_enum(TestAllTypes::BAR);
            message.mutable_standalone_message()->set_bb(1);
          })));
  EXPECT_EQ(
      value->DebugString(),
      "google.api.expr.test.v1.proto3.TestAllTypes{"
      "single_int32: 1, single_int64: 1, single_uint32: 1u, single_uint64: 1u, "
      "single_float: 1.0, single_double: 1.0, single_bool: true, "
      "single_string: "
      "\"foo\", single_bytes: b\"foo\", "
      "standalone_message: "
      "google.api.expr.test.v1.proto3.TestAllTypes.NestedMessage{bb: 1}, "
      "standalone_enum: "
      "google.api.expr.test.v1.proto3.TestAllTypes.NestedEnum.BAR}");
}

TEST_P(ProtoStructValueTest, StaticValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  TestAllTypes message = CreateTestMessage();
  ASSERT_OK_AND_ASSIGN(auto value,
                       ProtoStructValue::Create(value_factory, message));
  EXPECT_TRUE(value.Is<ProtoStructValue>());
  TestAllTypes scratch;
  EXPECT_THAT(*value->value(scratch), EqualsProto(message));
}

TEST_P(ProtoStructValueTest, DynamicLValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  TestAllTypes message = CreateTestMessage();
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoStructValue::Create(value_factory,
                               static_cast<const google::protobuf::Message&>(message)));
  EXPECT_TRUE(value.Is<ProtoStructValue>());
  TestAllTypes scratch;
  EXPECT_THAT(*value->value(scratch), EqualsProto(message));
}

TEST_P(ProtoStructValueTest, DynamicRValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoStructValue::Create(
          value_factory, static_cast<google::protobuf::Message&&>(CreateTestMessage())));
  EXPECT_TRUE(value.Is<ProtoStructValue>());
}

void BuildDescriptorDatabase(google::protobuf::SimpleDescriptorDatabase* database) {
  google::protobuf::FileDescriptorProto proto;
  TestAllTypes::descriptor()->file()->CopyTo(&proto);
  ASSERT_TRUE(database->Add(proto));
  for (int index = 0;
       index < TestAllTypes::descriptor()->file()->dependency_count();
       index++) {
    proto.Clear();
    TestAllTypes::descriptor()->file()->dependency(index)->CopyTo(&proto);
    ASSERT_TRUE(database->Add(proto));
  }
}

TEST_P(ProtoStructValueTest, DynamicLValueDifferentDescriptors) {
  TypeFactory type_factory(memory_manager());
  google::protobuf::SimpleDescriptorDatabase database;
  BuildDescriptorDatabase(&database);
  google::protobuf::DescriptorPool pool(&database);
  google::protobuf::DynamicMessageFactory factory(&pool);
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  const auto* desc =
      pool.FindMessageTypeByName(TestAllTypes::descriptor()->full_name());
  ASSERT_TRUE(desc != nullptr);
  const auto* prototype = factory.GetPrototype(desc);
  ASSERT_TRUE(prototype != nullptr);
  ASSERT_OK_AND_ASSIGN(auto value,
                       ProtoStructValue::Create(value_factory, *prototype));
  EXPECT_TRUE(value.Is<ProtoStructValue>());
}

TEST_P(ProtoStructValueTest, DynamicRValueDifferentDescriptors) {
  TypeFactory type_factory(memory_manager());
  google::protobuf::SimpleDescriptorDatabase database;
  BuildDescriptorDatabase(&database);
  google::protobuf::DescriptorPool pool(&database);
  google::protobuf::DynamicMessageFactory factory(&pool);
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  const auto* desc =
      pool.FindMessageTypeByName(TestAllTypes::descriptor()->full_name());
  ASSERT_TRUE(desc != nullptr);
  const auto* prototype = factory.GetPrototype(desc);
  ASSERT_TRUE(prototype != nullptr);
  auto* message = prototype->New();
  ASSERT_OK_AND_ASSIGN(
      auto value, ProtoStructValue::Create(value_factory, std::move(*message)));
  delete message;
  EXPECT_TRUE(value.Is<ProtoStructValue>());
}

INSTANTIATE_TEST_SUITE_P(ProtoStructValueTest, ProtoStructValueTest,
                         cel::base_internal::MemoryManagerTestModeAll());

}  // namespace
}  // namespace cel::extensions
