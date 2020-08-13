#include "eval/eval/container_access_step.h"

#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/struct.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "eval/eval/ident_step.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "base/status_macros.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using ::google::protobuf::Struct;

using google::api::expr::v1alpha1::Expr;
using google::api::expr::v1alpha1::SourceInfo;

using TestParamType = std::tuple<bool, bool>;

// Helper method. Looks up in registry and tests comparison operation.
CelValue EvaluateAttributeHelper(
    google::protobuf::Arena* arena, CelValue container, CelValue key, bool receiver_style,
    bool enable_unknown, const std::vector<CelAttributePattern>& patterns) {
  ExecutionPath path;

  Expr expr;
  SourceInfo source_info;
  auto call = expr.mutable_call_expr();

  call->set_function(builtin::kIndex);

  Expr* container_expr =
      (receiver_style) ? call->mutable_target() : call->add_args();
  Expr* key_expr = call->add_args();

  container_expr->mutable_ident_expr()->set_name("container");
  key_expr->mutable_ident_expr()->set_name("key");

  path.push_back(
      std::move(CreateIdentStep(&container_expr->ident_expr(), 1).value()));
  path.push_back(
      std::move(CreateIdentStep(&key_expr->ident_expr(), 2).value()));
  path.push_back(std::move(CreateContainerAccessStep(call, 3).value()));

  CelExpressionFlatImpl cel_expr(&expr, std::move(path), 0, {}, enable_unknown);
  Activation activation;

  activation.InsertValue("container", container);
  activation.InsertValue("key", key);

  activation.set_unknown_attribute_patterns(patterns);
  auto eval_status = cel_expr.Evaluate(activation, arena);

  EXPECT_OK(eval_status);
  return eval_status.value();
}

class ContainerAccessStepTest : public ::testing::Test {
 protected:
  ContainerAccessStepTest() {}

  void SetUp() override {}

  CelValue EvaluateAttribute(
      CelValue container, CelValue key, bool receiver_style,
      bool enable_unknown,
      const std::vector<CelAttributePattern>& patterns = {}) {
    return EvaluateAttributeHelper(&arena_, container, key, receiver_style,
                                   enable_unknown, patterns);
  }
  google::protobuf::Arena arena_;
};

class ContainerAccessStepUniformityTest
    : public ::testing::TestWithParam<TestParamType> {
 protected:
  ContainerAccessStepUniformityTest() {}

  void SetUp() override {}

  // Helper method. Looks up in registry and tests comparison operation.
  CelValue EvaluateAttribute(
      CelValue container, CelValue key, bool receiver_style,
      bool enable_unknown,
      const std::vector<CelAttributePattern>& patterns = {}) {
    return EvaluateAttributeHelper(&arena_, container, key, receiver_style,
                                   enable_unknown, patterns);
  }
  google::protobuf::Arena arena_;
};

TEST_P(ContainerAccessStepUniformityTest, TestListIndexAccess) {
  ContainerBackedListImpl cel_list({CelValue::CreateInt64(1),
                                    CelValue::CreateInt64(2),
                                    CelValue::CreateInt64(3)});

  TestParamType param = GetParam();
  CelValue result = EvaluateAttribute(CelValue::CreateList(&cel_list),
                                      CelValue::CreateInt64(1),
                                      std::get<0>(param), std::get<1>(param));

  ASSERT_TRUE(result.IsInt64());
  ASSERT_EQ(result.Int64OrDie(), 2);
}

TEST_P(ContainerAccessStepUniformityTest, TestListIndexAccessOutOfBounds) {
  ContainerBackedListImpl cel_list({CelValue::CreateInt64(1),
                                    CelValue::CreateInt64(2),
                                    CelValue::CreateInt64(3)});

  TestParamType param = GetParam();

  CelValue result = EvaluateAttribute(CelValue::CreateList(&cel_list),
                                      CelValue::CreateInt64(0),
                                      std::get<0>(param), std::get<1>(param));

  ASSERT_TRUE(result.IsInt64());
  result = EvaluateAttribute(CelValue::CreateList(&cel_list),
                             CelValue::CreateInt64(2), std::get<0>(param),
                             std::get<1>(param));

  ASSERT_TRUE(result.IsInt64());
  result = EvaluateAttribute(CelValue::CreateList(&cel_list),
                             CelValue::CreateInt64(-1), std::get<0>(param),
                             std::get<1>(param));

  ASSERT_TRUE(result.IsError());
  result = EvaluateAttribute(CelValue::CreateList(&cel_list),
                             CelValue::CreateInt64(3), std::get<0>(param),
                             std::get<1>(param));

  ASSERT_TRUE(result.IsError());
}

TEST_P(ContainerAccessStepUniformityTest, TestListIndexAccessNotAnInt) {
  ContainerBackedListImpl cel_list({CelValue::CreateInt64(1),
                                    CelValue::CreateInt64(2),
                                    CelValue::CreateInt64(3)});

  TestParamType param = GetParam();

  CelValue result = EvaluateAttribute(CelValue::CreateList(&cel_list),
                                      CelValue::CreateUint64(1),
                                      std::get<0>(param), std::get<1>(param));

  ASSERT_TRUE(result.IsError());
}

TEST_P(ContainerAccessStepUniformityTest, TestMapKeyAccess) {
  TestParamType param = GetParam();

  const std::string kKey0 = "testkey0";
  const std::string kKey1 = "testkey1";
  const std::string kKey2 = "testkey2";
  Struct cel_struct;
  (*cel_struct.mutable_fields())[kKey0].set_string_value("value0");
  (*cel_struct.mutable_fields())[kKey1].set_string_value("value1");
  (*cel_struct.mutable_fields())[kKey2].set_string_value("value2");

  CelValue result = EvaluateAttribute(
      CelProtoWrapper::CreateMessage(&cel_struct, &arena_),
      CelValue::CreateString(&kKey0), std::get<0>(param), std::get<1>(param));

  ASSERT_TRUE(result.IsString());
  ASSERT_EQ(result.StringOrDie().value(), "value0");
}

TEST_P(ContainerAccessStepUniformityTest, TestMapKeyAccessNotFound) {
  TestParamType param = GetParam();

  const std::string kKey0 = "testkey0";
  const std::string kKey1 = "testkey1";
  Struct cel_struct;
  (*cel_struct.mutable_fields())[kKey0].set_string_value("value0");

  CelValue result = EvaluateAttribute(
      CelProtoWrapper::CreateMessage(&cel_struct, &arena_),
      CelValue::CreateString(&kKey1), std::get<0>(param), std::get<1>(param));

  ASSERT_TRUE(result.IsError());
}

TEST_F(ContainerAccessStepTest, TestListIndexAccessUnknown) {
  ContainerBackedListImpl cel_list({CelValue::CreateInt64(1),
                                    CelValue::CreateInt64(2),
                                    CelValue::CreateInt64(3)});

  CelValue result = EvaluateAttribute(CelValue::CreateList(&cel_list),
                                      CelValue::CreateInt64(1), true, true, {});

  ASSERT_TRUE(result.IsInt64());
  ASSERT_EQ(result.Int64OrDie(), 2);

  std::vector<CelAttributePattern> patterns = {CelAttributePattern(
      "container",
      {CelAttributeQualifierPattern::Create(CelValue::CreateInt64(1))})};

  result = EvaluateAttribute(CelValue::CreateList(&cel_list),
                             CelValue::CreateInt64(1), true, true, patterns);

  ASSERT_TRUE(result.IsUnknownSet());
}

TEST_F(ContainerAccessStepTest, TestListUnknownKey) {
  ContainerBackedListImpl cel_list({CelValue::CreateInt64(1),
                                    CelValue::CreateInt64(2),
                                    CelValue::CreateInt64(3)});

  UnknownSet unknown_set;
  CelValue result =
      EvaluateAttribute(CelValue::CreateList(&cel_list),
                        CelValue::CreateUnknownSet(&unknown_set), true, true);

  ASSERT_TRUE(result.IsUnknownSet());
}

TEST_F(ContainerAccessStepTest, TestMapUnknownKey) {
  const std::string kKey0 = "testkey0";
  const std::string kKey1 = "testkey1";
  const std::string kKey2 = "testkey2";
  Struct cel_struct;
  (*cel_struct.mutable_fields())[kKey0].set_string_value("value0");
  (*cel_struct.mutable_fields())[kKey1].set_string_value("value1");
  (*cel_struct.mutable_fields())[kKey2].set_string_value("value2");

  UnknownSet unknown_set;
  CelValue result =
      EvaluateAttribute(CelProtoWrapper::CreateMessage(&cel_struct, &arena_),
                        CelValue::CreateUnknownSet(&unknown_set), true, true);

  ASSERT_TRUE(result.IsUnknownSet());
}

TEST_F(ContainerAccessStepTest, TestUnknownContainer) {
  UnknownSet unknown_set;
  CelValue result = EvaluateAttribute(CelValue::CreateUnknownSet(&unknown_set),
                                      CelValue::CreateInt64(1), true, true);

  ASSERT_TRUE(result.IsUnknownSet());
}

INSTANTIATE_TEST_SUITE_P(CombinedContainerTest,
                         ContainerAccessStepUniformityTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
