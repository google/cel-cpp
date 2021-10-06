#include "eval/eval/select_step.h"

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "eval/eval/ident_step.h"
#include "eval/public/activation.h"
#include "eval/public/cel_attribute.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/public/unknown_attribute_set.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "testutil/util.h"

namespace google::api::expr::runtime {

namespace {

using ::google::api::expr::v1alpha1::Expr;
using testing::Eq;
using testing::HasSubstr;
using cel::internal::StatusIs;

using testutil::EqualsProto;

// Helper method. Creates simple pipeline containing Select step and runs it.
absl::StatusOr<CelValue> RunExpression(const CelValue target,
                                       absl::string_view field, bool test,
                                       google::protobuf::Arena* arena,
                                       absl::string_view unknown_path,
                                       bool enable_unknowns) {
  ExecutionPath path;
  Expr dummy_expr;

  auto select = dummy_expr.mutable_select_expr();
  select->set_field(field.data());
  select->set_test_only(test);
  Expr* expr0 = select->mutable_operand();

  auto ident = expr0->mutable_ident_expr();
  ident->set_name("target");
  CEL_ASSIGN_OR_RETURN(auto step0, CreateIdentStep(ident, expr0->id()));
  CEL_ASSIGN_OR_RETURN(auto step1,
                       CreateSelectStep(select, dummy_expr.id(), unknown_path));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));

  CelExpressionFlatImpl cel_expr(&dummy_expr, std::move(path), 0, {},
                                 enable_unknowns);
  Activation activation;
  activation.InsertValue("target", target);

  return cel_expr.Evaluate(activation, arena);
}

absl::StatusOr<CelValue> RunExpression(const TestMessage* message,
                                       absl::string_view field, bool test,
                                       google::protobuf::Arena* arena,
                                       absl::string_view unknown_path,
                                       bool enable_unknowns) {
  return RunExpression(CelProtoWrapper::CreateMessage(message, arena), field,
                       test, arena, unknown_path, enable_unknowns);
}

absl::StatusOr<CelValue> RunExpression(const TestMessage* message,
                                       absl::string_view field, bool test,
                                       google::protobuf::Arena* arena,
                                       bool enable_unknowns) {
  return RunExpression(message, field, test, arena, "", enable_unknowns);
}

absl::StatusOr<CelValue> RunExpression(const CelMap* map_value,
                                       absl::string_view field, bool test,
                                       google::protobuf::Arena* arena,
                                       absl::string_view unknown_path,
                                       bool enable_unknowns) {
  return RunExpression(CelValue::CreateMap(map_value), field, test, arena,
                       unknown_path, enable_unknowns);
}

absl::StatusOr<CelValue> RunExpression(const CelMap* map_value,
                                       absl::string_view field, bool test,
                                       google::protobuf::Arena* arena,
                                       bool enable_unknowns) {
  return RunExpression(map_value, field, test, arena, "", enable_unknowns);
}

class SelectStepTest : public testing::TestWithParam<bool> {};

TEST_P(SelectStepTest, SelectMessageIsNull) {
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunExpression(static_cast<const TestMessage*>(nullptr),
                                     "bool_value", true, &arena, GetParam()));
  ASSERT_TRUE(result.IsError());
}

TEST_P(SelectStepTest, PresenseIsFalseTest) {
  TestMessage message;

  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(&message, "bool_value", true, &arena, GetParam()));
  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), false);
}

TEST_P(SelectStepTest, PresenseIsTrueTest) {
  TestMessage message;
  message.set_bool_value(true);

  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(&message, "bool_value", true, &arena, GetParam()));
  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), true);
}

TEST_P(SelectStepTest, MapPresenseIsFalseTest) {
  std::string key1 = "key1";
  std::vector<std::pair<CelValue, CelValue>> key_values{
      {CelValue::CreateString(&key1), CelValue::CreateInt64(1)}};

  auto map_value = CreateContainerBackedMap(
                       absl::Span<std::pair<CelValue, CelValue>>(key_values))
                       .value();

  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(map_value.get(), "key2", true, &arena, GetParam()));
  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), false);
}

TEST_P(SelectStepTest, MapPresenseIsTrueTest) {
  std::string key1 = "key1";
  std::vector<std::pair<CelValue, CelValue>> key_values{
      {CelValue::CreateString(&key1), CelValue::CreateInt64(1)}};

  auto map_value = CreateContainerBackedMap(
                       absl::Span<std::pair<CelValue, CelValue>>(key_values))
                       .value();

  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(map_value.get(), "key1", true, &arena, GetParam()));
  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), true);
}

TEST(SelectStepTest, MapPresenseIsErrorTest) {
  TestMessage message;
  google::protobuf::Arena arena;

  Expr select_expr;
  auto select = select_expr.mutable_select_expr();
  select->set_field("1");
  select->set_test_only(true);
  Expr* expr1 = select->mutable_operand();
  auto select_map = expr1->mutable_select_expr();
  select_map->set_field("int32_int32_map");
  Expr* expr0 = select_map->mutable_operand();
  auto ident = expr0->mutable_ident_expr();
  ident->set_name("target");

  ASSERT_OK_AND_ASSIGN(auto step0, CreateIdentStep(ident, expr0->id()));
  ASSERT_OK_AND_ASSIGN(auto step1,
                       CreateSelectStep(select_map, expr1->id(), ""));
  ASSERT_OK_AND_ASSIGN(auto step2,
                       CreateSelectStep(select, select_expr.id(), ""));

  ExecutionPath path;
  path.push_back(std::move(step0));
  path.push_back(std::move(step1));
  path.push_back(std::move(step2));
  CelExpressionFlatImpl cel_expr(&select_expr, std::move(path), 0, {}, false);
  Activation activation;
  activation.InsertValue("target",
                         CelProtoWrapper::CreateMessage(&message, &arena));

  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr.Evaluate(activation, &arena));
  EXPECT_TRUE(result.IsError());
  EXPECT_EQ(result.ErrorOrDie()->code(), absl::StatusCode::kInvalidArgument);
}

TEST(SelectStepTest, MapPresenseIsTrueWithUnknownTest) {
  UnknownSet unknown_set;
  std::string key1 = "key1";
  std::vector<std::pair<CelValue, CelValue>> key_values{
      {CelValue::CreateString(&key1),
       CelValue::CreateUnknownSet(&unknown_set)}};

  auto map_value = CreateContainerBackedMap(
                       absl::Span<std::pair<CelValue, CelValue>>(key_values))
                       .value();

  google::protobuf::Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result, RunExpression(map_value.get(), "key1",
                                                      true, &arena, true));
  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), true);
}

TEST_P(SelectStepTest, FieldIsNotPresentInProtoTest) {
  TestMessage message;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(&message, "fake_field", false, &arena, GetParam()));
  ASSERT_TRUE(result.IsError());
  EXPECT_THAT(result.ErrorOrDie()->code(), Eq(absl::StatusCode::kNotFound));
}

TEST_P(SelectStepTest, FieldIsNotSetTest) {
  TestMessage message;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(&message, "bool_value", false, &arena, GetParam()));
  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), false);
}

TEST_P(SelectStepTest, SimpleBoolTest) {
  TestMessage message;
  message.set_bool_value(true);
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(&message, "bool_value", false, &arena, GetParam()));
  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), true);
}

TEST_P(SelectStepTest, SimpleInt32Test) {
  TestMessage message;
  message.set_int32_value(1);

  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(&message, "int32_value", false, &arena, GetParam()));
  ASSERT_TRUE(result.IsInt64());
  EXPECT_EQ(result.Int64OrDie(), 1);
}

TEST_P(SelectStepTest, SimpleInt64Test) {
  TestMessage message;
  message.set_int64_value(1);

  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(&message, "int64_value", false, &arena, GetParam()));
  ASSERT_TRUE(result.IsInt64());
  EXPECT_EQ(result.Int64OrDie(), 1);
}

TEST_P(SelectStepTest, SimpleUInt32Test) {
  TestMessage message;
  message.set_uint32_value(1);
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(&message, "uint32_value", false, &arena, GetParam()));
  ASSERT_TRUE(result.IsUint64());
  EXPECT_EQ(result.Uint64OrDie(), 1);
}

TEST_P(SelectStepTest, SimpleUint64Test) {
  TestMessage message;
  message.set_uint64_value(1);
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(&message, "uint64_value", false, &arena, GetParam()));
  ASSERT_TRUE(result.IsUint64());
  EXPECT_EQ(result.Uint64OrDie(), 1);
}

TEST_P(SelectStepTest, SimpleStringTest) {
  TestMessage message;
  std::string value = "test";
  message.set_string_value(value);
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(&message, "string_value", false, &arena, GetParam()));
  ASSERT_TRUE(result.IsString());
  EXPECT_EQ(result.StringOrDie().value(), "test");
}


TEST_P(SelectStepTest, SimpleBytesTest) {
  TestMessage message;
  std::string value = "test";
  message.set_bytes_value(value);
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(&message, "bytes_value", false, &arena, GetParam()));
  ASSERT_TRUE(result.IsBytes());
  EXPECT_EQ(result.BytesOrDie().value(), "test");
}

TEST_P(SelectStepTest, SimpleMessageTest) {
  TestMessage message;
  TestMessage* message2 = message.mutable_message_value();
  message2->set_int32_value(1);
  message2->set_string_value("test");
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(&message, "message_value", false, &arena, GetParam()));
  ASSERT_TRUE(result.IsMessage());
  EXPECT_THAT(*message2, EqualsProto(*result.MessageOrDie()));
}

TEST_P(SelectStepTest, SimpleEnumTest) {
  TestMessage message;
  message.set_enum_value(TestMessage::TEST_ENUM_1);
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(&message, "enum_value", false, &arena, GetParam()));
  ASSERT_TRUE(result.IsInt64());
  EXPECT_THAT(result.Int64OrDie(), Eq(TestMessage::TEST_ENUM_1));
}

TEST_P(SelectStepTest, SimpleListTest) {
  TestMessage message;
  message.add_int32_list(1);
  message.add_int32_list(2);
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(&message, "int32_list", false, &arena, GetParam()));
  ASSERT_TRUE(result.IsList());

  const CelList* cel_list = result.ListOrDie();
  EXPECT_THAT(cel_list->size(), Eq(2));
}

TEST_P(SelectStepTest, SimpleMapTest) {
  TestMessage message;
  auto map_field = message.mutable_string_int32_map();
  (*map_field)["test0"] = 1;
  (*map_field)["test1"] = 2;
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(&message, "string_int32_map", false, &arena, GetParam()));
  ASSERT_TRUE(result.IsMap());

  const CelMap* cel_map = result.MapOrDie();
  EXPECT_THAT(cel_map->size(), Eq(2));
}

TEST_P(SelectStepTest, MapSimpleInt32Test) {
  std::string key1 = "key1";
  std::string key2 = "key2";
  std::vector<std::pair<CelValue, CelValue>> key_values{
      {CelValue::CreateString(&key1), CelValue::CreateInt64(1)},
      {CelValue::CreateString(&key2), CelValue::CreateInt64(2)}};

  auto map_value = CreateContainerBackedMap(
                       absl::Span<std::pair<CelValue, CelValue>>(key_values))
                       .value();
  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      RunExpression(map_value.get(), "key1", false, &arena, GetParam()));
  ASSERT_TRUE(result.IsInt64());
  EXPECT_EQ(result.Int64OrDie(), 1);
}

// Test Select behavior, when expression to select from is an Error.
TEST_P(SelectStepTest, CelErrorAsArgument) {
  ExecutionPath path;

  Expr dummy_expr;

  auto select = dummy_expr.mutable_select_expr();
  select->set_field("position");
  select->set_test_only(false);
  Expr* expr0 = select->mutable_operand();

  auto ident = expr0->mutable_ident_expr();
  ident->set_name("message");
  ASSERT_OK_AND_ASSIGN(auto step0, CreateIdentStep(ident, expr0->id()));
  ASSERT_OK_AND_ASSIGN(auto step1,
                       CreateSelectStep(select, dummy_expr.id(), ""));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));

  CelError error;

  google::protobuf::Arena arena;
  CelExpressionFlatImpl cel_expr(&dummy_expr, std::move(path), 0, {},
                                 GetParam());
  Activation activation;
  activation.InsertValue("message", CelValue::CreateError(&error));

  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr.Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsError());
  EXPECT_THAT(result.ErrorOrDie(), Eq(&error));
}

TEST(SelectStepTest, DisableMissingAttributeOK) {
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
  ASSERT_OK_AND_ASSIGN(auto step0, CreateIdentStep(ident, expr0->id()));
  ASSERT_OK_AND_ASSIGN(auto step1, CreateSelectStep(select, dummy_expr.id(),
                                                    "message.bool_value"));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));

  CelExpressionFlatImpl cel_expr(&dummy_expr, std::move(path), 0, {},
                                 /*enable_unknowns=*/false);
  Activation activation;
  activation.InsertValue("message",
                         CelProtoWrapper::CreateMessage(&message, &arena));

  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr.Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), true);

  CelAttributePattern pattern("message", {});
  activation.set_missing_attribute_patterns({pattern});

  ASSERT_OK_AND_ASSIGN(result, cel_expr.Evaluate(activation, &arena));
  EXPECT_EQ(result.BoolOrDie(), true);
}

TEST(SelectStepTest, UnrecoverableUnknownValueProducesError) {
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
  ASSERT_OK_AND_ASSIGN(auto step0, CreateIdentStep(ident, expr0->id()));
  ASSERT_OK_AND_ASSIGN(auto step1, CreateSelectStep(select, dummy_expr.id(),
                                                    "message.bool_value"));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));

  CelExpressionFlatImpl cel_expr(&dummy_expr, std::move(path), 0, {}, false,
                                 false,
                                 /*enable_missing_attribute_errors=*/true);
  Activation activation;
  activation.InsertValue("message",
                         CelProtoWrapper::CreateMessage(&message, &arena));

  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr.Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), true);

  CelAttributePattern pattern("message",
                              {CelAttributeQualifierPattern::Create(
                                  CelValue::CreateStringView("bool_value"))});
  activation.set_missing_attribute_patterns({pattern});

  ASSERT_OK_AND_ASSIGN(result, cel_expr.Evaluate(activation, &arena));
  EXPECT_THAT(*result.ErrorOrDie(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("MissingAttributeError: message.bool_value")));
}

TEST_P(SelectStepTest, UnknownValueProducesError) {
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
  ASSERT_OK_AND_ASSIGN(auto step0, CreateIdentStep(ident, expr0->id()));
  ASSERT_OK_AND_ASSIGN(auto step1, CreateSelectStep(select, dummy_expr.id(),
                                                    "message.bool_value"));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));

  CelExpressionFlatImpl cel_expr(&dummy_expr, std::move(path), 0, {},
                                 GetParam());
  Activation activation;
  activation.InsertValue("message",
                         CelProtoWrapper::CreateMessage(&message, &arena));

  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr.Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsBool());
  EXPECT_EQ(result.BoolOrDie(), true);

  google::protobuf::FieldMask mask;
  mask.add_paths("message.bool_value");

  activation.set_unknown_paths(mask);

  ASSERT_OK_AND_ASSIGN(result, cel_expr.Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsError());
  ASSERT_TRUE(IsUnknownValueError(result));
  EXPECT_THAT(GetUnknownPathsSetOrDie(result),
              Eq(std::set<std::string>({"message.bool_value"})));
}

TEST(SelectStepTest, UnknownPatternResolvesToUnknown) {
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

  ASSERT_OK(step0_status);
  ASSERT_OK(step1_status);

  path.push_back(*std::move(step0_status));
  path.push_back(*std::move(step1_status));

  CelExpressionFlatImpl cel_expr(&dummy_expr, std::move(path), 0, {}, true);

  {
    std::vector<CelAttributePattern> unknown_patterns;
    Activation activation;
    activation.InsertValue("message",
                           CelProtoWrapper::CreateMessage(&message, &arena));
    activation.set_unknown_attribute_patterns(unknown_patterns);

    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr.Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsBool());
    EXPECT_EQ(result.BoolOrDie(), true);
  }

  const std::string kSegmentCorrect1 = "bool_value";
  const std::string kSegmentIncorrect = "message_value";

  {
    std::vector<CelAttributePattern> unknown_patterns;
    unknown_patterns.push_back(CelAttributePattern("message", {}));
    Activation activation;
    activation.InsertValue("message",
                           CelProtoWrapper::CreateMessage(&message, &arena));
    activation.set_unknown_attribute_patterns(unknown_patterns);

    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr.Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsUnknownSet());
  }

  {
    std::vector<CelAttributePattern> unknown_patterns;
    unknown_patterns.push_back(CelAttributePattern(
        "message", {CelAttributeQualifierPattern::Create(
                       CelValue::CreateString(&kSegmentCorrect1))}));
    Activation activation;
    activation.InsertValue("message",
                           CelProtoWrapper::CreateMessage(&message, &arena));
    activation.set_unknown_attribute_patterns(unknown_patterns);

    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr.Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsUnknownSet());
  }

  {
    std::vector<CelAttributePattern> unknown_patterns;
    unknown_patterns.push_back(CelAttributePattern(
        "message", {CelAttributeQualifierPattern::CreateWildcard()}));
    Activation activation;
    activation.InsertValue("message",
                           CelProtoWrapper::CreateMessage(&message, &arena));
    activation.set_unknown_attribute_patterns(unknown_patterns);

    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr.Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsUnknownSet());
  }

  {
    std::vector<CelAttributePattern> unknown_patterns;
    unknown_patterns.push_back(CelAttributePattern(
        "message", {CelAttributeQualifierPattern::Create(
                       CelValue::CreateString(&kSegmentIncorrect))}));
    Activation activation;
    activation.InsertValue("message",
                           CelProtoWrapper::CreateMessage(&message, &arena));
    activation.set_unknown_attribute_patterns(unknown_patterns);

    ASSERT_OK_AND_ASSIGN(CelValue result,
                         cel_expr.Evaluate(activation, &arena));
    ASSERT_TRUE(result.IsBool());
    EXPECT_EQ(result.BoolOrDie(), true);
  }
}

INSTANTIATE_TEST_SUITE_P(SelectStepTest, SelectStepTest, testing::Bool());

}  // namespace

}  // namespace google::api::expr::runtime
