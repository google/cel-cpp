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

#include "extensions/protobuf/value.h"

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "base/internal/memory_manager_testing.h"
#include "base/testing/value_matchers.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "base/value_factory.h"
#include "extensions/protobuf/enum_value.h"
#include "extensions/protobuf/internal/testing.h"
#include "extensions/protobuf/type_provider.h"
#include "internal/testing.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"
#include "google/protobuf/generated_enum_reflection.h"

namespace cel::extensions {
namespace {

using ::cel_testing::ValueOf;
using testing::Eq;
using testing::EqualsProto;
using testing::Optional;
using cel::internal::IsOkAndHolds;

using TestAllTypes = ::google::api::expr::test::v1::proto3::TestAllTypes;

using ProtoValueTest = ProtoTest<>;

TEST_P(ProtoValueTest, DurationStatic) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::Duration duration_proto;
  duration_proto.set_seconds(1);
  ASSERT_OK_AND_ASSIGN(auto duration_value,
                       ProtoValue::Create(value_factory, duration_proto));
  EXPECT_EQ(duration_value->value(), absl::Seconds(1));
}

TEST_P(ProtoValueTest, DurationDynamicLValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::Duration duration_proto;
  duration_proto.set_seconds(1);
  ASSERT_OK_AND_ASSIGN(
      auto duration_value,
      ProtoValue::Create(value_factory,
                         static_cast<const google::protobuf::Message&>(duration_proto)));
  EXPECT_EQ(duration_value.As<DurationValue>()->value(), absl::Seconds(1));
}

TEST_P(ProtoValueTest, DurationDynamicRValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::Duration duration_proto;
  duration_proto.set_seconds(1);
  ASSERT_OK_AND_ASSIGN(
      auto duration_value,
      ProtoValue::Create(value_factory,
                         static_cast<google::protobuf::Message&&>(duration_proto)));
  EXPECT_EQ(duration_value.As<DurationValue>()->value(), absl::Seconds(1));
}

TEST_P(ProtoValueTest, TimestampStatic) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::Timestamp timestamp_proto;
  timestamp_proto.set_seconds(1);
  ASSERT_OK_AND_ASSIGN(auto timestamp_value,
                       ProtoValue::Create(value_factory, timestamp_proto));
  EXPECT_EQ(timestamp_value->value(), absl::UnixEpoch() + absl::Seconds(1));
}

TEST_P(ProtoValueTest, TimestampDynamicLValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::Timestamp timestamp_proto;
  timestamp_proto.set_seconds(1);
  ASSERT_OK_AND_ASSIGN(
      auto timestamp_value,
      ProtoValue::Create(value_factory,
                         static_cast<const google::protobuf::Message&>(timestamp_proto)));
  EXPECT_EQ(timestamp_value.As<TimestampValue>()->value(),
            absl::UnixEpoch() + absl::Seconds(1));
}

TEST_P(ProtoValueTest, TimestampDynamicRValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::Timestamp timestamp_proto;
  timestamp_proto.set_seconds(1);
  ASSERT_OK_AND_ASSIGN(
      auto timestamp_value,
      ProtoValue::Create(value_factory,
                         static_cast<google::protobuf::Message&&>(timestamp_proto)));
  EXPECT_EQ(timestamp_value.As<TimestampValue>()->value(),
            absl::UnixEpoch() + absl::Seconds(1));
}

TEST_P(ProtoValueTest, StructStatic) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  TestAllTypes struct_proto;
  struct_proto.set_single_bool(true);
  ASSERT_OK_AND_ASSIGN(auto struct_value,
                       ProtoValue::Create(value_factory, struct_proto));
  EXPECT_THAT(*struct_value->value(), EqualsProto(struct_proto));
}

TEST_P(ProtoValueTest, StructDynamicLValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  TestAllTypes struct_proto;
  struct_proto.set_single_bool(true);
  ASSERT_OK_AND_ASSIGN(
      auto struct_value,
      ProtoValue::Create(value_factory,
                         static_cast<const google::protobuf::Message&>(struct_proto)));
  EXPECT_THAT(*struct_value.As<ProtoStructValue>()->value(),
              EqualsProto(struct_proto));
}

TEST_P(ProtoValueTest, StructDynamicRValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  TestAllTypes struct_proto;
  struct_proto.set_single_bool(true);
  ASSERT_OK_AND_ASSIGN(
      auto struct_value,
      ProtoValue::Create(value_factory, static_cast<google::protobuf::Message&&>(
                                            TestAllTypes(struct_proto))));
  EXPECT_THAT(*struct_value.As<ProtoStructValue>()->value(),
              EqualsProto(struct_proto));
}

TEST_P(ProtoValueTest, StaticEnum) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  TestAllTypes::NestedEnum enum_proto = TestAllTypes::BAR;
  ASSERT_OK_AND_ASSIGN(auto enum_value,
                       ProtoValue::Create(value_factory, enum_proto));
  EXPECT_TRUE(ProtoEnumValue::Is(enum_value));
  EXPECT_EQ(
      ProtoEnumValue::descriptor(enum_value),
      google::protobuf::GetEnumDescriptor<TestAllTypes::NestedEnum>()->FindValueByNumber(
          enum_proto));
  EXPECT_THAT(ProtoEnumValue::value<TestAllTypes::NestedEnum>(enum_value),
              Optional(Eq(enum_proto)));
}

TEST_P(ProtoValueTest, DynamicEnum) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  TestAllTypes::NestedEnum enum_proto = TestAllTypes::BAR;
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoValue::Create(value_factory,
                         *google::protobuf::GetEnumDescriptor<TestAllTypes::NestedEnum>()
                              ->FindValueByNumber(enum_proto)));
  ASSERT_TRUE(value->Is<EnumValue>());
  EXPECT_TRUE(ProtoEnumValue::Is(value.As<EnumValue>()));
  EXPECT_EQ(
      ProtoEnumValue::descriptor(value.As<EnumValue>()),
      google::protobuf::GetEnumDescriptor<TestAllTypes::NestedEnum>()->FindValueByNumber(
          enum_proto));
  EXPECT_THAT(
      ProtoEnumValue::value<TestAllTypes::NestedEnum>(value.As<EnumValue>()),
      Optional(Eq(enum_proto)));
}

TEST_P(ProtoValueTest, StaticNullValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto null_value,
      ProtoValue::Create(value_factory,
                         google::protobuf::NullValue::NULL_VALUE));
  EXPECT_TRUE(null_value->Is<NullValue>());
}

TEST_P(ProtoValueTest, DynamicNullValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoValue::Create(
          value_factory,
          *google::protobuf::GetEnumDescriptor<google::protobuf::NullValue>()
               ->FindValueByNumber(google::protobuf::NullValue::NULL_VALUE)));
  EXPECT_TRUE(value->Is<NullValue>());
}

TEST_P(ProtoValueTest, StaticWrapperTypes) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  EXPECT_THAT(
      ProtoValue::Create(value_factory,
                         google::protobuf::BoolValue::default_instance()),
      IsOkAndHolds(ValueOf<BoolValue>(value_factory, false)));
  EXPECT_THAT(
      ProtoValue::Create(value_factory,
                         google::protobuf::BytesValue::default_instance()),
      IsOkAndHolds(ValueOf<BytesValue>(value_factory)));
  EXPECT_THAT(
      ProtoValue::Create(value_factory,
                         google::protobuf::FloatValue::default_instance()),
      IsOkAndHolds(ValueOf<DoubleValue>(value_factory, 0.0)));
  EXPECT_THAT(
      ProtoValue::Create(value_factory,
                         google::protobuf::DoubleValue::default_instance()),
      IsOkAndHolds(ValueOf<DoubleValue>(value_factory, 0.0)));
  EXPECT_THAT(
      ProtoValue::Create(value_factory,
                         google::protobuf::Int32Value::default_instance()),
      IsOkAndHolds(ValueOf<IntValue>(value_factory, 0)));
  EXPECT_THAT(
      ProtoValue::Create(value_factory,
                         google::protobuf::Int64Value::default_instance()),
      IsOkAndHolds(ValueOf<IntValue>(value_factory, 0)));
  EXPECT_THAT(
      ProtoValue::Create(value_factory,
                         google::protobuf::StringValue::default_instance()),
      IsOkAndHolds(ValueOf<StringValue>(value_factory)));
  EXPECT_THAT(
      ProtoValue::Create(value_factory,
                         google::protobuf::UInt32Value::default_instance()),
      IsOkAndHolds(ValueOf<UintValue>(value_factory, 0u)));
  EXPECT_THAT(
      ProtoValue::Create(value_factory,
                         google::protobuf::UInt64Value::default_instance()),
      IsOkAndHolds(ValueOf<UintValue>(value_factory, 0u)));
}

TEST_P(ProtoValueTest, DynamicWrapperTypesLValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  EXPECT_THAT(
      ProtoValue::Create(value_factory,
                         static_cast<const google::protobuf::Message&>(
                             google::protobuf::BoolValue::default_instance())),
      IsOkAndHolds(ValueOf<BoolValue>(value_factory, false)));
  EXPECT_THAT(
      ProtoValue::Create(value_factory,
                         static_cast<const google::protobuf::Message&>(
                             google::protobuf::BytesValue::default_instance())),
      IsOkAndHolds(ValueOf<BytesValue>(value_factory)));
  EXPECT_THAT(
      ProtoValue::Create(value_factory,
                         static_cast<const google::protobuf::Message&>(
                             google::protobuf::FloatValue::default_instance())),
      IsOkAndHolds(ValueOf<DoubleValue>(value_factory, 0.0)));
  EXPECT_THAT(ProtoValue::Create(
                  value_factory,
                  static_cast<const google::protobuf::Message&>(
                      google::protobuf::DoubleValue::default_instance())),
              IsOkAndHolds(ValueOf<DoubleValue>(value_factory, 0.0)));
  EXPECT_THAT(
      ProtoValue::Create(value_factory,
                         static_cast<const google::protobuf::Message&>(
                             google::protobuf::Int32Value::default_instance())),
      IsOkAndHolds(ValueOf<IntValue>(value_factory, 0)));
  EXPECT_THAT(
      ProtoValue::Create(value_factory,
                         static_cast<const google::protobuf::Message&>(
                             google::protobuf::Int64Value::default_instance())),
      IsOkAndHolds(ValueOf<IntValue>(value_factory, 0)));
  EXPECT_THAT(ProtoValue::Create(
                  value_factory,
                  static_cast<const google::protobuf::Message&>(
                      google::protobuf::StringValue::default_instance())),
              IsOkAndHolds(ValueOf<StringValue>(value_factory)));
  EXPECT_THAT(ProtoValue::Create(
                  value_factory,
                  static_cast<const google::protobuf::Message&>(
                      google::protobuf::UInt32Value::default_instance())),
              IsOkAndHolds(ValueOf<UintValue>(value_factory, 0u)));
  EXPECT_THAT(ProtoValue::Create(
                  value_factory,
                  static_cast<const google::protobuf::Message&>(
                      google::protobuf::UInt64Value::default_instance())),
              IsOkAndHolds(ValueOf<UintValue>(value_factory, 0u)));
}

TEST_P(ProtoValueTest, DynamicWrapperTypesRValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  EXPECT_THAT(
      ProtoValue::Create(value_factory, static_cast<google::protobuf::Message&&>(
                                            google::protobuf::BoolValue())),
      IsOkAndHolds(ValueOf<BoolValue>(value_factory, false)));
  EXPECT_THAT(
      ProtoValue::Create(value_factory, static_cast<google::protobuf::Message&&>(
                                            google::protobuf::BytesValue())),
      IsOkAndHolds(ValueOf<BytesValue>(value_factory)));
  EXPECT_THAT(
      ProtoValue::Create(value_factory, static_cast<google::protobuf::Message&&>(
                                            google::protobuf::FloatValue())),
      IsOkAndHolds(ValueOf<DoubleValue>(value_factory, 0.0)));
  EXPECT_THAT(
      ProtoValue::Create(value_factory, static_cast<google::protobuf::Message&&>(
                                            google::protobuf::DoubleValue())),
      IsOkAndHolds(ValueOf<DoubleValue>(value_factory, 0.0)));
  EXPECT_THAT(
      ProtoValue::Create(value_factory, static_cast<google::protobuf::Message&&>(
                                            google::protobuf::Int32Value())),
      IsOkAndHolds(ValueOf<IntValue>(value_factory, 0)));
  EXPECT_THAT(
      ProtoValue::Create(value_factory, static_cast<google::protobuf::Message&&>(
                                            google::protobuf::Int64Value())),
      IsOkAndHolds(ValueOf<IntValue>(value_factory, 0)));
  EXPECT_THAT(
      ProtoValue::Create(value_factory, static_cast<google::protobuf::Message&&>(
                                            google::protobuf::StringValue())),
      IsOkAndHolds(ValueOf<StringValue>(value_factory)));
  EXPECT_THAT(
      ProtoValue::Create(value_factory, static_cast<google::protobuf::Message&&>(
                                            google::protobuf::UInt32Value())),
      IsOkAndHolds(ValueOf<UintValue>(value_factory, 0u)));
  EXPECT_THAT(
      ProtoValue::Create(value_factory, static_cast<google::protobuf::Message&&>(
                                            google::protobuf::UInt64Value())),
      IsOkAndHolds(ValueOf<UintValue>(value_factory, 0u)));
}

INSTANTIATE_TEST_SUITE_P(ProtoValueTest, ProtoValueTest,
                         cel::base_internal::MemoryManagerTestModeAll(),
                         cel::base_internal::MemoryManagerTestModeTupleName);

}  // namespace
}  // namespace cel::extensions