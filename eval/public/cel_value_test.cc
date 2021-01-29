#include "eval/public/cel_value.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/strings/match.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "eval/public/unknown_attribute_set.h"
#include "eval/public/unknown_set.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

using testing::Eq;

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

  UnknownSet unknown_set;
  CelValue value_unknown = CelValue::CreateUnknownSet(&unknown_set);
  EXPECT_THAT(value_unknown.type(), Eq(CelValue::Type::kUnknownSet));

  CelValue missing_attribute_error =
      CreateMissingAttributeError(&arena, "destination.ip");
  EXPECT_TRUE(IsMissingAttributeError(missing_attribute_error));
  EXPECT_EQ(missing_attribute_error.ErrorOrDie()->code(),
            absl::StatusCode::kInvalidArgument);
  EXPECT_EQ(missing_attribute_error.ErrorOrDie()->message(),
            "MissingAttributeError: destination.ip");
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

  const UnknownSet* value_unknown;
  count += (value.GetValue(&value_unknown)) ? 1 : 0;

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

TEST(CelValueTest, TestCelType) {
  ::google::protobuf::Arena arena;

  CelValue value_bool = CelValue::CreateBool(false);
  EXPECT_THAT(value_bool.ObtainCelType().CelTypeOrDie().value(), Eq("bool"));

  CelValue value_int64 = CelValue::CreateInt64(0);
  EXPECT_THAT(value_int64.ObtainCelType().CelTypeOrDie().value(), Eq("int"));

  CelValue value_uint64 = CelValue::CreateUint64(0);
  EXPECT_THAT(value_uint64.ObtainCelType().CelTypeOrDie().value(), Eq("uint"));

  CelValue value_double = CelValue::CreateDouble(1.0);
  EXPECT_THAT(value_double.ObtainCelType().CelTypeOrDie().value(),
              Eq("double"));

  std::string str = "test";
  CelValue value_str = CelValue::CreateString(&str);
  EXPECT_THAT(value_str.ObtainCelType().CelTypeOrDie().value(), Eq("string"));

  std::string bytes_str = "bytes";
  CelValue value_bytes = CelValue::CreateBytes(&bytes_str);
  EXPECT_THAT(value_bytes.type(), Eq(CelValue::Type::kBytes));
  EXPECT_THAT(value_bytes.ObtainCelType().CelTypeOrDie().value(), Eq("bytes"));

  std::string msg_type_str = "google.api.expr.runtime.TestMessage";
  CelValue msg_type = CelValue::CreateCelTypeView(msg_type_str);
  EXPECT_TRUE(msg_type.IsCelType());
  EXPECT_THAT(msg_type.CelTypeOrDie().value(),
              Eq("google.api.expr.runtime.TestMessage"));
  EXPECT_THAT(msg_type.type(), Eq(CelValue::Type::kCelType));

  UnknownSet unknown_set;
  CelValue value_unknown = CelValue::CreateUnknownSet(&unknown_set);
  EXPECT_THAT(value_unknown.type(), Eq(CelValue::Type::kUnknownSet));
  EXPECT_TRUE(value_unknown.ObtainCelType().IsUnknownSet());

  CelValue missing_attribute_error =
      CreateMissingAttributeError(&arena, "destination.ip");
  EXPECT_TRUE(IsMissingAttributeError(missing_attribute_error));
  EXPECT_TRUE(missing_attribute_error.ObtainCelType().IsError());
}

// This test verifies CelValue support of Unknown type.
TEST(CelValueTest, TestUnknownSet) {
  UnknownSet unknown_set;

  CelValue value = CelValue::CreateUnknownSet(&unknown_set);
  EXPECT_TRUE(value.IsUnknownSet());
  EXPECT_THAT(value.UnknownSetOrDie(), Eq(&unknown_set));

  // test template getter
  const UnknownSet* value2;
  EXPECT_TRUE(value.GetValue(&value2));
  EXPECT_THAT(value2, Eq(&unknown_set));
  EXPECT_THAT(CountTypeMatch(value), Eq(1));
}

TEST(CelValueTest, UnknownFunctionResultErrors) {
  ::google::protobuf::Arena arena;

  CelValue value = CreateUnknownFunctionResultError(&arena, "message");
  EXPECT_TRUE(value.IsError());
  EXPECT_TRUE(IsUnknownFunctionResult(value));
}

TEST(CelValueTest, DebugString) {
  EXPECT_EQ(CelValue::CreateBool(true).DebugString(), "bool: 1");
  EXPECT_EQ(CelValue::CreateInt64(-12345).DebugString(), "int64: -12345");
  EXPECT_EQ(CelValue::CreateUint64(12345).DebugString(), "uint64: 12345");
  EXPECT_TRUE(absl::StartsWith(CelValue::CreateDouble(0.12345).DebugString(),
                               "double: 0.12345"));
  const std::string abc("abc");
  EXPECT_EQ(CelValue::CreateString(&abc).DebugString(), "string: abc");
  EXPECT_EQ(CelValue::CreateBytes(&abc).DebugString(), "bytes: abc");

  EXPECT_EQ(CelValue::CreateDuration(absl::Hours(24)).DebugString(),
            "Duration: 24h");

  EXPECT_EQ(
      CelValue::CreateTimestamp(absl::FromUnixSeconds(86400)).DebugString(),
      "Time: 1970-01-02T00:00:00+00:00");

  UnknownSet unknown_set;
  EXPECT_EQ(CelValue::CreateUnknownSet(&unknown_set).DebugString(),
            "UnknownSet");

  absl::Status error = absl::InternalError("Blah...");
  EXPECT_EQ(CelValue::CreateError(&error).DebugString(),
            "Error: INTERNAL: Blah...");
}

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
