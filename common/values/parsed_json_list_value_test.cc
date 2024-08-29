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

#include "google/protobuf/struct.pb.h"
#include "absl/base/nullability.h"
#include "absl/types/optional.h"
#include "common/allocator.h"
#include "common/type.h"
#include "common/value.h"
#include "common/value_kind.h"
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

using ::cel::internal::DynamicParseTextProto;
using ::cel::internal::GeneratedParseTextProto;
using ::cel::internal::GetTestingDescriptorPool;
using ::cel::internal::GetTestingMessageFactory;
using testing::PrintToStringParamName;
using testing::TestWithParam;

using TestAllTypesProto3 = ::google::api::expr::test::v1::proto3::TestAllTypes;

class ParsedJsonListValueTest : public TestWithParam<AllocatorKind> {
 public:
  void SetUp() override {
    switch (GetParam()) {
      case AllocatorKind::kArena:
        arena_.emplace();
        break;
      case AllocatorKind::kNewDelete:
        break;
    }
  }

  void TearDown() override { arena_.reset(); }

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

 private:
  absl::optional<google::protobuf::Arena> arena_;
};

TEST_P(ParsedJsonListValueTest, Generated) {
  ParsedJsonListValue value(
      GeneratedParseTextProto<google::protobuf::ListValue>(
          allocator(), R"pb()pb", descriptor_pool(), message_factory()));
  // Nothing for now. Future change will exercise more tests.
}

TEST_P(ParsedJsonListValueTest, Dynamic) {
  ParsedJsonListValue value(DynamicParseTextProto<google::protobuf::ListValue>(
      allocator(), R"pb()pb", descriptor_pool(), message_factory()));
  // Nothing for now. Future change will exercise more tests.
}

TEST_P(ParsedJsonListValueTest, Kind) {
  ParsedJsonListValue value;
  EXPECT_EQ(value.kind(), ParsedJsonListValue::kKind);
  EXPECT_EQ(value.kind(), ValueKind::kList);
}

TEST_P(ParsedJsonListValueTest, GetTypeName) {
  ParsedJsonListValue value;
  EXPECT_EQ(value.GetTypeName(), ParsedJsonListValue::kName);
  EXPECT_EQ(value.GetTypeName(), "google.protobuf.ListValue");
}

TEST_P(ParsedJsonListValueTest, GetRuntimeType) {
  ParsedJsonListValue value;
  EXPECT_EQ(value.GetRuntimeType(), JsonListType());
}

INSTANTIATE_TEST_SUITE_P(ParsedJsonListValueTest, ParsedJsonListValueTest,
                         ::testing::Values(AllocatorKind::kArena,
                                           AllocatorKind::kNewDelete),
                         PrintToStringParamName());

}  // namespace
}  // namespace cel
