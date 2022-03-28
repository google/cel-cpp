// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "eval/public/structs/proto_message_type_adapter.h"

#include "google/protobuf/wrappers.pb.h"
#include "google/protobuf/descriptor.pb.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"
#include "absl/status/status.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/containers/field_access.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/public/testing/matchers.h"
#include "eval/testutil/test_message.pb.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"
#include "internal/testing.h"

namespace google::api::expr::runtime {
namespace {

using testing::_;
using testing::EqualsProto;
using testing::HasSubstr;
using testing::Optional;
using cel::internal::IsOkAndHolds;
using cel::internal::StatusIs;

TEST(ProtoMessageTypeAdapter, HasFieldSingular) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory(),
      ProtoWrapperTypeOptions::kUnsetNull);

  TestMessage example;

  CelValue value = CelProtoWrapper::CreateMessage(&example, &arena);

  EXPECT_THAT(adapter.HasField("int64_value", value), IsOkAndHolds(false));
  example.set_int64_value(10);
  EXPECT_THAT(adapter.HasField("int64_value", value), IsOkAndHolds(true));
}

TEST(ProtoMessageTypeAdapter, HasFieldRepeated) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory(),
      ProtoWrapperTypeOptions::kUnsetNull);

  TestMessage example;

  CelValue value = CelProtoWrapper::CreateMessage(&example, &arena);

  EXPECT_THAT(adapter.HasField("int64_list", value), IsOkAndHolds(false));
  example.add_int64_list(10);
  EXPECT_THAT(adapter.HasField("int64_list", value), IsOkAndHolds(true));
}

TEST(ProtoMessageTypeAdapter, HasFieldMap) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory(),
      ProtoWrapperTypeOptions::kUnsetNull);

  TestMessage example;
  example.set_int64_value(10);

  CelValue value = CelProtoWrapper::CreateMessage(&example, &arena);

  EXPECT_THAT(adapter.HasField("int64_int32_map", value), IsOkAndHolds(false));
  (*example.mutable_int64_int32_map())[2] = 3;
  EXPECT_THAT(adapter.HasField("int64_int32_map", value), IsOkAndHolds(true));
}

TEST(ProtoMessageTypeAdapter, HasFieldUnknownField) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory(),
      ProtoWrapperTypeOptions::kUnsetNull);

  TestMessage example;
  example.set_int64_value(10);

  CelValue value = CelProtoWrapper::CreateMessage(&example, &arena);

  EXPECT_THAT(adapter.HasField("unknown_field", value),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST(ProtoMessageTypeAdapter, HasFieldNonMessageType) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory(),
      ProtoWrapperTypeOptions::kUnsetNull);

  CelValue value = CelValue::CreateInt64(10);

  EXPECT_THAT(adapter.HasField("unknown_field", value),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ProtoMessageTypeAdapter, GetFieldSingular) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory(),
      ProtoWrapperTypeOptions::kUnsetNull);
  cel::extensions::ProtoMemoryManager manager(&arena);

  TestMessage example;
  example.set_int64_value(10);

  CelValue value = CelProtoWrapper::CreateMessage(&example, &arena);

  EXPECT_THAT(adapter.GetField("int64_value", value, manager),
              IsOkAndHolds(test::IsCelInt64(10)));
}

TEST(ProtoMessageTypeAdapter, GetFieldNoSuchField) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory(),
      ProtoWrapperTypeOptions::kUnsetNull);
  cel::extensions::ProtoMemoryManager manager(&arena);

  TestMessage example;
  example.set_int64_value(10);

  CelValue value = CelProtoWrapper::CreateMessage(&example, &arena);

  EXPECT_THAT(adapter.GetField("unknown_field", value, manager),
              IsOkAndHolds(test::IsCelError(StatusIs(
                  absl::StatusCode::kNotFound, HasSubstr("unknown_field")))));
}

TEST(ProtoMessageTypeAdapter, GetFieldNotAMessage) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory(),
      ProtoWrapperTypeOptions::kUnsetNull);
  cel::extensions::ProtoMemoryManager manager(&arena);

  CelValue value = CelValue::CreateNull();

  EXPECT_THAT(adapter.GetField("int64_value", value, manager),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ProtoMessageTypeAdapter, GetFieldRepeated) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory(),
      ProtoWrapperTypeOptions::kUnsetNull);
  cel::extensions::ProtoMemoryManager manager(&arena);

  TestMessage example;
  example.add_int64_list(10);
  example.add_int64_list(20);

  CelValue value = CelProtoWrapper::CreateMessage(&example, &arena);

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       adapter.GetField("int64_list", value, manager));

  const CelList* held_value;
  ASSERT_TRUE(result.GetValue(&held_value)) << result.DebugString();

  EXPECT_EQ(held_value->size(), 2);
  EXPECT_THAT((*held_value)[0], test::IsCelInt64(10));
  EXPECT_THAT((*held_value)[1], test::IsCelInt64(20));
}

TEST(ProtoMessageTypeAdapter, GetFieldMap) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory(),
      ProtoWrapperTypeOptions::kUnsetNull);
  cel::extensions::ProtoMemoryManager manager(&arena);

  TestMessage example;
  (*example.mutable_int64_int32_map())[10] = 20;

  CelValue value = CelProtoWrapper::CreateMessage(&example, &arena);

  ASSERT_OK_AND_ASSIGN(CelValue result,
                       adapter.GetField("int64_int32_map", value, manager));

  const CelMap* held_value;
  ASSERT_TRUE(result.GetValue(&held_value)) << result.DebugString();

  EXPECT_EQ(held_value->size(), 1);
  EXPECT_THAT((*held_value)[CelValue::CreateInt64(10)],
              Optional(test::IsCelInt64(20)));
}

TEST(ProtoMessageTypeAdapter, GetFieldWrapperType) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory(),
      ProtoWrapperTypeOptions::kUnsetNull);
  cel::extensions::ProtoMemoryManager manager(&arena);

  TestMessage example;
  example.mutable_int64_wrapper_value()->set_value(10);

  CelValue value = CelProtoWrapper::CreateMessage(&example, &arena);

  EXPECT_THAT(adapter.GetField("int64_wrapper_value", value, manager),
              IsOkAndHolds(test::IsCelInt64(10)));
}

TEST(ProtoMessageTypeAdapter, GetFieldWrapperTypeUnsetNullUnbox) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory(),
      ProtoWrapperTypeOptions::kUnsetNull);
  cel::extensions::ProtoMemoryManager manager(&arena);

  TestMessage example;

  CelValue value = CelProtoWrapper::CreateMessage(&example, &arena);

  EXPECT_THAT(adapter.GetField("int64_wrapper_value", value, manager),
              IsOkAndHolds(test::IsCelNull()));

  // Wrapper field present, but default value.
  example.mutable_int64_wrapper_value()->clear_value();
  EXPECT_THAT(adapter.GetField("int64_wrapper_value", value, manager),
              IsOkAndHolds(test::IsCelInt64(_)));
}

TEST(ProtoMessageTypeAdapter, GetFieldWrapperTypeUnsetDefaultValueUnbox) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory(),
      ProtoWrapperTypeOptions::kUnsetProtoDefault);
  cel::extensions::ProtoMemoryManager manager(&arena);

  TestMessage example;

  CelValue value = CelProtoWrapper::CreateMessage(&example, &arena);

  EXPECT_THAT(adapter.GetField("int64_wrapper_value", value, manager),
              IsOkAndHolds(test::IsCelInt64(_)));

  // Wrapper field present with unset value is used to signal Null, but legacy
  // behavior just returns the proto default value.
  example.mutable_int64_wrapper_value()->clear_value();
  // Same behavior for this option.
  EXPECT_THAT(adapter.GetField("int64_wrapper_value", value, manager),
              IsOkAndHolds(test::IsCelInt64(_)));
}

TEST(ProtoMessageTypeAdapter, NewInstance) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory(),
      ProtoWrapperTypeOptions::kUnsetNull);
  cel::extensions::ProtoMemoryManager manager(&arena);

  ASSERT_OK_AND_ASSIGN(CelValue result, adapter.NewInstance(manager));
  const google::protobuf::Message* message;
  ASSERT_TRUE(result.GetValue(&message));
  EXPECT_THAT(message, EqualsProto(TestMessage::default_instance()));
}

TEST(ProtoMessageTypeAdapter, NewInstanceUnsupportedDescriptor) {
  google::protobuf::Arena arena;

  google::protobuf::DescriptorPool pool;
  google::protobuf::FileDescriptorProto faked_file;
  faked_file.set_name("faked.proto");
  faked_file.set_syntax("proto3");
  faked_file.set_package("google.api.expr.runtime");
  auto msg_descriptor = faked_file.add_message_type();
  msg_descriptor->set_name("FakeMessage");
  pool.BuildFile(faked_file);

  ProtoMessageTypeAdapter adapter(
      pool.FindMessageTypeByName("google.api.expr.runtime.FakeMessage"),
      google::protobuf::MessageFactory::generated_factory(),
      ProtoWrapperTypeOptions::kUnsetNull);
  cel::extensions::ProtoMemoryManager manager(&arena);

  // Message factory doesn't know how to create our custom message, even though
  // we provided a descriptor for it.
  EXPECT_THAT(
      adapter.NewInstance(manager),
      StatusIs(absl::StatusCode::kInvalidArgument, HasSubstr("FakeMessage")));
}

TEST(ProtoMessageTypeAdapter, DefinesField) {
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory(),
      ProtoWrapperTypeOptions::kUnsetNull);

  EXPECT_TRUE(adapter.DefinesField("int64_value"));
  EXPECT_FALSE(adapter.DefinesField("not_a_field"));
}

TEST(ProtoMessageTypeAdapter, SetFieldSingular) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory(),
      ProtoWrapperTypeOptions::kUnsetNull);
  cel::extensions::ProtoMemoryManager manager(&arena);

  ASSERT_OK_AND_ASSIGN(CelValue value, adapter.NewInstance(manager));

  ASSERT_OK(adapter.SetField("int64_value", CelValue::CreateInt64(10), manager,
                             value));

  const google::protobuf::Message* message;
  ASSERT_TRUE(value.GetValue(&message));
  EXPECT_THAT(message, EqualsProto("int64_value: 10"));

  ASSERT_THAT(adapter.SetField("not_a_field", CelValue::CreateInt64(10),
                               manager, value),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("field 'not_a_field': not found")));
}

TEST(ProtoMessageTypeAdapter, SetFieldMap) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory(),
      ProtoWrapperTypeOptions::kUnsetNull);
  cel::extensions::ProtoMemoryManager manager(&arena);

  CelMapBuilder builder;
  ASSERT_OK(builder.Add(CelValue::CreateInt64(1), CelValue::CreateInt64(2)));
  ASSERT_OK(builder.Add(CelValue::CreateInt64(2), CelValue::CreateInt64(4)));

  CelValue value_to_set = CelValue::CreateMap(&builder);

  ASSERT_OK_AND_ASSIGN(CelValue instance, adapter.NewInstance(manager));

  ASSERT_OK(
      adapter.SetField("int64_int32_map", value_to_set, manager, instance));

  const google::protobuf::Message* message;
  ASSERT_TRUE(instance.GetValue(&message));
  EXPECT_THAT(message, EqualsProto(R"pb(
                int64_int32_map { key: 1 value: 2 }
                int64_int32_map { key: 2 value: 4 }
              )pb"));
}

TEST(ProtoMessageTypeAdapter, SetFieldRepeated) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory(),
      ProtoWrapperTypeOptions::kUnsetNull);
  cel::extensions::ProtoMemoryManager manager(&arena);

  ContainerBackedListImpl list(
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2)});
  CelValue value_to_set = CelValue::CreateList(&list);
  ASSERT_OK_AND_ASSIGN(CelValue instance, adapter.NewInstance(manager));

  ASSERT_OK(adapter.SetField("int64_list", value_to_set, manager, instance));

  const google::protobuf::Message* message;
  ASSERT_TRUE(instance.GetValue(&message));
  EXPECT_THAT(message, EqualsProto(R"pb(
                int64_list: 1 int64_list: 2
              )pb"));
}

TEST(ProtoMessageTypeAdapter, SetFieldNotAField) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory(),
      ProtoWrapperTypeOptions::kUnsetNull);
  cel::extensions::ProtoMemoryManager manager(&arena);

  ASSERT_OK_AND_ASSIGN(CelValue instance, adapter.NewInstance(manager));

  ASSERT_THAT(adapter.SetField("not_a_field", CelValue::CreateInt64(10),
                               manager, instance),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("field 'not_a_field': not found")));
}

TEST(ProtoMesssageTypeAdapter, SetFieldWrongType) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory(),
      ProtoWrapperTypeOptions::kUnsetNull);
  cel::extensions::ProtoMemoryManager manager(&arena);

  ContainerBackedListImpl list(
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2)});
  CelValue list_value = CelValue::CreateList(&list);

  CelMapBuilder builder;
  ASSERT_OK(builder.Add(CelValue::CreateInt64(1), CelValue::CreateInt64(2)));
  ASSERT_OK(builder.Add(CelValue::CreateInt64(2), CelValue::CreateInt64(4)));

  CelValue map_value = CelValue::CreateMap(&builder);

  CelValue int_value = CelValue::CreateInt64(42);

  ASSERT_OK_AND_ASSIGN(CelValue instance, adapter.NewInstance(manager));

  EXPECT_THAT(adapter.SetField("int64_value", map_value, manager, instance),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(adapter.SetField("int64_value", list_value, manager, instance),
              StatusIs(absl::StatusCode::kInvalidArgument));

  EXPECT_THAT(
      adapter.SetField("int64_int32_map", list_value, manager, instance),
      StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(adapter.SetField("int64_int32_map", int_value, manager, instance),
              StatusIs(absl::StatusCode::kInvalidArgument));

  EXPECT_THAT(adapter.SetField("int64_list", int_value, manager, instance),
              StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(adapter.SetField("int64_list", map_value, manager, instance),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST(ProtoMesssageTypeAdapter, SetFieldNotAMessage) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory(),
      ProtoWrapperTypeOptions::kUnsetNull);
  cel::extensions::ProtoMemoryManager manager(&arena);

  CelValue int_value = CelValue::CreateInt64(42);
  CelValue instance = CelValue::CreateNull();

  EXPECT_THAT(adapter.SetField("int64_value", int_value, manager, instance),
              StatusIs(absl::StatusCode::kInternal));
}

TEST(ProtoMessageTypeAdapter, AdaptFromWellKnownType) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.protobuf.Int64Value"),
      google::protobuf::MessageFactory::generated_factory(),
      ProtoWrapperTypeOptions::kUnsetNull);
  cel::extensions::ProtoMemoryManager manager(&arena);

  ASSERT_OK_AND_ASSIGN(CelValue instance, adapter.NewInstance(manager));
  ASSERT_OK(
      adapter.SetField("value", CelValue::CreateInt64(42), manager, instance));

  ASSERT_OK(adapter.AdaptFromWellKnownType(manager, instance));

  EXPECT_THAT(instance, test::IsCelInt64(42));
}

TEST(ProtoMessageTypeAdapter, AdaptFromWellKnownTypeUnspecial) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory(),
      ProtoWrapperTypeOptions::kUnsetNull);
  cel::extensions::ProtoMemoryManager manager(&arena);

  ASSERT_OK_AND_ASSIGN(CelValue instance, adapter.NewInstance(manager));
  ASSERT_OK(adapter.SetField("int64_value", CelValue::CreateInt64(42), manager,
                             instance));

  ASSERT_OK(adapter.AdaptFromWellKnownType(manager, instance));

  // TestMessage should not be converted to a CEL primitive type.
  EXPECT_THAT(instance, test::IsCelMessage(EqualsProto("int64_value: 42")));
}

TEST(ProtoMessageTypeAdapter, AdaptFromWellKnownTypeNotAMessageError) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory(),
      ProtoWrapperTypeOptions::kUnsetNull);
  cel::extensions::ProtoMemoryManager manager(&arena);

  CelValue instance = CelValue::CreateNull();

  // Interpreter guaranteed to call this with a message type, otherwise,
  // something has broken.
  EXPECT_THAT(adapter.AdaptFromWellKnownType(manager, instance),
              StatusIs(absl::StatusCode::kInternal));
}

}  // namespace
}  // namespace google::api::expr::runtime
