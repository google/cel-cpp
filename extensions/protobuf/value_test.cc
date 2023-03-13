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
#include "base/internal/memory_manager_testing.h"
#include "base/type_factory.h"
#include "base/type_manager.h"
#include "base/value_factory.h"
#include "extensions/protobuf/internal/testing.h"
#include "extensions/protobuf/type_provider.h"
#include "internal/testing.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"

namespace cel::extensions {
namespace {

using testing::EqualsProto;

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

TEST_P(ProtoValueTest, Enum) {
  TypeFactory type_factory(memory_manager());
  ProtoTypeProvider type_provider;
  TypeManager type_manager(type_factory, type_provider);
  ValueFactory value_factory(type_manager);
  TestAllTypes::NestedEnum enum_proto = TestAllTypes::BAR;
  ASSERT_OK_AND_ASSIGN(auto enum_value,
                       ProtoValue::Create(value_factory, enum_proto));
  EXPECT_EQ(enum_value->number(), static_cast<int64_t>(TestAllTypes::BAR));
}

INSTANTIATE_TEST_SUITE_P(ProtoValueTest, ProtoValueTest,
                         cel::base_internal::MemoryManagerTestModeAll(),
                         cel::base_internal::MemoryManagerTestModeTupleName);

}  // namespace
}  // namespace cel::extensions
