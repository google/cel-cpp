// Copyright 2024 Google LLC
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

#include <cstdint>
#include <limits>
#include <memory>
#include <utility>

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "common/value_testing.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/proto_matchers.h"
#include "internal/testing.h"
#include "proto/test/v1/proto2/test_all_types.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/text_format.h"

namespace cel::extensions {
namespace {

using ::cel::internal::test::EqualsProto;
using ::cel::test::BoolValueIs;
using ::cel::test::BytesValueIs;
using ::cel::test::DoubleValueIs;
using ::cel::test::DurationValueIs;
using ::cel::test::IntValueIs;
using ::cel::test::StringValueIs;
using ::cel::test::StructValueFieldHas;
using ::cel::test::StructValueFieldIs;
using ::cel::test::TimestampValueIs;
using ::cel::test::UintValueIs;
using ::cel::test::ValueKindIs;
using ::google::api::expr::test::v1::proto2::TestAllTypes;
using testing::Eq;
using testing::IsTrue;
using testing::Pointee;
using cel::internal::IsOk;
using cel::internal::IsOkAndHolds;
using cel::internal::StatusIs;

template <typename T>
T ParseTextOrDie(absl::string_view text) {
  T proto;
  ABSL_CHECK(google::protobuf::TextFormat::ParseFromString(text, &proto));
  return proto;
}

class ProtoValueTest : public common_internal::ThreadCompatibleValueTest<> {
 protected:
  MemoryManager NewThreadCompatiblePoolingMemoryManager() override {
    return ProtoMemoryManager();
  }
};

class ProtoValueWrapTest : public ProtoValueTest {};

TEST_P(ProtoValueWrapTest, ProtoEnumToValue) {
  ASSERT_OK_AND_ASSIGN(
      auto enum_value,
      ProtoEnumToValue(value_factory(),
                       google::protobuf::NullValue::NULL_VALUE));
  ASSERT_TRUE(InstanceOf<NullValue>(enum_value));
  ASSERT_OK_AND_ASSIGN(enum_value,
                       ProtoEnumToValue(value_factory(), TestAllTypes::BAR));
  ASSERT_TRUE(InstanceOf<IntValue>(enum_value));
  ASSERT_THAT(Cast<IntValue>(enum_value).NativeValue(), Eq(1));
}

TEST_P(ProtoValueWrapTest, ProtoBoolValueToValue) {
  google::protobuf::BoolValue message;
  message.set_value(true);
  EXPECT_THAT(ProtoMessageToValue(value_manager(), message),
              IsOkAndHolds(BoolValueIs(Eq(true))));
  EXPECT_THAT(ProtoMessageToValue(value_manager(), std::move(message)),
              IsOkAndHolds(BoolValueIs(Eq(true))));
}

TEST_P(ProtoValueWrapTest, ProtoInt32ValueToValue) {
  google::protobuf::Int32Value message;
  message.set_value(1);
  EXPECT_THAT(ProtoMessageToValue(value_manager(), message),
              IsOkAndHolds(IntValueIs(Eq(1))));
  EXPECT_THAT(ProtoMessageToValue(value_manager(), std::move(message)),
              IsOkAndHolds(IntValueIs(Eq(1))));
}

TEST_P(ProtoValueWrapTest, ProtoInt64ValueToValue) {
  google::protobuf::Int64Value message;
  message.set_value(1);
  EXPECT_THAT(ProtoMessageToValue(value_manager(), message),
              IsOkAndHolds(IntValueIs(Eq(1))));
  EXPECT_THAT(ProtoMessageToValue(value_manager(), std::move(message)),
              IsOkAndHolds(IntValueIs(Eq(1))));
}

TEST_P(ProtoValueWrapTest, ProtoUInt32ValueToValue) {
  google::protobuf::UInt32Value message;
  message.set_value(1);
  EXPECT_THAT(ProtoMessageToValue(value_manager(), message),
              IsOkAndHolds(UintValueIs(Eq(1))));
  EXPECT_THAT(ProtoMessageToValue(value_manager(), std::move(message)),
              IsOkAndHolds(UintValueIs(Eq(1))));
}

TEST_P(ProtoValueWrapTest, ProtoUInt64ValueToValue) {
  google::protobuf::UInt64Value message;
  message.set_value(1);
  EXPECT_THAT(ProtoMessageToValue(value_manager(), message),
              IsOkAndHolds(UintValueIs(Eq(1))));
  EXPECT_THAT(ProtoMessageToValue(value_manager(), std::move(message)),
              IsOkAndHolds(UintValueIs(Eq(1))));
}

TEST_P(ProtoValueWrapTest, ProtoFloatValueToValue) {
  google::protobuf::FloatValue message;
  message.set_value(1);
  EXPECT_THAT(ProtoMessageToValue(value_manager(), message),
              IsOkAndHolds(DoubleValueIs(Eq(1))));
  EXPECT_THAT(ProtoMessageToValue(value_manager(), std::move(message)),
              IsOkAndHolds(DoubleValueIs(Eq(1))));
}

TEST_P(ProtoValueWrapTest, ProtoDoubleValueToValue) {
  google::protobuf::DoubleValue message;
  message.set_value(1);
  EXPECT_THAT(ProtoMessageToValue(value_manager(), message),
              IsOkAndHolds(DoubleValueIs(Eq(1))));
  EXPECT_THAT(ProtoMessageToValue(value_manager(), std::move(message)),
              IsOkAndHolds(DoubleValueIs(Eq(1))));
}

TEST_P(ProtoValueWrapTest, ProtoBytesValueToValue) {
  google::protobuf::BytesValue message;
  message.set_value("foo");
  EXPECT_THAT(ProtoMessageToValue(value_manager(), message),
              IsOkAndHolds(BytesValueIs(Eq("foo"))));
  EXPECT_THAT(ProtoMessageToValue(value_manager(), std::move(message)),
              IsOkAndHolds(BytesValueIs(Eq("foo"))));
}

TEST_P(ProtoValueWrapTest, ProtoStringValueToValue) {
  google::protobuf::StringValue message;
  message.set_value("foo");
  EXPECT_THAT(ProtoMessageToValue(value_manager(), message),
              IsOkAndHolds(StringValueIs(Eq("foo"))));
  EXPECT_THAT(ProtoMessageToValue(value_manager(), std::move(message)),
              IsOkAndHolds(StringValueIs(Eq("foo"))));
}

TEST_P(ProtoValueWrapTest, ProtoDurationToValue) {
  google::protobuf::Duration message;
  message.set_seconds(1);
  message.set_nanos(1);
  EXPECT_THAT(ProtoMessageToValue(value_manager(), message),
              IsOkAndHolds(DurationValueIs(
                  Eq(absl::Seconds(1) + absl::Nanoseconds(1)))));
  EXPECT_THAT(ProtoMessageToValue(value_manager(), std::move(message)),
              IsOkAndHolds(DurationValueIs(
                  Eq(absl::Seconds(1) + absl::Nanoseconds(1)))));
}

TEST_P(ProtoValueWrapTest, ProtoTimestampToValue) {
  google::protobuf::Timestamp message;
  message.set_seconds(1);
  message.set_nanos(1);
  EXPECT_THAT(
      ProtoMessageToValue(value_manager(), message),
      IsOkAndHolds(TimestampValueIs(
          Eq(absl::UnixEpoch() + absl::Seconds(1) + absl::Nanoseconds(1)))));
  EXPECT_THAT(
      ProtoMessageToValue(value_manager(), std::move(message)),
      IsOkAndHolds(TimestampValueIs(
          Eq(absl::UnixEpoch() + absl::Seconds(1) + absl::Nanoseconds(1)))));
}

TEST_P(ProtoValueWrapTest, ProtoMessageToValue) {
  TestAllTypes message;
  EXPECT_THAT(ProtoMessageToValue(value_manager(), message),
              IsOkAndHolds(ValueKindIs(Eq(ValueKind::kStruct))));
  EXPECT_THAT(ProtoMessageToValue(value_manager(), std::move(message)),
              IsOkAndHolds(ValueKindIs(Eq(ValueKind::kStruct))));
}

TEST_P(ProtoValueWrapTest, GetFieldByName) {
  ASSERT_OK_AND_ASSIGN(
      auto value,
      ProtoMessageToValue(value_manager(), ParseTextOrDie<TestAllTypes>(
                                               R"pb(single_int32: 1,
                                                    single_int64: 1
                                                    single_uint32: 1
                                                    single_uint64: 1
                                                    single_float: 1
                                                    single_double: 1
                                                    single_bool: true
                                                    single_string: "foo"
                                                    single_bytes: "foo")pb")));
  EXPECT_THAT(value, StructValueIs(StructValueFieldIs(
                         &value_manager(), "single_int32", IntValueIs(Eq(1)))));
  EXPECT_THAT(value,
              StructValueIs(StructValueFieldHas("single_int32", IsTrue())));
  EXPECT_THAT(value, StructValueIs(StructValueFieldIs(
                         &value_manager(), "single_int64", IntValueIs(Eq(1)))));
  EXPECT_THAT(value,
              StructValueIs(StructValueFieldHas("single_int64", IsTrue())));
  EXPECT_THAT(
      value, StructValueIs(StructValueFieldIs(&value_manager(), "single_uint32",
                                              UintValueIs(Eq(1)))));
  EXPECT_THAT(value,
              StructValueIs(StructValueFieldHas("single_uint32", IsTrue())));
  EXPECT_THAT(
      value, StructValueIs(StructValueFieldIs(&value_manager(), "single_uint64",
                                              UintValueIs(Eq(1)))));
  EXPECT_THAT(value,
              StructValueIs(StructValueFieldHas("single_uint64", IsTrue())));
}

INSTANTIATE_TEST_SUITE_P(ProtoValueTest, ProtoValueWrapTest,
                         testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting),
                         ProtoValueTest::ToString);

}  // namespace
}  // namespace cel::extensions
