#include "eval/eval/create_struct_step.h"

#include <string>
#include <utility>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "eval/eval/ident_step.h"
#include "eval/eval/test_type_registry.h"
#include "eval/public/activation.h"
#include "eval/public/cel_type_registry.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/public/structs/proto_message_type_adapter.h"
#include "eval/public/structs/protobuf_descriptor_type_provider.h"
#include "eval/testutil/test_message.pb.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "testutil/util.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::ast::internal::Expr;
using ::google::protobuf::Arena;
using ::google::protobuf::Message;
using testing::Eq;
using testing::IsNull;
using testing::Not;
using testing::Pointwise;
using cel::internal::StatusIs;
using testutil::EqualsProto;

// Helper method. Creates simple pipeline containing CreateStruct step that
// builds message and runs it.
absl::StatusOr<CelValue> RunExpression(absl::string_view field,
                                       const CelValue& value,
                                       google::protobuf::Arena* arena,
                                       bool enable_unknowns) {
  ExecutionPath path;
  CelTypeRegistry type_registry;
  type_registry.RegisterTypeProvider(
      std::make_unique<ProtobufDescriptorProvider>(
          google::protobuf::DescriptorPool::generated_pool(),
          google::protobuf::MessageFactory::generated_factory()));

  Expr expr0;
  Expr expr1;

  auto& ident = expr0.mutable_ident_expr();
  ident.set_name("message");
  CEL_ASSIGN_OR_RETURN(auto step0, CreateIdentStep(ident, expr0.id()));

  auto& create_struct = expr1.mutable_struct_expr();
  create_struct.set_message_name("google.api.expr.runtime.TestMessage");

  auto& entry = create_struct.mutable_entries().emplace_back();
  entry.set_field_key(std::string(field));

  auto adapter = type_registry.FindTypeAdapter(create_struct.message_name());
  if (!adapter.has_value() || adapter->mutation_apis() == nullptr) {
    return absl::Status(absl::StatusCode::kFailedPrecondition,
                        "missing proto message type");
  }
  CEL_ASSIGN_OR_RETURN(
      auto step1, CreateCreateStructStep(create_struct,
                                         adapter->mutation_apis(), expr1.id()));

  path.push_back(std::move(step0));
  path.push_back(std::move(step1));

  CelExpressionFlatImpl cel_expr(&expr1, std::move(path), &type_registry, 0, {},
                                 enable_unknowns);
  Activation activation;
  activation.InsertValue("message", value);

  return cel_expr.Evaluate(activation, arena);
}

void RunExpressionAndGetMessage(absl::string_view field, const CelValue& value,
                                google::protobuf::Arena* arena, TestMessage* test_msg,
                                bool enable_unknowns) {
  ASSERT_OK_AND_ASSIGN(auto result,
                       RunExpression(field, value, arena, enable_unknowns));
  ASSERT_TRUE(result.IsMessage());

  const Message* msg = result.MessageOrDie();
  ASSERT_THAT(msg, Not(IsNull()));

  ASSERT_EQ(msg->GetDescriptor(), TestMessage::descriptor());
  test_msg->MergeFrom(*msg);
}

void RunExpressionAndGetMessage(absl::string_view field,
                                std::vector<CelValue> values,
                                google::protobuf::Arena* arena, TestMessage* test_msg,
                                bool enable_unknowns) {
  ContainerBackedListImpl cel_list(std::move(values));

  CelValue value = CelValue::CreateList(&cel_list);

  ASSERT_OK_AND_ASSIGN(auto result,
                       RunExpression(field, value, arena, enable_unknowns));
  ASSERT_TRUE(result.IsMessage());

  const Message* msg = result.MessageOrDie();
  ASSERT_THAT(msg, Not(IsNull()));

  ASSERT_EQ(msg->GetDescriptor(), TestMessage::descriptor());
  test_msg->MergeFrom(*msg);
}

// Helper method. Creates simple pipeline containing CreateStruct step that
// builds Map and runs it.
absl::StatusOr<CelValue> RunCreateMapExpression(
    const std::vector<std::pair<CelValue, CelValue>>& values,
    google::protobuf::Arena* arena, bool enable_unknowns) {
  ExecutionPath path;
  Activation activation;

  Expr expr0;
  Expr expr1;

  std::vector<Expr> exprs;
  exprs.reserve(values.size() * 2);
  int index = 0;

  auto& create_struct = expr1.mutable_struct_expr();
  for (const auto& item : values) {
    std::string key_name = absl::StrCat("key", index);
    std::string value_name = absl::StrCat("value", index);

    auto& key_expr = exprs.emplace_back();
    auto& key_ident = key_expr.mutable_ident_expr();
    key_ident.set_name(key_name);
    CEL_ASSIGN_OR_RETURN(auto step_key,
                         CreateIdentStep(key_ident, exprs.back().id()));

    auto& value_expr = exprs.emplace_back();
    auto& value_ident = value_expr.mutable_ident_expr();
    value_ident.set_name(value_name);
    CEL_ASSIGN_OR_RETURN(auto step_value,
                         CreateIdentStep(value_ident, exprs.back().id()));

    path.push_back(std::move(step_key));
    path.push_back(std::move(step_value));

    activation.InsertValue(key_name, item.first);
    activation.InsertValue(value_name, item.second);

    create_struct.mutable_entries().emplace_back();
    index++;
  }

  CEL_ASSIGN_OR_RETURN(auto step1,
                       CreateCreateStructStep(create_struct, expr1.id()));
  path.push_back(std::move(step1));

  CelExpressionFlatImpl cel_expr(&expr1, std::move(path), &TestTypeRegistry(),
                                 0, {}, enable_unknowns);
  return cel_expr.Evaluate(activation, arena);
}

class CreateCreateStructStepTest : public testing::TestWithParam<bool> {};

TEST_P(CreateCreateStructStepTest, TestEmptyMessageCreation) {
  ExecutionPath path;
  CelTypeRegistry type_registry;
  type_registry.RegisterTypeProvider(
      std::make_unique<ProtobufDescriptorProvider>(
          google::protobuf::DescriptorPool::generated_pool(),
          google::protobuf::MessageFactory::generated_factory()));
  Expr expr1;

  auto& create_struct = expr1.mutable_struct_expr();
  create_struct.set_message_name("google.api.expr.runtime.TestMessage");
  auto adapter = type_registry.FindTypeAdapter(create_struct.message_name());
  ASSERT_TRUE(adapter.has_value() && adapter->mutation_apis() != nullptr);

  ASSERT_OK_AND_ASSIGN(
      auto step, CreateCreateStructStep(create_struct, adapter->mutation_apis(),
                                        expr1.id()));
  path.push_back(std::move(step));

  CelExpressionFlatImpl cel_expr(&expr1, std::move(path), &type_registry, 0, {},
                                 GetParam());
  Activation activation;

  google::protobuf::Arena arena;

  ASSERT_OK_AND_ASSIGN(CelValue result, cel_expr.Evaluate(activation, &arena));
  ASSERT_TRUE(result.IsMessage());
  const Message* msg = result.MessageOrDie();
  ASSERT_THAT(msg, Not(IsNull()));

  ASSERT_EQ(msg->GetDescriptor(), TestMessage::descriptor());
}

TEST_P(CreateCreateStructStepTest, TestMessageCreationBadField) {
  ExecutionPath path;
  CelTypeRegistry type_registry;
  type_registry.RegisterTypeProvider(
      std::make_unique<ProtobufDescriptorProvider>(
          google::protobuf::DescriptorPool::generated_pool(),
          google::protobuf::MessageFactory::generated_factory()));
  Expr expr1;

  auto& create_struct = expr1.mutable_struct_expr();
  create_struct.set_message_name("google.api.expr.runtime.TestMessage");
  auto& entry = create_struct.mutable_entries().emplace_back();
  entry.set_field_key("bad_field");
  auto& value = entry.mutable_value();
  value.mutable_const_expr().set_bool_value(true);
  auto adapter = type_registry.FindTypeAdapter(create_struct.message_name());
  ASSERT_TRUE(adapter.has_value() && adapter->mutation_apis() != nullptr);

  EXPECT_THAT(CreateCreateStructStep(create_struct, adapter->mutation_apis(),
                                     expr1.id())
                  .status(),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       testing::HasSubstr("'bad_field'")));
}

// Test message creation if unknown argument is passed
TEST(CreateCreateStructStepTest, TestMessageCreateWithUnknown) {
  Arena arena;
  TestMessage test_msg;
  UnknownSet unknown_set;

  auto eval_status = RunExpression(
      "bool_value", CelValue::CreateUnknownSet(&unknown_set), &arena, true);
  ASSERT_OK(eval_status);
  ASSERT_TRUE(eval_status->IsUnknownSet());
}

// Test that fields of type bool are set correctly
TEST_P(CreateCreateStructStepTest, TestSetBoolField) {
  Arena arena;
  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "bool_value", CelValue::CreateBool(true), &arena, &test_msg, GetParam()));
  ASSERT_EQ(test_msg.bool_value(), true);
}

// Test that fields of type int32_t are set correctly
TEST_P(CreateCreateStructStepTest, TestSetInt32Field) {
  Arena arena;
  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "int32_value", CelValue::CreateInt64(1), &arena, &test_msg, GetParam()));

  ASSERT_EQ(test_msg.int32_value(), 1);
}

// Test that fields of type uint32_t are set correctly.
TEST_P(CreateCreateStructStepTest, TestSetUInt32Field) {
  Arena arena;
  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(
      RunExpressionAndGetMessage("uint32_value", CelValue::CreateUint64(1),
                                 &arena, &test_msg, GetParam()));

  ASSERT_EQ(test_msg.uint32_value(), 1);
}

// Test that fields of type int64_t are set correctly.
TEST_P(CreateCreateStructStepTest, TestSetInt64Field) {
  Arena arena;
  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "int64_value", CelValue::CreateInt64(1), &arena, &test_msg, GetParam()));

  EXPECT_EQ(test_msg.int64_value(), 1);
}

// Test that fields of type uint64_t are set correctly.
TEST_P(CreateCreateStructStepTest, TestSetUInt64Field) {
  Arena arena;
  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(
      RunExpressionAndGetMessage("uint64_value", CelValue::CreateUint64(1),
                                 &arena, &test_msg, GetParam()));

  EXPECT_EQ(test_msg.uint64_value(), 1);
}

// Test that fields of type float are set correctly
TEST_P(CreateCreateStructStepTest, TestSetFloatField) {
  Arena arena;
  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(
      RunExpressionAndGetMessage("float_value", CelValue::CreateDouble(2.0),
                                 &arena, &test_msg, GetParam()));

  EXPECT_DOUBLE_EQ(test_msg.float_value(), 2.0);
}

// Test that fields of type double are set correctly
TEST_P(CreateCreateStructStepTest, TestSetDoubleField) {
  Arena arena;
  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(
      RunExpressionAndGetMessage("double_value", CelValue::CreateDouble(2.0),
                                 &arena, &test_msg, GetParam()));
  EXPECT_DOUBLE_EQ(test_msg.double_value(), 2.0);
}

// Test that fields of type string are set correctly.
TEST_P(CreateCreateStructStepTest, TestSetStringField) {
  const std::string kTestStr = "test";

  Arena arena;
  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "string_value", CelValue::CreateString(&kTestStr), &arena, &test_msg,
      GetParam()));
  EXPECT_EQ(test_msg.string_value(), kTestStr);
}


// Test that fields of type bytes are set correctly.
TEST_P(CreateCreateStructStepTest, TestSetBytesField) {
  Arena arena;

  const std::string kTestStr = "test";
  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "bytes_value", CelValue::CreateBytes(&kTestStr), &arena, &test_msg,
      GetParam()));
  EXPECT_EQ(test_msg.bytes_value(), kTestStr);
}

// Test that fields of type duration are set correctly.
TEST_P(CreateCreateStructStepTest, TestSetDurationField) {
  Arena arena;

  google::protobuf::Duration test_duration;
  test_duration.set_seconds(2);
  test_duration.set_nanos(3);
  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "duration_value", CelProtoWrapper::CreateDuration(&test_duration), &arena,
      &test_msg, GetParam()));
  EXPECT_THAT(test_msg.duration_value(), EqualsProto(test_duration));
}

// Test that fields of type timestamp are set correctly.
TEST_P(CreateCreateStructStepTest, TestSetTimestampField) {
  Arena arena;

  google::protobuf::Timestamp test_timestamp;
  test_timestamp.set_seconds(2);
  test_timestamp.set_nanos(3);
  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "timestamp_value", CelProtoWrapper::CreateTimestamp(&test_timestamp),
      &arena, &test_msg, GetParam()));
  EXPECT_THAT(test_msg.timestamp_value(), EqualsProto(test_timestamp));
}

// Test that fields of type Message are set correctly.
TEST_P(CreateCreateStructStepTest, TestSetMessageField) {
  Arena arena;

  // Create payload message and set some fields.
  TestMessage orig_msg;
  orig_msg.set_bool_value(true);
  orig_msg.set_string_value("test");

  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "message_value", CelProtoWrapper::CreateMessage(&orig_msg, &arena),
      &arena, &test_msg, GetParam()));
  EXPECT_THAT(test_msg.message_value(), EqualsProto(orig_msg));
}

// Test that fields of type Any are set correctly.
TEST_P(CreateCreateStructStepTest, TestSetAnyField) {
  Arena arena;

  // Create payload message and set some fields.
  TestMessage orig_embedded_msg;
  orig_embedded_msg.set_bool_value(true);
  orig_embedded_msg.set_string_value("embedded");

  TestMessage orig_msg;
  orig_msg.mutable_any_value()->PackFrom(orig_embedded_msg);

  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "any_value", CelProtoWrapper::CreateMessage(&orig_embedded_msg, &arena),
      &arena, &test_msg, GetParam()));
  EXPECT_THAT(test_msg, EqualsProto(orig_msg));

  TestMessage test_embedded_msg;
  ASSERT_TRUE(test_msg.any_value().UnpackTo(&test_embedded_msg));
  EXPECT_THAT(test_embedded_msg, EqualsProto(orig_embedded_msg));
}

// Test that fields of type Message are set correctly.
TEST_P(CreateCreateStructStepTest, TestSetEnumField) {
  Arena arena;
  TestMessage test_msg;

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "enum_value", CelValue::CreateInt64(TestMessage::TEST_ENUM_2), &arena,
      &test_msg, GetParam()));
  EXPECT_EQ(test_msg.enum_value(), TestMessage::TEST_ENUM_2);
}

// Test that fields of type bool are set correctly
TEST_P(CreateCreateStructStepTest, TestSetRepeatedBoolField) {
  Arena arena;
  TestMessage test_msg;

  std::vector<bool> kValues = {true, false};
  std::vector<CelValue> values;
  for (auto value : kValues) {
    values.push_back(CelValue::CreateBool(value));
  }

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "bool_list", values, &arena, &test_msg, GetParam()));
  ASSERT_THAT(test_msg.bool_list(), Pointwise(Eq(), kValues));
}

// Test that repeated fields of type int32_t are set correctly
TEST_P(CreateCreateStructStepTest, TestSetRepeatedInt32Field) {
  Arena arena;
  TestMessage test_msg;

  std::vector<int32_t> kValues = {23, 12};
  std::vector<CelValue> values;
  for (auto value : kValues) {
    values.push_back(CelValue::CreateInt64(value));
  }

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "int32_list", values, &arena, &test_msg, GetParam()));
  ASSERT_THAT(test_msg.int32_list(), Pointwise(Eq(), kValues));
}

// Test that repeated fields of type uint32_t are set correctly
TEST_P(CreateCreateStructStepTest, TestSetRepeatedUInt32Field) {
  Arena arena;
  TestMessage test_msg;

  std::vector<uint32_t> kValues = {23, 12};
  std::vector<CelValue> values;
  for (auto value : kValues) {
    values.push_back(CelValue::CreateUint64(value));
  }

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "uint32_list", values, &arena, &test_msg, GetParam()));
  ASSERT_THAT(test_msg.uint32_list(), Pointwise(Eq(), kValues));
}

// Test that repeated fields of type int64_t are set correctly
TEST_P(CreateCreateStructStepTest, TestSetRepeatedInt64Field) {
  Arena arena;
  TestMessage test_msg;

  std::vector<int64_t> kValues = {23, 12};
  std::vector<CelValue> values;
  for (auto value : kValues) {
    values.push_back(CelValue::CreateInt64(value));
  }

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "int64_list", values, &arena, &test_msg, GetParam()));
  ASSERT_THAT(test_msg.int64_list(), Pointwise(Eq(), kValues));
}

// Test that repeated fields of type uint64_t are set correctly
TEST_P(CreateCreateStructStepTest, TestSetRepeatedUInt64Field) {
  Arena arena;
  TestMessage test_msg;

  std::vector<uint64_t> kValues = {23, 12};
  std::vector<CelValue> values;
  for (auto value : kValues) {
    values.push_back(CelValue::CreateUint64(value));
  }

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "uint64_list", values, &arena, &test_msg, GetParam()));
  ASSERT_THAT(test_msg.uint64_list(), Pointwise(Eq(), kValues));
}

// Test that repeated fields of type float are set correctly
TEST_P(CreateCreateStructStepTest, TestSetRepeatedFloatField) {
  Arena arena;
  TestMessage test_msg;

  std::vector<float> kValues = {23, 12};
  std::vector<CelValue> values;
  for (auto value : kValues) {
    values.push_back(CelValue::CreateDouble(value));
  }

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "float_list", values, &arena, &test_msg, GetParam()));
  ASSERT_THAT(test_msg.float_list(), Pointwise(Eq(), kValues));
}

// Test that repeated fields of type uint32_t are set correctly
TEST_P(CreateCreateStructStepTest, TestSetRepeatedDoubleField) {
  Arena arena;
  TestMessage test_msg;

  std::vector<double> kValues = {23, 12};
  std::vector<CelValue> values;
  for (auto value : kValues) {
    values.push_back(CelValue::CreateDouble(value));
  }

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "double_list", values, &arena, &test_msg, GetParam()));
  ASSERT_THAT(test_msg.double_list(), Pointwise(Eq(), kValues));
}

// Test that repeated fields of type String are set correctly
TEST_P(CreateCreateStructStepTest, TestSetRepeatedStringField) {
  Arena arena;
  TestMessage test_msg;

  std::vector<std::string> kValues = {"test1", "test2"};
  std::vector<CelValue> values;
  for (const auto& value : kValues) {
    values.push_back(CelValue::CreateString(&value));
  }

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "string_list", values, &arena, &test_msg, GetParam()));
  ASSERT_THAT(test_msg.string_list(), Pointwise(Eq(), kValues));
}

// Test that repeated fields of type String are set correctly
TEST_P(CreateCreateStructStepTest, TestSetRepeatedBytesField) {
  Arena arena;
  TestMessage test_msg;

  std::vector<std::string> kValues = {"test1", "test2"};
  std::vector<CelValue> values;
  for (const auto& value : kValues) {
    values.push_back(CelValue::CreateBytes(&value));
  }

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "bytes_list", values, &arena, &test_msg, GetParam()));
  ASSERT_THAT(test_msg.bytes_list(), Pointwise(Eq(), kValues));
}


// Test that repeated fields of type Message are set correctly
TEST_P(CreateCreateStructStepTest, TestSetRepeatedMessageField) {
  Arena arena;
  TestMessage test_msg;

  std::vector<TestMessage> kValues(2);
  kValues[0].set_string_value("test1");
  kValues[1].set_string_value("test2");
  std::vector<CelValue> values;
  for (const auto& value : kValues) {
    values.push_back(CelProtoWrapper::CreateMessage(&value, &arena));
  }

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "message_list", values, &arena, &test_msg, GetParam()));
  ASSERT_THAT(test_msg.message_list()[0], EqualsProto(kValues[0]));
  ASSERT_THAT(test_msg.message_list()[1], EqualsProto(kValues[1]));
}


// Test that fields of type map<string, ...> are set correctly
TEST_P(CreateCreateStructStepTest, TestSetStringMapField) {
  Arena arena;
  TestMessage test_msg;

  std::vector<std::pair<CelValue, CelValue>> entries;

  const std::vector<std::string> kKeys = {"test2", "test1"};

  entries.push_back(
      {CelValue::CreateString(&kKeys[0]), CelValue::CreateInt64(2)});
  entries.push_back(
      {CelValue::CreateString(&kKeys[1]), CelValue::CreateInt64(1)});

  auto cel_map =
      *CreateContainerBackedMap(absl::Span<std::pair<CelValue, CelValue>>(
          entries.data(), entries.size()));

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "string_int32_map", CelValue::CreateMap(cel_map.get()), &arena, &test_msg,
      GetParam()));

  ASSERT_EQ(test_msg.string_int32_map().size(), 2);
  ASSERT_EQ(test_msg.string_int32_map().at(kKeys[0]), 2);
  ASSERT_EQ(test_msg.string_int32_map().at(kKeys[1]), 1);
}

// Test that fields of type map<int64_t, ...> are set correctly
TEST_P(CreateCreateStructStepTest, TestSetInt64MapField) {
  Arena arena;
  TestMessage test_msg;

  std::vector<std::pair<CelValue, CelValue>> entries;

  const std::vector<int64_t> kKeys = {3, 4};

  entries.push_back(
      {CelValue::CreateInt64(kKeys[0]), CelValue::CreateInt64(1)});
  entries.push_back(
      {CelValue::CreateInt64(kKeys[1]), CelValue::CreateInt64(2)});

  auto cel_map =
      *CreateContainerBackedMap(absl::Span<std::pair<CelValue, CelValue>>(
          entries.data(), entries.size()));

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "int64_int32_map", CelValue::CreateMap(cel_map.get()), &arena, &test_msg,
      GetParam()));

  ASSERT_EQ(test_msg.int64_int32_map().size(), 2);
  ASSERT_EQ(test_msg.int64_int32_map().at(kKeys[0]), 1);
  ASSERT_EQ(test_msg.int64_int32_map().at(kKeys[1]), 2);
}

// Test that fields of type map<uint64_t, ...> are set correctly
TEST_P(CreateCreateStructStepTest, TestSetUInt64MapField) {
  Arena arena;
  TestMessage test_msg;

  std::vector<std::pair<CelValue, CelValue>> entries;

  const std::vector<uint64_t> kKeys = {3, 4};

  entries.push_back(
      {CelValue::CreateUint64(kKeys[0]), CelValue::CreateInt64(1)});
  entries.push_back(
      {CelValue::CreateUint64(kKeys[1]), CelValue::CreateInt64(2)});

  auto cel_map =
      *CreateContainerBackedMap(absl::Span<std::pair<CelValue, CelValue>>(
          entries.data(), entries.size()));

  ASSERT_NO_FATAL_FAILURE(RunExpressionAndGetMessage(
      "uint64_int32_map", CelValue::CreateMap(cel_map.get()), &arena, &test_msg,
      GetParam()));

  ASSERT_EQ(test_msg.uint64_int32_map().size(), 2);
  ASSERT_EQ(test_msg.uint64_int32_map().at(kKeys[0]), 1);
  ASSERT_EQ(test_msg.uint64_int32_map().at(kKeys[1]), 2);
}

// Test that Empty Map is created successfully.
TEST_P(CreateCreateStructStepTest, TestCreateEmptyMap) {
  Arena arena;
  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunCreateMapExpression({}, &arena, GetParam()));
  ASSERT_TRUE(result.IsMap());

  const CelMap* cel_map = result.MapOrDie();
  ASSERT_EQ(cel_map->size(), 0);
}

// Test message creation if unknown argument is passed
TEST(CreateCreateStructStepTest, TestMapCreateWithUnknown) {
  Arena arena;
  UnknownSet unknown_set;
  std::vector<std::pair<CelValue, CelValue>> entries;

  std::vector<std::string> kKeys = {"test2", "test1"};

  entries.push_back(
      {CelValue::CreateString(&kKeys[0]), CelValue::CreateInt64(2)});
  entries.push_back({CelValue::CreateString(&kKeys[1]),
                     CelValue::CreateUnknownSet(&unknown_set)});

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunCreateMapExpression(entries, &arena, true));
  ASSERT_TRUE(result.IsUnknownSet());
}

// Test that String Map is created successfully.
TEST_P(CreateCreateStructStepTest, TestCreateStringMap) {
  Arena arena;

  std::vector<std::pair<CelValue, CelValue>> entries;

  std::vector<std::string> kKeys = {"test2", "test1"};

  entries.push_back(
      {CelValue::CreateString(&kKeys[0]), CelValue::CreateInt64(2)});
  entries.push_back(
      {CelValue::CreateString(&kKeys[1]), CelValue::CreateInt64(1)});

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       RunCreateMapExpression(entries, &arena, GetParam()));
  ASSERT_TRUE(result.IsMap());

  const CelMap* cel_map = result.MapOrDie();
  ASSERT_EQ(cel_map->size(), 2);

  auto lookup0 = (*cel_map)[CelValue::CreateString(&kKeys[0])];
  ASSERT_TRUE(lookup0.has_value());
  ASSERT_TRUE(lookup0->IsInt64());
  EXPECT_EQ(lookup0->Int64OrDie(), 2);

  auto lookup1 = (*cel_map)[CelValue::CreateString(&kKeys[1])];
  ASSERT_TRUE(lookup1.has_value());
  ASSERT_TRUE(lookup1->IsInt64());
  EXPECT_EQ(lookup1->Int64OrDie(), 1);
}

INSTANTIATE_TEST_SUITE_P(CombinedCreateStructTest, CreateCreateStructStepTest,
                         testing::Bool());

}  // namespace

}  // namespace google::api::expr::runtime
