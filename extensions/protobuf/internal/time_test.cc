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

#include "extensions/protobuf/internal/time.h"

#include "google/protobuf/duration.pb.h"
#include "google/protobuf/timestamp.pb.h"
#include "google/protobuf/descriptor.pb.h"
#include "internal/testing.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include "google/protobuf/dynamic_message.h"

namespace cel::extensions::protobuf_internal {
namespace {

using testing::Eq;
using cel::internal::IsOkAndHolds;

TEST(Duration, Generated) {
  EXPECT_THAT(AbslDurationFromDurationProto(google::protobuf::Duration()),
              IsOkAndHolds(Eq(absl::ZeroDuration())));
}

TEST(Duration, Custom) {
  google::protobuf::SimpleDescriptorDatabase database;
  {
    google::protobuf::FileDescriptorProto fd;
    google::protobuf::Duration::descriptor()->file()->CopyTo(&fd);
    ASSERT_TRUE(database.Add(fd));
  }
  google::protobuf::DescriptorPool pool(&database);
  pool.AllowUnknownDependencies();
  google::protobuf::DynamicMessageFactory factory(&pool);
  factory.SetDelegateToGeneratedFactory(false);
  EXPECT_THAT(AbslDurationFromDurationProto(*factory.GetPrototype(
                  pool.FindMessageTypeByName("google.protobuf.Duration"))),
              IsOkAndHolds(Eq(absl::ZeroDuration())));
}

TEST(Timestamp, Generated) {
  EXPECT_THAT(AbslDurationFromDurationProto(google::protobuf::Duration()),
              IsOkAndHolds(Eq(absl::ZeroDuration())));
}

TEST(Timestamp, Custom) {
  google::protobuf::SimpleDescriptorDatabase database;
  {
    google::protobuf::FileDescriptorProto fd;
    google::protobuf::Timestamp::descriptor()->file()->CopyTo(&fd);
    ASSERT_TRUE(database.Add(fd));
  }
  google::protobuf::DescriptorPool pool(&database);
  pool.AllowUnknownDependencies();
  google::protobuf::DynamicMessageFactory factory(&pool);
  factory.SetDelegateToGeneratedFactory(false);
  EXPECT_THAT(AbslTimeFromTimestampProto(*factory.GetPrototype(
                  pool.FindMessageTypeByName("google.protobuf.Timestamp"))),
              IsOkAndHolds(Eq(absl::UnixEpoch())));
}

}  // namespace
}  // namespace cel::extensions::protobuf_internal
