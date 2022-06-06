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
#include "google/protobuf/message_lite.h"
#include "absl/status/status.h"
#include "eval/public/cel_value.h"
#include "eval/public/containers/container_backed_list_impl.h"
#include "eval/public/containers/container_backed_map_impl.h"
#include "eval/public/containers/field_access.h"
#include "eval/public/message_wrapper.h"
#include "eval/public/structs/cel_proto_wrapper.h"
#include "eval/public/structs/legacy_type_adapter.h"
#include "eval/public/structs/legacy_type_info_apis.h"
#include "eval/public/testing/matchers.h"
#include "eval/testutil/test_message.pb.h"
#include "extensions/protobuf/memory_manager.h"
#include "internal/status_macros.h"
#include "internal/testing.h"
#include "testutil/util.h"

namespace google::api::expr::runtime {
namespace {

using ::cel::extensions::ProtoMemoryManager;
using ::google::protobuf::Int64Value;
using testing::_;
using testing::HasSubstr;
using testing::Optional;
using cel::internal::IsOkAndHolds;
using cel::internal::StatusIs;
using testutil::EqualsProto;

class ProtoMessageTypeAccessorTest : public testing::TestWithParam<bool> {
 public:
  ProtoMessageTypeAccessorTest()
      : type_specific_instance_(
            google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
                "google.api.expr.runtime.TestMessage"),
            google::protobuf::MessageFactory::generated_factory()) {}

  const LegacyTypeAccessApis& GetAccessApis() {
    bool use_generic_instance = GetParam();
    if (use_generic_instance) {
      // implementation detail: in general, type info implementations may
      // return a different accessor object based on the messsage instance, but
      // this implemenation returns the same one no matter the message.
      return *GetGenericProtoTypeInfoInstance().GetAccessApis(dummy_);

    } else {
      return type_specific_instance_;
    }
  }

 private:
  ProtoMessageTypeAdapter type_specific_instance_;
  CelValue::MessageWrapper dummy_;
};

TEST_P(ProtoMessageTypeAccessorTest, HasFieldSingular) {
  google::protobuf::Arena arena;
  const LegacyTypeAccessApis& accessor = GetAccessApis();
  TestMessage example;

  MessageWrapper value(&example, nullptr);

  EXPECT_THAT(accessor.HasField("int64_value", value), IsOkAndHolds(false));
  example.set_int64_value(10);
  EXPECT_THAT(accessor.HasField("int64_value", value), IsOkAndHolds(true));
}

TEST_P(ProtoMessageTypeAccessorTest, HasFieldRepeated) {
  google::protobuf::Arena arena;
  const LegacyTypeAccessApis& accessor = GetAccessApis();

  TestMessage example;

  MessageWrapper value(&example, nullptr);

  EXPECT_THAT(accessor.HasField("int64_list", value), IsOkAndHolds(false));
  example.add_int64_list(10);
  EXPECT_THAT(accessor.HasField("int64_list", value), IsOkAndHolds(true));
}

TEST_P(ProtoMessageTypeAccessorTest, HasFieldMap) {
  google::protobuf::Arena arena;
  const LegacyTypeAccessApis& accessor = GetAccessApis();

  TestMessage example;
  example.set_int64_value(10);

  MessageWrapper value(&example, nullptr);

  EXPECT_THAT(accessor.HasField("int64_int32_map", value), IsOkAndHolds(false));
  (*example.mutable_int64_int32_map())[2] = 3;
  EXPECT_THAT(accessor.HasField("int64_int32_map", value), IsOkAndHolds(true));
}

TEST_P(ProtoMessageTypeAccessorTest, HasFieldUnknownField) {
  google::protobuf::Arena arena;
  const LegacyTypeAccessApis& accessor = GetAccessApis();

  TestMessage example;
  example.set_int64_value(10);

  MessageWrapper value(&example, nullptr);

  EXPECT_THAT(accessor.HasField("unknown_field", value),
              StatusIs(absl::StatusCode::kNotFound));
}

TEST_P(ProtoMessageTypeAccessorTest, HasFieldNonMessageType) {
  google::protobuf::Arena arena;
  const LegacyTypeAccessApis& accessor = GetAccessApis();

  MessageWrapper value(static_cast<const google::protobuf::MessageLite*>(nullptr),
                       nullptr);

  EXPECT_THAT(accessor.HasField("unknown_field", value),
              StatusIs(absl::StatusCode::kInternal));
}

TEST_P(ProtoMessageTypeAccessorTest, GetFieldSingular) {
  google::protobuf::Arena arena;
  const LegacyTypeAccessApis& accessor = GetAccessApis();

  ProtoMemoryManager manager(&arena);

  TestMessage example;
  example.set_int64_value(10);

  MessageWrapper value(&example, nullptr);

  EXPECT_THAT(accessor.GetField("int64_value", value,
                                ProtoWrapperTypeOptions::kUnsetNull, manager),
              IsOkAndHolds(test::IsCelInt64(10)));
}

TEST_P(ProtoMessageTypeAccessorTest, GetFieldNoSuchField) {
  google::protobuf::Arena arena;
  const LegacyTypeAccessApis& accessor = GetAccessApis();

  ProtoMemoryManager manager(&arena);

  TestMessage example;
  example.set_int64_value(10);

  MessageWrapper value(&example, nullptr);

  EXPECT_THAT(accessor.GetField("unknown_field", value,
                                ProtoWrapperTypeOptions::kUnsetNull, manager),
              IsOkAndHolds(test::IsCelError(StatusIs(
                  absl::StatusCode::kNotFound, HasSubstr("unknown_field")))));
}

TEST_P(ProtoMessageTypeAccessorTest, GetFieldNotAMessage) {
  google::protobuf::Arena arena;
  const LegacyTypeAccessApis& accessor = GetAccessApis();

  ProtoMemoryManager manager(&arena);

  MessageWrapper value(static_cast<const google::protobuf::MessageLite*>(nullptr),
                       nullptr);

  EXPECT_THAT(accessor.GetField("int64_value", value,
                                ProtoWrapperTypeOptions::kUnsetNull, manager),
              StatusIs(absl::StatusCode::kInternal));
}

TEST_P(ProtoMessageTypeAccessorTest, GetFieldRepeated) {
  google::protobuf::Arena arena;
  const LegacyTypeAccessApis& accessor = GetAccessApis();

  ProtoMemoryManager manager(&arena);

  TestMessage example;
  example.add_int64_list(10);
  example.add_int64_list(20);

  MessageWrapper value(&example, nullptr);

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      accessor.GetField("int64_list", value,
                        ProtoWrapperTypeOptions::kUnsetNull, manager));

  const CelList* held_value;
  ASSERT_TRUE(result.GetValue(&held_value)) << result.DebugString();

  EXPECT_EQ(held_value->size(), 2);
  EXPECT_THAT((*held_value)[0], test::IsCelInt64(10));
  EXPECT_THAT((*held_value)[1], test::IsCelInt64(20));
}

TEST_P(ProtoMessageTypeAccessorTest, GetFieldMap) {
  google::protobuf::Arena arena;
  const LegacyTypeAccessApis& accessor = GetAccessApis();

  ProtoMemoryManager manager(&arena);

  TestMessage example;
  (*example.mutable_int64_int32_map())[10] = 20;

  MessageWrapper value(&example, nullptr);

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      accessor.GetField("int64_int32_map", value,
                        ProtoWrapperTypeOptions::kUnsetNull, manager));

  const CelMap* held_value;
  ASSERT_TRUE(result.GetValue(&held_value)) << result.DebugString();

  EXPECT_EQ(held_value->size(), 1);
  EXPECT_THAT((*held_value)[CelValue::CreateInt64(10)],
              Optional(test::IsCelInt64(20)));
}

TEST_P(ProtoMessageTypeAccessorTest, GetFieldWrapperType) {
  google::protobuf::Arena arena;
  const LegacyTypeAccessApis& accessor = GetAccessApis();

  ProtoMemoryManager manager(&arena);

  TestMessage example;
  example.mutable_int64_wrapper_value()->set_value(10);

  MessageWrapper value(&example, nullptr);

  EXPECT_THAT(accessor.GetField("int64_wrapper_value", value,
                                ProtoWrapperTypeOptions::kUnsetNull, manager),
              IsOkAndHolds(test::IsCelInt64(10)));
}

TEST_P(ProtoMessageTypeAccessorTest, GetFieldWrapperTypeUnsetNullUnbox) {
  google::protobuf::Arena arena;
  const LegacyTypeAccessApis& accessor = GetAccessApis();

  ProtoMemoryManager manager(&arena);

  TestMessage example;

  MessageWrapper value(&example, nullptr);

  EXPECT_THAT(accessor.GetField("int64_wrapper_value", value,
                                ProtoWrapperTypeOptions::kUnsetNull, manager),
              IsOkAndHolds(test::IsCelNull()));

  // Wrapper field present, but default value.
  example.mutable_int64_wrapper_value()->clear_value();
  EXPECT_THAT(accessor.GetField("int64_wrapper_value", value,
                                ProtoWrapperTypeOptions::kUnsetNull, manager),
              IsOkAndHolds(test::IsCelInt64(_)));
}

TEST_P(ProtoMessageTypeAccessorTest,
       GetFieldWrapperTypeUnsetDefaultValueUnbox) {
  google::protobuf::Arena arena;
  const LegacyTypeAccessApis& accessor = GetAccessApis();

  ProtoMemoryManager manager(&arena);

  TestMessage example;

  MessageWrapper value(&example, nullptr);

  EXPECT_THAT(
      accessor.GetField("int64_wrapper_value", value,
                        ProtoWrapperTypeOptions::kUnsetProtoDefault, manager),
      IsOkAndHolds(test::IsCelInt64(_)));

  // Wrapper field present with unset value is used to signal Null, but legacy
  // behavior just returns the proto default value.
  example.mutable_int64_wrapper_value()->clear_value();
  // Same behavior for this option.
  EXPECT_THAT(
      accessor.GetField("int64_wrapper_value", value,
                        ProtoWrapperTypeOptions::kUnsetProtoDefault, manager),
      IsOkAndHolds(test::IsCelInt64(_)));
}

TEST_P(ProtoMessageTypeAccessorTest, IsEqualTo) {
  google::protobuf::Arena arena;
  const LegacyTypeAccessApis& accessor = GetAccessApis();

  ProtoMemoryManager manager(&arena);

  TestMessage example;
  example.mutable_int64_wrapper_value()->set_value(10);
  TestMessage example2;
  example2.mutable_int64_wrapper_value()->set_value(10);

  MessageWrapper value(&example, nullptr);
  MessageWrapper value2(&example2, nullptr);

  EXPECT_TRUE(accessor.IsEqualTo(value, value2));
  EXPECT_TRUE(accessor.IsEqualTo(value2, value));
}

TEST_P(ProtoMessageTypeAccessorTest, IsEqualToSameTypeInequal) {
  google::protobuf::Arena arena;
  const LegacyTypeAccessApis& accessor = GetAccessApis();

  ProtoMemoryManager manager(&arena);

  TestMessage example;
  example.mutable_int64_wrapper_value()->set_value(10);
  TestMessage example2;
  example2.mutable_int64_wrapper_value()->set_value(12);

  MessageWrapper value(&example, nullptr);
  MessageWrapper value2(&example2, nullptr);

  EXPECT_FALSE(accessor.IsEqualTo(value, value2));
  EXPECT_FALSE(accessor.IsEqualTo(value2, value));
}

TEST_P(ProtoMessageTypeAccessorTest, IsEqualToDifferentTypeInequal) {
  google::protobuf::Arena arena;
  const LegacyTypeAccessApis& accessor = GetAccessApis();

  ProtoMemoryManager manager(&arena);

  TestMessage example;
  example.mutable_int64_wrapper_value()->set_value(10);
  Int64Value example2;
  example2.set_value(10);

  MessageWrapper value(&example, nullptr);
  MessageWrapper value2(&example2, nullptr);

  EXPECT_FALSE(accessor.IsEqualTo(value, value2));
  EXPECT_FALSE(accessor.IsEqualTo(value2, value));
}

TEST_P(ProtoMessageTypeAccessorTest, IsEqualToNonMessageInequal) {
  google::protobuf::Arena arena;
  const LegacyTypeAccessApis& accessor = GetAccessApis();

  ProtoMemoryManager manager(&arena);

  TestMessage example;
  example.mutable_int64_wrapper_value()->set_value(10);
  TestMessage example2;
  example2.mutable_int64_wrapper_value()->set_value(10);

  MessageWrapper value(&example, nullptr);
  // Upcast to message lite to prevent unwrapping to message.
  MessageWrapper value2(static_cast<const google::protobuf::MessageLite*>(&example2),
                        nullptr);

  EXPECT_FALSE(accessor.IsEqualTo(value, value2));
  EXPECT_FALSE(accessor.IsEqualTo(value2, value));
}

INSTANTIATE_TEST_SUITE_P(GenericAndSpecific, ProtoMessageTypeAccessorTest,
                         testing::Bool());

TEST(GetGenericProtoTypeInfoInstance, GetTypeName) {
  const LegacyTypeInfoApis& info_api = GetGenericProtoTypeInfoInstance();

  TestMessage test_message;
  CelValue::MessageWrapper wrapped_message(&test_message, nullptr);

  EXPECT_EQ(info_api.GetTypename(wrapped_message), test_message.GetTypeName());
}

TEST(GetGenericProtoTypeInfoInstance, DebugString) {
  const LegacyTypeInfoApis& info_api = GetGenericProtoTypeInfoInstance();

  TestMessage test_message;
  test_message.set_string_value("abcd");
  CelValue::MessageWrapper wrapped_message(&test_message, nullptr);

  EXPECT_EQ(info_api.DebugString(wrapped_message),
            test_message.ShortDebugString());
}

TEST(GetGenericProtoTypeInfoInstance, GetAccessApis) {
  const LegacyTypeInfoApis& info_api = GetGenericProtoTypeInfoInstance();

  TestMessage test_message;
  test_message.set_string_value("abcd");
  CelValue::MessageWrapper wrapped_message(&test_message, nullptr);

  auto* accessor = info_api.GetAccessApis(wrapped_message);
  google::protobuf::Arena arena;
  ProtoMemoryManager manager(&arena);

  ASSERT_OK_AND_ASSIGN(
      CelValue result,
      accessor->GetField("string_value", wrapped_message,
                         ProtoWrapperTypeOptions::kUnsetNull, manager));
  EXPECT_THAT(result, test::IsCelString("abcd"));
}

TEST(GetGenericProtoTypeInfoInstance, FallbackForNonMessage) {
  const LegacyTypeInfoApis& info_api = GetGenericProtoTypeInfoInstance();

  TestMessage test_message;
  test_message.set_string_value("abcd");
  // Upcast to signal no google::protobuf::Message / reflection support.
  CelValue::MessageWrapper wrapped_message(
      static_cast<const google::protobuf::MessageLite*>(&test_message), nullptr);

  EXPECT_EQ(info_api.GetTypename(wrapped_message), "<unknown message>");
  EXPECT_EQ(info_api.DebugString(wrapped_message), "<unknown message>");

  // Check for not-null.
  CelValue::MessageWrapper null_message(
      static_cast<const google::protobuf::Message*>(nullptr), nullptr);

  EXPECT_EQ(info_api.GetTypename(null_message), "<unknown message>");
  EXPECT_EQ(info_api.DebugString(null_message), "<unknown message>");
}

TEST(ProtoMessageTypeAdapter, NewInstance) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory());
  ProtoMemoryManager manager(&arena);

  ASSERT_OK_AND_ASSIGN(CelValue::MessageWrapper::Builder result,
                       adapter.NewInstance(manager));
  EXPECT_EQ(result.message_ptr()->SerializeAsString(), "");
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
      google::protobuf::MessageFactory::generated_factory());
  ProtoMemoryManager manager(&arena);

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
      google::protobuf::MessageFactory::generated_factory());

  EXPECT_TRUE(adapter.DefinesField("int64_value"));
  EXPECT_FALSE(adapter.DefinesField("not_a_field"));
}

TEST(ProtoMessageTypeAdapter, SetFieldSingular) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory());
  ProtoMemoryManager manager(&arena);

  ASSERT_OK_AND_ASSIGN(CelValue::MessageWrapper::Builder value,
                       adapter.NewInstance(manager));

  ASSERT_OK(adapter.SetField("int64_value", CelValue::CreateInt64(10), manager,
                             value));

  TestMessage message;
  message.set_int64_value(10);
  EXPECT_EQ(value.message_ptr()->SerializeAsString(),
            message.SerializeAsString());

  ASSERT_THAT(adapter.SetField("not_a_field", CelValue::CreateInt64(10),
                               manager, value),
              StatusIs(absl::StatusCode::kInvalidArgument,
                       HasSubstr("field 'not_a_field': not found")));
}

TEST(ProtoMessageTypeAdapter, SetFieldRepeated) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory());
  ProtoMemoryManager manager(&arena);

  ContainerBackedListImpl list(
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2)});
  CelValue value_to_set = CelValue::CreateList(&list);
  ASSERT_OK_AND_ASSIGN(CelValue::MessageWrapper::Builder instance,
                       adapter.NewInstance(manager));

  ASSERT_OK(adapter.SetField("int64_list", value_to_set, manager, instance));

  TestMessage message;
  message.add_int64_list(1);
  message.add_int64_list(2);

  EXPECT_EQ(instance.message_ptr()->SerializeAsString(),
            message.SerializeAsString());
}

TEST(ProtoMessageTypeAdapter, SetFieldNotAField) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory());
  ProtoMemoryManager manager(&arena);

  ASSERT_OK_AND_ASSIGN(CelValue::MessageWrapper::Builder instance,
                       adapter.NewInstance(manager));

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
      google::protobuf::MessageFactory::generated_factory());
  ProtoMemoryManager manager(&arena);

  ContainerBackedListImpl list(
      {CelValue::CreateInt64(1), CelValue::CreateInt64(2)});
  CelValue list_value = CelValue::CreateList(&list);

  CelMapBuilder builder;
  ASSERT_OK(builder.Add(CelValue::CreateInt64(1), CelValue::CreateInt64(2)));
  ASSERT_OK(builder.Add(CelValue::CreateInt64(2), CelValue::CreateInt64(4)));

  CelValue map_value = CelValue::CreateMap(&builder);

  CelValue int_value = CelValue::CreateInt64(42);

  ASSERT_OK_AND_ASSIGN(CelValue::MessageWrapper::Builder instance,
                       adapter.NewInstance(manager));

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
      google::protobuf::MessageFactory::generated_factory());
  ProtoMemoryManager manager(&arena);

  CelValue int_value = CelValue::CreateInt64(42);
  CelValue::MessageWrapper::Builder instance(
      static_cast<google::protobuf::MessageLite*>(nullptr));

  EXPECT_THAT(adapter.SetField("int64_value", int_value, manager, instance),
              StatusIs(absl::StatusCode::kInternal));
}

TEST(ProtoMesssageTypeAdapter, SetFieldNullMessage) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory());
  ProtoMemoryManager manager(&arena);

  CelValue int_value = CelValue::CreateInt64(42);
  CelValue::MessageWrapper::Builder instance(
      static_cast<google::protobuf::Message*>(nullptr));

  EXPECT_THAT(adapter.SetField("int64_value", int_value, manager, instance),
              StatusIs(absl::StatusCode::kInternal));
}

TEST(ProtoMessageTypeAdapter, AdaptFromWellKnownType) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.protobuf.Int64Value"),
      google::protobuf::MessageFactory::generated_factory());
  ProtoMemoryManager manager(&arena);

  ASSERT_OK_AND_ASSIGN(CelValue::MessageWrapper::Builder instance,
                       adapter.NewInstance(manager));
  ASSERT_OK(
      adapter.SetField("value", CelValue::CreateInt64(42), manager, instance));

  ASSERT_OK_AND_ASSIGN(CelValue value,
                       adapter.AdaptFromWellKnownType(manager, instance));

  EXPECT_THAT(value, test::IsCelInt64(42));
}

TEST(ProtoMessageTypeAdapter, AdaptFromWellKnownTypeUnspecial) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory());
  ProtoMemoryManager manager(&arena);

  ASSERT_OK_AND_ASSIGN(CelValue::MessageWrapper::Builder instance,
                       adapter.NewInstance(manager));

  ASSERT_OK(adapter.SetField("int64_value", CelValue::CreateInt64(42), manager,
                             instance));
  ASSERT_OK_AND_ASSIGN(CelValue value,
                       adapter.AdaptFromWellKnownType(manager, instance));

  // TestMessage should not be converted to a CEL primitive type.
  EXPECT_THAT(value, test::IsCelMessage(EqualsProto("int64_value: 42")));
}

TEST(ProtoMessageTypeAdapter, AdaptFromWellKnownTypeNotAMessageError) {
  google::protobuf::Arena arena;
  ProtoMessageTypeAdapter adapter(
      google::protobuf::DescriptorPool::generated_pool()->FindMessageTypeByName(
          "google.api.expr.runtime.TestMessage"),
      google::protobuf::MessageFactory::generated_factory());
  ProtoMemoryManager manager(&arena);

  CelValue::MessageWrapper::Builder instance(
      static_cast<google::protobuf::MessageLite*>(nullptr));

  // Interpreter guaranteed to call this with a message type, otherwise,
  // something has broken.
  EXPECT_THAT(adapter.AdaptFromWellKnownType(manager, instance),
              StatusIs(absl::StatusCode::kInternal));
}

}  // namespace
}  // namespace google::api::expr::runtime
