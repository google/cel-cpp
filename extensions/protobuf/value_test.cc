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

#include <memory>

#include "google/protobuf/api.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "base/internal/memory_manager_testing.h"
#include "base/testing/value_matchers.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "base/value_factory.h"
#include "extensions/protobuf/enum_value.h"
#include "extensions/protobuf/internal/descriptors.h"
#include "extensions/protobuf/internal/testing.h"
#include "extensions/protobuf/struct_value.h"
#include "extensions/protobuf/type_provider.h"
#include "internal/testing.h"
#include "testutil/util.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"
#include "google/protobuf/generated_enum_reflection.h"

namespace cel::extensions {
namespace {

using ::cel_testing::ValueOf;
using google::api::expr::testutil::EqualsProto;
using testing::Eq;
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

TEST_P(ProtoValueTest, StaticValueNullValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  auto value_proto = std::make_unique<google::protobuf::Value>();
  value_proto->set_null_value(google::protobuf::NULL_VALUE);
  EXPECT_THAT(ProtoValue::Create(value_factory, std::move(value_proto)),
              IsOkAndHolds(ValueOf<NullValue>(value_factory)));
}

TEST_P(ProtoValueTest, StaticLValueValueNullValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::Value value_proto;
  value_proto.set_null_value(google::protobuf::NULL_VALUE);
  EXPECT_THAT(ProtoValue::Create(value_factory, value_proto),
              IsOkAndHolds(ValueOf<NullValue>(value_factory)));
}

TEST_P(ProtoValueTest, StaticRValueValueNullValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::Value value_proto;
  value_proto.set_null_value(google::protobuf::NULL_VALUE);
  EXPECT_THAT(ProtoValue::Create(value_factory, std::move(value_proto)),
              IsOkAndHolds(ValueOf<NullValue>(value_factory)));
}

TEST_P(ProtoValueTest, StaticValueBoolValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  auto value_proto = std::make_unique<google::protobuf::Value>();
  value_proto->set_bool_value(true);
  EXPECT_THAT(ProtoValue::Create(value_factory, std::move(value_proto)),
              IsOkAndHolds(ValueOf<BoolValue>(value_factory, true)));
}

TEST_P(ProtoValueTest, StaticLValueValueBoolValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::Value value_proto;
  value_proto.set_bool_value(true);
  EXPECT_THAT(ProtoValue::Create(value_factory, value_proto),
              IsOkAndHolds(ValueOf<BoolValue>(value_factory, true)));
}

TEST_P(ProtoValueTest, StaticRValueValueBoolValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::Value value_proto;
  value_proto.set_bool_value(true);
  EXPECT_THAT(ProtoValue::Create(value_factory, std::move(value_proto)),
              IsOkAndHolds(ValueOf<BoolValue>(value_factory, true)));
}

TEST_P(ProtoValueTest, StaticValueNumberValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  auto value_proto = std::make_unique<google::protobuf::Value>();
  value_proto->set_number_value(1.0);
  EXPECT_THAT(ProtoValue::Create(value_factory, std::move(value_proto)),
              IsOkAndHolds(ValueOf<DoubleValue>(value_factory, 1.0)));
}

TEST_P(ProtoValueTest, StaticLValueValueNumberValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::Value value_proto;
  value_proto.set_number_value(1.0);
  EXPECT_THAT(ProtoValue::Create(value_factory, value_proto),
              IsOkAndHolds(ValueOf<DoubleValue>(value_factory, 1.0)));
}

TEST_P(ProtoValueTest, StaticRValueValueNumberValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::Value value_proto;
  value_proto.set_number_value(1.0);
  EXPECT_THAT(ProtoValue::Create(value_factory, std::move(value_proto)),
              IsOkAndHolds(ValueOf<DoubleValue>(value_factory, 1.0)));
}

TEST_P(ProtoValueTest, StaticValueStringValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  auto value_proto = std::make_unique<google::protobuf::Value>();
  value_proto->set_string_value("foo");
  EXPECT_THAT(ProtoValue::Create(value_factory, std::move(value_proto)),
              IsOkAndHolds(ValueOf<StringValue>(value_factory, "foo")));
}

TEST_P(ProtoValueTest, StaticLValueValueStringValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::Value value_proto;
  value_proto.set_string_value("foo");
  EXPECT_THAT(ProtoValue::Create(value_factory, value_proto),
              IsOkAndHolds(ValueOf<StringValue>(value_factory, "foo")));
}

TEST_P(ProtoValueTest, StaticRValueValueStringValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::Value value_proto;
  value_proto.set_string_value("foo");
  EXPECT_THAT(ProtoValue::Create(value_factory, std::move(value_proto)),
              IsOkAndHolds(ValueOf<StringValue>(value_factory, "foo")));
}

TEST_P(ProtoValueTest, StaticValueListValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  auto value_proto = std::make_unique<google::protobuf::Value>();
  value_proto->mutable_list_value()->add_values()->set_bool_value(true);
  ASSERT_OK_AND_ASSIGN(
      auto value, ProtoValue::Create(value_factory, std::move(value_proto)));
  EXPECT_TRUE(value->Is<ListValue>());
  EXPECT_EQ(value->As<ListValue>().size(), 1);
  EXPECT_THAT(value->As<ListValue>().Get(value_factory, 0),
              IsOkAndHolds(ValueOf<BoolValue>(value_factory, true)));
}

TEST_P(ProtoValueTest, StaticLValueValueListValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::Value value_proto;
  value_proto.mutable_list_value()->add_values()->set_bool_value(true);
  ASSERT_OK_AND_ASSIGN(auto value,
                       ProtoValue::Create(value_factory, value_proto));
  EXPECT_TRUE(value->Is<ListValue>());
  EXPECT_EQ(value->As<ListValue>().size(), 1);
  EXPECT_THAT(value->As<ListValue>().Get(value_factory, 0),
              IsOkAndHolds(ValueOf<BoolValue>(value_factory, true)));
}

TEST_P(ProtoValueTest, StaticRValueValueListValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::Value value_proto;
  value_proto.mutable_list_value()->add_values()->set_bool_value(true);
  ASSERT_OK_AND_ASSIGN(
      auto value, ProtoValue::Create(value_factory, std::move(value_proto)));
  EXPECT_TRUE(value->Is<ListValue>());
  EXPECT_EQ(value->As<ListValue>().size(), 1);
  EXPECT_THAT(value->As<ListValue>().Get(value_factory, 0),
              IsOkAndHolds(ValueOf<BoolValue>(value_factory, true)));
}

TEST_P(ProtoValueTest, StaticValueStructValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::Value bool_value_proto;
  bool_value_proto.set_bool_value(true);
  auto value_proto = std::make_unique<google::protobuf::Value>();
  value_proto->mutable_struct_value()->mutable_fields()->insert(
      {"foo", bool_value_proto});
  ASSERT_OK_AND_ASSIGN(
      auto value, ProtoValue::Create(value_factory, std::move(value_proto)));
  EXPECT_TRUE(value->Is<MapValue>());
  EXPECT_EQ(value->As<MapValue>().size(), 1);
  ASSERT_OK_AND_ASSIGN(auto key, value_factory.CreateStringValue("foo"));
  EXPECT_THAT(value->As<MapValue>().Get(value_factory, key),
              IsOkAndHolds(ValueOf<BoolValue>(value_factory, true)));
}

TEST_P(ProtoValueTest, StaticLValueValueStructValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::Value bool_value_proto;
  bool_value_proto.set_bool_value(true);
  google::protobuf::Value value_proto;
  value_proto.mutable_struct_value()->mutable_fields()->insert(
      {"foo", bool_value_proto});
  ASSERT_OK_AND_ASSIGN(auto value,
                       ProtoValue::Create(value_factory, value_proto));
  EXPECT_TRUE(value->Is<MapValue>());
  EXPECT_EQ(value->As<MapValue>().size(), 1);
  ASSERT_OK_AND_ASSIGN(auto key, value_factory.CreateStringValue("foo"));
  EXPECT_THAT(value->As<MapValue>().Get(value_factory, key),
              IsOkAndHolds(ValueOf<BoolValue>(value_factory, true)));
}

TEST_P(ProtoValueTest, StaticRValueValueStructValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::Value bool_value_proto;
  bool_value_proto.set_bool_value(true);
  google::protobuf::Value value_proto;
  value_proto.mutable_struct_value()->mutable_fields()->insert(
      {"foo", bool_value_proto});
  ASSERT_OK_AND_ASSIGN(
      auto value, ProtoValue::Create(value_factory, std::move(value_proto)));
  EXPECT_TRUE(value->Is<MapValue>());
  EXPECT_EQ(value->As<MapValue>().size(), 1);
  ASSERT_OK_AND_ASSIGN(auto key, value_factory.CreateStringValue("foo"));
  EXPECT_THAT(value->As<MapValue>().Get(value_factory, key),
              IsOkAndHolds(ValueOf<BoolValue>(value_factory, true)));
}

TEST_P(ProtoValueTest, StaticValueUnset) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  auto value_proto = std::make_unique<google::protobuf::Value>();
  EXPECT_THAT(ProtoValue::Create(value_factory, std::move(value_proto)),
              IsOkAndHolds(ValueOf<NullValue>(value_factory)));
}

TEST_P(ProtoValueTest, StaticLValueValueUnset) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::Value value_proto;
  EXPECT_THAT(ProtoValue::Create(value_factory, value_proto),
              IsOkAndHolds(ValueOf<NullValue>(value_factory)));
}

TEST_P(ProtoValueTest, StaticRValueValueUnset) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::Value value_proto;
  EXPECT_THAT(ProtoValue::Create(value_factory, std::move(value_proto)),
              IsOkAndHolds(ValueOf<NullValue>(value_factory)));
}

TEST_P(ProtoValueTest, StaticListValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  auto list_value_proto = std::make_unique<google::protobuf::ListValue>();
  list_value_proto->add_values()->set_bool_value(true);
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoValue::Create(value_factory, std::move(list_value_proto)));
  EXPECT_EQ(value->size(), 1);
  EXPECT_THAT(value->Get(value_factory, 0),
              IsOkAndHolds(ValueOf<BoolValue>(value_factory, true)));
}

TEST_P(ProtoValueTest, StaticLValueListValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::ListValue list_value_proto;
  list_value_proto.add_values()->set_bool_value(true);
  ASSERT_OK_AND_ASSIGN(auto value,
                       ProtoValue::Create(value_factory, list_value_proto));
  EXPECT_EQ(value->size(), 1);
  EXPECT_THAT(value->Get(value_factory, 0),
              IsOkAndHolds(ValueOf<BoolValue>(value_factory, true)));
}

TEST_P(ProtoValueTest, StaticRValueListValue) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::ListValue list_value_proto;
  list_value_proto.add_values()->set_bool_value(true);
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoValue::Create(value_factory, std::move(list_value_proto)));
  EXPECT_EQ(value->size(), 1);
  EXPECT_THAT(value->Get(value_factory, 0),
              IsOkAndHolds(ValueOf<BoolValue>(value_factory, true)));
}

TEST_P(ProtoValueTest, StaticStruct) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::Value bool_value_proto;
  bool_value_proto.set_bool_value(true);
  auto struct_proto = std::make_unique<google::protobuf::Struct>();
  struct_proto->mutable_fields()->insert({"foo", bool_value_proto});
  ASSERT_OK_AND_ASSIGN(
      auto value, ProtoValue::Create(value_factory, std::move(struct_proto)));
  EXPECT_EQ(value->size(), 1);
  ASSERT_OK_AND_ASSIGN(auto key, value_factory.CreateStringValue("foo"));
  EXPECT_THAT(value->Get(value_factory, key),
              IsOkAndHolds(ValueOf<BoolValue>(value_factory, true)));
}

TEST_P(ProtoValueTest, StaticLValueStruct) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::Value bool_value_proto;
  bool_value_proto.set_bool_value(true);
  google::protobuf::Struct struct_proto;
  struct_proto.mutable_fields()->insert({"foo", bool_value_proto});
  ASSERT_OK_AND_ASSIGN(auto value,
                       ProtoValue::Create(value_factory, struct_proto));
  EXPECT_EQ(value->size(), 1);
  ASSERT_OK_AND_ASSIGN(auto key, value_factory.CreateStringValue("foo"));
  EXPECT_THAT(value->Get(value_factory, key),
              IsOkAndHolds(ValueOf<BoolValue>(value_factory, true)));
}

TEST_P(ProtoValueTest, StaticRValueStruct) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  google::protobuf::Value bool_value_proto;
  bool_value_proto.set_bool_value(true);
  google::protobuf::Struct struct_proto;
  struct_proto.mutable_fields()->insert({"foo", bool_value_proto});
  ASSERT_OK_AND_ASSIGN(
      auto value, ProtoValue::Create(value_factory, std::move(struct_proto)));
  EXPECT_EQ(value->size(), 1);
  ASSERT_OK_AND_ASSIGN(auto key, value_factory.CreateStringValue("foo"));
  EXPECT_THAT(value->Get(value_factory, key),
              IsOkAndHolds(ValueOf<BoolValue>(value_factory, true)));
}

enum class ProtoValueAnyTestRunner {
  kGenerated,
  kCustom,
};

template <typename S>
void AbslStringify(S& sink, ProtoValueAnyTestRunner value) {
  switch (value) {
    case ProtoValueAnyTestRunner::kGenerated:
      sink.Append("Generated");
      break;
    case ProtoValueAnyTestRunner::kCustom:
      sink.Append("Custom");
      break;
  }
}

class ProtoValueAnyTest : public ProtoTest<ProtoValueAnyTestRunner> {
 protected:
  template <typename T>
  void Run(
      const T& message,
      absl::FunctionRef<void(ValueFactory&, const Handle<Value>&)> tester) {
    google::protobuf::Any any;
    ASSERT_TRUE(any.PackFrom(message));
    switch (std::get<1>(GetParam())) {
      case ProtoValueAnyTestRunner::kGenerated: {
        TypeFactory type_factory(memory_manager());
        ProtoTypeProvider type_provider;
        TypeManager type_manager(type_factory, type_provider);
        ValueFactory value_factory(type_manager);
        ASSERT_OK_AND_ASSIGN(auto value,
                             ProtoValue::Create(value_factory, message));
        tester(value_factory, value);
        return;
      }
      case ProtoValueAnyTestRunner::kCustom: {
      }
        protobuf_internal::WithCustomDescriptorPool(
            memory_manager(), any, *T::descriptor(),
            [&](TypeProvider& type_provider,
                const google::protobuf::Message& custom_message) {
              TypeFactory type_factory(memory_manager());
              TypeManager type_manager(type_factory, type_provider);
              ValueFactory value_factory(type_manager);
              ASSERT_OK_AND_ASSIGN(
                  auto value,
                  ProtoValue::Create(value_factory, custom_message));
              tester(value_factory, value);
            });
        return;
    }
  }

  template <typename T>
  void Run(const T& message,
           absl::FunctionRef<void(const Handle<Value>&)> tester) {
    Run(message, [&](ValueFactory& value_factory, const Handle<Value>& value) {
      tester(value);
    });
  }
};

TEST_P(ProtoValueAnyTest, AnyBoolWrapper) {
  google::protobuf::BoolValue payload;
  payload.set_value(true);
  Run(payload, [](const Handle<Value>& value) {
    EXPECT_EQ(value.As<BoolValue>()->value(), true);
  });
}

TEST_P(ProtoValueAnyTest, AnyInt32Wrapper) {
  google::protobuf::Int32Value payload;
  payload.set_value(1);
  Run(payload, [](const Handle<Value>& value) {
    EXPECT_EQ(value.As<IntValue>()->value(), 1);
  });
}

TEST_P(ProtoValueAnyTest, AnyInt64Wrapper) {
  google::protobuf::Int64Value payload;
  payload.set_value(1);
  Run(payload, [](const Handle<Value>& value) {
    EXPECT_EQ(value.As<IntValue>()->value(), 1);
  });
}

TEST_P(ProtoValueAnyTest, AnyUInt32Wrapper) {
  google::protobuf::UInt32Value payload;
  payload.set_value(1);
  Run(payload, [](const Handle<Value>& value) {
    EXPECT_EQ(value.As<UintValue>()->value(), 1);
  });
}

TEST_P(ProtoValueAnyTest, AnyUInt64Wrapper) {
  google::protobuf::UInt64Value payload;
  payload.set_value(1);
  Run(payload, [](const Handle<Value>& value) {
    EXPECT_EQ(value.As<UintValue>()->value(), 1);
  });
}

TEST_P(ProtoValueAnyTest, AnyFloatWrapper) {
  google::protobuf::FloatValue payload;
  payload.set_value(1);
  Run(payload, [](const Handle<Value>& value) {
    EXPECT_EQ(value.As<DoubleValue>()->value(), 1);
  });
}

TEST_P(ProtoValueAnyTest, AnyDoubleWrapper) {
  google::protobuf::DoubleValue payload;
  payload.set_value(1);
  Run(payload, [](const Handle<Value>& value) {
    EXPECT_EQ(value.As<DoubleValue>()->value(), 1);
  });
}

TEST_P(ProtoValueAnyTest, AnyBytesWrapper) {
  google::protobuf::BytesValue payload;
  payload.set_value("foo");
  Run(payload, [](const Handle<Value>& value) {
    EXPECT_EQ(value.As<BytesValue>()->ToString(), "foo");
  });
}

TEST_P(ProtoValueAnyTest, AnyStringWrapper) {
  google::protobuf::StringValue payload;
  payload.set_value("foo");
  Run(payload, [](const Handle<Value>& value) {
    EXPECT_EQ(value.As<StringValue>()->ToString(), "foo");
  });
}

TEST_P(ProtoValueAnyTest, AnyDuration) {
  google::protobuf::Duration payload;
  payload.set_seconds(1);
  Run(payload, [](const Handle<Value>& value) {
    EXPECT_EQ(value.As<DurationValue>()->value(), absl::Seconds(1));
  });
}

TEST_P(ProtoValueAnyTest, AnyTimestamp) {
  google::protobuf::Timestamp payload;
  payload.set_seconds(1);
  Run(payload, [](const Handle<Value>& value) {
    EXPECT_EQ(value.As<TimestampValue>()->value(),
              absl::UnixEpoch() + absl::Seconds(1));
  });
}

TEST_P(ProtoValueAnyTest, AnyValue) {
  google::protobuf::Value payload;
  payload.set_bool_value(true);
  Run(payload, [](const Handle<Value>& value) {
    EXPECT_TRUE(value.As<BoolValue>()->value());
  });
}

TEST_P(ProtoValueAnyTest, AnyListValue) {
  google::protobuf::ListValue payload;
  payload.add_values()->set_bool_value(true);
  Run(payload, [](ValueFactory& value_factory, const Handle<Value>& value) {
    ASSERT_TRUE(value->Is<ListValue>());
    EXPECT_EQ(value.As<ListValue>()->size(), 1);
    ASSERT_OK_AND_ASSIGN(auto element,
                         value->As<ListValue>().Get(value_factory, 0));
    ASSERT_TRUE(element->Is<BoolValue>());
    EXPECT_TRUE(element.As<BoolValue>()->value());
  });
}

TEST_P(ProtoValueAnyTest, AnyMessage) {
  google::protobuf::Struct payload;
  payload.mutable_fields()->insert(
      {"foo", google::protobuf::Value::default_instance()});
  Run(payload, [](ValueFactory& value_factory, const Handle<Value>& value) {
    ASSERT_TRUE(value->Is<MapValue>());
    EXPECT_EQ(value.As<MapValue>()->size(), 1);
    ASSERT_OK_AND_ASSIGN(auto key, value_factory.CreateStringValue("foo"));
    ASSERT_OK_AND_ASSIGN(auto field,
                         value->As<MapValue>().Get(value_factory, key));
    ASSERT_TRUE(field->Is<NullValue>());
  });
}

TEST_P(ProtoValueAnyTest, AnyStruct) {
  google::protobuf::Api payload;
  payload.set_name("foo");
  Run(payload, [&payload](const Handle<Value>& value) {
    ASSERT_TRUE(value->Is<ProtoStructValue>());
    EXPECT_EQ(value->As<ProtoStructValue>().value()->SerializeAsString(),
              payload.SerializeAsString());
  });
}

INSTANTIATE_TEST_SUITE_P(
    ProtoValueAnyTest, ProtoValueAnyTest,
    testing::Combine(cel::base_internal::MemoryManagerTestModeAll(),
                     testing::Values(ProtoValueAnyTestRunner::kGenerated,
                                     ProtoValueAnyTestRunner::kCustom)));

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
