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

#include "extensions/protobuf/internal/any.h"

#include <memory>

#include "google/protobuf/any.pb.h"
#include "google/protobuf/descriptor.pb.h"
#include "absl/memory/memory.h"
#include "absl/strings/cord.h"
#include "extensions/protobuf/internal/any_lite.h"
#include "internal/testing.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include "google/protobuf/dynamic_message.h"

namespace cel::extensions::protobuf_internal {
namespace {

TEST(Any, CustomFromProto) {
  google::protobuf::SimpleDescriptorDatabase database;
  {
    google::protobuf::FileDescriptorProto fd;
    google::protobuf::Any::descriptor()->file()->CopyTo(&fd);
    ASSERT_TRUE(database.Add(fd));
  }
  google::protobuf::DescriptorPool pool(&database);
  pool.AllowUnknownDependencies();
  google::protobuf::DynamicMessageFactory factory(&pool);
  factory.SetDelegateToGeneratedFactory(false);
  ASSERT_OK_AND_ASSIGN(auto unwrapped,
                       UnwrapDynamicAnyProto(*factory.GetPrototype(
                           pool.FindMessageTypeByName("google.protobuf.Any"))));
  EXPECT_EQ(unwrapped.type_url(), "");
  EXPECT_EQ(unwrapped.value(), "");
}

TEST(Any, GeneratedToProto) {
  google::protobuf::Any proto;
  ASSERT_OK(WrapGeneratedAnyProto("type.googleapis.com/foo.Bar",
                                  absl::Cord("blah"), proto));
  EXPECT_EQ(proto.type_url(), "type.googleapis.com/foo.Bar");
  EXPECT_EQ(proto.value(), "blah");
}

TEST(Any, CustomToProto) {
  google::protobuf::SimpleDescriptorDatabase database;
  {
    google::protobuf::FileDescriptorProto fd;
    google::protobuf::Any::descriptor()->file()->CopyTo(&fd);
    ASSERT_TRUE(database.Add(fd));
  }
  google::protobuf::DescriptorPool pool(&database);
  pool.AllowUnknownDependencies();
  google::protobuf::DynamicMessageFactory factory(&pool);
  factory.SetDelegateToGeneratedFactory(false);
  std::unique_ptr<google::protobuf::Message> proto = absl::WrapUnique(
      factory.GetPrototype(pool.FindMessageTypeByName("google.protobuf.Any"))
          ->New());
  const auto* descriptor = proto->GetDescriptor();
  const auto* reflection = proto->GetReflection();
  const auto* type_url_field = descriptor->FindFieldByName("type_url");
  ASSERT_NE(type_url_field, nullptr);
  const auto* value_field = descriptor->FindFieldByName("value");
  ASSERT_NE(value_field, nullptr);

  ASSERT_OK(WrapDynamicAnyProto("type.googleapis.com/foo.Bar",
                                absl::Cord("blah"), *proto));

  EXPECT_EQ(reflection->GetString(*proto, type_url_field),
            "type.googleapis.com/foo.Bar");
  EXPECT_EQ(reflection->GetString(*proto, value_field), "blah");
}

}  // namespace
}  // namespace cel::extensions::protobuf_internal
