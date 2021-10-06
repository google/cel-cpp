#include "eval/public/containers/field_access.h"

#include <limits>

#include "google/protobuf/arena.h"
#include "google/protobuf/message.h"
#include "absl/status/status.h"
#include "absl/time/time.h"
#include "internal/proto_util.h"
#include "internal/testing.h"
#include "proto/test/v1/proto3/test_all_types.pb.h"

namespace google::api::expr::runtime {

namespace {

using google::api::expr::internal::MakeGoogleApiDurationMax;
using google::api::expr::internal::MakeGoogleApiTimeMax;
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
  auto status = SetValueToSingleField(
      CelValue::CreateDuration(MakeGoogleApiDurationMax()), field, &msg,
      &arena);
  EXPECT_TRUE(status.ok());
}

TEST(FieldAccessTest, SetDurationBadDuration) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_duration");
  auto status = SetValueToSingleField(
      CelValue::CreateDuration(MakeGoogleApiDurationMax() + absl::Seconds(1)),
      field, &msg, &arena);
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
  auto status = SetValueToSingleField(
      CelValue::CreateTimestamp(MakeGoogleApiTimeMax()), field, &msg, &arena);
  EXPECT_TRUE(status.ok());
}

TEST(FieldAccessTest, SetTimestampBadTime) {
  Arena arena;
  TestAllTypes msg;
  const FieldDescriptor* field =
      TestAllTypes::descriptor()->FindFieldByName("single_timestamp");
  auto status = SetValueToSingleField(
      CelValue::CreateTimestamp(MakeGoogleApiTimeMax() + absl::Seconds(1)),
      field, &msg, &arena);
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
