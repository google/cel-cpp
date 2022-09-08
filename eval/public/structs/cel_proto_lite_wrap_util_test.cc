// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "eval/public/structs/cel_proto_lite_wrap_util.h"

#include <cassert>
#include <limits>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "google/protobuf/dynamic_message.h"
#include "google/protobuf/message.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/time/time.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/structs/trivial_legacy_type_info.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/proto_time_encoding.h"
#include "internal/testing.h"
#include "testutil/util.h"

namespace google::api::expr::runtime::internal {

namespace {

using testing::Eq;
using testing::UnorderedPointwise;
using cel::internal::StatusIs;

using google::protobuf::Duration;
using google::protobuf::ListValue;
using google::protobuf::Struct;
using google::protobuf::Timestamp;
using google::protobuf::Value;

using google::protobuf::Any;
using google::protobuf::BoolValue;
using google::protobuf::BytesValue;
using google::protobuf::DoubleValue;
using google::protobuf::FloatValue;
using google::protobuf::Int32Value;
using google::protobuf::Int64Value;
using google::protobuf::StringValue;
using google::protobuf::UInt32Value;
using google::protobuf::UInt64Value;

using google::protobuf::Arena;

class CelProtoWrapperTest : public ::testing::Test {
 protected:
  CelProtoWrapperTest() : type_info_(TrivialTypeInfo::GetInstance()) {
    factory_.SetDelegateToGeneratedFactory(true);
  }

  template <class MessageType>
  void ExpectWrappedMessage(const CelValue& value, const MessageType& message) {
    // Test the input value wraps to the destination message type.
    MessageType* tested_message = nullptr;
    absl::StatusOr<MessageType*> result =
        CreateMessageFromValue(value, tested_message, arena());
    EXPECT_OK(result);
    tested_message = *result;
    EXPECT_TRUE(tested_message != nullptr);
    EXPECT_THAT(*tested_message, testutil::EqualsProto(message));

    // Test the same as above, but with allocated message.
    MessageType* created_message = Arena::CreateMessage<MessageType>(arena());
    result = CreateMessageFromValue(value, created_message, arena());
    EXPECT_EQ(created_message, *result);
    created_message = *result;
    EXPECT_TRUE(created_message != nullptr);
    EXPECT_THAT(*created_message, testutil::EqualsProto(message));
  }

  template <class MessageType, class T>
  void ExpectUnwrappedPrimitive(const MessageType& message, T result) {
    CelValue cel_value = CreateCelValue(message, type_info(), arena());
    T value;
    EXPECT_TRUE(cel_value.GetValue(&value));
    EXPECT_THAT(value, Eq(result));

    T dyn_value;
    auto reflected_copy = ReflectedCopy(message);
    absl::StatusOr<CelValue> cel_dyn_value =
        UnwrapFromWellKnownType(reflected_copy.get(), type_info(), arena());
    EXPECT_OK(cel_dyn_value.status());
    EXPECT_THAT(cel_dyn_value->type(), Eq(cel_value.type()));
    EXPECT_TRUE(cel_dyn_value->GetValue(&dyn_value));
    EXPECT_THAT(value, Eq(dyn_value));

    Any any;
    any.PackFrom(message);
    CelValue any_cel_value = CreateCelValue(any, type_info(), arena());
    LOG(INFO) << "vitos: " << message.DebugString()
              << ", cel_value: " << any_cel_value.DebugString();
    T any_value;
    EXPECT_TRUE(any_cel_value.GetValue(&any_value));
    EXPECT_THAT(any_value, Eq(result));
  }

  template <class MessageType>
  void ExpectUnwrappedMessage(const MessageType& message,
                              google::protobuf::Message* result) {
    CelValue cel_value = CreateCelValue(message, type_info(), arena());
    if (result == nullptr) {
      EXPECT_TRUE(cel_value.IsNull());
      return;
    }
    EXPECT_TRUE(cel_value.IsMessage());
    EXPECT_THAT(cel_value.MessageOrDie(), testutil::EqualsProto(*result));
  }

  std::unique_ptr<google::protobuf::Message> ReflectedCopy(
      const google::protobuf::Message& message) {
    std::unique_ptr<google::protobuf::Message> dynamic_value(
        factory_.GetPrototype(message.GetDescriptor())->New());
    dynamic_value->CopyFrom(message);
    return dynamic_value;
  }

  Arena* arena() { return &arena_; }
  const LegacyTypeInfoApis* type_info() const { return type_info_; }

 private:
  Arena arena_;
  const LegacyTypeInfoApis* type_info_;
  google::protobuf::DynamicMessageFactory factory_;
};

TEST_F(CelProtoWrapperTest, TestType) {
  Duration msg_duration;
  msg_duration.set_seconds(2);
  msg_duration.set_nanos(3);

  CelValue value_duration2 = CreateCelValue(msg_duration, type_info(), arena());
  EXPECT_THAT(value_duration2.type(), Eq(CelValue::Type::kDuration));

  Timestamp msg_timestamp;
  msg_timestamp.set_seconds(2);
  msg_timestamp.set_nanos(3);

  CelValue value_timestamp2 =
      CreateCelValue(msg_timestamp, type_info(), arena());
  EXPECT_THAT(value_timestamp2.type(), Eq(CelValue::Type::kTimestamp));
}

// This test verifies CelValue support of Duration type.
TEST_F(CelProtoWrapperTest, TestDuration) {
  Duration msg_duration;
  msg_duration.set_seconds(2);
  msg_duration.set_nanos(3);
  CelValue value = CreateCelValue(msg_duration, type_info(), arena());
  EXPECT_THAT(value.type(), Eq(CelValue::Type::kDuration));

  Duration out;
  auto status = cel::internal::EncodeDuration(value.DurationOrDie(), &out);
  EXPECT_TRUE(status.ok());
  EXPECT_THAT(out, testutil::EqualsProto(msg_duration));
}

// This test verifies CelValue support of Timestamp type.
TEST_F(CelProtoWrapperTest, TestTimestamp) {
  Timestamp msg_timestamp;
  msg_timestamp.set_seconds(2);
  msg_timestamp.set_nanos(3);

  CelValue value = CreateCelValue(msg_timestamp, type_info(), arena());

  EXPECT_TRUE(value.IsTimestamp());
  Timestamp out;
  auto status = cel::internal::EncodeTime(value.TimestampOrDie(), &out);
  EXPECT_TRUE(status.ok());
  EXPECT_THAT(out, testutil::EqualsProto(msg_timestamp));
}

// Dynamic Values test
//
TEST_F(CelProtoWrapperTest, CreateCelValueNull) {
  Value json;
  json.set_null_value(google::protobuf::NullValue::NULL_VALUE);
  ExpectUnwrappedMessage(json, nullptr);
}

// Test support for unwrapping a google::protobuf::Value to a CEL value.
TEST_F(CelProtoWrapperTest, UnwrapDynamicValueNull) {
  Value value_msg;
  value_msg.set_null_value(google::protobuf::NullValue::NULL_VALUE);

  ASSERT_OK_AND_ASSIGN(CelValue value,
                       UnwrapFromWellKnownType(ReflectedCopy(value_msg).get(),
                                               type_info(), arena()));
  EXPECT_TRUE(value.IsNull());
}

TEST_F(CelProtoWrapperTest, CreateCelValueBool) {
  bool value = true;

  CelValue cel_value = CreateCelValue(value, type_info(), arena());
  EXPECT_TRUE(cel_value.IsBool());
  EXPECT_EQ(cel_value.BoolOrDie(), value);

  Value json;
  json.set_bool_value(true);
  ExpectUnwrappedPrimitive(json, value);
}

TEST_F(CelProtoWrapperTest, CreateCelValueDouble) {
  double value = 1.0;

  CelValue cel_value = CreateCelValue(value, type_info(), arena());
  EXPECT_TRUE(cel_value.IsDouble());
  EXPECT_DOUBLE_EQ(cel_value.DoubleOrDie(), value);

  cel_value = CreateCelValue(static_cast<float>(value), type_info(), arena());
  EXPECT_TRUE(cel_value.IsDouble());
  EXPECT_DOUBLE_EQ(cel_value.DoubleOrDie(), value);

  Value json;
  json.set_number_value(value);
  ExpectUnwrappedPrimitive(json, value);
}

TEST_F(CelProtoWrapperTest, CreateCelValueInt) {
  int64_t value = 10;

  CelValue cel_value = CreateCelValue(value, type_info(), arena());
  EXPECT_TRUE(cel_value.IsInt64());
  EXPECT_EQ(cel_value.Int64OrDie(), value);

  cel_value = CreateCelValue(static_cast<int32_t>(value), type_info(), arena());
  EXPECT_TRUE(cel_value.IsInt64());
  EXPECT_EQ(cel_value.Int64OrDie(), value);
}

TEST_F(CelProtoWrapperTest, CreateCelValueUint) {
  uint64_t value = 10;

  CelValue cel_value = CreateCelValue(value, type_info(), arena());
  EXPECT_TRUE(cel_value.IsUint64());
  EXPECT_EQ(cel_value.Uint64OrDie(), value);

  cel_value =
      CreateCelValue(static_cast<uint32_t>(value), type_info(), arena());
  EXPECT_TRUE(cel_value.IsUint64());
  EXPECT_EQ(cel_value.Uint64OrDie(), value);
}

TEST_F(CelProtoWrapperTest, CreateCelValueString) {
  const std::string test = "test";
  auto value = CelValue::StringHolder(&test);

  CelValue cel_value = CreateCelValue(test, type_info(), arena());
  EXPECT_TRUE(cel_value.IsString());
  EXPECT_EQ(cel_value.StringOrDie().value(), test);

  Value json;
  json.set_string_value(test);
  ExpectUnwrappedPrimitive(json, value);
}

TEST_F(CelProtoWrapperTest, CreateCelValueCord) {
  const std::string test1 = "test1";
  const std::string test2 = "test2";
  absl::Cord value;
  value.Append(test1);
  value.Append(test2);
  CelValue cel_value = CreateCelValue(value, type_info(), arena());
  EXPECT_TRUE(cel_value.IsBytes());
  EXPECT_EQ(cel_value.BytesOrDie().value(), test1 + test2);
}

TEST_F(CelProtoWrapperTest, CreateCelValueStruct) {
  const std::vector<std::string> kFields = {"field1", "field2", "field3"};
  Struct value_struct;

  auto& value1 = (*value_struct.mutable_fields())[kFields[0]];
  value1.set_bool_value(true);

  auto& value2 = (*value_struct.mutable_fields())[kFields[1]];
  value2.set_number_value(1.0);

  auto& value3 = (*value_struct.mutable_fields())[kFields[2]];
  value3.set_string_value("test");

  CelValue value = CreateCelValue(value_struct, type_info(), arena());
  ASSERT_TRUE(value.IsMap());

  const CelMap* cel_map = value.MapOrDie();
  EXPECT_EQ(cel_map->size(), 3);

  CelValue field1 = CelValue::CreateString(&kFields[0]);
  auto field1_presence = cel_map->Has(field1);
  ASSERT_OK(field1_presence);
  EXPECT_TRUE(*field1_presence);
  auto lookup1 = (*cel_map)[field1];
  ASSERT_TRUE(lookup1.has_value());
  ASSERT_TRUE(lookup1->IsBool());
  EXPECT_EQ(lookup1->BoolOrDie(), true);

  CelValue field2 = CelValue::CreateString(&kFields[1]);
  auto field2_presence = cel_map->Has(field2);
  ASSERT_OK(field2_presence);
  EXPECT_TRUE(*field2_presence);
  auto lookup2 = (*cel_map)[field2];
  ASSERT_TRUE(lookup2.has_value());
  ASSERT_TRUE(lookup2->IsDouble());
  EXPECT_DOUBLE_EQ(lookup2->DoubleOrDie(), 1.0);

  CelValue field3 = CelValue::CreateString(&kFields[2]);
  auto field3_presence = cel_map->Has(field3);
  ASSERT_OK(field3_presence);
  EXPECT_TRUE(*field3_presence);
  auto lookup3 = (*cel_map)[field3];
  ASSERT_TRUE(lookup3.has_value());
  ASSERT_TRUE(lookup3->IsString());
  EXPECT_EQ(lookup3->StringOrDie().value(), "test");

  CelValue wrong_key = CelValue::CreateBool(true);
  EXPECT_THAT(cel_map->Has(wrong_key),
              StatusIs(absl::StatusCode::kInvalidArgument));
  absl::optional<CelValue> lockup_wrong_key = (*cel_map)[wrong_key];
  ASSERT_TRUE(lockup_wrong_key.has_value());
  EXPECT_TRUE((*lockup_wrong_key).IsError());

  std::string missing = "missing_field";
  CelValue missing_field = CelValue::CreateString(&missing);
  auto missing_field_presence = cel_map->Has(missing_field);
  ASSERT_OK(missing_field_presence);
  EXPECT_FALSE(*missing_field_presence);
  EXPECT_EQ((*cel_map)[missing_field], absl::nullopt);

  const CelList* key_list = cel_map->ListKeys().value();
  ASSERT_EQ(key_list->size(), kFields.size());

  std::vector<std::string> result_keys;
  for (int i = 0; i < key_list->size(); i++) {
    CelValue key = (*key_list)[i];
    ASSERT_TRUE(key.IsString());
    result_keys.push_back(std::string(key.StringOrDie().value()));
  }

  EXPECT_THAT(result_keys, UnorderedPointwise(Eq(), kFields));
}

// Test support for google::protobuf::Struct when it is created as dynamic
// message
TEST_F(CelProtoWrapperTest, UnwrapDynamicStruct) {
  Struct struct_msg;
  const std::string kFieldInt = "field_int";
  const std::string kFieldBool = "field_bool";
  (*struct_msg.mutable_fields())[kFieldInt].set_number_value(1.);
  (*struct_msg.mutable_fields())[kFieldBool].set_bool_value(true);
  auto reflected_copy = ReflectedCopy(struct_msg);
  ASSERT_OK_AND_ASSIGN(
      CelValue value,
      UnwrapFromWellKnownType(reflected_copy.get(), type_info(), arena()));
  EXPECT_TRUE(value.IsMap());
  const CelMap* cel_map = value.MapOrDie();
  ASSERT_TRUE(cel_map != nullptr);

  {
    auto lookup = (*cel_map)[CelValue::CreateString(&kFieldInt)];
    ASSERT_TRUE(lookup.has_value());
    auto v = lookup.value();
    ASSERT_TRUE(v.IsDouble());
    EXPECT_THAT(v.DoubleOrDie(), testing::DoubleEq(1.));
  }
  {
    auto lookup = (*cel_map)[CelValue::CreateString(&kFieldBool)];
    ASSERT_TRUE(lookup.has_value());
    auto v = lookup.value();
    ASSERT_TRUE(v.IsBool());
    EXPECT_EQ(v.BoolOrDie(), true);
  }
  {
    auto presence = cel_map->Has(CelValue::CreateBool(true));
    ASSERT_FALSE(presence.ok());
    EXPECT_EQ(presence.status().code(), absl::StatusCode::kInvalidArgument);
    auto lookup = (*cel_map)[CelValue::CreateBool(true)];
    ASSERT_TRUE(lookup.has_value());
    auto v = lookup.value();
    ASSERT_TRUE(v.IsError());
  }
}

TEST_F(CelProtoWrapperTest, UnwrapDynamicValueStruct) {
  const std::string kField1 = "field1";
  const std::string kField2 = "field2";
  Value value_msg;
  (*value_msg.mutable_struct_value()->mutable_fields())[kField1]
      .set_number_value(1);
  (*value_msg.mutable_struct_value()->mutable_fields())[kField2]
      .set_number_value(2);
  auto reflected_copy = ReflectedCopy(value_msg);
  ASSERT_OK_AND_ASSIGN(
      CelValue value,
      UnwrapFromWellKnownType(reflected_copy.get(), type_info(), arena()));
  EXPECT_TRUE(value.IsMap());
  EXPECT_TRUE(
      (*value.MapOrDie())[CelValue::CreateString(&kField1)].has_value());
  EXPECT_TRUE(
      (*value.MapOrDie())[CelValue::CreateString(&kField2)].has_value());
}

TEST_F(CelProtoWrapperTest, CreateCelValueList) {
  const std::vector<std::string> kFields = {"field1", "field2", "field3"};

  ListValue list_value;

  list_value.add_values()->set_bool_value(true);
  list_value.add_values()->set_number_value(1.0);
  list_value.add_values()->set_string_value("test");

  CelValue value = CreateCelValue(list_value, type_info(), arena());
  ASSERT_TRUE(value.IsList());

  const CelList* cel_list = value.ListOrDie();

  ASSERT_EQ(cel_list->size(), 3);

  CelValue value1 = (*cel_list)[0];
  ASSERT_TRUE(value1.IsBool());
  EXPECT_EQ(value1.BoolOrDie(), true);

  auto value2 = (*cel_list)[1];
  ASSERT_TRUE(value2.IsDouble());
  EXPECT_DOUBLE_EQ(value2.DoubleOrDie(), 1.0);

  auto value3 = (*cel_list)[2];
  ASSERT_TRUE(value3.IsString());
  EXPECT_EQ(value3.StringOrDie().value(), "test");

  Value proto_value;
  *proto_value.mutable_list_value() = list_value;
  CelValue cel_value = CreateCelValue(list_value, type_info(), arena());
  ASSERT_TRUE(cel_value.IsList());
}

TEST_F(CelProtoWrapperTest, UnwrapListValue) {
  Value value_msg;
  value_msg.mutable_list_value()->add_values()->set_number_value(1.);
  value_msg.mutable_list_value()->add_values()->set_number_value(2.);

  ASSERT_OK_AND_ASSIGN(
      CelValue value,
      UnwrapFromWellKnownType(&value_msg.list_value(), type_info(), arena()));
  EXPECT_TRUE(value.IsList());
  EXPECT_THAT((*value.ListOrDie())[0].DoubleOrDie(), testing::DoubleEq(1));
  EXPECT_THAT((*value.ListOrDie())[1].DoubleOrDie(), testing::DoubleEq(2));
}

TEST_F(CelProtoWrapperTest, UnwrapDynamicValueListValue) {
  Value value_msg;
  value_msg.mutable_list_value()->add_values()->set_number_value(1.);
  value_msg.mutable_list_value()->add_values()->set_number_value(2.);

  auto reflected_copy = ReflectedCopy(value_msg);
  ASSERT_OK_AND_ASSIGN(
      CelValue value,
      UnwrapFromWellKnownType(reflected_copy.get(), type_info(), arena()));
  EXPECT_TRUE(value.IsList());
  EXPECT_THAT((*value.ListOrDie())[0].DoubleOrDie(), testing::DoubleEq(1));
  EXPECT_THAT((*value.ListOrDie())[1].DoubleOrDie(), testing::DoubleEq(2));
}

TEST_F(CelProtoWrapperTest, UnwrapNullptr) {
  google::protobuf::MessageLite* msg = nullptr;
  ASSERT_OK_AND_ASSIGN(CelValue value,
                       UnwrapFromWellKnownType(msg, type_info(), arena()));
  EXPECT_TRUE(value.IsNull());
}

TEST_F(CelProtoWrapperTest, UnwrapDuration) {
  Duration duration;
  duration.set_seconds(10);
  ASSERT_OK_AND_ASSIGN(
      CelValue value, UnwrapFromWellKnownType(&duration, type_info(), arena()));
  EXPECT_TRUE(value.IsDuration());
  EXPECT_EQ(value.DurationOrDie() / absl::Seconds(1), 10);
}

TEST_F(CelProtoWrapperTest, UnwrapTimestamp) {
  Timestamp t;
  t.set_seconds(1615852799);

  ASSERT_OK_AND_ASSIGN(CelValue value,
                       UnwrapFromWellKnownType(&t, type_info(), arena()));
  EXPECT_TRUE(value.IsTimestamp());
  EXPECT_EQ(value.TimestampOrDie(), absl::FromUnixSeconds(1615852799));
}

TEST_F(CelProtoWrapperTest, UnwrapUnknown) {
  TestMessage msg;
  EXPECT_THAT(UnwrapFromWellKnownType(&msg, type_info(), arena()),
              StatusIs(absl::StatusCode::kNotFound));
}

// Test support of google.protobuf.Any in CelValue.
TEST_F(CelProtoWrapperTest, UnwrapAnyValue) {
  const std::string test = "test";
  auto string_value = CelValue::StringHolder(&test);

  Value json;
  json.set_string_value(test);

  Any any;
  any.PackFrom(json);
  ExpectUnwrappedPrimitive(any, string_value);
}

TEST_F(CelProtoWrapperTest, UnwrapAnyOfNonWellKnownType) {
  TestMessage test_message;
  test_message.set_string_value("test");

  Any any;
  any.PackFrom(test_message);
  EXPECT_TRUE(CreateCelValue(any, type_info(), arena()).IsError());
}

TEST_F(CelProtoWrapperTest, UnwrapNestedAny) {
  TestMessage test_message;
  test_message.set_string_value("test");

  Any any1;
  any1.PackFrom(test_message);
  Any any2;
  any2.PackFrom(any1);
  EXPECT_TRUE(CreateCelValue(any2, type_info(), arena()).IsError());
}

TEST_F(CelProtoWrapperTest, UnwrapInvalidAny) {
  Any any;
  CelValue value = CreateCelValue(any, type_info(), arena());
  ASSERT_TRUE(value.IsError());

  any.set_type_url("/");
  ASSERT_TRUE(CreateCelValue(any, type_info(), arena()).IsError());

  any.set_type_url("/invalid.proto.name");
  ASSERT_TRUE(CreateCelValue(any, type_info(), arena()).IsError());
}

// Test support of google.protobuf.<Type>Value wrappers in CelValue.
TEST_F(CelProtoWrapperTest, UnwrapBoolWrapper) {
  bool value = true;

  BoolValue wrapper;
  wrapper.set_value(value);
  ExpectUnwrappedPrimitive(wrapper, value);
}

TEST_F(CelProtoWrapperTest, UnwrapInt32Wrapper) {
  int64_t value = 12;

  Int32Value wrapper;
  wrapper.set_value(value);
  ExpectUnwrappedPrimitive(wrapper, value);
}

TEST_F(CelProtoWrapperTest, UnwrapUInt32Wrapper) {
  uint64_t value = 12;

  UInt32Value wrapper;
  wrapper.set_value(value);
  ExpectUnwrappedPrimitive(wrapper, value);
}

TEST_F(CelProtoWrapperTest, UnwrapInt64Wrapper) {
  int64_t value = 12;

  Int64Value wrapper;
  wrapper.set_value(value);
  ExpectUnwrappedPrimitive(wrapper, value);
}

TEST_F(CelProtoWrapperTest, UnwrapUInt64Wrapper) {
  uint64_t value = 12;

  UInt64Value wrapper;
  wrapper.set_value(value);
  ExpectUnwrappedPrimitive(wrapper, value);
}

TEST_F(CelProtoWrapperTest, UnwrapFloatWrapper) {
  double value = 42.5;

  FloatValue wrapper;
  wrapper.set_value(value);
  ExpectUnwrappedPrimitive(wrapper, value);
}

TEST_F(CelProtoWrapperTest, UnwrapDoubleWrapper) {
  double value = 42.5;

  DoubleValue wrapper;
  wrapper.set_value(value);
  ExpectUnwrappedPrimitive(wrapper, value);
}

TEST_F(CelProtoWrapperTest, UnwrapStringWrapper) {
  std::string text = "42";
  auto value = CelValue::StringHolder(&text);

  StringValue wrapper;
  wrapper.set_value(text);
  ExpectUnwrappedPrimitive(wrapper, value);
}

TEST_F(CelProtoWrapperTest, UnwrapBytesWrapper) {
  std::string text = "42";
  auto value = CelValue::BytesHolder(&text);

  BytesValue wrapper;
  wrapper.set_value("42");
  ExpectUnwrappedPrimitive(wrapper, value);
}

TEST_F(CelProtoWrapperTest, WrapNull) {
  auto cel_value = CelValue::CreateNull();

  Value json;
  json.set_null_value(protobuf::NULL_VALUE);
  ExpectWrappedMessage(cel_value, json);

  Any any;
  any.PackFrom(json);
  ExpectWrappedMessage(cel_value, any);
}

TEST_F(CelProtoWrapperTest, WrapBool) {
  auto cel_value = CelValue::CreateBool(true);

  Value json;
  json.set_bool_value(true);
  ExpectWrappedMessage(cel_value, json);

  BoolValue wrapper;
  wrapper.set_value(true);
  ExpectWrappedMessage(cel_value, wrapper);

  Any any;
  any.PackFrom(wrapper);
  ExpectWrappedMessage(cel_value, any);
}

TEST_F(CelProtoWrapperTest, WrapBytes) {
  std::string str = "hello world";
  auto cel_value = CelValue::CreateBytes(CelValue::BytesHolder(&str));

  BytesValue wrapper;
  wrapper.set_value(str);
  ExpectWrappedMessage(cel_value, wrapper);

  Any any;
  any.PackFrom(wrapper);
  ExpectWrappedMessage(cel_value, any);
}

TEST_F(CelProtoWrapperTest, WrapBytesToValue) {
  std::string str = "hello world";
  auto cel_value = CelValue::CreateBytes(CelValue::BytesHolder(&str));

  Value json;
  json.set_string_value("aGVsbG8gd29ybGQ=");
  ExpectWrappedMessage(cel_value, json);
}

TEST_F(CelProtoWrapperTest, WrapDuration) {
  auto cel_value = CelValue::CreateDuration(absl::Seconds(300));

  Duration d;
  d.set_seconds(300);
  ExpectWrappedMessage(cel_value, d);

  Any any;
  any.PackFrom(d);
  ExpectWrappedMessage(cel_value, any);
}

TEST_F(CelProtoWrapperTest, WrapDurationToValue) {
  auto cel_value = CelValue::CreateDuration(absl::Seconds(300));

  Value json;
  json.set_string_value("300s");
  ExpectWrappedMessage(cel_value, json);
}

TEST_F(CelProtoWrapperTest, WrapDouble) {
  double num = 1.5;
  auto cel_value = CelValue::CreateDouble(num);

  Value json;
  json.set_number_value(num);
  ExpectWrappedMessage(cel_value, json);

  DoubleValue wrapper;
  wrapper.set_value(num);
  ExpectWrappedMessage(cel_value, wrapper);

  Any any;
  any.PackFrom(wrapper);
  ExpectWrappedMessage(cel_value, any);
}

TEST_F(CelProtoWrapperTest, WrapDoubleToFloatValue) {
  double num = 1.5;
  auto cel_value = CelValue::CreateDouble(num);

  FloatValue wrapper;
  wrapper.set_value(num);
  ExpectWrappedMessage(cel_value, wrapper);

  // Imprecise double -> float representation results in truncation.
  double small_num = -9.9e-100;
  wrapper.set_value(small_num);
  cel_value = CelValue::CreateDouble(small_num);
  ExpectWrappedMessage(cel_value, wrapper);
}

TEST_F(CelProtoWrapperTest, WrapDoubleOverflow) {
  double lowest_double = std::numeric_limits<double>::lowest();
  auto cel_value = CelValue::CreateDouble(lowest_double);

  // Double exceeds float precision, overflow to -infinity.
  FloatValue wrapper;
  wrapper.set_value(-std::numeric_limits<float>::infinity());
  ExpectWrappedMessage(cel_value, wrapper);

  double max_double = std::numeric_limits<double>::max();
  cel_value = CelValue::CreateDouble(max_double);

  wrapper.set_value(std::numeric_limits<float>::infinity());
  ExpectWrappedMessage(cel_value, wrapper);
}

TEST_F(CelProtoWrapperTest, WrapInt64) {
  int32_t num = std::numeric_limits<int32_t>::lowest();
  auto cel_value = CelValue::CreateInt64(num);

  Value json;
  json.set_number_value(static_cast<double>(num));
  ExpectWrappedMessage(cel_value, json);

  Int64Value wrapper;
  wrapper.set_value(num);
  ExpectWrappedMessage(cel_value, wrapper);

  Any any;
  any.PackFrom(wrapper);
  ExpectWrappedMessage(cel_value, any);
}

TEST_F(CelProtoWrapperTest, WrapInt64ToInt32Value) {
  int32_t num = std::numeric_limits<int32_t>::lowest();
  auto cel_value = CelValue::CreateInt64(num);

  Int32Value wrapper;
  wrapper.set_value(num);
  ExpectWrappedMessage(cel_value, wrapper);
}

TEST_F(CelProtoWrapperTest, WrapFailureInt64ToInt32Value) {
  int64_t num = std::numeric_limits<int64_t>::lowest();
  auto cel_value = CelValue::CreateInt64(num);

  Int32Value* result = nullptr;
  EXPECT_THAT(CreateMessageFromValue(cel_value, result, arena()),
              StatusIs(absl::StatusCode::kInternal));
}

TEST_F(CelProtoWrapperTest, WrapInt64ToValue) {
  int64_t max = std::numeric_limits<int64_t>::max();
  auto cel_value = CelValue::CreateInt64(max);

  Value json;
  json.set_string_value(absl::StrCat(max));
  ExpectWrappedMessage(cel_value, json);

  int64_t min = std::numeric_limits<int64_t>::min();
  cel_value = CelValue::CreateInt64(min);

  json.set_string_value(absl::StrCat(min));
  ExpectWrappedMessage(cel_value, json);
}

TEST_F(CelProtoWrapperTest, WrapUint64) {
  uint32_t num = std::numeric_limits<uint32_t>::max();
  auto cel_value = CelValue::CreateUint64(num);

  Value json;
  json.set_number_value(static_cast<double>(num));
  ExpectWrappedMessage(cel_value, json);

  UInt64Value wrapper;
  wrapper.set_value(num);
  ExpectWrappedMessage(cel_value, wrapper);

  Any any;
  any.PackFrom(wrapper);
  ExpectWrappedMessage(cel_value, any);
}

TEST_F(CelProtoWrapperTest, WrapUint64ToUint32Value) {
  uint32_t num = std::numeric_limits<uint32_t>::max();
  auto cel_value = CelValue::CreateUint64(num);

  UInt32Value wrapper;
  wrapper.set_value(num);
  ExpectWrappedMessage(cel_value, wrapper);
}

TEST_F(CelProtoWrapperTest, WrapUint64ToValue) {
  uint64_t num = std::numeric_limits<uint64_t>::max();
  auto cel_value = CelValue::CreateUint64(num);

  Value json;
  json.set_string_value(absl::StrCat(num));
  ExpectWrappedMessage(cel_value, json);
}

TEST_F(CelProtoWrapperTest, WrapFailureUint64ToUint32Value) {
  uint64_t num = std::numeric_limits<uint64_t>::max();
  auto cel_value = CelValue::CreateUint64(num);

  UInt32Value* result = nullptr;
  EXPECT_THAT(CreateMessageFromValue(cel_value, result, arena()),
              StatusIs(absl::StatusCode::kInternal));
}

TEST_F(CelProtoWrapperTest, WrapString) {
  std::string str = "test";
  auto cel_value = CelValue::CreateString(CelValue::StringHolder(&str));

  Value json;
  json.set_string_value(str);
  ExpectWrappedMessage(cel_value, json);

  StringValue wrapper;
  wrapper.set_value(str);
  ExpectWrappedMessage(cel_value, wrapper);

  Any any;
  any.PackFrom(wrapper);
  ExpectWrappedMessage(cel_value, any);
}

TEST_F(CelProtoWrapperTest, WrapTimestamp) {
  absl::Time ts = absl::FromUnixSeconds(1615852799);
  auto cel_value = CelValue::CreateTimestamp(ts);

  Timestamp t;
  t.set_seconds(1615852799);
  ExpectWrappedMessage(cel_value, t);

  Any any;
  any.PackFrom(t);
  ExpectWrappedMessage(cel_value, any);
}

TEST_F(CelProtoWrapperTest, WrapTimestampToValue) {
  absl::Time ts = absl::FromUnixSeconds(1615852799);
  auto cel_value = CelValue::CreateTimestamp(ts);

  Value json;
  json.set_string_value("2021-03-15T23:59:59Z");
  ExpectWrappedMessage(cel_value, json);
}

TEST_F(CelProtoWrapperTest, WrapList) {
  std::vector<CelValue> list_elems = {
      CelValue::CreateDouble(1.5),
      CelValue::CreateInt64(-2L),
  };
  ContainerBackedListImpl list(std::move(list_elems));
  auto cel_value = CelValue::CreateList(&list);

  Value json;
  json.mutable_list_value()->add_values()->set_number_value(1.5);
  json.mutable_list_value()->add_values()->set_number_value(-2.);
  ExpectWrappedMessage(cel_value, json);
  ExpectWrappedMessage(cel_value, json.list_value());

  Any any;
  any.PackFrom(json.list_value());
  ExpectWrappedMessage(cel_value, any);
}

TEST_F(CelProtoWrapperTest, WrapFailureListValueBadJSON) {
  TestMessage message;
  std::vector<CelValue> list_elems = {
      CelValue::CreateDouble(1.5),
      CreateCelValue(message, type_info(), arena()),
  };
  ContainerBackedListImpl list(std::move(list_elems));
  auto cel_value = CelValue::CreateList(&list);

  Value* json = nullptr;
  EXPECT_THAT(CreateMessageFromValue(cel_value, json, arena()),
              StatusIs(absl::StatusCode::kInternal));
}

TEST_F(CelProtoWrapperTest, WrapStruct) {
  const std::string kField1 = "field1";
  std::vector<std::pair<CelValue, CelValue>> args = {
      {CelValue::CreateString(CelValue::StringHolder(&kField1)),
       CelValue::CreateBool(true)}};
  auto cel_map =
      CreateContainerBackedMap(
          absl::Span<std::pair<CelValue, CelValue>>(args.data(), args.size()))
          .value();
  auto cel_value = CelValue::CreateMap(cel_map.get());

  Value json;
  (*json.mutable_struct_value()->mutable_fields())[kField1].set_bool_value(
      true);
  ExpectWrappedMessage(cel_value, json);
  ExpectWrappedMessage(cel_value, json.struct_value());

  Any any;
  any.PackFrom(json.struct_value());
  ExpectWrappedMessage(cel_value, any);
}

TEST_F(CelProtoWrapperTest, WrapFailureStructBadKeyType) {
  std::vector<std::pair<CelValue, CelValue>> args = {
      {CelValue::CreateInt64(1L), CelValue::CreateBool(true)}};
  auto cel_map =
      CreateContainerBackedMap(
          absl::Span<std::pair<CelValue, CelValue>>(args.data(), args.size()))
          .value();
  auto cel_value = CelValue::CreateMap(cel_map.get());

  Value* json = nullptr;
  EXPECT_THAT(CreateMessageFromValue(cel_value, json, arena()),
              StatusIs(absl::StatusCode::kInternal));
}

TEST_F(CelProtoWrapperTest, WrapFailureStructBadValueType) {
  const std::string kField1 = "field1";
  TestMessage bad_value;
  std::vector<std::pair<CelValue, CelValue>> args = {
      {CelValue::CreateString(CelValue::StringHolder(&kField1)),
       CreateCelValue(bad_value, type_info(), arena())}};
  auto cel_map =
      CreateContainerBackedMap(
          absl::Span<std::pair<CelValue, CelValue>>(args.data(), args.size()))
          .value();
  auto cel_value = CelValue::CreateMap(cel_map.get());
  Value* json = nullptr;
  EXPECT_THAT(CreateMessageFromValue(cel_value, json, arena()),
              StatusIs(absl::StatusCode::kInternal));
}

TEST_F(CelProtoWrapperTest, WrapFailureWrongType) {
  auto cel_value = CelValue::CreateNull();
  {
    BoolValue* wrong_type = nullptr;
    EXPECT_THAT(CreateMessageFromValue(cel_value, wrong_type, arena()),
                StatusIs(absl::StatusCode::kInternal));
  }
  {
    BytesValue* wrong_type = nullptr;
    EXPECT_THAT(CreateMessageFromValue(cel_value, wrong_type, arena()),
                StatusIs(absl::StatusCode::kInternal));
  }
  {
    DoubleValue* wrong_type = nullptr;
    EXPECT_THAT(CreateMessageFromValue(cel_value, wrong_type, arena()),
                StatusIs(absl::StatusCode::kInternal));
  }
  {
    Duration* wrong_type = nullptr;
    EXPECT_THAT(CreateMessageFromValue(cel_value, wrong_type, arena()),
                StatusIs(absl::StatusCode::kInternal));
  }
  {
    FloatValue* wrong_type = nullptr;
    EXPECT_THAT(CreateMessageFromValue(cel_value, wrong_type, arena()),
                StatusIs(absl::StatusCode::kInternal));
  }
  {
    Int32Value* wrong_type = nullptr;
    EXPECT_THAT(CreateMessageFromValue(cel_value, wrong_type, arena()),
                StatusIs(absl::StatusCode::kInternal));
  }
  {
    Int64Value* wrong_type = nullptr;
    EXPECT_THAT(CreateMessageFromValue(cel_value, wrong_type, arena()),
                StatusIs(absl::StatusCode::kInternal));
  }
  {
    ListValue* wrong_type = nullptr;
    EXPECT_THAT(CreateMessageFromValue(cel_value, wrong_type, arena()),
                StatusIs(absl::StatusCode::kInternal));
  }
  {
    StringValue* wrong_type = nullptr;
    EXPECT_THAT(CreateMessageFromValue(cel_value, wrong_type, arena()),
                StatusIs(absl::StatusCode::kInternal));
  }
  {
    Struct* wrong_type = nullptr;
    EXPECT_THAT(CreateMessageFromValue(cel_value, wrong_type, arena()),
                StatusIs(absl::StatusCode::kInternal));
  }
  {
    Timestamp* wrong_type = nullptr;
    EXPECT_THAT(CreateMessageFromValue(cel_value, wrong_type, arena()),
                StatusIs(absl::StatusCode::kInternal));
  }
  {
    UInt32Value* wrong_type = nullptr;
    EXPECT_THAT(CreateMessageFromValue(cel_value, wrong_type, arena()),
                StatusIs(absl::StatusCode::kInternal));
  }
  {
    UInt64Value* wrong_type = nullptr;
    EXPECT_THAT(CreateMessageFromValue(cel_value, wrong_type, arena()),
                StatusIs(absl::StatusCode::kInternal));
  }
}

TEST_F(CelProtoWrapperTest, WrapFailureErrorToAny) {
  auto cel_value = CreateNoSuchFieldError(arena(), "error_field");
  Any* message = nullptr;
  EXPECT_THAT(CreateMessageFromValue(cel_value, message, arena()),
              StatusIs(absl::StatusCode::kInternal));
}

TEST_F(CelProtoWrapperTest, WrapFailureErrorToValue) {
  auto cel_value = CreateNoSuchFieldError(arena(), "error_field");
  Value* message = nullptr;
  EXPECT_THAT(CreateMessageFromValue(cel_value, message, arena()),
              StatusIs(absl::StatusCode::kInternal));
}

TEST_F(CelProtoWrapperTest, DebugString) {
  ListValue list_value;
  list_value.add_values()->set_bool_value(true);
  list_value.add_values()->set_number_value(1.0);
  list_value.add_values()->set_string_value("test");
  CelValue value = CreateCelValue(list_value, type_info(), arena());
  EXPECT_EQ(value.DebugString(),
            "CelList: [bool: 1, double: 1.000000, string: test]");

  Struct value_struct;
  auto& value1 = (*value_struct.mutable_fields())["a"];
  value1.set_bool_value(true);
  auto& value2 = (*value_struct.mutable_fields())["b"];
  value2.set_number_value(1.0);
  auto& value3 = (*value_struct.mutable_fields())["c"];
  value3.set_string_value("test");

  value = CreateCelValue(value_struct, type_info(), arena());
  EXPECT_THAT(
      value.DebugString(),
      testing::AllOf(testing::StartsWith("CelMap: {"),
                     testing::HasSubstr("<string: a>: <bool: 1>"),
                     testing::HasSubstr("<string: b>: <double: 1.0"),
                     testing::HasSubstr("<string: c>: <string: test>")));
}

TEST_F(CelProtoWrapperTest, CreateMessageFromValueUnimplementedUnknownType) {
  TestMessage* test_message_ptr = nullptr;
  TestMessage test_message;
  CelValue cel_value = CreateCelValue(test_message, type_info(), arena());
  absl::StatusOr<TestMessage*> result =
      CreateMessageFromValue(cel_value, test_message_ptr, arena());
  EXPECT_THAT(result, StatusIs(absl::StatusCode::kUnimplemented));
}

}  // namespace

}  // namespace google::api::expr::runtime::internal
