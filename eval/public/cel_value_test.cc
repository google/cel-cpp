#include "eval/public/cel_value.h"

#include "google/protobuf/any.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "eval/testutil/test_message.pb.h"
#include "testutil/util.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

using testing::Eq;
using testing::UnorderedPointwise;

using google::protobuf::Duration;
using google::protobuf::Timestamp;
using google::protobuf::Value;
using google::protobuf::ListValue;
using google::protobuf::Struct;

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

class DummyMap : public CelMap {
 public:
  absl::optional<CelValue> operator[](CelValue value) const override {
    return CelValue::CreateNull();
  }
  const CelList* ListKeys() const override { return nullptr; }

  int size() const override { return 0; }
};

class DummyList : public CelList {
 public:
  int size() const override { return 0; }

  CelValue operator[](int index) const override {
    return CelValue::CreateNull();
  }
};

TEST(CelValueTest, TestType) {
  ::google::protobuf::Arena arena;

  CelValue value_bool = CelValue::CreateBool(false);
  EXPECT_THAT(value_bool.type(), Eq(CelValue::Type::kBool));

  CelValue value_int64 = CelValue::CreateInt64(0);
  EXPECT_THAT(value_int64.type(), Eq(CelValue::Type::kInt64));

  CelValue value_uint64 = CelValue::CreateUint64(1);
  EXPECT_THAT(value_uint64.type(), Eq(CelValue::Type::kUint64));

  CelValue value_double = CelValue::CreateDouble(1.0);
  EXPECT_THAT(value_double.type(), Eq(CelValue::Type::kDouble));

  std::string str = "test";
  CelValue value_str = CelValue::CreateString(&str);
  EXPECT_THAT(value_str.type(), Eq(CelValue::Type::kString));

  std::string bytes_str = "bytes";
  CelValue value_bytes = CelValue::CreateBytes(&bytes_str);
  EXPECT_THAT(value_bytes.type(), Eq(CelValue::Type::kBytes));

  Duration msg_duration;
  msg_duration.set_seconds(2);
  msg_duration.set_nanos(3);
  CelValue value_duration1 = CelValue::CreateDuration(&msg_duration);
  EXPECT_THAT(value_duration1.type(), Eq(CelValue::Type::kDuration));

  CelValue value_duration2 = CelValue::CreateMessage(&msg_duration, &arena);
  EXPECT_THAT(value_duration2.type(), Eq(CelValue::Type::kDuration));

  Timestamp msg_timestamp;
  msg_timestamp.set_seconds(2);
  msg_timestamp.set_nanos(3);
  CelValue value_timestamp1 = CelValue::CreateTimestamp(&msg_timestamp);
  EXPECT_THAT(value_timestamp1.type(), Eq(CelValue::Type::kTimestamp));

  CelValue value_timestamp2 = CelValue::CreateMessage(&msg_timestamp, &arena);
  EXPECT_THAT(value_timestamp2.type(), Eq(CelValue::Type::kTimestamp));
}

int CountTypeMatch(const CelValue& value) {
  int count = 0;
  bool value_bool;
  count += (value.GetValue(&value_bool)) ? 1 : 0;

  int64_t value_int64;
  count += (value.GetValue(&value_int64)) ? 1 : 0;

  uint64_t value_uint64;
  count += (value.GetValue(&value_uint64)) ? 1 : 0;

  double value_double;
  count += (value.GetValue(&value_double)) ? 1 : 0;

  std::string test = "";
  CelValue::StringHolder value_str(&test);
  count += (value.GetValue(&value_str)) ? 1 : 0;

  CelValue::BytesHolder value_bytes(&test);
  count += (value.GetValue(&value_bytes)) ? 1 : 0;

  const google::protobuf::Message* value_msg;
  count += (value.GetValue(&value_msg)) ? 1 : 0;

  const CelList* value_list;
  count += (value.GetValue(&value_list)) ? 1 : 0;

  const CelMap* value_map;
  count += (value.GetValue(&value_map)) ? 1 : 0;

  const CelError* value_error;
  count += (value.GetValue(&value_error)) ? 1 : 0;

  return count;
}

// This test verifies CelValue support of bool type.
TEST(CelValueTest, TestBool) {
  CelValue value = CelValue::CreateBool(true);
  EXPECT_TRUE(value.IsBool());
  EXPECT_THAT(value.BoolOrDie(), Eq(true));

  // test template getter
  bool value2 = false;
  EXPECT_TRUE(value.GetValue(&value2));
  EXPECT_EQ(value2, true);
  EXPECT_THAT(CountTypeMatch(value), Eq(1));
}

// This test verifies CelValue support of int64_t type.
TEST(CelValueTest, TestInt64) {
  int64_t v = 1;
  CelValue value = CelValue::CreateInt64(v);
  EXPECT_TRUE(value.IsInt64());
  EXPECT_THAT(value.Int64OrDie(), Eq(1));

  // test template getter
  int64_t value2 = 0;
  EXPECT_TRUE(value.GetValue(&value2));
  EXPECT_EQ(value2, 1);
  EXPECT_THAT(CountTypeMatch(value), Eq(1));
}

// This test verifies CelValue support of uint64_t type.
TEST(CelValueTest, TestUint64) {
  uint64_t v = 1;
  CelValue value = CelValue::CreateUint64(v);
  EXPECT_TRUE(value.IsUint64());
  EXPECT_THAT(value.Uint64OrDie(), Eq(1));

  // test template getter
  uint64_t value2 = 0;
  EXPECT_TRUE(value.GetValue(&value2));
  EXPECT_EQ(value2, 1);
  EXPECT_THAT(CountTypeMatch(value), Eq(1));
}

// This test verifies CelValue support of int64_t type.
TEST(CelValueTest, TestDouble) {
  double v0 = 1.;
  CelValue value = CelValue::CreateDouble(v0);
  EXPECT_TRUE(value.IsDouble());
  EXPECT_THAT(value.DoubleOrDie(), Eq(v0));

  // test template getter
  double value2 = 0;
  EXPECT_TRUE(value.GetValue(&value2));
  EXPECT_DOUBLE_EQ(value2, 1);
  EXPECT_THAT(CountTypeMatch(value), Eq(1));
}

// This test verifies CelValue support of string type.
TEST(CelValueTest, TestString) {
  constexpr char kTestStr0[] = "test0";
  std::string v = kTestStr0;

  CelValue value = CelValue::CreateString(&v);
  //  CelValue value = CelValue::CreateString("test");
  EXPECT_TRUE(value.IsString());
  EXPECT_THAT(value.StringOrDie().value(), Eq(std::string(kTestStr0)));

  // test template getter
  std::string test = "";
  CelValue::StringHolder value2(&test);
  EXPECT_TRUE(value.GetValue(&value2));
  EXPECT_THAT(value2.value(), Eq(kTestStr0));
  EXPECT_THAT(CountTypeMatch(value), Eq(1));
}

// This test verifies CelValue support of Bytes type.
TEST(CelValueTest, TestBytes) {
  constexpr char kTestStr0[] = "test0";
  std::string v = kTestStr0;
  absl::string_view sv(v);

  CelValue value = CelValue::CreateBytes(&v);
  //  CelValue value = CelValue::CreateString("test");
  EXPECT_TRUE(value.IsBytes());
  EXPECT_THAT(value.BytesOrDie().value(), Eq(std::string(kTestStr0)));

  // test template getter
  std::string test = "";
  CelValue::BytesHolder value2(&test);
  EXPECT_TRUE(value.GetValue(&value2));
  EXPECT_THAT(value2.value(), Eq(kTestStr0));
  EXPECT_THAT(CountTypeMatch(value), Eq(1));
}

// This test verifies CelValue support of Duration type.
TEST(CelValueTest, TestDuration) {
  google::protobuf::Arena arena;

  Duration msg_duration;
  msg_duration.set_seconds(2);
  msg_duration.set_nanos(3);
  CelValue value_duration1 = CelValue::CreateDuration(&msg_duration);
  EXPECT_THAT(value_duration1.type(), Eq(CelValue::Type::kDuration));

  CelValue value_duration2 = CelValue::CreateMessage(&msg_duration, &arena);
  EXPECT_THAT(value_duration2.type(), Eq(CelValue::Type::kDuration));

  CelValue value = CelValue::CreateDuration(&msg_duration);
  //  CelValue value = CelValue::CreateString("test");
  EXPECT_TRUE(value.IsDuration());
  Duration out;
  expr::internal::EncodeDuration(value.DurationOrDie(), &out);
  EXPECT_THAT(out, testutil::EqualsProto(msg_duration));
}

// This test verifies CelValue support of Timestamp type.
TEST(CelValueTest, TestTimestamp) {
  google::protobuf::Arena arena;

  Timestamp msg_timestamp;
  msg_timestamp.set_seconds(2);
  msg_timestamp.set_nanos(3);
  CelValue value_timestamp1 = CelValue::CreateTimestamp(&msg_timestamp);
  EXPECT_THAT(value_timestamp1.type(), Eq(CelValue::Type::kTimestamp));

  CelValue value_timestamp2 = CelValue::CreateMessage(&msg_timestamp, &arena);
  EXPECT_THAT(value_timestamp2.type(), Eq(CelValue::Type::kTimestamp));

  CelValue value = CelValue::CreateTimestamp(&msg_timestamp);
  //  CelValue value = CelValue::CreateString("test");
  EXPECT_TRUE(value.IsTimestamp());
  Timestamp out;
  expr::internal::EncodeTime(value.TimestampOrDie(), &out);
  EXPECT_THAT(out, testutil::EqualsProto(msg_timestamp));
}

// This test verifies CelValue support of List type.
TEST(CelValueTest, TestList) {
  DummyList dummy_list;

  CelValue value = CelValue::CreateList(&dummy_list);
  EXPECT_TRUE(value.IsList());
  EXPECT_THAT(value.ListOrDie(), Eq(&dummy_list));

  // test template getter
  const CelList* value2;
  EXPECT_TRUE(value.GetValue(&value2));
  EXPECT_THAT(value2, Eq(&dummy_list));
  EXPECT_THAT(CountTypeMatch(value), Eq(1));
}

// This test verifies CelValue support of Map type.
TEST(CelValueTest, TestMap) {
  DummyMap dummy_map;

  CelValue value = CelValue::CreateMap(&dummy_map);
  EXPECT_TRUE(value.IsMap());
  EXPECT_THAT(value.MapOrDie(), Eq(&dummy_map));

  // test template getter
  const CelMap* value2;
  EXPECT_TRUE(value.GetValue(&value2));
  EXPECT_THAT(value2, Eq(&dummy_map));
  EXPECT_THAT(CountTypeMatch(value), Eq(1));
}

// Dynamic Values test
//

TEST(CelValueTest, TestValueFieldNull) {
  ::google::protobuf::Arena arena;

  Value value1;
  value1.set_null_value(google::protobuf::NullValue::NULL_VALUE);

  CelValue value = CelValue::CreateMessage(&value1, &arena);
  ASSERT_TRUE(value.IsNull());
}

TEST(CelValueTest, TestValueFieldBool) {
  ::google::protobuf::Arena arena;

  Value value1;
  value1.set_bool_value(true);

  CelValue value = CelValue::CreateMessage(&value1, &arena);
  ASSERT_TRUE(value.IsBool());
  EXPECT_EQ(value.BoolOrDie(), true);
}

TEST(CelValueTest, TestValueFieldNumeric) {
  ::google::protobuf::Arena arena;

  Value value1;
  value1.set_number_value(1.0);

  CelValue value = CelValue::CreateMessage(&value1, &arena);
  ASSERT_TRUE(value.IsDouble());
  EXPECT_DOUBLE_EQ(value.DoubleOrDie(), 1.0);
}

TEST(CelValueTest, TestValueFieldString) {
  ::google::protobuf::Arena arena;

  const std::string kTest = "test";

  Value value1;
  value1.set_string_value(kTest);

  CelValue value = CelValue::CreateMessage(&value1, &arena);
  ASSERT_TRUE(value.IsString());
  EXPECT_EQ(value.StringOrDie().value(), kTest);
}

TEST(CelValueTest, TestValueFieldStruct) {
  ::google::protobuf::Arena arena;

  const std::vector<std::string> kFields = {"field1", "field2", "field3"};

  Struct value_struct;

  auto& value1 = (*value_struct.mutable_fields())[kFields[0]];
  value1.set_bool_value(true);

  auto& value2 = (*value_struct.mutable_fields())[kFields[1]];
  value2.set_number_value(1.0);

  auto& value3 = (*value_struct.mutable_fields())[kFields[2]];
  value3.set_string_value("test");

  CelValue value = CelValue::CreateMessage(&value_struct, &arena);
  ASSERT_TRUE(value.IsMap());

  const CelMap* cel_map = value.MapOrDie();

  auto lookup1 = (*cel_map)[CelValue::CreateString(&kFields[0])];
  ASSERT_TRUE(lookup1.has_value());
  ASSERT_TRUE(lookup1.value().IsBool());
  EXPECT_EQ(lookup1.value().BoolOrDie(), true);

  auto lookup2 = (*cel_map)[CelValue::CreateString(&kFields[1])];
  ASSERT_TRUE(lookup2.has_value());
  ASSERT_TRUE(lookup2.value().IsDouble());
  EXPECT_DOUBLE_EQ(lookup2.value().DoubleOrDie(), 1.0);

  auto lookup3 = (*cel_map)[CelValue::CreateString(&kFields[2])];
  ASSERT_TRUE(lookup3.has_value());
  ASSERT_TRUE(lookup3.value().IsString());
  EXPECT_EQ(lookup3.value().StringOrDie().value(), "test");

  const CelList* key_list = cel_map->ListKeys();
  ASSERT_EQ(key_list->size(), kFields.size());

  std::vector<std::string> result_keys;
  for (int i = 0; i < key_list->size(); i++) {
    CelValue key = (*key_list)[i];
    ASSERT_TRUE(key.IsString());
    result_keys.push_back(std::string(key.StringOrDie().value()));
  }

  EXPECT_THAT(result_keys, UnorderedPointwise(Eq(), kFields));
}

TEST(CelValueTest, TestListFieldStruct) {
  ::google::protobuf::Arena arena;

  const std::vector<std::string> kFields = {"field1", "field2", "field3"};

  ListValue list_value;

  list_value.add_values()->set_bool_value(true);
  list_value.add_values()->set_number_value(1.0);
  list_value.add_values()->set_string_value("test");

  CelValue value = CelValue::CreateMessage(&list_value, &arena);
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
}

// Test support of google.protobuf.Any in CelValue.
TEST(CelValueTest, TestAnyValue) {
  ::google::protobuf::Arena arena;
  Any any;

  TestMessage test_message;
  test_message.set_string_value("test");

  any.PackFrom(test_message);

  CelValue value = CelValue::CreateMessage(&any, &arena);
  ASSERT_TRUE(value.IsMessage());

  const google::protobuf::Message* unpacked_message = value.MessageOrDie();
  EXPECT_THAT(test_message, testutil::EqualsProto(*unpacked_message));
}

TEST(CelValueTest, TestHandlingInvalidAnyValue) {
  ::google::protobuf::Arena arena;
  Any any;

  CelValue value = CelValue::CreateMessage(&any, &arena);
  ASSERT_TRUE(value.IsError());

  any.set_type_url("/");
  ASSERT_TRUE(CelValue::CreateMessage(&any, &arena).IsError());

  any.set_type_url("/invalid.proto.name");
  ASSERT_TRUE(CelValue::CreateMessage(&any, &arena).IsError());
}

// Test support of google.protobuf.<Type>Value wrappers in CelValue.
TEST(CelValueTest, TestBoolWrapper) {
  ::google::protobuf::Arena arena;

  BoolValue wrapper;
  wrapper.set_value(true);

  CelValue value = CelValue::CreateMessage(&wrapper, &arena);
  ASSERT_TRUE(value.IsBool());

  EXPECT_EQ(value.BoolOrDie(), wrapper.value());
}

TEST(CelValueTest, TestInt32Wrapper) {
  ::google::protobuf::Arena arena;

  Int32Value wrapper;
  wrapper.set_value(12);

  CelValue value = CelValue::CreateMessage(&wrapper, &arena);
  ASSERT_TRUE(value.IsInt64());

  EXPECT_EQ(value.Int64OrDie(), wrapper.value());
}

TEST(CelValueTest, TestUInt32Wrapper) {
  ::google::protobuf::Arena arena;

  UInt32Value wrapper;
  wrapper.set_value(12);

  CelValue value = CelValue::CreateMessage(&wrapper, &arena);
  ASSERT_TRUE(value.IsUint64());

  EXPECT_EQ(value.Uint64OrDie(), wrapper.value());
}

TEST(CelValueTest, TestInt64Wrapper) {
  ::google::protobuf::Arena arena;

  Int64Value wrapper;
  wrapper.set_value(12);

  CelValue value = CelValue::CreateMessage(&wrapper, &arena);
  ASSERT_TRUE(value.IsInt64());

  EXPECT_EQ(value.Int64OrDie(), wrapper.value());
}

TEST(CelValueTest, TestUInt64Wrapper) {
  ::google::protobuf::Arena arena;

  UInt64Value wrapper;
  wrapper.set_value(12);

  CelValue value = CelValue::CreateMessage(&wrapper, &arena);
  ASSERT_TRUE(value.IsUint64());

  EXPECT_EQ(value.Uint64OrDie(), wrapper.value());
}

TEST(CelValueTest, TestFloatWrapper) {
  ::google::protobuf::Arena arena;

  FloatValue wrapper;
  wrapper.set_value(42);

  CelValue value = CelValue::CreateMessage(&wrapper, &arena);
  ASSERT_TRUE(value.IsDouble());

  EXPECT_DOUBLE_EQ(value.DoubleOrDie(), wrapper.value());
}

TEST(CelValueTest, TestDoubleWrapper) {
  ::google::protobuf::Arena arena;

  DoubleValue wrapper;
  wrapper.set_value(42);

  CelValue value = CelValue::CreateMessage(&wrapper, &arena);
  ASSERT_TRUE(value.IsDouble());

  EXPECT_DOUBLE_EQ(value.DoubleOrDie(), wrapper.value());
}

TEST(CelValueTest, TestStringWrapper) {
  ::google::protobuf::Arena arena;

  StringValue wrapper;
  wrapper.set_value("42");

  CelValue value = CelValue::CreateMessage(&wrapper, &arena);
  ASSERT_TRUE(value.IsString());

  EXPECT_EQ(value.StringOrDie().value(), wrapper.value());
}

TEST(CelValueTest, TestBytesWrapper) {
  ::google::protobuf::Arena arena;

  BytesValue wrapper;
  wrapper.set_value("42");

  CelValue value = CelValue::CreateMessage(&wrapper, &arena);
  ASSERT_TRUE(value.IsBytes());

  EXPECT_EQ(value.BytesOrDie().value(), wrapper.value());
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
