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

class ParsedJsonMapValueTest : public TestWithParam<AllocatorKind> {
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

TEST_P(ParsedJsonMapValueTest, Generated) {
  ParsedJsonMapValue value(GeneratedParseTextProto<google::protobuf::Struct>(
      allocator(), R"pb()pb", descriptor_pool(), message_factory()));
  // Nothing for now. Future change will exercise more tests.
}

TEST_P(ParsedJsonMapValueTest, Dynamic) {
  ParsedJsonMapValue value(DynamicParseTextProto<google::protobuf::Struct>(
      allocator(), R"pb()pb", descriptor_pool(), message_factory()));
  // Nothing for now. Future change will exercise more tests.
}

TEST_P(ParsedJsonMapValueTest, Kind) {
  ParsedJsonMapValue value;
  EXPECT_EQ(value.kind(), ParsedJsonMapValue::kKind);
  EXPECT_EQ(value.kind(), ValueKind::kMap);
}

TEST_P(ParsedJsonMapValueTest, GetTypeName) {
  ParsedJsonMapValue value;
  EXPECT_EQ(value.GetTypeName(), ParsedJsonMapValue::kName);
  EXPECT_EQ(value.GetTypeName(), "google.protobuf.Struct");
}

TEST_P(ParsedJsonMapValueTest, GetRuntimeType) {
  ParsedJsonMapValue value;
  EXPECT_EQ(value.GetRuntimeType(), JsonMapType());
}

INSTANTIATE_TEST_SUITE_P(ParsedJsonMapValueTest, ParsedJsonMapValueTest,
                         ::testing::Values(AllocatorKind::kArena,
                                           AllocatorKind::kNewDelete),
                         PrintToStringParamName());

}  // namespace
}  // namespace cel
