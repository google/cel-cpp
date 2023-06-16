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

#include "extensions/protobuf/struct_type.h"

#include <set>
#include <utility>

#include "google/protobuf/type.pb.h"
#include "absl/container/flat_hash_set.h"
#include "absl/functional/function_ref.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/time/time.h"
#include "base/internal/memory_manager_testing.h"
#include "base/memory.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "base/types/list_type.h"
#include "base/types/map_type.h"
#include "base/value_factory.h"
#include "base/values/list_value_builder.h"
#include "base/values/map_value_builder.h"
#include "base/values/struct_value_builder.h"
#include "extensions/protobuf/internal/testing.h"
#include "extensions/protobuf/struct_value.h"
#include "extensions/protobuf/type.h"
#include "extensions/protobuf/type_provider.h"
#include "extensions/protobuf/value.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "testutil/util.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"
#include "google/protobuf/text_format.h"

namespace cel::extensions {
namespace {

using google::api::expr::testutil::EqualsProto;
using cel::internal::StatusIs;

template <typename T>
T ParseTextOrDie(absl::string_view text) {
  T proto;
  ABSL_CHECK(google::protobuf::TextFormat::ParseFromString(text, &proto));
  return proto;
}

using TestAllTypes = google::api::expr::test::v1::proto3::TestAllTypes;

using ProtoStructTypeTest = ProtoTest<>;

TEST_P(ProtoStructTypeTest, CreateStatically) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type, ProtoType::Resolve<google::protobuf::Field>(type_manager));
  EXPECT_TRUE(type->Is<StructType>());
  EXPECT_TRUE(type->Is<ProtoStructType>());
  EXPECT_EQ(type->kind(), Kind::kStruct);
  EXPECT_EQ(type->name(), "google.protobuf.Field");
  EXPECT_EQ(&type->descriptor(), google::protobuf::Field::descriptor());
}

TEST_P(ProtoStructTypeTest, CreateDynamically) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type,
      ProtoType::Resolve(type_manager, *google::protobuf::Field::descriptor()));
  EXPECT_TRUE(type->Is<StructType>());
  EXPECT_TRUE(type->Is<ProtoStructType>());
  EXPECT_EQ(type->kind(), Kind::kStruct);
  EXPECT_EQ(type->name(), "google.protobuf.Field");
  EXPECT_EQ(&type.As<ProtoStructType>()->descriptor(),
            google::protobuf::Field::descriptor());
}

TEST_P(ProtoStructTypeTest, FindFieldByName) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type, ProtoType::Resolve<google::protobuf::Field>(type_manager));
  ASSERT_OK_AND_ASSIGN(auto field,
                       type->FindFieldByName(type_manager, "default_value"));
  ASSERT_TRUE(field.has_value());
  EXPECT_EQ(field->number, 11);
  EXPECT_EQ(field->name, "default_value");
  EXPECT_EQ(field->type, type_factory.GetStringType());
}

TEST_P(ProtoStructTypeTest, FindFieldByNumber) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type, ProtoType::Resolve<google::protobuf::Field>(type_manager));
  ASSERT_OK_AND_ASSIGN(auto field, type->FindFieldByNumber(type_manager, 11));
  ASSERT_TRUE(field.has_value());
  EXPECT_EQ(field->number, 11);
  EXPECT_EQ(field->name, "default_value");
  EXPECT_EQ(field->type, type_factory.GetStringType());
}

TEST_P(ProtoStructTypeTest, EnumField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type, ProtoType::Resolve<google::protobuf::Field>(type_manager));
  ASSERT_OK_AND_ASSIGN(auto field,
                       type->FindFieldByName(type_manager, "cardinality"));
  ASSERT_TRUE(field.has_value());
  EXPECT_TRUE(field->type->Is<EnumType>());
  EXPECT_EQ(field->type->name(), "google.protobuf.Field.Cardinality");
}

TEST_P(ProtoStructTypeTest, BoolField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type, ProtoType::Resolve<google::protobuf::Field>(type_manager));
  ASSERT_OK_AND_ASSIGN(auto field,
                       type->FindFieldByName(type_manager, "packed"));
  ASSERT_TRUE(field.has_value());
  EXPECT_EQ(field->type, type_factory.GetBoolType());
}

TEST_P(ProtoStructTypeTest, IntField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type, ProtoType::Resolve<google::protobuf::Field>(type_manager));
  ASSERT_OK_AND_ASSIGN(auto field,
                       type->FindFieldByName(type_manager, "oneof_index"));
  ASSERT_TRUE(field.has_value());
  EXPECT_EQ(field->type, type_factory.GetIntType());
}

TEST_P(ProtoStructTypeTest, StringListField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type, ProtoType::Resolve<google::protobuf::Type>(type_manager));
  ASSERT_OK_AND_ASSIGN(auto field,
                       type->FindFieldByName(type_manager, "oneofs"));
  ASSERT_TRUE(field.has_value());
  EXPECT_TRUE(field->type->Is<ListType>());
  EXPECT_EQ(field->type.As<ListType>()->element(),
            type_factory.GetStringType());
}

TEST_P(ProtoStructTypeTest, StructListField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(
      auto type, ProtoType::Resolve<google::protobuf::Field>(type_manager));
  ASSERT_OK_AND_ASSIGN(auto field,
                       type->FindFieldByName(type_manager, "options"));
  ASSERT_TRUE(field.has_value());
  EXPECT_TRUE(field->type->Is<ListType>());
  EXPECT_EQ(field->type.As<ListType>()->element()->name(),
            "google.protobuf.Option");
}

TEST_P(ProtoStructTypeTest, MapField) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(auto type,
                       ProtoType::Resolve<TestAllTypes>(type_manager));
  ASSERT_OK_AND_ASSIGN(
      auto field, type->FindFieldByName(type_manager, "map_string_string"));
  ASSERT_TRUE(field.has_value());
  EXPECT_TRUE(field->type->Is<MapType>());
  EXPECT_EQ(field->type.As<MapType>()->key(), type_factory.GetStringType());
  EXPECT_EQ(field->type.As<MapType>()->value(), type_factory.GetStringType());
}

using ::cel::base_internal::FieldIdFactory;

TEST_P(ProtoStructTypeTest, NewFieldIteratorIds) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(auto type,
                       ProtoType::Resolve<TestAllTypes>(type_manager));
  ASSERT_OK_AND_ASSIGN(auto iterator, type->NewFieldIterator(memory_manager()));
  std::set<StructType::FieldId> actual_ids;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(auto id, iterator->NextId(type_manager));
    actual_ids.insert(id);
  }
  EXPECT_THAT(iterator->NextId(type_manager),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  std::set<StructType::FieldId> expected_ids;
  const auto* const descriptor = TestAllTypes::descriptor();
  for (int index = 0; index < descriptor->field_count(); ++index) {
    expected_ids.insert(
        FieldIdFactory::Make(descriptor->field(index)->number()));
  }
  EXPECT_EQ(actual_ids, expected_ids);
}

TEST_P(ProtoStructTypeTest, NewFieldIteratorName) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(auto type,
                       ProtoType::Resolve<TestAllTypes>(type_manager));
  ASSERT_OK_AND_ASSIGN(auto iterator, type->NewFieldIterator(memory_manager()));
  std::set<absl::string_view> actual_names;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(auto name, iterator->NextName(type_manager));
    actual_names.insert(name);
  }
  EXPECT_THAT(iterator->NextName(type_manager),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  std::set<absl::string_view> expected_names;
  const auto* const descriptor = TestAllTypes::descriptor();
  for (int index = 0; index < descriptor->field_count(); ++index) {
    expected_names.insert(descriptor->field(index)->name());
  }
  EXPECT_EQ(actual_names, expected_names);
}

TEST_P(ProtoStructTypeTest, NewFieldIteratorNumbers) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(auto type,
                       ProtoType::Resolve<TestAllTypes>(type_manager));
  ASSERT_OK_AND_ASSIGN(auto iterator, type->NewFieldIterator(memory_manager()));
  std::set<int64_t> actual_numbers;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(auto number, iterator->NextNumber(type_manager));
    actual_numbers.insert(number);
  }
  EXPECT_THAT(iterator->NextNumber(type_manager),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  std::set<int64_t> expected_numbers;
  const auto* const descriptor = TestAllTypes::descriptor();
  for (int index = 0; index < descriptor->field_count(); ++index) {
    expected_numbers.insert(descriptor->field(index)->number());
  }
  EXPECT_EQ(actual_numbers, expected_numbers);
}

TEST_P(ProtoStructTypeTest, NewFieldIteratorTypes) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ASSERT_OK_AND_ASSIGN(auto type,
                       ProtoType::Resolve<TestAllTypes>(type_manager));
  ASSERT_OK_AND_ASSIGN(auto iterator, type->NewFieldIterator(memory_manager()));
  absl::flat_hash_set<Handle<Type>> actual_types;
  while (iterator->HasNext()) {
    ASSERT_OK_AND_ASSIGN(auto type, iterator->NextType(type_manager));
    actual_types.insert(std::move(type));
  }
  EXPECT_THAT(iterator->NextType(type_manager),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  // We cannot really test actual_types, as hand translating TestAllTypes would
  // be obnoxious. Otherwise we would simply be testing the same logic against
  // itself, which would not be useful.
}

TEST_P(ProtoStructTypeTest, NewValueBuilder) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto type,
                       ProtoType::Resolve<TestAllTypes>(type_manager));
  ASSERT_OK_AND_ASSIGN(auto builder, type->NewValueBuilder(value_factory));
  ASSERT_OK_AND_ASSIGN(auto value, std::move(*builder).Build());
  EXPECT_THAT(*value->As<ProtoStructValue>().value(),
              EqualsProto(TestAllTypes::default_instance()));
}

INSTANTIATE_TEST_SUITE_P(ProtoStructTypeTest, ProtoStructTypeTest,
                         cel::base_internal::MemoryManagerTestModeAll(),
                         cel::base_internal::MemoryManagerTestModeTupleName);

using ProtoStructValueBuilderTest = ProtoTest<>;

void TestProtoStructValueBuilderImpl(
    MemoryManager& memory_manager, StructType::FieldId id,
    absl::FunctionRef<absl::StatusOr<Handle<Value>>(ValueFactory&)> value_maker,
    absl::string_view text_proto) {
  TypeFactory type_factory(memory_manager);
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(auto type,
                       ProtoType::Resolve<TestAllTypes>(type_manager));
  ASSERT_OK_AND_ASSIGN(auto builder, type->NewValueBuilder(value_factory));
  ASSERT_OK_AND_ASSIGN(auto field, value_maker(value_factory));
  EXPECT_THAT(builder->SetField(
                  id, value_factory.CreateErrorValue(absl::CancelledError())),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_OK(builder->SetField(id, field));
  ASSERT_OK_AND_ASSIGN(auto value, std::move(*builder).Build());
  ASSERT_TRUE(value->Is<ProtoStructValue>());
  EXPECT_THAT(*value->As<ProtoStructValue>().value(),
              EqualsProto(ParseTextOrDie<TestAllTypes>(text_proto)));
}

void TestProtoStructValueBuilderByNumber(
    MemoryManager& memory_manager, int64_t field,
    absl::FunctionRef<absl::StatusOr<Handle<Value>>(ValueFactory&)> value_maker,
    absl::string_view text_proto) {
  TestProtoStructValueBuilderImpl(memory_manager,
                                  base_internal::FieldIdFactory::Make(field),
                                  value_maker, text_proto);
}

void TestProtoStructValueBuilderByName(
    MemoryManager& memory_manager, absl::string_view field,
    absl::FunctionRef<absl::StatusOr<Handle<Value>>(ValueFactory&)> value_maker,
    absl::string_view text_proto) {
  TestProtoStructValueBuilderImpl(memory_manager,
                                  base_internal::FieldIdFactory::Make(field),
                                  value_maker, text_proto);
}

void TestProtoStructValueBuilder(
    MemoryManager& memory_manager, absl::string_view field,
    absl::FunctionRef<absl::StatusOr<Handle<Value>>(ValueFactory&)> value_maker,
    absl::string_view text_proto) {
  ASSERT_NO_FATAL_FAILURE(TestProtoStructValueBuilderByName(
      memory_manager, field, value_maker, text_proto));
  ASSERT_NO_FATAL_FAILURE(TestProtoStructValueBuilderByNumber(
      memory_manager,
      TestAllTypes::descriptor()->FindFieldByName(field)->number(), value_maker,
      text_proto));
}

#define TEST_PROTO_STRUCT_VALUE_BUILDER(...) \
  ASSERT_NO_FATAL_FAILURE(TestProtoStructValueBuilder(__VA_ARGS__))

TEST_P(ProtoStructValueBuilderTest, Null) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "optional_null_value",
      [](ValueFactory& value_factory) { return value_factory.GetNullValue(); },
      R"pb(optional_null_value: 0)pb");
}

TEST_P(ProtoStructValueBuilderTest, Bool) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_bool",
      [](ValueFactory& value_factory) {
        return value_factory.CreateBoolValue(true);
      },
      R"pb(single_bool: true)pb");
}

TEST_P(ProtoStructValueBuilderTest, Int) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_int32",
      [](ValueFactory& value_factory) {
        return value_factory.CreateIntValue(1);
      },
      R"pb(single_int32: 1)pb");
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_int64",
      [](ValueFactory& value_factory) {
        return value_factory.CreateIntValue(1);
      },
      R"pb(single_int64: 1)pb");
}

TEST_P(ProtoStructValueBuilderTest, Uint) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_uint32",
      [](ValueFactory& value_factory) {
        return value_factory.CreateUintValue(1);
      },
      R"pb(single_uint32: 1)pb");
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_uint64",
      [](ValueFactory& value_factory) {
        return value_factory.CreateUintValue(1);
      },
      R"pb(single_uint64: 1)pb");
}

TEST_P(ProtoStructValueBuilderTest, Double) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_float",
      [](ValueFactory& value_factory) {
        return value_factory.CreateDoubleValue(1.0);
      },
      R"pb(single_float: 1)pb");
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_double",
      [](ValueFactory& value_factory) {
        return value_factory.CreateDoubleValue(1.0);
      },
      R"pb(single_double: 1)pb");
}

TEST_P(ProtoStructValueBuilderTest, String) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_string",
      [](ValueFactory& value_factory) {
        return value_factory.CreateStringValue("foo");
      },
      R"pb(single_string: "foo")pb");
}

TEST_P(ProtoStructValueBuilderTest, Bytes) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_bytes",
      [](ValueFactory& value_factory) {
        return value_factory.CreateBytesValue("foo");
      },
      R"pb(single_bytes: "foo")pb");
}

TEST_P(ProtoStructValueBuilderTest, Enum) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "standalone_enum",
      [](ValueFactory& value_factory) {
        return ProtoValue::Create(value_factory, TestAllTypes::BAR);
      },
      R"pb(standalone_enum: 1)pb");
}

TEST_P(ProtoStructValueBuilderTest, Struct) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "standalone_message",
      [](ValueFactory& value_factory) {
        TestAllTypes::NestedMessage value;
        value.set_bb(1);
        return ProtoValue::Create(value_factory, value);
      },
      R"pb(standalone_message: { bb: 1 })pb");
}

TEST_P(ProtoStructValueBuilderTest, Duration) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_duration",
      [](ValueFactory& value_factory) {
        return value_factory.CreateDurationValue(absl::Seconds(1) +
                                                 absl::Nanoseconds(1));
      },
      R"pb(single_duration: { seconds: 1, nanos: 1 })pb");
}

TEST_P(ProtoStructValueBuilderTest, Timestamp) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_timestamp",
      [](ValueFactory& value_factory) {
        return value_factory.CreateTimestampValue(
            absl::UnixEpoch() + absl::Seconds(1) + absl::Nanoseconds(1));
      },
      R"pb(single_timestamp: { seconds: 1, nanos: 1 })pb");
}

TEST_P(ProtoStructValueBuilderTest, BoolWrapper) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_bool_wrapper",
      [](ValueFactory& value_factory) {
        return value_factory.CreateBoolValue(true);
      },
      R"pb(single_bool_wrapper: { value: true })pb");
}

TEST_P(ProtoStructValueBuilderTest, IntWrapper) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_int32_wrapper",
      [](ValueFactory& value_factory) {
        return value_factory.CreateIntValue(1);
      },
      R"pb(single_int32_wrapper: { value: 1 })pb");
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_int64_wrapper",
      [](ValueFactory& value_factory) {
        return value_factory.CreateIntValue(1);
      },
      R"pb(single_int64_wrapper: { value: 1 })pb");
}

TEST_P(ProtoStructValueBuilderTest, UintWrapper) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_uint32_wrapper",
      [](ValueFactory& value_factory) {
        return value_factory.CreateUintValue(1);
      },
      R"pb(single_uint32_wrapper: { value: 1 })pb");
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_uint64_wrapper",
      [](ValueFactory& value_factory) {
        return value_factory.CreateUintValue(1);
      },
      R"pb(single_uint64_wrapper: { value: 1 })pb");
}

TEST_P(ProtoStructValueBuilderTest, DoubleWrapper) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_float_wrapper",
      [](ValueFactory& value_factory) {
        return value_factory.CreateDoubleValue(1.0);
      },
      R"pb(single_float_wrapper: { value: 1 })pb");
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_double_wrapper",
      [](ValueFactory& value_factory) {
        return value_factory.CreateDoubleValue(1.0);
      },
      R"pb(single_double_wrapper: { value: 1 })pb");
}

TEST_P(ProtoStructValueBuilderTest, StringWrapper) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_string_wrapper",
      [](ValueFactory& value_factory) {
        return value_factory.CreateStringValue("foo");
      },
      R"pb(single_string_wrapper: { value: "foo" })pb");
}

TEST_P(ProtoStructValueBuilderTest, BytesWrapper) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_bytes_wrapper",
      [](ValueFactory& value_factory) {
        return value_factory.CreateBytesValue("foo");
      },
      R"pb(single_bytes_wrapper: { value: "foo" })pb");
}

TEST_P(ProtoStructValueBuilderTest, AnyNull) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_any",
      [](ValueFactory& value_factory) { return value_factory.GetNullValue(); },
      R"pb(single_any {
             [type.googleapis.com/google.protobuf.Value] {}
           })pb");
}

TEST_P(ProtoStructValueBuilderTest, AnyBool) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_any",
      [](ValueFactory& value_factory) {
        return value_factory.CreateBoolValue(true);
      },
      R"pb(single_any {
             [type.googleapis.com/google.protobuf.BoolValue] { value: true }
           })pb");
}

TEST_P(ProtoStructValueBuilderTest, AnyInt) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_any",
      [](ValueFactory& value_factory) {
        return value_factory.CreateIntValue(1);
      },
      R"pb(single_any {
             [type.googleapis.com/google.protobuf.Int64Value] { value: 1 }
           })pb");
}

TEST_P(ProtoStructValueBuilderTest, AnyUint) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_any",
      [](ValueFactory& value_factory) {
        return value_factory.CreateUintValue(1);
      },
      R"pb(single_any {
             [type.googleapis.com/google.protobuf.UInt64Value] { value: 1 }
           })pb");
}

TEST_P(ProtoStructValueBuilderTest, AnyDouble) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_any",
      [](ValueFactory& value_factory) {
        return value_factory.CreateDoubleValue(1);
      },
      R"pb(single_any {
             [type.googleapis.com/google.protobuf.DoubleValue] { value: 1 }
           })pb");
}

TEST_P(ProtoStructValueBuilderTest, AnyBytes) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_any",
      [](ValueFactory& value_factory) {
        return value_factory.CreateBytesValue("foo");
      },
      R"pb(single_any {
             [type.googleapis.com/google.protobuf.BytesValue] { value: "foo" }
           })pb");
}

TEST_P(ProtoStructValueBuilderTest, AnyString) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_any",
      [](ValueFactory& value_factory) {
        return value_factory.CreateStringValue("foo");
      },
      R"pb(single_any {
             [type.googleapis.com/google.protobuf.StringValue] { value: "foo" }
           })pb");
}

TEST_P(ProtoStructValueBuilderTest, AnyDuration) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_any",
      [](ValueFactory& value_factory) {
        return value_factory.CreateDurationValue(absl::Seconds(1) +
                                                 absl::Nanoseconds(1));
      },
      R"pb(single_any {
             [type.googleapis.com/google.protobuf.Duration] {
               seconds: 1,
               nanos: 1
             }
           })pb");
}

TEST_P(ProtoStructValueBuilderTest, AnyTimestamp) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_any",
      [](ValueFactory& value_factory) {
        return value_factory.CreateTimestampValue(
            absl::UnixEpoch() + absl::Seconds(1) + absl::Nanoseconds(1));
      },
      R"pb(single_any {
             [type.googleapis.com/google.protobuf.Timestamp] {
               seconds: 1,
               nanos: 1
             }
           })pb");
}

TEST_P(ProtoStructValueBuilderTest, AnyMessage) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "single_any",
      [](ValueFactory& value_factory) {
        TestAllTypes::NestedMessage message;
        message.set_bb(1);
        return ProtoValue::Create(value_factory, message);
      },
      R"pb(single_any {
             [type.googleapis.com/google.api.expr.test.v1.proto3.TestAllTypes
                  .NestedMessage] { bb: 1 }
           })pb");
}

TEST_P(ProtoStructValueBuilderTest, RepeatedNull) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "repeated_null_value",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        ListValueBuilder<NullValue> builder(
            value_factory, value_factory.type_factory().GetNullType());
        CEL_RETURN_IF_ERROR(builder.Add(value_factory.GetNullValue()));
        CEL_RETURN_IF_ERROR(builder.Add(value_factory.GetNullValue()));
        return std::move(builder).Build();
      },
      R"pb(repeated_null_value: 0, repeated_null_value: 0)pb");
}

TEST_P(ProtoStructValueBuilderTest, RepeatedBool) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "repeated_bool",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        ListValueBuilder<BoolValue> builder(
            value_factory, value_factory.type_factory().GetBoolType());
        CEL_RETURN_IF_ERROR(builder.Add(true));
        CEL_RETURN_IF_ERROR(builder.Add(false));
        return std::move(builder).Build();
      },
      R"pb(repeated_bool: true, repeated_bool: false)pb");
}

TEST_P(ProtoStructValueBuilderTest, RepeatedInt) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "repeated_int32",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        ListValueBuilder<IntValue> builder(
            value_factory, value_factory.type_factory().GetIntType());
        CEL_RETURN_IF_ERROR(builder.Add(1));
        CEL_RETURN_IF_ERROR(builder.Add(0));
        return std::move(builder).Build();
      },
      R"pb(repeated_int32: 1, repeated_int32: 0)pb");
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "repeated_int64",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        ListValueBuilder<IntValue> builder(
            value_factory, value_factory.type_factory().GetIntType());
        CEL_RETURN_IF_ERROR(builder.Add(1));
        CEL_RETURN_IF_ERROR(builder.Add(0));
        return std::move(builder).Build();
      },
      R"pb(repeated_int64: 1, repeated_int64: 0)pb");
}

TEST_P(ProtoStructValueBuilderTest, RepeatedUint) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "repeated_uint32",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        ListValueBuilder<UintValue> builder(
            value_factory, value_factory.type_factory().GetUintType());
        CEL_RETURN_IF_ERROR(builder.Add(1));
        CEL_RETURN_IF_ERROR(builder.Add(0));
        return std::move(builder).Build();
      },
      R"pb(repeated_uint32: 1, repeated_uint32: 0)pb");
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "repeated_uint64",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        ListValueBuilder<UintValue> builder(
            value_factory, value_factory.type_factory().GetUintType());
        CEL_RETURN_IF_ERROR(builder.Add(1));
        CEL_RETURN_IF_ERROR(builder.Add(0));
        return std::move(builder).Build();
      },
      R"pb(repeated_uint64: 1, repeated_uint64: 0)pb");
}

TEST_P(ProtoStructValueBuilderTest, RepeatedDouble) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "repeated_float",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        ListValueBuilder<DoubleValue> builder(
            value_factory, value_factory.type_factory().GetDoubleType());
        CEL_RETURN_IF_ERROR(builder.Add(1));
        CEL_RETURN_IF_ERROR(builder.Add(0));
        return std::move(builder).Build();
      },
      R"pb(repeated_float: 1, repeated_float: 0)pb");
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "repeated_double",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        ListValueBuilder<DoubleValue> builder(
            value_factory, value_factory.type_factory().GetDoubleType());
        CEL_RETURN_IF_ERROR(builder.Add(1));
        CEL_RETURN_IF_ERROR(builder.Add(0));
        return std::move(builder).Build();
      },
      R"pb(repeated_double: 1, repeated_double: 0)pb");
}

TEST_P(ProtoStructValueBuilderTest, RepeatedString) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "repeated_string",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        ListValueBuilder<StringValue> builder(
            value_factory, value_factory.type_factory().GetStringType());
        CEL_RETURN_IF_ERROR(
            builder.Add(value_factory.CreateUncheckedStringValue("foo")));
        CEL_RETURN_IF_ERROR(
            builder.Add(value_factory.CreateUncheckedStringValue("bar")));
        return std::move(builder).Build();
      },
      R"pb(repeated_string: "foo", repeated_string: "bar")pb");
}

TEST_P(ProtoStructValueBuilderTest, RepeatedBytes) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "repeated_bytes",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        ListValueBuilder<BytesValue> builder(
            value_factory, value_factory.type_factory().GetBytesType());
        CEL_ASSIGN_OR_RETURN(auto value, value_factory.CreateBytesValue("foo"));
        CEL_RETURN_IF_ERROR(builder.Add(std::move(value)));
        CEL_ASSIGN_OR_RETURN(value, value_factory.CreateBytesValue("bar"));
        CEL_RETURN_IF_ERROR(builder.Add(std::move(value)));
        return std::move(builder).Build();
      },
      R"pb(repeated_bytes: "foo", repeated_bytes: "bar")pb");
}

TEST_P(ProtoStructValueBuilderTest, RepeatedEnum) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "repeated_nested_enum",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        CEL_ASSIGN_OR_RETURN(auto type,
                             ProtoType::Resolve<TestAllTypes::NestedEnum>(
                                 value_factory.type_manager()));
        ListValueBuilder<EnumValue> builder(value_factory, std::move(type));
        CEL_ASSIGN_OR_RETURN(
            auto value, ProtoValue::Create(value_factory, TestAllTypes::FOO));
        CEL_RETURN_IF_ERROR(builder.Add(std::move(value)));
        CEL_ASSIGN_OR_RETURN(
            value, ProtoValue::Create(value_factory, TestAllTypes::BAR));
        CEL_RETURN_IF_ERROR(builder.Add(std::move(value)));
        return std::move(builder).Build();
      },
      R"pb(repeated_nested_enum: 0, repeated_nested_enum: 1)pb");
}

TEST_P(ProtoStructValueBuilderTest, RepeatedCoercedEnum) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "repeated_nested_enum",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        ListValueBuilder<IntValue> builder(
            value_factory, value_factory.type_factory().GetIntType());
        CEL_RETURN_IF_ERROR(builder.Add(TestAllTypes::FOO));
        CEL_RETURN_IF_ERROR(builder.Add(TestAllTypes::BAR));
        return std::move(builder).Build();
      },
      R"pb(repeated_nested_enum: 0, repeated_nested_enum: 1)pb");
}

TEST_P(ProtoStructValueBuilderTest, RepeatedStruct) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "repeated_nested_message",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        CEL_ASSIGN_OR_RETURN(auto type,
                             ProtoType::Resolve<TestAllTypes::NestedMessage>(
                                 value_factory.type_manager()));
        ListValueBuilder<StructValue> builder(value_factory, std::move(type));
        TestAllTypes::NestedMessage proto_value;
        proto_value.set_bb(1);
        CEL_ASSIGN_OR_RETURN(auto value,
                             ProtoValue::Create(value_factory, proto_value));
        CEL_RETURN_IF_ERROR(builder.Add(std::move(value).As<StructValue>()));
        proto_value.Clear();
        CEL_ASSIGN_OR_RETURN(value,
                             ProtoValue::Create(value_factory, proto_value));
        CEL_RETURN_IF_ERROR(builder.Add(std::move(value).As<StructValue>()));
        return std::move(builder).Build();
      },
      R"pb(repeated_nested_message: { bb: 1 },
           repeated_nested_message: {})pb");
}

TEST_P(ProtoStructValueBuilderTest, RepeatedDuration) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "repeated_duration",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        ListValueBuilder<DurationValue> builder(
            value_factory, value_factory.type_factory().GetDurationType());
        CEL_ASSIGN_OR_RETURN(auto value,
                             value_factory.CreateDurationValue(
                                 absl::Seconds(1) + absl::Nanoseconds(1)));
        CEL_RETURN_IF_ERROR(builder.Add(std::move(value)));
        CEL_ASSIGN_OR_RETURN(
            value, value_factory.CreateDurationValue(absl::ZeroDuration()));
        CEL_RETURN_IF_ERROR(builder.Add(std::move(value)));
        return std::move(builder).Build();
      },
      R"pb(repeated_duration: { seconds: 1, nanos: 1 },
           repeated_duration: {})pb");
}

TEST_P(ProtoStructValueBuilderTest, RepeatedTimestamp) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "repeated_timestamp",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        ListValueBuilder<TimestampValue> builder(
            value_factory, value_factory.type_factory().GetTimestampType());
        CEL_ASSIGN_OR_RETURN(
            auto value,
            value_factory.CreateTimestampValue(
                absl::UnixEpoch() + absl::Seconds(1) + absl::Nanoseconds(1)));
        CEL_RETURN_IF_ERROR(builder.Add(std::move(value)));
        CEL_ASSIGN_OR_RETURN(
            value, value_factory.CreateTimestampValue(absl::UnixEpoch()));
        CEL_RETURN_IF_ERROR(builder.Add(std::move(value)));
        return std::move(builder).Build();
      },
      R"pb(repeated_timestamp: { seconds: 1, nanos: 1 },
           repeated_timestamp: {})pb");
}

TEST_P(ProtoStructValueBuilderTest, RepeatedBoolWrapper) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "repeated_bool_wrapper",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        ListValueBuilder<BoolValue> builder(
            value_factory, value_factory.type_factory().GetBoolType());
        CEL_RETURN_IF_ERROR(builder.Add(true));
        CEL_RETURN_IF_ERROR(builder.Add(false));
        return std::move(builder).Build();
      },
      R"pb(repeated_bool_wrapper: { value: true },
           repeated_bool_wrapper: { value: false })pb");
}

TEST_P(ProtoStructValueBuilderTest, RepeatedIntWrapper) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "repeated_int32_wrapper",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        ListValueBuilder<IntValue> builder(
            value_factory, value_factory.type_factory().GetIntType());
        CEL_RETURN_IF_ERROR(builder.Add(1));
        CEL_RETURN_IF_ERROR(builder.Add(0));
        return std::move(builder).Build();
      },
      R"pb(repeated_int32_wrapper: { value: 1 },
           repeated_int32_wrapper: { value: 0 })pb");
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "repeated_int64_wrapper",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        ListValueBuilder<IntValue> builder(
            value_factory, value_factory.type_factory().GetIntType());
        CEL_RETURN_IF_ERROR(builder.Add(1));
        CEL_RETURN_IF_ERROR(builder.Add(0));
        return std::move(builder).Build();
      },
      R"pb(repeated_int64_wrapper: { value: 1 },
           repeated_int64_wrapper: { value: 0 })pb");
}

TEST_P(ProtoStructValueBuilderTest, RepeatedUintWrapper) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "repeated_uint32_wrapper",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        ListValueBuilder<UintValue> builder(
            value_factory, value_factory.type_factory().GetUintType());
        CEL_RETURN_IF_ERROR(builder.Add(1));
        CEL_RETURN_IF_ERROR(builder.Add(0));
        return std::move(builder).Build();
      },
      R"pb(repeated_uint32_wrapper: { value: 1 },
           repeated_uint32_wrapper: { value: 0 })pb");
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "repeated_uint64_wrapper",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        ListValueBuilder<UintValue> builder(
            value_factory, value_factory.type_factory().GetUintType());
        CEL_RETURN_IF_ERROR(builder.Add(1));
        CEL_RETURN_IF_ERROR(builder.Add(0));
        return std::move(builder).Build();
      },
      R"pb(repeated_uint64_wrapper: { value: 1 },
           repeated_uint64_wrapper: { value: 0 })pb");
}

TEST_P(ProtoStructValueBuilderTest, RepeatedDoubleWrapper) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "repeated_float_wrapper",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        ListValueBuilder<DoubleValue> builder(
            value_factory, value_factory.type_factory().GetDoubleType());
        CEL_RETURN_IF_ERROR(builder.Add(1));
        CEL_RETURN_IF_ERROR(builder.Add(0));
        return std::move(builder).Build();
      },
      R"pb(repeated_float_wrapper: { value: 1 },
           repeated_float_wrapper: { value: 0 })pb");
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "repeated_double_wrapper",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        ListValueBuilder<DoubleValue> builder(
            value_factory, value_factory.type_factory().GetDoubleType());
        CEL_RETURN_IF_ERROR(builder.Add(1));
        CEL_RETURN_IF_ERROR(builder.Add(0));
        return std::move(builder).Build();
      },
      R"pb(repeated_double_wrapper: { value: 1 },
           repeated_double_wrapper: { value: 0 })pb");
}

TEST_P(ProtoStructValueBuilderTest, RepeatedStringWrapper) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "repeated_string_wrapper",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        ListValueBuilder<StringValue> builder(
            value_factory, value_factory.type_factory().GetStringType());
        CEL_RETURN_IF_ERROR(
            builder.Add(value_factory.CreateUncheckedStringValue("foo")));
        CEL_RETURN_IF_ERROR(
            builder.Add(value_factory.CreateUncheckedStringValue("bar")));
        return std::move(builder).Build();
      },
      R"pb(repeated_string_wrapper: { value: "foo" },
           repeated_string_wrapper: { value: "bar" })pb");
}

TEST_P(ProtoStructValueBuilderTest, RepeatedBytesWrapper) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "repeated_bytes_wrapper",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        ListValueBuilder<BytesValue> builder(
            value_factory, value_factory.type_factory().GetBytesType());
        CEL_ASSIGN_OR_RETURN(auto value, value_factory.CreateBytesValue("foo"));
        CEL_RETURN_IF_ERROR(builder.Add(std::move(value)));
        CEL_ASSIGN_OR_RETURN(value, value_factory.CreateBytesValue("bar"));
        CEL_RETURN_IF_ERROR(builder.Add(std::move(value)));
        return std::move(builder).Build();
      },
      R"pb(repeated_bytes_wrapper: { value: "foo" },
           repeated_bytes_wrapper: { value: "bar" })pb");
}

TEST_P(ProtoStructValueBuilderTest, MapBoolInt) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_bool_int64",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        MapValueBuilder<BoolValue, IntValue> builder(
            value_factory, value_factory.type_factory().GetBoolType(),
            value_factory.type_factory().GetIntType());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(true, 0).status());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(false, 1).status());
        return std::move(builder).Build();
      },
      R"pb(map_bool_int64: { key: true, value: 0 },
           map_bool_int64: { key: false, value: 1 })pb");
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_bool_int32",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        MapValueBuilder<BoolValue, IntValue> builder(
            value_factory, value_factory.type_factory().GetBoolType(),
            value_factory.type_factory().GetIntType());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(true, 0).status());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(false, 1).status());
        return std::move(builder).Build();
      },
      R"pb(map_bool_int32: { key: true, value: 0 },
           map_bool_int32: { key: false, value: 1 })pb");
}

TEST_P(ProtoStructValueBuilderTest, MapIntBool) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_int64_bool",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        MapValueBuilder<IntValue, BoolValue> builder(
            value_factory, value_factory.type_factory().GetIntType(),
            value_factory.type_factory().GetBoolType());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(1, false).status());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(0, true).status());
        return std::move(builder).Build();
      },
      R"pb(map_int64_bool: { key: 1, value: false },
           map_int64_bool: { key: 0, value: true })pb");
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_int32_bool",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        MapValueBuilder<IntValue, BoolValue> builder(
            value_factory, value_factory.type_factory().GetIntType(),
            value_factory.type_factory().GetBoolType());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(1, false).status());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(0, true).status());
        return std::move(builder).Build();
      },
      R"pb(map_int32_bool: { key: 1, value: false },
           map_int32_bool: { key: 0, value: true })pb");
}

TEST_P(ProtoStructValueBuilderTest, MapUintDouble) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_uint64_double",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        MapValueBuilder<UintValue, DoubleValue> builder(
            value_factory, value_factory.type_factory().GetUintType(),
            value_factory.type_factory().GetDoubleType());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(1, 0).status());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(0, 1).status());
        return std::move(builder).Build();
      },
      R"pb(map_uint64_double: { key: 1, value: 0 },
           map_uint64_double: { key: 0, value: 1 })pb");
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_uint32_double",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        MapValueBuilder<UintValue, DoubleValue> builder(
            value_factory, value_factory.type_factory().GetUintType(),
            value_factory.type_factory().GetDoubleType());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(1, 0).status());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(0, 1).status());
        return std::move(builder).Build();
      },
      R"pb(map_uint32_double: { key: 1, value: 0 },
           map_uint32_double: { key: 0, value: 1 })pb");
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_uint64_float",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        MapValueBuilder<UintValue, DoubleValue> builder(
            value_factory, value_factory.type_factory().GetUintType(),
            value_factory.type_factory().GetDoubleType());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(1, 0).status());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(0, 1).status());
        return std::move(builder).Build();
      },
      R"pb(map_uint64_float: { key: 1, value: 0 },
           map_uint64_float: { key: 0, value: 1 })pb");
}

TEST_P(ProtoStructValueBuilderTest, MapStringUint) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_string_uint64",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        MapValueBuilder<StringValue, UintValue> builder(
            value_factory, value_factory.type_factory().GetStringType(),
            value_factory.type_factory().GetUintType());
        CEL_RETURN_IF_ERROR(
            builder
                .InsertOrAssign(value_factory.CreateUncheckedStringValue("bar"),
                                0)
                .status());
        CEL_RETURN_IF_ERROR(
            builder
                .InsertOrAssign(value_factory.CreateUncheckedStringValue("foo"),
                                1)
                .status());
        return std::move(builder).Build();
      },
      R"pb(map_string_uint64: { key: "bar", value: 0 },
           map_string_uint64: { key: "foo", value: 1 })pb");
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_string_uint32",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        MapValueBuilder<StringValue, UintValue> builder(
            value_factory, value_factory.type_factory().GetStringType(),
            value_factory.type_factory().GetUintType());
        CEL_RETURN_IF_ERROR(
            builder
                .InsertOrAssign(value_factory.CreateUncheckedStringValue("bar"),
                                0)
                .status());
        CEL_RETURN_IF_ERROR(
            builder
                .InsertOrAssign(value_factory.CreateUncheckedStringValue("foo"),
                                1)
                .status());
        return std::move(builder).Build();
      },
      R"pb(map_string_uint32: { key: "bar", value: 0 },
           map_string_uint32: { key: "foo", value: 1 })pb");
}

TEST_P(ProtoStructValueBuilderTest, MapStringString) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_string_string",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        MapValueBuilder<StringValue, StringValue> builder(
            value_factory, value_factory.type_factory().GetStringType(),
            value_factory.type_factory().GetStringType());
        CEL_RETURN_IF_ERROR(
            builder
                .InsertOrAssign(value_factory.CreateUncheckedStringValue("bar"),
                                value_factory.CreateUncheckedStringValue("foo"))
                .status());
        CEL_RETURN_IF_ERROR(
            builder
                .InsertOrAssign(value_factory.CreateUncheckedStringValue("foo"),
                                value_factory.CreateUncheckedStringValue("bar"))
                .status());
        return std::move(builder).Build();
      },
      R"pb(map_string_string: { key: "bar", value: "foo" },
           map_string_string: { key: "foo", value: "bar" })pb");
}

TEST_P(ProtoStructValueBuilderTest, MapIntBytes) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_int64_bytes",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        MapValueBuilder<IntValue, BytesValue> builder(
            value_factory, value_factory.type_factory().GetIntType(),
            value_factory.type_factory().GetBytesType());
        CEL_ASSIGN_OR_RETURN(auto value, value_factory.CreateBytesValue("foo"));
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(1, value).status());
        CEL_ASSIGN_OR_RETURN(value, value_factory.CreateBytesValue("bar"));
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(0, value).status());
        return std::move(builder).Build();
      },
      R"pb(map_int64_bytes: { key: 1, value: "foo" },
           map_int64_bytes: { key: 0, value: "bar" })pb");
}

TEST_P(ProtoStructValueBuilderTest, MapIntBoolWrapper) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_int64_bool_wrapper",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        MapValueBuilder<IntValue, BoolValue> builder(
            value_factory, value_factory.type_factory().GetIntType(),
            value_factory.type_factory().GetBoolType());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(1, false).status());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(0, true).status());
        return std::move(builder).Build();
      },
      R"pb(map_int64_bool_wrapper: {
             key: 1,
             value: { value: false }
           },
           map_int64_bool_wrapper: {
             key: 0,
             value: { value: true }
           })pb");
}

TEST_P(ProtoStructValueBuilderTest, MapIntIntWrapper) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_int64_int64_wrapper",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        MapValueBuilder<IntValue, IntValue> builder(
            value_factory, value_factory.type_factory().GetIntType(),
            value_factory.type_factory().GetIntType());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(1, 0).status());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(0, 1).status());
        return std::move(builder).Build();
      },
      R"pb(map_int64_int64_wrapper: {
             key: 1,
             value: { value: 0 }
           },
           map_int64_int64_wrapper: {
             key: 0,
             value: { value: 1 }
           })pb");
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_int64_int32_wrapper",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        MapValueBuilder<IntValue, IntValue> builder(
            value_factory, value_factory.type_factory().GetIntType(),
            value_factory.type_factory().GetIntType());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(1, 0).status());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(0, 1).status());
        return std::move(builder).Build();
      },
      R"pb(map_int64_int32_wrapper: {
             key: 1,
             value: { value: 0 }
           },
           map_int64_int32_wrapper: {
             key: 0,
             value: { value: 1 }
           })pb");
}

TEST_P(ProtoStructValueBuilderTest, MapIntUintWrapper) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_int64_uint64_wrapper",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        MapValueBuilder<IntValue, UintValue> builder(
            value_factory, value_factory.type_factory().GetIntType(),
            value_factory.type_factory().GetUintType());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(1, 0).status());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(0, 1).status());
        return std::move(builder).Build();
      },
      R"pb(map_int64_uint64_wrapper: {
             key: 1,
             value: { value: 0 }
           },
           map_int64_uint64_wrapper: {
             key: 0,
             value: { value: 1 }
           })pb");
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_int64_uint32_wrapper",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        MapValueBuilder<IntValue, UintValue> builder(
            value_factory, value_factory.type_factory().GetIntType(),
            value_factory.type_factory().GetUintType());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(1, 0).status());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(0, 1).status());
        return std::move(builder).Build();
      },
      R"pb(map_int64_uint32_wrapper: {
             key: 1,
             value: { value: 0 }
           },
           map_int64_uint32_wrapper: {
             key: 0,
             value: { value: 1 }
           })pb");
}

TEST_P(ProtoStructValueBuilderTest, MapIntDoubleWrapper) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_int64_double_wrapper",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        MapValueBuilder<IntValue, DoubleValue> builder(
            value_factory, value_factory.type_factory().GetIntType(),
            value_factory.type_factory().GetDoubleType());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(1, 0).status());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(0, 1).status());
        return std::move(builder).Build();
      },
      R"pb(map_int64_double_wrapper: {
             key: 1,
             value: { value: 0 }
           },
           map_int64_double_wrapper: {
             key: 0,
             value: { value: 1 }
           })pb");
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_int64_float_wrapper",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        MapValueBuilder<IntValue, DoubleValue> builder(
            value_factory, value_factory.type_factory().GetIntType(),
            value_factory.type_factory().GetDoubleType());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(1, 0).status());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(0, 1).status());
        return std::move(builder).Build();
      },
      R"pb(map_int64_float_wrapper: {
             key: 1,
             value: { value: 0 }
           },
           map_int64_float_wrapper: {
             key: 0,
             value: { value: 1 }
           })pb");
}

TEST_P(ProtoStructValueBuilderTest, MapIntBytesWrapper) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_int64_bytes_wrapper",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        MapValueBuilder<IntValue, BytesValue> builder(
            value_factory, value_factory.type_factory().GetIntType(),
            value_factory.type_factory().GetBytesType());
        CEL_ASSIGN_OR_RETURN(auto value, value_factory.CreateBytesValue("foo"));
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(1, value).status());
        CEL_ASSIGN_OR_RETURN(value, value_factory.CreateBytesValue("bar"));
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(0, value).status());
        return std::move(builder).Build();
      },
      R"pb(map_int64_bytes_wrapper: {
             key: 1,
             value: { value: "foo" }
           },
           map_int64_bytes_wrapper: {
             key: 0,
             value: { value: "bar" }
           })pb");
}

TEST_P(ProtoStructValueBuilderTest, MapIntStringWrapper) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_int64_string_wrapper",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        MapValueBuilder<IntValue, StringValue> builder(
            value_factory, value_factory.type_factory().GetIntType(),
            value_factory.type_factory().GetStringType());
        CEL_ASSIGN_OR_RETURN(auto value,
                             value_factory.CreateStringValue("foo"));
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(1, value).status());
        CEL_ASSIGN_OR_RETURN(value, value_factory.CreateStringValue("bar"));
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(0, value).status());
        return std::move(builder).Build();
      },
      R"pb(map_int64_string_wrapper: {
             key: 1,
             value: { value: "foo" }
           },
           map_int64_string_wrapper: {
             key: 0,
             value: { value: "bar" }
           })pb");
}

TEST_P(ProtoStructValueBuilderTest, MapIntDuration) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_int64_duration",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        MapValueBuilder<IntValue, DurationValue> builder(
            value_factory, value_factory.type_factory().GetIntType(),
            value_factory.type_factory().GetDurationType());
        CEL_RETURN_IF_ERROR(
            builder.InsertOrAssign(1, absl::ZeroDuration()).status());
        CEL_RETURN_IF_ERROR(
            builder.InsertOrAssign(0, absl::Seconds(1) + absl::Nanoseconds(1))
                .status());
        return std::move(builder).Build();
      },
      R"pb(map_int64_duration: {
             key: 1,
             value: { seconds: 0, nanos: 0 }
           },
           map_int64_duration: {
             key: 0,
             value: { seconds: 1, nanos: 1 }
           })pb");
}

TEST_P(ProtoStructValueBuilderTest, MapIntTimestamp) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_int64_timestamp",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        MapValueBuilder<IntValue, TimestampValue> builder(
            value_factory, value_factory.type_factory().GetIntType(),
            value_factory.type_factory().GetTimestampType());
        CEL_RETURN_IF_ERROR(
            builder.InsertOrAssign(1, absl::UnixEpoch() + absl::ZeroDuration())
                .status());
        CEL_RETURN_IF_ERROR(builder
                                .InsertOrAssign(0, absl::UnixEpoch() +
                                                       absl::Seconds(1) +
                                                       absl::Nanoseconds(1))
                                .status());
        return std::move(builder).Build();
      },
      R"pb(map_int64_timestamp: {
             key: 1,
             value: { seconds: 0, nanos: 0 }
           },
           map_int64_timestamp: {
             key: 0,
             value: { seconds: 1, nanos: 1 }
           })pb");
}

TEST_P(ProtoStructValueBuilderTest, MapIntNull) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_int64_null_value",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        CEL_ASSIGN_OR_RETURN(auto type,
                             ProtoType::Resolve<TestAllTypes::NestedEnum>(
                                 value_factory.type_manager()));
        MapValueBuilder<IntValue, NullValue> builder(
            value_factory, value_factory.type_factory().GetIntType(),
            value_factory.type_factory().GetNullType());
        CEL_RETURN_IF_ERROR(
            builder.InsertOrAssign(1, value_factory.GetNullValue()).status());
        CEL_RETURN_IF_ERROR(
            builder.InsertOrAssign(0, value_factory.GetNullValue()).status());
        return std::move(builder).Build();
      },
      R"pb(map_int64_null_value: { key: 1, value: 0 },
           map_int64_null_value: { key: 0, value: 0 })pb");
}

TEST_P(ProtoStructValueBuilderTest, MapIntEnum) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_int64_enum",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        CEL_ASSIGN_OR_RETURN(auto type,
                             ProtoType::Resolve<TestAllTypes::NestedEnum>(
                                 value_factory.type_manager()));
        MapValueBuilder<IntValue, EnumValue> builder(
            value_factory, value_factory.type_factory().GetIntType(),
            std::move(type));
        CEL_ASSIGN_OR_RETURN(
            auto value, ProtoValue::Create(value_factory, TestAllTypes::FOO));
        CEL_RETURN_IF_ERROR(
            builder.InsertOrAssign(1, std::move(value)).status());
        CEL_ASSIGN_OR_RETURN(
            value, ProtoValue::Create(value_factory, TestAllTypes::BAR));
        CEL_RETURN_IF_ERROR(
            builder.InsertOrAssign(0, std::move(value)).status());
        return std::move(builder).Build();
      },
      R"pb(map_int64_enum: { key: 1, value: 0 },
           map_int64_enum: { key: 0, value: 1 })pb");
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_int64_enum",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        CEL_ASSIGN_OR_RETURN(auto type,
                             ProtoType::Resolve<TestAllTypes::NestedEnum>(
                                 value_factory.type_manager()));
        MapValueBuilder<IntValue, IntValue> builder(
            value_factory, value_factory.type_factory().GetIntType(),
            value_factory.type_factory().GetIntType());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(1, 0).status());
        CEL_RETURN_IF_ERROR(builder.InsertOrAssign(0, 1).status());
        return std::move(builder).Build();
      },
      R"pb(map_int64_enum: { key: 1, value: 0 },
           map_int64_enum: { key: 0, value: 1 })pb");
}

TEST_P(ProtoStructValueBuilderTest, MapIntMessage) {
  TEST_PROTO_STRUCT_VALUE_BUILDER(
      memory_manager(), "map_int64_message",
      [](ValueFactory& value_factory) -> absl::StatusOr<Handle<Value>> {
        CEL_ASSIGN_OR_RETURN(auto type,
                             ProtoType::Resolve<TestAllTypes::NestedMessage>(
                                 value_factory.type_manager()));
        MapValueBuilder<IntValue, StructValue> builder(
            value_factory, value_factory.type_factory().GetIntType(),
            std::move(type));
        TestAllTypes::NestedMessage message;
        CEL_ASSIGN_OR_RETURN(auto value,
                             ProtoValue::Create(value_factory, message));
        CEL_RETURN_IF_ERROR(
            builder.InsertOrAssign(1, std::move(value)).status());
        message.Clear();
        message.set_bb(1);
        CEL_ASSIGN_OR_RETURN(value, ProtoValue::Create(value_factory, message));
        CEL_RETURN_IF_ERROR(
            builder.InsertOrAssign(0, std::move(value)).status());
        return std::move(builder).Build();
      },
      R"pb(map_int64_message: {
             key: 1,
             value: { bb: 0 }
           },
           map_int64_message: {
             key: 0,
             value: { bb: 1 }
           })pb");
}

INSTANTIATE_TEST_SUITE_P(ProtoStructValueBuilderTest,
                         ProtoStructValueBuilderTest,
                         cel::base_internal::MemoryManagerTestModeAll(),
                         cel::base_internal::MemoryManagerTestModeTupleName);

}  // namespace
}  // namespace cel::extensions
