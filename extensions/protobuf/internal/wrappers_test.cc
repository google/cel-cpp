// Copyright 2023 Google LLC
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

#include "extensions/protobuf/internal/wrappers.h"

#include "google/protobuf/wrappers.pb.h"
#include "google/protobuf/descriptor.pb.h"
#include "internal/testing.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include "google/protobuf/dynamic_message.h"

namespace cel::extensions::protobuf_internal {
namespace {

using testing::Eq;
using testing::VariantWith;
using cel::internal::IsOkAndHolds;

TEST(BoolWrapper, Generated) {
  EXPECT_THAT(UnwrapBoolValueProto(google::protobuf::BoolValue()),
              IsOkAndHolds(Eq(false)));
}

TEST(BoolWrapper, Custom) {
  google::protobuf::SimpleDescriptorDatabase database;
  {
    google::protobuf::FileDescriptorProto fd;
    google::protobuf::BoolValue::descriptor()->file()->CopyTo(&fd);
    ASSERT_TRUE(database.Add(fd));
  }
  google::protobuf::DescriptorPool pool(&database);
  pool.AllowUnknownDependencies();
  google::protobuf::DynamicMessageFactory factory(&pool);
  factory.SetDelegateToGeneratedFactory(false);
  EXPECT_THAT(UnwrapBoolValueProto(*factory.GetPrototype(
                  pool.FindMessageTypeByName("google.protobuf.BoolValue"))),
              IsOkAndHolds(Eq(false)));
}

TEST(BytesWrapper, Generated) {
  EXPECT_THAT(UnwrapBytesValueProto(google::protobuf::BytesValue()),
              IsOkAndHolds(Eq(absl::Cord())));
}

TEST(BytesWrapper, Custom) {
  google::protobuf::SimpleDescriptorDatabase database;
  {
    google::protobuf::FileDescriptorProto fd;
    google::protobuf::BytesValue::descriptor()->file()->CopyTo(&fd);
    ASSERT_TRUE(database.Add(fd));
  }
  google::protobuf::DescriptorPool pool(&database);
  pool.AllowUnknownDependencies();
  google::protobuf::DynamicMessageFactory factory(&pool);
  factory.SetDelegateToGeneratedFactory(false);
  EXPECT_THAT(UnwrapBytesValueProto(*factory.GetPrototype(
                  pool.FindMessageTypeByName("google.protobuf.BytesValue"))),
              IsOkAndHolds(Eq(absl::Cord())));
}

TEST(DoubleWrapper, Generated) {
  EXPECT_THAT(UnwrapDoubleValueProto(google::protobuf::FloatValue()),
              IsOkAndHolds(Eq(0.0f)));
  EXPECT_THAT(UnwrapDoubleValueProto(google::protobuf::DoubleValue()),
              IsOkAndHolds(Eq(0.0)));
}

TEST(DoubleWrapper, Custom) {
  google::protobuf::SimpleDescriptorDatabase database;
  {
    google::protobuf::FileDescriptorProto fd;
    google::protobuf::DoubleValue::descriptor()->file()->CopyTo(&fd);
    ASSERT_TRUE(database.Add(fd));
  }
  google::protobuf::DescriptorPool pool(&database);
  pool.AllowUnknownDependencies();
  google::protobuf::DynamicMessageFactory factory(&pool);
  factory.SetDelegateToGeneratedFactory(false);
  EXPECT_THAT(UnwrapDoubleValueProto(*factory.GetPrototype(
                  pool.FindMessageTypeByName("google.protobuf.FloatValue"))),
              IsOkAndHolds(Eq(0.0f)));
  EXPECT_THAT(UnwrapDoubleValueProto(*factory.GetPrototype(
                  pool.FindMessageTypeByName("google.protobuf.DoubleValue"))),
              IsOkAndHolds(Eq(0.0)));
}

TEST(IntWrapper, Generated) {
  EXPECT_THAT(UnwrapIntValueProto(google::protobuf::Int32Value()),
              IsOkAndHolds(Eq(0)));
  EXPECT_THAT(UnwrapIntValueProto(google::protobuf::Int64Value()),
              IsOkAndHolds(Eq(0)));
}

TEST(IntWrapper, Custom) {
  google::protobuf::SimpleDescriptorDatabase database;
  {
    google::protobuf::FileDescriptorProto fd;
    google::protobuf::Int64Value::descriptor()->file()->CopyTo(&fd);
    ASSERT_TRUE(database.Add(fd));
  }
  google::protobuf::DescriptorPool pool(&database);
  pool.AllowUnknownDependencies();
  google::protobuf::DynamicMessageFactory factory(&pool);
  factory.SetDelegateToGeneratedFactory(false);
  EXPECT_THAT(UnwrapIntValueProto(*factory.GetPrototype(
                  pool.FindMessageTypeByName("google.protobuf.Int32Value"))),
              IsOkAndHolds(Eq(0)));
  EXPECT_THAT(UnwrapIntValueProto(*factory.GetPrototype(
                  pool.FindMessageTypeByName("google.protobuf.Int64Value"))),
              IsOkAndHolds(Eq(0)));
}

TEST(StringWrapper, Generated) {
  EXPECT_THAT(UnwrapStringValueProto(google::protobuf::StringValue()),
              IsOkAndHolds(VariantWith<absl::string_view>("")));
}

TEST(StringWrapper, Custom) {
  google::protobuf::SimpleDescriptorDatabase database;
  {
    google::protobuf::FileDescriptorProto fd;
    google::protobuf::StringValue::descriptor()->file()->CopyTo(&fd);
    ASSERT_TRUE(database.Add(fd));
  }
  google::protobuf::DescriptorPool pool(&database);
  pool.AllowUnknownDependencies();
  google::protobuf::DynamicMessageFactory factory(&pool);
  factory.SetDelegateToGeneratedFactory(false);
  EXPECT_THAT(UnwrapStringValueProto(*factory.GetPrototype(
                  pool.FindMessageTypeByName("google.protobuf.StringValue"))),
              IsOkAndHolds(VariantWith<absl::string_view>("")));
}

TEST(UintWrapper, Generated) {
  EXPECT_THAT(UnwrapUIntValueProto(google::protobuf::UInt32Value()),
              IsOkAndHolds(Eq(0u)));
  EXPECT_THAT(UnwrapUIntValueProto(google::protobuf::UInt64Value()),
              IsOkAndHolds(Eq(0u)));
}

TEST(UintWrapper, Custom) {
  google::protobuf::SimpleDescriptorDatabase database;
  {
    google::protobuf::FileDescriptorProto fd;
    google::protobuf::UInt64Value::descriptor()->file()->CopyTo(&fd);
    ASSERT_TRUE(database.Add(fd));
  }
  google::protobuf::DescriptorPool pool(&database);
  pool.AllowUnknownDependencies();
  google::protobuf::DynamicMessageFactory factory(&pool);
  factory.SetDelegateToGeneratedFactory(false);
  EXPECT_THAT(UnwrapUIntValueProto(*factory.GetPrototype(
                  pool.FindMessageTypeByName("google.protobuf.UInt32Value"))),
              IsOkAndHolds(Eq(0u)));
  EXPECT_THAT(UnwrapUIntValueProto(*factory.GetPrototype(
                  pool.FindMessageTypeByName("google.protobuf.UInt64Value"))),
              IsOkAndHolds(Eq(0u)));
}

}  // namespace
}  // namespace cel::extensions::protobuf_internal
