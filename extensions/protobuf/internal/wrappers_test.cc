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

#include <limits>
#include <memory>

#include "google/protobuf/wrappers.pb.h"
#include "google/protobuf/descriptor.pb.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "extensions/protobuf/internal/wrappers_lite.h"
#include "internal/testing.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include "google/protobuf/dynamic_message.h"

namespace cel::extensions::protobuf_internal {
namespace {

using ::absl_testing::IsOkAndHolds;
using ::absl_testing::StatusIs;
using ::testing::Eq;

TEST(BoolWrapper, GeneratedFromProto) {
  EXPECT_THAT(UnwrapGeneratedBoolValueProto(google::protobuf::BoolValue()),
              IsOkAndHolds(Eq(false)));
}

TEST(BoolWrapper, CustomFromProto) {
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
  EXPECT_THAT(UnwrapDynamicBoolValueProto(*factory.GetPrototype(
                  pool.FindMessageTypeByName("google.protobuf.BoolValue"))),
              IsOkAndHolds(Eq(false)));
}

TEST(BoolWrapper, GeneratedToProto) {
  google::protobuf::BoolValue proto;
  ASSERT_OK(WrapGeneratedBoolValueProto(true, proto));
  EXPECT_TRUE(proto.value());
}

TEST(BoolWrapper, CustomToProto) {
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
  std::unique_ptr<google::protobuf::Message> proto = absl::WrapUnique(
      factory
          .GetPrototype(pool.FindMessageTypeByName("google.protobuf.BoolValue"))
          ->New());
  const auto* descriptor = proto->GetDescriptor();
  const auto* reflection = proto->GetReflection();
  const auto* value_field = descriptor->FindFieldByName("value");
  ASSERT_NE(value_field, nullptr);

  ASSERT_OK(WrapDynamicBoolValueProto(true, *proto));

  EXPECT_TRUE(reflection->GetBool(*proto, value_field));
}

TEST(BytesWrapper, GeneratedFromProto) {
  EXPECT_THAT(UnwrapGeneratedBytesValueProto(google::protobuf::BytesValue()),
              IsOkAndHolds(Eq(absl::Cord())));
}

TEST(BytesWrapper, CustomFromProto) {
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
  EXPECT_THAT(UnwrapDynamicBytesValueProto(*factory.GetPrototype(
                  pool.FindMessageTypeByName("google.protobuf.BytesValue"))),
              IsOkAndHolds(Eq(absl::Cord())));
}

TEST(BytesWrapper, GeneratedToProto) {
  google::protobuf::BytesValue proto;
  ASSERT_OK(WrapGeneratedBytesValueProto(absl::Cord("foo"), proto));
  EXPECT_EQ(proto.value(), "foo");
}

TEST(BytesWrapper, CustomToProto) {
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
  std::unique_ptr<google::protobuf::Message> proto =
      absl::WrapUnique(factory
                           .GetPrototype(pool.FindMessageTypeByName(
                               "google.protobuf.BytesValue"))
                           ->New());
  const auto* descriptor = proto->GetDescriptor();
  const auto* reflection = proto->GetReflection();
  const auto* value_field = descriptor->FindFieldByName("value");
  ASSERT_NE(value_field, nullptr);

  ASSERT_OK(WrapDynamicBytesValueProto(absl::Cord("foo"), *proto));

  EXPECT_EQ(reflection->GetString(*proto, value_field), "foo");
}

TEST(DoubleWrapper, GeneratedFromProto) {
  EXPECT_THAT(UnwrapGeneratedFloatValueProto(google::protobuf::FloatValue()),
              IsOkAndHolds(Eq(0.0f)));
  EXPECT_THAT(UnwrapGeneratedDoubleValueProto(google::protobuf::DoubleValue()),
              IsOkAndHolds(Eq(0.0)));
}

TEST(DoubleWrapper, CustomFromProto) {
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
  EXPECT_THAT(UnwrapDynamicFloatValueProto(*factory.GetPrototype(
                  pool.FindMessageTypeByName("google.protobuf.FloatValue"))),
              IsOkAndHolds(Eq(0.0f)));
  EXPECT_THAT(UnwrapDynamicDoubleValueProto(*factory.GetPrototype(
                  pool.FindMessageTypeByName("google.protobuf.DoubleValue"))),
              IsOkAndHolds(Eq(0.0)));
}

TEST(DoubleWrapper, GeneratedToProto) {
  {
    google::protobuf::FloatValue proto;
    ASSERT_OK(WrapGeneratedFloatValueProto(1.0f, proto));
    EXPECT_EQ(proto.value(), 1.0f);
  }
  {
    google::protobuf::DoubleValue proto;
    ASSERT_OK(WrapGeneratedDoubleValueProto(1.0, proto));
    EXPECT_EQ(proto.value(), 1.0);
  }
}

TEST(DoubleWrapper, CustomToProto) {
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
  {
    std::unique_ptr<google::protobuf::Message> proto =
        absl::WrapUnique(factory
                             .GetPrototype(pool.FindMessageTypeByName(
                                 "google.protobuf.FloatValue"))
                             ->New());
    const auto* descriptor = proto->GetDescriptor();
    const auto* reflection = proto->GetReflection();
    const auto* value_field = descriptor->FindFieldByName("value");
    ASSERT_NE(value_field, nullptr);

    ASSERT_OK(WrapDynamicFloatValueProto(1.0f, *proto));

    EXPECT_EQ(reflection->GetFloat(*proto, value_field), 1.0f);
  }
  {
    std::unique_ptr<google::protobuf::Message> proto =
        absl::WrapUnique(factory
                             .GetPrototype(pool.FindMessageTypeByName(
                                 "google.protobuf.DoubleValue"))
                             ->New());
    const auto* descriptor = proto->GetDescriptor();
    const auto* reflection = proto->GetReflection();
    const auto* value_field = descriptor->FindFieldByName("value");
    ASSERT_NE(value_field, nullptr);

    ASSERT_OK(WrapDynamicDoubleValueProto(1.0, *proto));

    EXPECT_EQ(reflection->GetDouble(*proto, value_field), 1.0);
  }
}

TEST(IntWrapper, GeneratedFromProto) {
  EXPECT_THAT(UnwrapGeneratedInt32ValueProto(google::protobuf::Int32Value()),
              IsOkAndHolds(Eq(0)));
  EXPECT_THAT(UnwrapGeneratedInt64ValueProto(google::protobuf::Int64Value()),
              IsOkAndHolds(Eq(0)));
}

TEST(IntWrapper, CustomFromProto) {
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
  EXPECT_THAT(UnwrapDynamicInt32ValueProto(*factory.GetPrototype(
                  pool.FindMessageTypeByName("google.protobuf.Int32Value"))),
              IsOkAndHolds(Eq(0)));
  EXPECT_THAT(UnwrapDynamicInt64ValueProto(*factory.GetPrototype(
                  pool.FindMessageTypeByName("google.protobuf.Int64Value"))),
              IsOkAndHolds(Eq(0)));
}

TEST(IntWrapper, GeneratedToProto) {
  {
    google::protobuf::Int32Value proto;
    ASSERT_OK(WrapGeneratedInt32ValueProto(1, proto));
    EXPECT_EQ(proto.value(), 1);
  }
  {
    google::protobuf::Int64Value proto;
    ASSERT_OK(WrapGeneratedInt64ValueProto(1, proto));
    EXPECT_EQ(proto.value(), 1);
  }
}

TEST(IntWrapper, CustomToProto) {
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
  {
    std::unique_ptr<google::protobuf::Message> proto =
        absl::WrapUnique(factory
                             .GetPrototype(pool.FindMessageTypeByName(
                                 "google.protobuf.Int32Value"))
                             ->New());
    const auto* descriptor = proto->GetDescriptor();
    const auto* reflection = proto->GetReflection();
    const auto* value_field = descriptor->FindFieldByName("value");
    ASSERT_NE(value_field, nullptr);

    ASSERT_OK(WrapDynamicInt32ValueProto(1, *proto));

    EXPECT_EQ(reflection->GetInt32(*proto, value_field), 1);
  }
  {
    std::unique_ptr<google::protobuf::Message> proto =
        absl::WrapUnique(factory
                             .GetPrototype(pool.FindMessageTypeByName(
                                 "google.protobuf.Int64Value"))
                             ->New());
    const auto* descriptor = proto->GetDescriptor();
    const auto* reflection = proto->GetReflection();
    const auto* value_field = descriptor->FindFieldByName("value");
    ASSERT_NE(value_field, nullptr);

    ASSERT_OK(WrapDynamicInt64ValueProto(1, *proto));

    EXPECT_EQ(reflection->GetInt64(*proto, value_field), 1);
  }
}

TEST(StringWrapper, GeneratedFromProto) {
  EXPECT_THAT(UnwrapGeneratedStringValueProto(google::protobuf::StringValue()),
              IsOkAndHolds(absl::Cord()));
}

TEST(StringWrapper, CustomFromProto) {
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
  EXPECT_THAT(UnwrapDynamicStringValueProto(*factory.GetPrototype(
                  pool.FindMessageTypeByName("google.protobuf.StringValue"))),
              IsOkAndHolds(absl::Cord()));
}

TEST(StringWrapper, GeneratedToProto) {
  google::protobuf::StringValue proto;
  ASSERT_OK(WrapGeneratedStringValueProto(absl::Cord("foo"), proto));
  EXPECT_EQ(proto.value(), "foo");
}

TEST(StringWrapper, CustomToProto) {
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
  std::unique_ptr<google::protobuf::Message> proto =
      absl::WrapUnique(factory
                           .GetPrototype(pool.FindMessageTypeByName(
                               "google.protobuf.StringValue"))
                           ->New());
  const auto* descriptor = proto->GetDescriptor();
  const auto* reflection = proto->GetReflection();
  const auto* value_field = descriptor->FindFieldByName("value");
  ASSERT_NE(value_field, nullptr);

  ASSERT_OK(WrapDynamicStringValueProto(absl::Cord("foo"), *proto));

  EXPECT_EQ(reflection->GetString(*proto, value_field), "foo");
}

TEST(UintWrapper, GeneratedFromProto) {
  EXPECT_THAT(UnwrapGeneratedUInt32ValueProto(google::protobuf::UInt32Value()),
              IsOkAndHolds(Eq(0u)));
  EXPECT_THAT(UnwrapGeneratedUInt64ValueProto(google::protobuf::UInt64Value()),
              IsOkAndHolds(Eq(0u)));
}

TEST(UintWrapper, CustomFromProto) {
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
  EXPECT_THAT(UnwrapDynamicUInt32ValueProto(*factory.GetPrototype(
                  pool.FindMessageTypeByName("google.protobuf.UInt32Value"))),
              IsOkAndHolds(Eq(0u)));
  EXPECT_THAT(UnwrapDynamicUInt64ValueProto(*factory.GetPrototype(
                  pool.FindMessageTypeByName("google.protobuf.UInt64Value"))),
              IsOkAndHolds(Eq(0u)));
}

TEST(UintWrapper, GeneratedToProto) {
  {
    google::protobuf::UInt32Value proto;
    ASSERT_OK(WrapGeneratedUInt32ValueProto(1, proto));
    EXPECT_EQ(proto.value(), 1);
  }
  {
    google::protobuf::UInt64Value proto;
    ASSERT_OK(WrapGeneratedUInt64ValueProto(1, proto));
    EXPECT_EQ(proto.value(), 1);
  }
}

TEST(UintWrapper, CustomToProto) {
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
  {
    std::unique_ptr<google::protobuf::Message> proto =
        absl::WrapUnique(factory
                             .GetPrototype(pool.FindMessageTypeByName(
                                 "google.protobuf.UInt32Value"))
                             ->New());
    const auto* descriptor = proto->GetDescriptor();
    const auto* reflection = proto->GetReflection();
    const auto* value_field = descriptor->FindFieldByName("value");
    ASSERT_NE(value_field, nullptr);

    ASSERT_OK(WrapDynamicUInt32ValueProto(1, *proto));

    EXPECT_EQ(reflection->GetUInt32(*proto, value_field), 1);
  }
  {
    std::unique_ptr<google::protobuf::Message> proto =
        absl::WrapUnique(factory
                             .GetPrototype(pool.FindMessageTypeByName(
                                 "google.protobuf.UInt64Value"))
                             ->New());
    const auto* descriptor = proto->GetDescriptor();
    const auto* reflection = proto->GetReflection();
    const auto* value_field = descriptor->FindFieldByName("value");
    ASSERT_NE(value_field, nullptr);

    ASSERT_OK(WrapDynamicUInt64ValueProto(1, *proto));

    EXPECT_EQ(reflection->GetUInt64(*proto, value_field), 1);
  }
}

}  // namespace
}  // namespace cel::extensions::protobuf_internal
