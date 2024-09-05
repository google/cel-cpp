// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "absl/base/nullability.h"
#include "absl/log/die_if_null.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/status/statusor.h"
#include "absl/strings/cord.h"
#include "absl/types/optional.h"
#include "common/allocator.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_reflector.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "common/value_manager.h"
#include "internal/parse_text_proto.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "internal/testing_message_factory.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/message.h"

namespace cel {
namespace {

using ::absl_testing::StatusIs;
using ::cel::internal::DynamicParseTextProto;
using ::cel::internal::GetTestingDescriptorPool;
using ::cel::internal::GetTestingMessageFactory;
using ::testing::_;
using ::testing::PrintToStringParamName;
using ::testing::TestWithParam;

using TestAllTypesProto3 = ::google::api::expr::test::v1::proto3::TestAllTypes;

class ParsedMapFieldValueTest : public TestWithParam<AllocatorKind> {
 public:
  void SetUp() override {
    switch (GetParam()) {
      case AllocatorKind::kArena:
        arena_.emplace();
        value_manager_ = NewThreadCompatibleValueManager(
            MemoryManager::Pooling(arena()),
            NewThreadCompatibleTypeReflector(MemoryManager::Pooling(arena())));
        break;
      case AllocatorKind::kNewDelete:
        value_manager_ = NewThreadCompatibleValueManager(
            MemoryManager::ReferenceCounting(),
            NewThreadCompatibleTypeReflector(
                MemoryManager::ReferenceCounting()));
        break;
    }
  }

  void TearDown() override {
    value_manager_.reset();
    arena_.reset();
  }

  Allocator<> allocator() {
    return arena_ ? ArenaAllocator(&*arena_) : NewDeleteAllocator();
  }

  absl::Nullable<google::protobuf::Arena*> arena() { return allocator().arena(); }

  absl::Nonnull<const google::protobuf::DescriptorPool*> descriptor_pool() {
    return GetTestingDescriptorPool();
  }

  absl::Nonnull<google::protobuf::MessageFactory*> message_factory() {
    return GetTestingMessageFactory();
  }

  ValueManager& value_manager() { return **value_manager_; }

 private:
  absl::optional<google::protobuf::Arena> arena_;
  absl::optional<Shared<ValueManager>> value_manager_;
};

TEST_P(ParsedMapFieldValueTest, Default) {
  ParsedMapFieldValue value;
  EXPECT_FALSE(value);
}

TEST_P(ParsedMapFieldValueTest, Field) {
  auto message = DynamicParseTextProto<TestAllTypesProto3>(
      allocator(), R"pb()pb", descriptor_pool(), message_factory());
  ParsedMapFieldValue value(
      message, ABSL_DIE_IF_NULL(message->GetDescriptor()->FindFieldByName(
                   "map_int64_int64")));
  EXPECT_TRUE(value);
}

TEST_P(ParsedMapFieldValueTest, Kind) {
  ParsedMapFieldValue value;
  EXPECT_EQ(value.kind(), ParsedMapFieldValue::kKind);
  EXPECT_EQ(value.kind(), ValueKind::kMap);
}

TEST_P(ParsedMapFieldValueTest, GetTypeName) {
  ParsedMapFieldValue value;
  EXPECT_EQ(value.GetTypeName(), ParsedMapFieldValue::kName);
  EXPECT_EQ(value.GetTypeName(), "map");
}

TEST_P(ParsedMapFieldValueTest, GetRuntimeType) {
  ParsedMapFieldValue value;
  EXPECT_EQ(value.GetRuntimeType(), MapType());
}

TEST_P(ParsedMapFieldValueTest, DebugString) {
  auto message = DynamicParseTextProto<TestAllTypesProto3>(
      allocator(), R"pb()pb", descriptor_pool(), message_factory());
  ParsedMapFieldValue valid_value(
      message, ABSL_DIE_IF_NULL(message->GetDescriptor()->FindFieldByName(
                   "map_int64_int64")));
  EXPECT_THAT(valid_value.DebugString(), _);
}

TEST_P(ParsedMapFieldValueTest, IsZeroValue) {
  auto message = DynamicParseTextProto<TestAllTypesProto3>(
      allocator(), R"pb()pb", descriptor_pool(), message_factory());
  ParsedMapFieldValue valid_value(
      message, ABSL_DIE_IF_NULL(message->GetDescriptor()->FindFieldByName(
                   "map_int64_int64")));
  EXPECT_TRUE(valid_value.IsZeroValue());
}

TEST_P(ParsedMapFieldValueTest, SerializeTo) {
  auto message = DynamicParseTextProto<TestAllTypesProto3>(
      allocator(), R"pb()pb", descriptor_pool(), message_factory());
  ParsedMapFieldValue valid_value(
      message, ABSL_DIE_IF_NULL(message->GetDescriptor()->FindFieldByName(
                   "map_int64_int64")));
  absl::Cord serialized;
  EXPECT_THAT(valid_value.SerializeTo(value_manager(), serialized),
              StatusIs(absl::StatusCode::kUnimplemented));
}

TEST_P(ParsedMapFieldValueTest, ConvertToJson) {
  auto message = DynamicParseTextProto<TestAllTypesProto3>(
      allocator(), R"pb()pb", descriptor_pool(), message_factory());
  ParsedMapFieldValue valid_value(
      message, ABSL_DIE_IF_NULL(message->GetDescriptor()->FindFieldByName(
                   "map_int64_int64")));
  EXPECT_THAT(valid_value.ConvertToJson(value_manager()),
              StatusIs(absl::StatusCode::kUnimplemented));
}

TEST_P(ParsedMapFieldValueTest, Equal) {
  auto message = DynamicParseTextProto<TestAllTypesProto3>(
      allocator(), R"pb()pb", descriptor_pool(), message_factory());
  ParsedMapFieldValue valid_value(
      message, ABSL_DIE_IF_NULL(message->GetDescriptor()->FindFieldByName(
                   "map_int64_int64")));
  EXPECT_THAT(valid_value.Equal(value_manager(), BoolValue()),
              StatusIs(absl::StatusCode::kUnimplemented));
}

TEST_P(ParsedMapFieldValueTest, Empty) {
  auto message = DynamicParseTextProto<TestAllTypesProto3>(
      allocator(), R"pb()pb", descriptor_pool(), message_factory());
  ParsedMapFieldValue valid_value(
      message, ABSL_DIE_IF_NULL(message->GetDescriptor()->FindFieldByName(
                   "map_int64_int64")));
  EXPECT_TRUE(valid_value.IsEmpty());
}

TEST_P(ParsedMapFieldValueTest, Size) {
  auto message = DynamicParseTextProto<TestAllTypesProto3>(
      allocator(), R"pb()pb", descriptor_pool(), message_factory());
  ParsedMapFieldValue valid_value(
      message, ABSL_DIE_IF_NULL(message->GetDescriptor()->FindFieldByName(
                   "map_int64_int64")));
  EXPECT_EQ(valid_value.Size(), 0);
}

TEST_P(ParsedMapFieldValueTest, Get) {
  auto message = DynamicParseTextProto<TestAllTypesProto3>(
      allocator(), R"pb()pb", descriptor_pool(), message_factory());
  ParsedMapFieldValue valid_value(
      message, ABSL_DIE_IF_NULL(message->GetDescriptor()->FindFieldByName(
                   "map_int64_int64")));
  EXPECT_THAT(valid_value.Get(value_manager(), BoolValue()),
              StatusIs(absl::StatusCode::kUnimplemented));
}

TEST_P(ParsedMapFieldValueTest, Find) {
  auto message = DynamicParseTextProto<TestAllTypesProto3>(
      allocator(), R"pb()pb", descriptor_pool(), message_factory());
  ParsedMapFieldValue valid_value(
      message, ABSL_DIE_IF_NULL(message->GetDescriptor()->FindFieldByName(
                   "map_int64_int64")));
  EXPECT_THAT(valid_value.Find(value_manager(), BoolValue()),
              StatusIs(absl::StatusCode::kUnimplemented));
}

TEST_P(ParsedMapFieldValueTest, Has) {
  auto message = DynamicParseTextProto<TestAllTypesProto3>(
      allocator(), R"pb()pb", descriptor_pool(), message_factory());
  ParsedMapFieldValue valid_value(
      message, ABSL_DIE_IF_NULL(message->GetDescriptor()->FindFieldByName(
                   "map_int64_int64")));
  EXPECT_THAT(valid_value.Has(value_manager(), BoolValue()),
              StatusIs(absl::StatusCode::kUnimplemented));
}

TEST_P(ParsedMapFieldValueTest, ListKeys) {
  auto message = DynamicParseTextProto<TestAllTypesProto3>(
      allocator(), R"pb()pb", descriptor_pool(), message_factory());
  ParsedMapFieldValue valid_value(
      message, ABSL_DIE_IF_NULL(message->GetDescriptor()->FindFieldByName(
                   "map_int64_int64")));
  EXPECT_THAT(valid_value.ListKeys(value_manager()),
              StatusIs(absl::StatusCode::kUnimplemented));
}

TEST_P(ParsedMapFieldValueTest, ForEach) {
  auto message = DynamicParseTextProto<TestAllTypesProto3>(
      allocator(), R"pb()pb", descriptor_pool(), message_factory());
  ParsedMapFieldValue valid_value(
      message, ABSL_DIE_IF_NULL(message->GetDescriptor()->FindFieldByName(
                   "map_int64_int64")));
  EXPECT_THAT(valid_value.ForEach(value_manager(),
                                  [](const Value&, const Value&)
                                      -> absl::StatusOr<bool> { return true; }),
              StatusIs(absl::StatusCode::kUnimplemented));
}

TEST_P(ParsedMapFieldValueTest, NewIterator) {
  auto message = DynamicParseTextProto<TestAllTypesProto3>(
      allocator(), R"pb()pb", descriptor_pool(), message_factory());
  ParsedMapFieldValue valid_value(
      message, ABSL_DIE_IF_NULL(message->GetDescriptor()->FindFieldByName(
                   "map_int64_int64")));
  EXPECT_THAT(valid_value.NewIterator(value_manager()),
              StatusIs(absl::StatusCode::kUnimplemented));
}

INSTANTIATE_TEST_SUITE_P(ParsedMapFieldValueTest, ParsedMapFieldValueTest,
                         ::testing::Values(AllocatorKind::kArena,
                                           AllocatorKind::kNewDelete),
                         PrintToStringParamName());

}  // namespace
}  // namespace cel
