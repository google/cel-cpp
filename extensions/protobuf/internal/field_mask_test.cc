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

#include "extensions/protobuf/internal/field_mask.h"

#include <memory>

#include "google/protobuf/field_mask.pb.h"
#include "google/protobuf/descriptor.pb.h"
#include "absl/memory/memory.h"
#include "extensions/protobuf/internal/field_mask_lite.h"
#include "internal/testing.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/descriptor_database.h"
#include "google/protobuf/dynamic_message.h"

namespace cel::extensions::protobuf_internal {
namespace {

using testing::Eq;
using cel::internal::IsOkAndHolds;

TEST(FieldMask, GeneratedFromProto) {
  google::protobuf::FieldMask proto;
  proto.add_paths("foo");
  proto.add_paths("bar");
  EXPECT_THAT(GeneratedFieldMaskProtoToJsonString(proto),
              IsOkAndHolds(Eq(JsonString("foo,bar"))));
}

TEST(Any, CustomFromProto) {
  google::protobuf::SimpleDescriptorDatabase database;
  {
    google::protobuf::FileDescriptorProto fd;
    google::protobuf::FieldMask::descriptor()->file()->CopyTo(&fd);
    ASSERT_TRUE(database.Add(fd));
  }
  google::protobuf::DescriptorPool pool(&database);
  pool.AllowUnknownDependencies();
  google::protobuf::DynamicMessageFactory factory(&pool);
  factory.SetDelegateToGeneratedFactory(false);
  std::unique_ptr<google::protobuf::Message> proto = absl::WrapUnique(
      factory
          .GetPrototype(pool.FindMessageTypeByName("google.protobuf.FieldMask"))
          ->New());
  const auto* descriptor = proto->GetDescriptor();
  const auto* reflection = proto->GetReflection();
  const auto* paths_field = descriptor->FindFieldByName("paths");
  ASSERT_NE(paths_field, nullptr);

  reflection->AddString(proto.get(), paths_field, "foo");
  reflection->AddString(proto.get(), paths_field, "bar");

  EXPECT_THAT(DynamicFieldMaskProtoToJsonString(*proto),
              IsOkAndHolds(Eq(JsonString("foo,bar"))));
}

}  // namespace
}  // namespace cel::extensions::protobuf_internal
