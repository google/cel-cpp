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

#include "extensions/protobuf/value.h"

#include "google/protobuf/struct.pb.h"
#include "absl/status/status.h"
#include "common/casting.h"
#include "common/value.h"
#include "common/value_testing.h"
#include "internal/testing.h"
#include "proto/test/v1/proto2/test_all_types.pb.h"

namespace cel::extensions {
namespace {

using ::google::api::expr::test::v1::proto2::TestAllTypes;
using testing::Eq;
using cel::internal::IsOkAndHolds;
using cel::internal::StatusIs;

class ProtoValueTest : public common_internal::ThreadCompatibleValueTest<> {};

TEST_P(ProtoValueTest, ProtoEnumToValue) {
  ASSERT_OK_AND_ASSIGN(
      auto enum_value,
      ProtoEnumToValue(value_factory(),
                       google::protobuf::NullValue::NULL_VALUE));
  ASSERT_TRUE(InstanceOf<NullValue>(enum_value));
  ASSERT_OK_AND_ASSIGN(enum_value,
                       ProtoEnumToValue(value_factory(), TestAllTypes::BAR));
  ASSERT_TRUE(InstanceOf<IntValue>(enum_value));
  ASSERT_THAT(Cast<IntValue>(enum_value).NativeValue(), Eq(1));
}

TEST_P(ProtoValueTest, ProtoEnumFromValue) {
  EXPECT_THAT(ProtoEnumFromValue<google::protobuf::NullValue>(NullValueView{}),
              IsOkAndHolds(Eq(google::protobuf::NULL_VALUE)));
  EXPECT_THAT(
      ProtoEnumFromValue<google::protobuf::NullValue>(IntValueView{0xdeadbeef}),
      IsOkAndHolds(Eq(google::protobuf::NULL_VALUE)));
  EXPECT_THAT(
      ProtoEnumFromValue<google::protobuf::NullValue>(StringValueView{}),
      StatusIs(absl::StatusCode::kInvalidArgument));
  EXPECT_THAT(ProtoEnumFromValue<TestAllTypes::NestedEnum>(IntValueView{1}),
              IsOkAndHolds(Eq(TestAllTypes::BAR)));
  EXPECT_THAT(ProtoEnumFromValue<TestAllTypes::NestedEnum>(IntValueView{1000}),
              StatusIs(absl::StatusCode::kNotFound));
  EXPECT_THAT(ProtoEnumFromValue<TestAllTypes::NestedEnum>(StringValueView{}),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

INSTANTIATE_TEST_SUITE_P(
    ProtoValueTest, ProtoValueTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    ProtoValueTest::ToString);

}  // namespace
}  // namespace cel::extensions
