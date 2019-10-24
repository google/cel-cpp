#include "eval/eval/select_step.h"

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "gmock/gmock.h"
#include "gtest/gtest.h"
#include "eval/eval/container_backed_map_impl.h"
#include "eval/eval/ident_step.h"
#include "eval/testutil/test_message.pb.h"
#include "testutil/util.h"

namespace google {
namespace api {
namespace expr {
namespace runtime {
namespace {

using testing::Eq;

using testutil::EqualsProto;

using google::api::expr::v1alpha1::Expr;

// Helper method. Creates simple pipeline containing Select step and runs it.
cel_base::StatusOr<CelValue> RunExpression(const CelValue target,
                                       absl::string_view field, bool test,
                                       google::protobuf::Arena* arena,
                                       absl::string_view unknown_path) {
  ExecutionPath path;

  Expr dummy_expr;

  auto select = dummy_expr.mutable_select_expr();
  select->set_field(field.data());
  select->set_test_only(test);
  Expr* expr0 = select->mutable_operand();

  auto ident = expr0->mutable_ident_expr();
  ident->set_name("target");
  auto step0_status = CreateIdentStep(ident, expr0->id());
  auto step1_status = CreateSelectStep(select, dummy_expr.id(), unknown_path);

  if (!step0_status.ok()) {
    return step0_status.status();
  }

  if (!step1_status.ok()) {
    return step1_status.status();
  }

  path.push_back(std::move(step0_status.ValueOrDie()));
  path.push_back(std::move(step1_status.ValueOrDie()));

  CelExpressionFlatImpl cel_expr(&dummy_expr, std::move(path), 0);
  Activation activation;
  activation.InsertValue("target", target);

  return cel_expr.Evaluate(activation, arena);
}

cel_base::StatusOr<CelValue> RunExpression(const TestMessage* message,
                                       absl::string_view field, bool test,
                                       google::protobuf::Arena* arena,
                                       absl::string_view unknown_path) {
  return RunExpression(CelValue::CreateMessage(message, arena), field, test,
                       arena, unknown_path);
}

cel_base::StatusOr<CelValue> RunExpression(const TestMessage* message,
                                       absl::string_view field, bool test,
                                       google::protobuf::Arena* arena) {
  return RunExpression(message, field, test, arena, "");
}

cel_base::StatusOr<CelValue> RunExpression(const CelMap* map_value,
                                       absl::string_view field, bool test,
                                       google::protobuf::Arena* arena,
                                       absl::string_view unknown_path) {
  return RunExpression(CelValue::CreateMap(map_value), field, test, arena,
                       unknown_path);
}

cel_base::StatusOr<CelValue> RunExpression(const CelMap* map_value,
                                       absl::string_view field, bool test,
                                       google::protobuf::Arena* arena) {
  return RunExpression(map_value, field, test, arena, "");
}

TEST(SelectStepTest, SelectMessageIsNull) {
  google::protobuf::Arena arena;

  auto run_status = RunExpression(static_cast<const TestMessage*>(nullptr),
                                  "bool_value", true, &arena);
  ASSERT_TRUE(run_status.ok());

  CelValue result = run_status.ValueOrDie();

  ASSERT_TRUE(result.IsError());
}

TEST(SelectStepTest, PresenseIsFalseTest) {
  TestMessage message;

  google::protobuf::Arena arena;

  auto run_status = RunExpression(&message, "bool_value", true, &arena);
  ASSERT_TRUE(run_status.ok());

  CelValue result = run_status.ValueOrDie();

  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), false);
}

TEST(SelectStepTest, PresenseIsTrueTest) {
  TestMessage message;
  message.set_bool_value(true);

  google::protobuf::Arena arena;

  auto run_status = RunExpression(&message, "bool_value", true, &arena);
  ASSERT_TRUE(run_status.ok());

  CelValue result = run_status.ValueOrDie();

  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), true);
}

TEST(SelectStepTest, MapPresenseIsFalseTest) {
  std::string key1 = "key1";
  std::vector<std::pair<CelValue, CelValue>> key_values{
      {CelValue::CreateString(&key1), CelValue::CreateInt64(1)}};

  auto map_value = CreateContainerBackedMap(
      absl::Span<std::pair<CelValue, CelValue>>(key_values));

  google::protobuf::Arena arena;

  auto run_status = RunExpression(map_value.get(), "key2", true, &arena);
  CelValue result = run_status.ValueOrDie();

  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), false);
}

TEST(SelectStepTest, MapPresenseIsTrueTest) {
  std::string key1 = "key1";
  std::vector<std::pair<CelValue, CelValue>> key_values{
      {CelValue::CreateString(&key1), CelValue::CreateInt64(1)}};

  auto map_value = CreateContainerBackedMap(
      absl::Span<std::pair<CelValue, CelValue>>(key_values));

  google::protobuf::Arena arena;

  auto run_status = RunExpression(map_value.get(), "key1", true, &arena);
  CelValue result = run_status.ValueOrDie();

  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), true);
}

TEST(SelectStepTest, FieldIsNotPresentInProtoTest) {
  TestMessage message;

  google::protobuf::Arena arena;

  auto run_status = RunExpression(&message, "fake_field", false, &arena);
  ASSERT_TRUE(run_status.ok());

  CelValue result = run_status.ValueOrDie();
  ASSERT_TRUE(result.IsError());

  EXPECT_THAT(result.ErrorOrDie()->code(), Eq(cel_base::StatusCode::kNotFound));
}

TEST(SelectStepTest, FieldIsNotSetTest) {
  TestMessage message;

  google::protobuf::Arena arena;

  auto run_status = RunExpression(&message, "bool_value", false, &arena);
  ASSERT_TRUE(run_status.ok());

  CelValue result = run_status.ValueOrDie();

  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), false);
}

TEST(SelectStepTest, SimpleBoolTest) {
  TestMessage message;
  message.set_bool_value(true);

  google::protobuf::Arena arena;

  auto run_status = RunExpression(&message, "bool_value", false, &arena);
  ASSERT_TRUE(run_status.ok());

  CelValue result = run_status.ValueOrDie();

  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), true);
}

TEST(SelectStepTest, SimpleInt32Test) {
  TestMessage message;
  message.set_int32_value(1);

  google::protobuf::Arena arena;

  auto run_status = RunExpression(&message, "int32_value", false, &arena);
  ASSERT_TRUE(run_status.ok());

  CelValue result = run_status.ValueOrDie();

  ASSERT_TRUE(result.IsInt64());
  EXPECT_EQ(result.Int64OrDie(), 1);
}

TEST(SelectStepTest, SimpleInt64Test) {
  TestMessage message;
  message.set_int64_value(1);

  google::protobuf::Arena arena;

  auto run_status = RunExpression(&message, "int64_value", false, &arena);
  ASSERT_TRUE(run_status.ok());

  CelValue result = run_status.ValueOrDie();

  ASSERT_TRUE(result.IsInt64());
  EXPECT_EQ(result.Int64OrDie(), 1);
}

TEST(SelectStepTest, SimpleUInt32Test) {
  TestMessage message;
  message.set_uint32_value(1);

  google::protobuf::Arena arena;

  auto run_status = RunExpression(&message, "uint32_value", false, &arena);
  ASSERT_TRUE(run_status.ok());

  CelValue result = run_status.ValueOrDie();

  ASSERT_TRUE(result.IsUint64());
  EXPECT_EQ(result.Uint64OrDie(), 1);
}

TEST(SelectStepTest, SimpleUint64Test) {
  TestMessage message;
  message.set_uint64_value(1);

  google::protobuf::Arena arena;

  auto run_status = RunExpression(&message, "uint64_value", false, &arena);
  ASSERT_TRUE(run_status.ok());

  CelValue result = run_status.ValueOrDie();

  ASSERT_TRUE(result.IsUint64());
  EXPECT_EQ(result.Uint64OrDie(), 1);
}

TEST(SelectStepTest, SimpleStringTest) {
  TestMessage message;
  std::string value = "test";
  message.set_string_value(value);

  google::protobuf::Arena arena;

  auto run_status = RunExpression(&message, "string_value", false, &arena);
  ASSERT_TRUE(run_status.ok());

  CelValue result = run_status.ValueOrDie();

  ASSERT_TRUE(result.IsString());
  EXPECT_EQ(result.StringOrDie().value(), "test");
}


TEST(SelectStepTest, SimpleBytesTest) {
  TestMessage message;
  std::string value = "test";
  message.set_bytes_value(value);

  google::protobuf::Arena arena;

  auto run_status = RunExpression(&message, "bytes_value", false, &arena);
  ASSERT_TRUE(run_status.ok());

  CelValue result = run_status.ValueOrDie();

  ASSERT_TRUE(result.IsBytes());
  EXPECT_EQ(result.BytesOrDie().value(), "test");
}

TEST(SelectStepTest, SimpleMessageTest) {
  TestMessage message;

  TestMessage* message2 = message.mutable_message_value();
  message2->set_int32_value(1);
  message2->set_string_value("test");

  google::protobuf::Arena arena;

  auto run_status = RunExpression(&message, "message_value", false, &arena);
  ASSERT_TRUE(run_status.ok());

  CelValue result = run_status.ValueOrDie();

  ASSERT_TRUE(result.IsMessage());
  EXPECT_THAT(*message2, EqualsProto(*result.MessageOrDie()));
}

TEST(SelectStepTest, SimpleEnumTest) {
  TestMessage message;

  message.set_enum_value(TestMessage::TEST_ENUM_1);

  google::protobuf::Arena arena;

  auto run_status = RunExpression(&message, "enum_value", false, &arena);
  ASSERT_TRUE(run_status.ok());

  CelValue result = run_status.ValueOrDie();

  ASSERT_TRUE(result.IsInt64());
  EXPECT_THAT(result.Int64OrDie(), Eq(TestMessage::TEST_ENUM_1));
}

TEST(SelectStepTest, SimpleListTest) {
  TestMessage message;

  message.add_int32_list(1);
  message.add_int32_list(2);

  google::protobuf::Arena arena;

  auto run_status = RunExpression(&message, "int32_list", false, &arena);
  ASSERT_TRUE(run_status.ok());

  CelValue result = run_status.ValueOrDie();

  ASSERT_TRUE(result.IsList());

  const CelList* cel_list = result.ListOrDie();

  EXPECT_THAT(cel_list->size(), Eq(2));
}

TEST(SelectStepTest, SimpleMapTest) {
  TestMessage message;
  auto map_field = message.mutable_string_int32_map();
  (*map_field)["test0"] = 1;
  (*map_field)["test1"] = 2;

  google::protobuf::Arena arena;

  auto run_status = RunExpression(&message, "string_int32_map", false, &arena);
  ASSERT_TRUE(run_status.ok());

  CelValue result = run_status.ValueOrDie();

  ASSERT_TRUE(result.IsMap());

  const CelMap* cel_map = result.MapOrDie();

  EXPECT_THAT(cel_map->size(), Eq(2));
}

TEST(SelectStepTest, MapSimpleInt32Test) {
  std::string key1 = "key1";
  std::string key2 = "key2";
  std::vector<std::pair<CelValue, CelValue>> key_values{
      {CelValue::CreateString(&key1), CelValue::CreateInt64(1)},
      {CelValue::CreateString(&key2), CelValue::CreateInt64(2)}};

  auto map_value = CreateContainerBackedMap(
      absl::Span<std::pair<CelValue, CelValue>>(key_values));

  google::protobuf::Arena arena;

  auto run_status = RunExpression(map_value.get(), "key1", false, &arena);
  ASSERT_TRUE(run_status.ok());

  CelValue result = run_status.ValueOrDie();

  ASSERT_TRUE(result.IsInt64());
  EXPECT_EQ(result.Int64OrDie(), 1);
}

// Test Select behavior, when expression to select from is an Error.
TEST(SelectStepTest, CelErrorAsArgument) {
  ExecutionPath path;

  Expr dummy_expr;

  auto select = dummy_expr.mutable_select_expr();
  select->set_field("position");
  select->set_test_only(false);
  Expr* expr0 = select->mutable_operand();

  auto ident = expr0->mutable_ident_expr();
  ident->set_name("message");
  auto step0_status = CreateIdentStep(ident, expr0->id());
  auto step1_status = CreateSelectStep(select, dummy_expr.id(), "");

  ASSERT_TRUE(step0_status.ok());
  ASSERT_TRUE(step1_status.ok());

  path.push_back(std::move(step0_status.ValueOrDie()));
  path.push_back(std::move(step1_status.ValueOrDie()));

  CelError error;

  google::protobuf::Arena arena;
  CelExpressionFlatImpl cel_expr(&dummy_expr, std::move(path), 0);
  Activation activation;
  activation.InsertValue("message", CelValue::CreateError(&error));

  auto status = cel_expr.Evaluate(activation, &arena);
  ASSERT_TRUE(status.ok());

  auto result = status.ValueOrDie();
  ASSERT_TRUE(result.IsError());
  EXPECT_THAT(result.ErrorOrDie(), Eq(&error));
}

TEST(SelectStepTest, UnknownValueProducesError) {
  TestMessage message;
  message.set_bool_value(true);
  google::protobuf::Arena arena;
  ExecutionPath path;

  Expr dummy_expr;

  auto select = dummy_expr.mutable_select_expr();
  select->set_field("bool_value");
  select->set_test_only(false);
  Expr* expr0 = select->mutable_operand();

  auto ident = expr0->mutable_ident_expr();
  ident->set_name("message");
  auto step0_status = CreateIdentStep(ident, expr0->id());
  auto step1_status =
      CreateSelectStep(select, dummy_expr.id(), "message.bool_value");

  ASSERT_TRUE(step0_status.ok());
  ASSERT_TRUE(step1_status.ok());

  path.push_back(std::move(step0_status.ValueOrDie()));
  path.push_back(std::move(step1_status.ValueOrDie()));

  CelExpressionFlatImpl cel_expr(&dummy_expr, std::move(path), 0);
  Activation activation;
  activation.InsertValue("message", CelValue::CreateMessage(&message, &arena));

  auto eval_status0 = cel_expr.Evaluate(activation, &arena);
  ASSERT_TRUE(eval_status0.ok());

  CelValue result = eval_status0.ValueOrDie();

  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), true);

  google::protobuf::FieldMask mask;
  mask.add_paths("message.bool_value");

  activation.set_unknown_paths(mask);

  auto eval_status1 = cel_expr.Evaluate(activation, &arena);
  ASSERT_TRUE(eval_status1.ok());

  result = eval_status1.ValueOrDie();

  ASSERT_TRUE(result.IsError());
}

}  // namespace
}  // namespace runtime
}  // namespace expr
}  // namespace api
}  // namespace google
