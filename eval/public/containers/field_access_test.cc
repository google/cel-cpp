// Copyright 2021 Google LLC
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

#include "eval/public/containers/field_access.h"

#include <limits>

#include "google/protobuf/arena.h"
#include "google/protobuf/message.h"
#include "absl/status/status.h"
#include "absl/time/time.h"
#include "internal/testing.h"
#include "internal/time.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"

namespace google::api::expr::runtime {

namespace {

using ::cel::internal::MaxDuration;
using ::cel::internal::MaxTimestamp;
using google::protobuf::Arena;
using google::protobuf::FieldDescriptor;
using test::v1::proto3::TestAllTypes;
using testing::HasSubstr;
using cel::internal::StatusIs;

TEST(FieldAccessTest, SetDuration) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_duration");
  auto status = SetValueToSingleField(CelValue::CreateDuration(MaxDuration()),
                                      field, &msg, &arena);
  EXPECT_TRUE(status.ok());
}

TEST(FieldAccessTest, SetDurationBadDuration) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_duration");
  auto status = SetValueToSingleField(
      CelValue::CreateDuration(MaxDuration() + absl::Seconds(1)), field, &msg,
      &arena);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(FieldAccessTest, SetDurationBadInputType) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_duration");
  auto status =
      SetValueToSingleField(CelValue::CreateInt64(1), field, &msg, &arena);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(FieldAccessTest, SetTimestamp) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_timestamp");
  auto status = SetValueToSingleField(CelValue::CreateTimestamp(MaxTimestamp()),
                                      field, &msg, &arena);
  EXPECT_TRUE(status.ok());
}

TEST(FieldAccessTest, SetTimestampBadTime) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_timestamp");
  auto status = SetValueToSingleField(
      CelValue::CreateTimestamp(MaxTimestamp() + absl::Seconds(1)), field, &msg,
      &arena);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(FieldAccessTest, SetTimestampBadInputType) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_timestamp");
  auto status =
      SetValueToSingleField(CelValue::CreateInt64(1), field, &msg, &arena);
  EXPECT_FALSE(status.ok());
  EXPECT_EQ(status.code(), absl::StatusCode::kInvalidArgument);
}

TEST(FieldAccessTest, SetInt32Overflow) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_int32");
  EXPECT_THAT(
      SetValueToSingleField(
          CelValue::CreateInt64(std::numeric_limits<int32_t>::max() + 1L),
          field, &msg, &arena),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Could not assign")));
}

TEST(FieldAccessTest, SetUint32Overflow) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_uint32");
  EXPECT_THAT(
      SetValueToSingleField(
          CelValue::CreateUint64(std::numeric_limits<uint32_t>::max() + 1L),
          field, &msg, &arena),
      StatusIs(absl::StatusCode::kInvalidArgument,
               HasSubstr("Could not assign")));
}

}  // namespace

}  // namespace google::api::expr::runtime
