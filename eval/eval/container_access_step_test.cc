#include "eval/eval/container_access_step.h"

#include <string>
#include <utility>
#include <vector>

#include "google/protobuf/struct.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "eval/eval/container_backed_list_impl.h"
#include "eval/eval/container_backed_map_impl.h"
#include "eval/eval/ident_step.h"
#include "eval/public/cel_builtins.h"
#include "eval/public/cel_value.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {

namespace {

using ::google::protobuf::Struct;

using google::api::expr::v1alpha1::Expr;
using google::api::expr::v1alpha1::SourceInfo;

class ContainerAccessStepTest : public ::testing::Test {
 protected:
  ContainerAccessStepTest() {}

  void SetUp() override {}

  // Helper method. Looks up in registry and tests comparison operation.
  CelValue PerformRun(CelValue container, CelValue key, bool receiver_style) {
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

    path.push_back(std::move(
        CreateIdentStep(&container_expr->ident_expr(), 1).ValueOrDie()));
    path.push_back(
        std::move(CreateIdentStep(&key_expr->ident_expr(), 2).ValueOrDie()));
    path.push_back(std::move(CreateContainerAccessStep(call, 3).ValueOrDie()));

    CelExpressionFlatImpl cel_expr(&expr, std::move(path), 0);
    Activation activation;

    activation.InsertValue("container", container);
    activation.InsertValue("key", key);
    auto eval_status = cel_expr.Evaluate(activation, &arena_);

    EXPECT_TRUE(eval_status.ok());
    return eval_status.ValueOrDie();
  }
  google::protobuf::Arena arena_;
};

TEST_F(ContainerAccessStepTest, TestListIndexAccess) {
  ContainerBackedListImpl cel_list({CelValue::CreateInt64(1),
                                    CelValue::CreateInt64(2),
                                    CelValue::CreateInt64(3)});

  CelValue result = PerformRun(CelValue::CreateList(&cel_list),
                               CelValue::CreateInt64(1), true);

  ASSERT_TRUE(result.IsInt64());
  ASSERT_EQ(result.Int64OrDie(), 2);
}

TEST_F(ContainerAccessStepTest, TestListIndexAccessOutOfBounds) {
  ContainerBackedListImpl cel_list({CelValue::CreateInt64(1),
                                    CelValue::CreateInt64(2),
                                    CelValue::CreateInt64(3)});

  CelValue result = PerformRun(CelValue::CreateList(&cel_list),
                               CelValue::CreateInt64(0), true);

  ASSERT_TRUE(result.IsInt64());
  result = PerformRun(CelValue::CreateList(&cel_list), CelValue::CreateInt64(2),
                      true);

  ASSERT_TRUE(result.IsInt64());
  result = PerformRun(CelValue::CreateList(&cel_list),
                      CelValue::CreateInt64(-1), true);

  ASSERT_TRUE(result.IsError());
  result = PerformRun(CelValue::CreateList(&cel_list), CelValue::CreateInt64(3),
                      true);

  ASSERT_TRUE(result.IsError());
}

TEST_F(ContainerAccessStepTest, TestListIndexAccessNotAnInt) {
  ContainerBackedListImpl cel_list({CelValue::CreateInt64(1),
                                    CelValue::CreateInt64(2),
                                    CelValue::CreateInt64(3)});

  CelValue result = PerformRun(CelValue::CreateList(&cel_list),
                               CelValue::CreateUint64(1), true);

  ASSERT_TRUE(result.IsError());
}

TEST_F(ContainerAccessStepTest, TestMapKeyAccess) {
  const std::string kKey0 = "testkey0";
  const std::string kKey1 = "testkey1";
  const std::string kKey2 = "testkey2";
  Struct cel_struct;
  (*cel_struct.mutable_fields())[kKey0].set_string_value("value0");
  (*cel_struct.mutable_fields())[kKey1].set_string_value("value1");
  (*cel_struct.mutable_fields())[kKey2].set_string_value("value2");

  CelValue result = PerformRun(CelValue::CreateMessage(&cel_struct, &arena_),
                               CelValue::CreateString(&kKey0), true);

  ASSERT_TRUE(result.IsString());
  ASSERT_EQ(result.StringOrDie().value(), "value0");
}

TEST_F(ContainerAccessStepTest, TestMapKeyAccessNotFound) {
  const std::string kKey0 = "testkey0";
  const std::string kKey1 = "testkey1";
  Struct cel_struct;
  (*cel_struct.mutable_fields())[kKey0].set_string_value("value0");

  CelValue result = PerformRun(CelValue::CreateMessage(&cel_struct, &arena_),
                               CelValue::CreateString(&kKey1), true);

  ASSERT_TRUE(result.IsError());
}

}  // namespace

}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
