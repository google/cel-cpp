#include "eval/public/activation_bind_helper.h"
#include "eval/public/activation.h"

#include "eval/testutil/test_message.pb.h"

#include "gmock/gmock.h"
#include "gtest/gtest.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

TEST(ActivationBindHelperTest, TestSingleBoolBind) {
  TestMessage message;
  message.set_bool_value(true);

  google::protobuf::Arena arena;

  Activation activation;

  ASSERT_TRUE(BindProtoToActivation(&message, &arena, &activation).ok());

  auto result = activation.FindValue("bool_value", &arena);

  ASSERT_TRUE(result.has_value());

  CelValue value = result.value();

  ASSERT_TRUE(value.IsBool());
  EXPECT_EQ(value.BoolOrDie(), true);
}

TEST(ActivationBindHelperTest, TestSingleInt32Bind) {
  TestMessage message;
  message.set_int32_value(42);

  google::protobuf::Arena arena;

  Activation activation;

  ASSERT_TRUE(BindProtoToActivation(&message, &arena, &activation).ok());

  auto result = activation.FindValue("int32_value", &arena);

  ASSERT_TRUE(result.has_value());

  CelValue value = result.value();

  ASSERT_TRUE(value.IsInt64());
  EXPECT_EQ(value.Int64OrDie(), 42);
}

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
