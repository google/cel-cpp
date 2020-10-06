#include "eval/public/testing/debug_string.h"

#include "google/protobuf/struct.pb.h"
#include "google/protobuf/arena.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "absl/status/status.h"
#include "absl/time/time.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_function.h"
#include "eval/public/cel_value.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/public/unknown_attribute_set.h"
#include "eval/public/unknown_function_result_set.h"
#include "eval/public/unknown_set.h"
#include "eval/testutil/test_message.pb.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {
namespace test {
namespace {
using testing::AnyOf;
using testing::HasSubstr;

TEST(DebugString, Primitives) {
  constexpr char data[] = {
      '\x01', '\x01', '\x00', '\xc0', '\xff',
  };
  std::string bytestring(data, 5);
  CelError error = absl::InternalError("error");

  EXPECT_EQ(DebugString(CelValue::CreateStringView("hello world")),
            "<string,'hello world'>");
  EXPECT_EQ(DebugString(CelValue::CreateBytes(&bytestring)),
            "<bytes,0x010100c0ff>");
  EXPECT_EQ(DebugString(CelValue::CreateBool(false)), "<bool,false>");
  EXPECT_EQ(DebugString(CelValue::CreateDouble(1.5)), "<double,1.500000>");
  EXPECT_EQ(DebugString(CelValue::CreateDuration(absl::Seconds(2))),
            "<Duration,2s>");
  EXPECT_THAT(DebugString(CelValue::CreateTimestamp(absl::FromUnixSeconds(1))),
              AnyOf(HasSubstr("<Timestamp,1969-12-31T"),
                    HasSubstr("<Timestamp,1970-01-01T")));
  EXPECT_EQ(DebugString(CelValue::CreateError(&error)),
            "<CelError,INTERNAL: error>");
  EXPECT_EQ(DebugString(CelValue::CreateInt64(-1)),
            "<int64,-1>");  // no transform

  EXPECT_EQ(DebugString(CelValue::CreateUint64(1)),
            "<uint64,1>");  // no transform
}

TEST(DebugString, Messages) {
  google::protobuf::Arena arena;
  TestMessage message;
  message.add_int64_list(1);
  message.add_int64_list(2);

  EXPECT_EQ(DebugString(CelValue::CreateNull()), "<Message,NULL>");
  EXPECT_EQ(DebugString(CelProtoWrapper::CreateMessage(&message, &arena)),
            "<Message,int64_list: 1\nint64_list: 2\n>");
}

TEST(DebugString, Lists) {
  google::protobuf::Arena arena;

  protobuf::ListValue list_msg;
  list_msg.add_values()->set_bool_value(true);
  list_msg.add_values()->set_bool_value(false);

  // converted to a list
  EXPECT_EQ(DebugString(CelProtoWrapper::CreateMessage(&list_msg, &arena)),
            "<CelList,[<bool,true>, <bool,false>]>");
}

TEST(DebugString, Maps) {
  google::protobuf::Arena arena;

  // Converted to a map on CelValue::Create.
  protobuf::Struct struct_msg;
  (*struct_msg.mutable_fields())["field1"].set_bool_value(true);
  (*struct_msg.mutable_fields())["field2"].set_bool_value(false);

  // Ordering isn't guaranteed for the backing map for a converted struct.
  EXPECT_THAT(DebugString(CelProtoWrapper::CreateMessage(&struct_msg, &arena)),
              AnyOf("<CelMap,{"
                    "<string,'field1'>:<bool,true>, "
                    "<string,'field2'>:<bool,false>"
                    "}>",
                    "<CelMap,{"
                    "<string,'field2'>:<bool,false>, "
                    "<string,'field1'>:<bool,true>"
                    "}>"));
}

TEST(DebugString, UnknownSet) {
  google::protobuf::Arena arena;
  google::api::expr::v1alpha1::Expr ident;
  ident.mutable_ident_expr()->set_name("var");
  CelAttribute attr(
      ident,
      {CelAttributeQualifier::Create(CelValue::CreateInt64(1)),
       CelAttributeQualifier::Create(CelValue::CreateStringView("field"))});
  UnknownFunctionResult function_result(
      CelFunctionDescriptor("IntFn", false, {CelValue::Type::kInt64}), 1,
      {CelValue::CreateInt64(1)});
  UnknownSet unknown_set(UnknownAttributeSet({&attr}),
                         UnknownFunctionResultSet(&function_result));
  EXPECT_EQ(DebugString(CelValue::CreateUnknownSet(&unknown_set)),
            "<UnknownSet,{attributes:[var.1.'field'], "
            "functions:[IntFn(<int64,1>)]}>");  // no transform
}

TEST(DebugString, Type) {
  constexpr char data[] = {
      '\x01', '\x01', '\x00', '\xc0', '\xff',
  };
  std::string bytestring(data, 5);
  EXPECT_EQ(
      DebugString(CelValue::CreateStringView("hello world").ObtainCelType()),
      "<CelType,'string'>");
  EXPECT_EQ(DebugString(CelValue::CreateBytes(&bytestring).ObtainCelType()),
            "<CelType,'bytes'>");
  EXPECT_EQ(DebugString(CelValue::CreateBool(false).ObtainCelType()),
            "<CelType,'bool'>");
  EXPECT_EQ(DebugString(CelValue::CreateDouble(1.5).ObtainCelType()),
            "<CelType,'double'>");
  EXPECT_EQ(
      DebugString(CelValue::CreateDuration(absl::Seconds(2)).ObtainCelType()),
      "<CelType,'google.protobuf.Duration'>");
  EXPECT_EQ(
      DebugString(
          CelValue::CreateTimestamp(absl::FromUnixSeconds(1)).ObtainCelType()),
      "<CelType,'google.protobuf.Timestamp'>");
  EXPECT_EQ(DebugString(CelValue::CreateInt64(-1).ObtainCelType()),
            "<CelType,'int'>");

  EXPECT_EQ(DebugString(CelValue::CreateUint64(1).ObtainCelType()),
            "<CelType,'uint'>");

  google::protobuf::Arena arena;

  protobuf::ListValue list_msg;
  EXPECT_EQ(
      DebugString(
          CelProtoWrapper::CreateMessage(&list_msg, &arena).ObtainCelType()),
      "<CelType,'list'>");
  // Converted to a map on CelValue::Create.
  protobuf::Struct struct_msg;
  EXPECT_EQ(
      DebugString(
          CelProtoWrapper::CreateMessage(&struct_msg, &arena).ObtainCelType()),
      "<CelType,'map'>");
}

}  // namespace
}  // namespace test
}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
