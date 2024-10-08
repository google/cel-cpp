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

#include "extensions/protobuf/type.h"

#include "google/protobuf/any.pb.h"
#include "google/protobuf/duration.pb.h"
#include "google/protobuf/struct.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/wrappers.pb.h"
#include "common/type.h"
#include "common/type_testing.h"
#include "internal/testing.h"
#include "proto/test/v1/proto2/test_all_types.pb.h"
#include "google/protobuf/generated_enum_reflection.h"

namespace cel::extensions {
namespace {

using ::absl_testing::IsOkAndHolds;
using ::google::api::expr::test::v1::proto2::TestAllTypes;
using ::testing::Eq;

class ProtoTypeTest : public common_internal::ThreadCompatibleTypeTest<> {};

TEST_P(ProtoTypeTest, ProtoTypeToType) {
  EXPECT_THAT(ProtoTypeToType(type_factory(),
                              google::protobuf::FloatValue::GetDescriptor()),
              IsOkAndHolds(Eq(DoubleWrapperType{})));
  EXPECT_THAT(ProtoTypeToType(type_factory(),
                              google::protobuf::DoubleValue::GetDescriptor()),
              IsOkAndHolds(Eq(DoubleWrapperType{})));
  EXPECT_THAT(ProtoTypeToType(type_factory(),
                              google::protobuf::Int32Value::GetDescriptor()),
              IsOkAndHolds(Eq(IntWrapperType{})));
  EXPECT_THAT(ProtoTypeToType(type_factory(),
                              google::protobuf::Int64Value::GetDescriptor()),
              IsOkAndHolds(Eq(IntWrapperType{})));
  EXPECT_THAT(ProtoTypeToType(type_factory(),
                              google::protobuf::UInt32Value::GetDescriptor()),
              IsOkAndHolds(Eq(UintWrapperType{})));
  EXPECT_THAT(ProtoTypeToType(type_factory(),
                              google::protobuf::UInt64Value::GetDescriptor()),
              IsOkAndHolds(Eq(UintWrapperType{})));
  EXPECT_THAT(ProtoTypeToType(type_factory(),
                              google::protobuf::StringValue::GetDescriptor()),
              IsOkAndHolds(Eq(StringWrapperType{})));
  EXPECT_THAT(ProtoTypeToType(type_factory(),
                              google::protobuf::BytesValue::GetDescriptor()),
              IsOkAndHolds(Eq(BytesWrapperType{})));
  EXPECT_THAT(ProtoTypeToType(type_factory(),
                              google::protobuf::BoolValue::GetDescriptor()),
              IsOkAndHolds(Eq(BoolWrapperType{})));
  EXPECT_THAT(ProtoTypeToType(type_factory(),
                              google::protobuf::Duration::GetDescriptor()),
              IsOkAndHolds(Eq(DurationType{})));
  EXPECT_THAT(ProtoTypeToType(type_factory(),
                              google::protobuf::Timestamp::GetDescriptor()),
              IsOkAndHolds(Eq(TimestampType{})));
  EXPECT_THAT(
      ProtoTypeToType(type_factory(), google::protobuf::Any::GetDescriptor()),
      IsOkAndHolds(Eq(AnyType{})));
  EXPECT_THAT(
      ProtoTypeToType(type_factory(), google::protobuf::Value::GetDescriptor()),
      IsOkAndHolds(Eq(DynType{})));
  EXPECT_THAT(ProtoTypeToType(type_factory(),
                              google::protobuf::ListValue::GetDescriptor()),
              IsOkAndHolds(Eq(ListType{})));
  EXPECT_THAT(ProtoTypeToType(type_factory(),
                              google::protobuf::Struct::GetDescriptor()),
              IsOkAndHolds(Eq(MapType(type_factory().GetStringDynMapType()))));
  EXPECT_THAT(ProtoTypeToType(type_factory(),
                              google::protobuf::Struct::GetDescriptor()),
              IsOkAndHolds(Eq(MapType(type_factory().GetStringDynMapType()))));
  EXPECT_THAT(ProtoTypeToType(type_factory(), TestAllTypes::GetDescriptor()),
              IsOkAndHolds(Eq(MessageType(TestAllTypes::GetDescriptor()))));
}

TEST_P(ProtoTypeTest, ProtoEnumTypeToType) {
  EXPECT_THAT(ProtoEnumTypeToType(
                  type_factory(),
                  google::protobuf::GetEnumDescriptor<google::protobuf::NullValue>()),
              IsOkAndHolds(Eq(NullType{})));
  EXPECT_THAT(ProtoEnumTypeToType(
                  type_factory(),
                  google::protobuf::GetEnumDescriptor<TestAllTypes::NestedEnum>()),
              IsOkAndHolds(Eq(IntType{})));
}

INSTANTIATE_TEST_SUITE_P(
    ProtoTypeTest, ProtoTypeTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    ProtoTypeTest::ToString);

}  // namespace
}  // namespace cel::extensions
