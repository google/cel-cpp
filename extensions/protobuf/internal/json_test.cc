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

#include "extensions/protobuf/internal/json.h"

#include "google/protobuf/any.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/empty.pb.h"
#include "google/protobuf/field_mask.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "absl/log/absl_check.h"
#include "absl/strings/string_view.h"
#include "common/json.h"
#include "common/value_testing.h"
#include "internal/testing.h"
#include "proto/test/v1/proto2/test_all_types.pb.h"
#include "google/protobuf/text_format.h"

namespace cel::extensions::protobuf_internal {
namespace {

using ::google::api::expr::test::v1::proto2::TestAllTypes;
using testing::_;
using testing::Eq;
using testing::VariantWith;
using cel::internal::IsOkAndHolds;
using cel::internal::StatusIs;

template <typename T>
T ParseTextOrDie(absl::string_view text) {
  T proto;
  ABSL_CHECK(google::protobuf::TextFormat::ParseFromString(text, &proto));
  return proto;
}

class ProtoJsonTest : public common_internal::ThreadCompatibleValueTest<> {};

TEST_P(ProtoJsonTest, Bool) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           single_bool: true
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(
          MakeJsonObject({{JsonString("singleBool"), true}}))));
}

TEST_P(ProtoJsonTest, Int32) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           single_int32: 1
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(
          MakeJsonObject({{JsonString("singleInt32"), 1.0}}))));
}

TEST_P(ProtoJsonTest, Int64) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           single_int64: 1
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(
          MakeJsonObject({{JsonString("singleInt64"), 1.0}}))));
}

TEST_P(ProtoJsonTest, UInt32) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           single_uint32: 1
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(
          MakeJsonObject({{JsonString("singleUint32"), 1.0}}))));
}

TEST_P(ProtoJsonTest, UInt64) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           single_uint64: 1
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(
          MakeJsonObject({{JsonString("singleUint64"), 1.0}}))));
}

TEST_P(ProtoJsonTest, Float) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           single_float: 1
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(
          MakeJsonObject({{JsonString("singleFloat"), 1.0}}))));
}

TEST_P(ProtoJsonTest, Double) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           single_double: 1
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(
          MakeJsonObject({{JsonString("singleDouble"), 1.0}}))));
}

TEST_P(ProtoJsonTest, Bytes) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           single_bytes: "foo"
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(
          MakeJsonObject({{JsonString("singleBytes"), JsonBytes("foo")}}))));
}

TEST_P(ProtoJsonTest, String) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           single_string: "foo"
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(
          MakeJsonObject({{JsonString("singleString"), JsonString("foo")}}))));
}

TEST_P(ProtoJsonTest, Null) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           null_value: NULL_VALUE
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(
          MakeJsonObject({{JsonString("nullValue"), kJsonNull}}))));
}

TEST_P(ProtoJsonTest, Enum) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           single_nested_enum: BAR
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
          {{JsonString("singleNestedEnum"), JsonString("BAR")}}))));
}

TEST_P(ProtoJsonTest, Message) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           single_nested_message { bb: 1 }
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(
          MakeJsonObject({{JsonString("singleNestedMessage"),
                           MakeJsonObject({{JsonString("bb"), 1.0}})}}))));
}

TEST_P(ProtoJsonTest, BoolValue) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(),
                         google::protobuf::BoolValue::default_instance()),
      IsOkAndHolds(VariantWith<JsonBool>(false)));
}

TEST_P(ProtoJsonTest, Int32Value) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(),
                         google::protobuf::Int32Value::default_instance()),
      IsOkAndHolds(VariantWith<JsonNumber>(0)));
}

TEST_P(ProtoJsonTest, Int64Value) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(),
                         google::protobuf::Int64Value::default_instance()),
      IsOkAndHolds(VariantWith<JsonNumber>(0)));
}

TEST_P(ProtoJsonTest, UInt32Value) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(),
                         google::protobuf::UInt32Value::default_instance()),
      IsOkAndHolds(VariantWith<JsonNumber>(0)));
}

TEST_P(ProtoJsonTest, UInt64Value) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(),
                         google::protobuf::UInt64Value::default_instance()),
      IsOkAndHolds(VariantWith<JsonNumber>(0)));
}

TEST_P(ProtoJsonTest, FloatValue) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(),
                         google::protobuf::FloatValue::default_instance()),
      IsOkAndHolds(VariantWith<JsonNumber>(0)));
}

TEST_P(ProtoJsonTest, DoubleValue) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(),
                         google::protobuf::DoubleValue::default_instance()),
      IsOkAndHolds(VariantWith<JsonNumber>(0)));
}

TEST_P(ProtoJsonTest, BytesValue) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(),
                         google::protobuf::BytesValue::default_instance()),
      IsOkAndHolds(VariantWith<JsonString>(JsonString(""))));
}

TEST_P(ProtoJsonTest, StringValue) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(),
                         google::protobuf::StringValue::default_instance()),
      IsOkAndHolds(VariantWith<JsonString>(JsonString(""))));
}

TEST_P(ProtoJsonTest, Duration) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(),
                         google::protobuf::Duration::default_instance()),
      IsOkAndHolds(VariantWith<JsonString>(JsonString("0s"))));
}

TEST_P(ProtoJsonTest, Timestamp) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(),
                         google::protobuf::Timestamp::default_instance()),
      IsOkAndHolds(
          VariantWith<JsonString>(JsonString("1970-01-01T00:00:00Z"))));
}

TEST_P(ProtoJsonTest, FieldMask) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(),
                         google::protobuf::FieldMask::default_instance()),
      IsOkAndHolds(VariantWith<JsonString>(JsonString(""))));
}

TEST_P(ProtoJsonTest, Value) {
  EXPECT_THAT(ProtoMessageToJson(value_manager(),
                                 google::protobuf::Value::default_instance()),
              IsOkAndHolds(VariantWith<JsonNull>(kJsonNull)));
}

TEST_P(ProtoJsonTest, Struct) {
  EXPECT_THAT(ProtoMessageToJson(value_manager(),
                                 google::protobuf::Struct::default_instance()),
              IsOkAndHolds(VariantWith<JsonObject>(JsonObject())));
}

TEST_P(ProtoJsonTest, Empty) {
  EXPECT_THAT(ProtoMessageToJson(value_manager(),
                                 google::protobuf::Empty::default_instance()),
              IsOkAndHolds(VariantWith<JsonObject>(JsonObject())));
}

TEST_P(ProtoJsonTest, TestAllTypes) {
  TestAllTypes message;
  message.set_single_int64(1);
  EXPECT_THAT(ProtoMessageToJson(value_manager(), message),
              IsOkAndHolds(VariantWith<JsonObject>(
                  MakeJsonObject({{JsonString("singleInt64"), Json(1.0)}}))));
}

TEST_P(ProtoJsonTest, ListValue) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(),
                         google::protobuf::ListValue::default_instance()),
      IsOkAndHolds(VariantWith<JsonArray>(JsonArray())));
}

TEST_P(ProtoJsonTest, RepeatedBool) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           repeated_bool: true
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
          {{JsonString("repeatedBool"), MakeJsonArray({true})}}))));
}

TEST_P(ProtoJsonTest, RepeatedInt32) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           repeated_int32: 1
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
          {{JsonString("repeatedInt32"), MakeJsonArray({1.0})}}))));
}

TEST_P(ProtoJsonTest, RepeatedInt64) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           repeated_int64: 1
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
          {{JsonString("repeatedInt64"), MakeJsonArray({1.0})}}))));
}

TEST_P(ProtoJsonTest, RepeatedUInt32) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           repeated_uint32: 1
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
          {{JsonString("repeatedUint32"), MakeJsonArray({1.0})}}))));
}

TEST_P(ProtoJsonTest, RepeatedUInt64) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           repeated_uint64: 1
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
          {{JsonString("repeatedUint64"), MakeJsonArray({1.0})}}))));
}

TEST_P(ProtoJsonTest, RepeatedFloat) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           repeated_float: 1.0
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
          {{JsonString("repeatedFloat"), MakeJsonArray({1.0})}}))));
}

TEST_P(ProtoJsonTest, RepeatedDouble) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           repeated_double: 1.0
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
          {{JsonString("repeatedDouble"), MakeJsonArray({1.0})}}))));
}

TEST_P(ProtoJsonTest, RepeatedBytes) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           repeated_bytes: "foo"
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(
          MakeJsonObject({{JsonString("repeatedBytes"),
                           MakeJsonArray({JsonBytes("foo")})}}))));
}

TEST_P(ProtoJsonTest, RepeatedString) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           repeated_string: "foo"
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(
          MakeJsonObject({{JsonString("repeatedString"),
                           MakeJsonArray({JsonString("foo")})}}))));
}

TEST_P(ProtoJsonTest, RepeatedNull) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           repeated_null_value: NULL_VALUE
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
          {{JsonString("repeatedNullValue"), MakeJsonArray({kJsonNull})}}))));
}

TEST_P(ProtoJsonTest, RepeatedEnum) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           repeated_nested_enum: FOO
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(
          MakeJsonObject({{JsonString("repeatedNestedEnum"),
                           MakeJsonArray({JsonString("FOO")})}}))));
}

TEST_P(ProtoJsonTest, RepeatedMessage) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           repeated_nested_message { bb: 1 }
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
          {{JsonString("repeatedNestedMessage"),
            MakeJsonArray({MakeJsonObject({{JsonString("bb"), 1.0}})})}}))));
}

TEST_P(ProtoJsonTest, MapBoolBool) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           map_bool_bool { key: true value: true }
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(
          MakeJsonObject({{JsonString("mapBoolBool"),
                           MakeJsonObject({{JsonString("true"), true}})}}))));
}

TEST_P(ProtoJsonTest, MapInt32Int32) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           map_int32_int32 { key: 1 value: 1 }
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(
          MakeJsonObject({{JsonString("mapInt32Int32"),
                           MakeJsonObject({{JsonString("1"), 1.0}})}}))));
}

TEST_P(ProtoJsonTest, MapInt64Int64) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           map_int64_int64 { key: 1 value: 1 }
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(
          MakeJsonObject({{JsonString("mapInt64Int64"),
                           MakeJsonObject({{JsonString("1"), 1.0}})}}))));
}

TEST_P(ProtoJsonTest, MapUint32Uint32) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           map_uint32_uint32 { key: 1 value: 1 }
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(
          MakeJsonObject({{JsonString("mapUint32Uint32"),
                           MakeJsonObject({{JsonString("1"), 1.0}})}}))));
}

TEST_P(ProtoJsonTest, MapUint64Uint64) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           map_uint64_uint64 { key: 1 value: 1 }
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(
          MakeJsonObject({{JsonString("mapUint64Uint64"),
                           MakeJsonObject({{JsonString("1"), 1.0}})}}))));
}

TEST_P(ProtoJsonTest, MapStringString) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           map_string_string { key: "foo" value: "foo" }
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
          {{JsonString("mapStringString"),
            MakeJsonObject({{JsonString("foo"), JsonString("foo")}})}}))));
}

TEST_P(ProtoJsonTest, MapStringFloat) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           map_string_float { key: "foo" value: 1 }
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(
          MakeJsonObject({{JsonString("mapStringFloat"),
                           MakeJsonObject({{JsonString("foo"), 1.0}})}}))));
}

TEST_P(ProtoJsonTest, MapStringDouble) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           map_string_double { key: "foo" value: 1 }
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(
          MakeJsonObject({{JsonString("mapStringDouble"),
                           MakeJsonObject({{JsonString("foo"), 1.0}})}}))));
}

TEST_P(ProtoJsonTest, MapStringNull) {
  EXPECT_THAT(ProtoMessageToJson(
                  value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                    map_string_null_value { key: "foo" value: NULL_VALUE }
                  )pb")),
              IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
                  {{JsonString("mapStringNullValue"),
                    MakeJsonObject({{JsonString("foo"), kJsonNull}})}}))));
}

TEST_P(ProtoJsonTest, MapStringEnum) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           map_string_enum { key: "foo" value: BAR }
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
          {{JsonString("mapStringEnum"),
            MakeJsonObject({{JsonString("foo"), JsonString("BAR")}})}}))));
}

TEST_P(ProtoJsonTest, MapStringMessage) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(), ParseTextOrDie<TestAllTypes>(R"pb(
                           map_string_message {
                             key: "foo"
                             value: { bb: 1 }
                           }
                         )pb")),
      IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
          {{JsonString("mapStringMessage"),
            MakeJsonObject({{JsonString("foo"),
                             MakeJsonObject({{JsonString("bb"), 1.0}})}})}}))));
}

template <typename T>
google::protobuf::Any MakeAnyProto() {
  google::protobuf::Any any;
  ABSL_CHECK(any.PackFrom(T::default_instance()));
  return any;
}

TEST_P(ProtoJsonTest, AnyBool) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(),
                         MakeAnyProto<google::protobuf::BoolValue>()),
      IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
          {{JsonString("@type"),
            JsonString("type.googleapis.com/google.protobuf.BoolValue")},
           {JsonString("value"), false}}))));
}

TEST_P(ProtoJsonTest, AnyInt32) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(),
                         MakeAnyProto<google::protobuf::Int32Value>()),
      IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
          {{JsonString("@type"),
            JsonString("type.googleapis.com/google.protobuf.Int32Value")},
           {JsonString("value"), 0.0}}))));
}

TEST_P(ProtoJsonTest, AnyInt64) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(),
                         MakeAnyProto<google::protobuf::Int64Value>()),
      IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
          {{JsonString("@type"),
            JsonString("type.googleapis.com/google.protobuf.Int64Value")},
           {JsonString("value"), 0.0}}))));
}

TEST_P(ProtoJsonTest, AnyUInt32) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(),
                         MakeAnyProto<google::protobuf::UInt32Value>()),
      IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
          {{JsonString("@type"),
            JsonString("type.googleapis.com/google.protobuf.UInt32Value")},
           {JsonString("value"), 0.0}}))));
}

TEST_P(ProtoJsonTest, AnyUInt64) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(),
                         MakeAnyProto<google::protobuf::UInt64Value>()),
      IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
          {{JsonString("@type"),
            JsonString("type.googleapis.com/google.protobuf.UInt64Value")},
           {JsonString("value"), 0.0}}))));
}

TEST_P(ProtoJsonTest, AnyFloat) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(),
                         MakeAnyProto<google::protobuf::FloatValue>()),
      IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
          {{JsonString("@type"),
            JsonString("type.googleapis.com/google.protobuf.FloatValue")},
           {JsonString("value"), 0.0}}))));
}

TEST_P(ProtoJsonTest, AnyDouble) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(),
                         MakeAnyProto<google::protobuf::DoubleValue>()),
      IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
          {{JsonString("@type"),
            JsonString("type.googleapis.com/google.protobuf.DoubleValue")},
           {JsonString("value"), 0.0}}))));
}

TEST_P(ProtoJsonTest, AnyBytes) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(),
                         MakeAnyProto<google::protobuf::BytesValue>()),
      IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
          {{JsonString("@type"),
            JsonString("type.googleapis.com/google.protobuf.BytesValue")},
           {JsonString("value"), JsonString("")}}))));
}

TEST_P(ProtoJsonTest, AnyString) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(),
                         MakeAnyProto<google::protobuf::StringValue>()),
      IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
          {{JsonString("@type"),
            JsonString("type.googleapis.com/google.protobuf.StringValue")},
           {JsonString("value"), JsonString("")}}))));
}

TEST_P(ProtoJsonTest, AnyDuration) {
  EXPECT_THAT(ProtoMessageToJson(value_manager(),
                                 MakeAnyProto<google::protobuf::Duration>()),
              IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
                  {{JsonString("@type"),
                    JsonString("type.googleapis.com/google.protobuf.Duration")},
                   {JsonString("value"), JsonString("0s")}}))));
}

TEST_P(ProtoJsonTest, AnyTimestamp) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(),
                         MakeAnyProto<google::protobuf::Timestamp>()),
      IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
          {{JsonString("@type"),
            JsonString("type.googleapis.com/google.protobuf.Timestamp")},
           {JsonString("value"), JsonString("1970-01-01T00:00:00Z")}}))));
}

TEST_P(ProtoJsonTest, AnyFieldMask) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(),
                         MakeAnyProto<google::protobuf::FieldMask>()),
      IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
          {{JsonString("@type"),
            JsonString("type.googleapis.com/google.protobuf.FieldMask")},
           {JsonString("value"), JsonString("")}}))));
}

TEST_P(ProtoJsonTest, AnyValue) {
  EXPECT_THAT(ProtoMessageToJson(value_manager(),
                                 MakeAnyProto<google::protobuf::Value>()),
              IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
                  {{JsonString("@type"),
                    JsonString("type.googleapis.com/google.protobuf.Value")},
                   {JsonString("value"), kJsonNull}}))));
}

TEST_P(ProtoJsonTest, AnyListValue) {
  EXPECT_THAT(
      ProtoMessageToJson(value_manager(),
                         MakeAnyProto<google::protobuf::ListValue>()),
      IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
          {{JsonString("@type"),
            JsonString("type.googleapis.com/google.protobuf.ListValue")},
           {JsonString("value"), JsonArray()}}))));
}

TEST_P(ProtoJsonTest, AnyStruct) {
  EXPECT_THAT(ProtoMessageToJson(value_manager(),
                                 MakeAnyProto<google::protobuf::Struct>()),
              IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
                  {{JsonString("@type"),
                    JsonString("type.googleapis.com/google.protobuf.Struct")},
                   {JsonString("value"), JsonObject()}}))));
}

TEST_P(ProtoJsonTest, AnyEmpty) {
  EXPECT_THAT(ProtoMessageToJson(value_manager(),
                                 MakeAnyProto<google::protobuf::Empty>()),
              IsOkAndHolds(VariantWith<JsonObject>(MakeJsonObject(
                  {{JsonString("@type"),
                    JsonString("type.googleapis.com/google.protobuf.Empty")},
                   {JsonString("value"), JsonObject()}}))));
}

TEST_P(ProtoJsonTest, EnumBehavior) {
  EXPECT_THAT(
      ProtoEnumToJson(google::protobuf::GetEnumDescriptor<TestAllTypes::NestedEnum>(), 1),
      VariantWith<JsonString>(Eq(JsonString("BAR"))));
  EXPECT_THAT(ProtoEnumToJson(
                  google::protobuf::GetEnumDescriptor<TestAllTypes::NestedEnum>(), 99),
              VariantWith<JsonNumber>(Eq(99.0)));
}

TEST_P(ProtoJsonTest, NullBehavior) {
  EXPECT_THAT(ProtoEnumToJson(
                  google::protobuf::GetEnumDescriptor<google::protobuf::NullValue>(), 0),
              VariantWith<JsonNull>(_));
  EXPECT_THAT(ProtoEnumToJson(
                  google::protobuf::GetEnumDescriptor<google::protobuf::NullValue>(), 99),
              VariantWith<JsonNull>(_));
}

INSTANTIATE_TEST_SUITE_P(
    ProtoJsonTest, ProtoJsonTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    ProtoJsonTest::ToString);

}  // namespace
}  // namespace cel::extensions::protobuf_internal
