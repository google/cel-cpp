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

#include "common/value.h"

#include <sstream>

#include "google/protobuf/struct.pb.h"
#include "google/protobuf/type.pb.h"
#include "google/protobuf/descriptor.pb.h"
#include "absl/status/status.h"
#include "common/native_type.h"
#include "common/value_testing.h"
#include "internal/testing.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/generated_enum_reflection.h"

namespace cel {
namespace {

using ::absl_testing::StatusIs;
using ::testing::_;
using ::testing::NotNull;

TEST(Value, KindDebugDeath) {
  Value value;
  static_cast<void>(value);
  EXPECT_DEBUG_DEATH(static_cast<void>(value.kind()), _);
}

TEST(Value, GetTypeName) {
  Value value;
  static_cast<void>(value);
  EXPECT_DEBUG_DEATH(static_cast<void>(value.GetTypeName()), _);
}

TEST(Value, DebugStringUinitializedValue) {
  Value value;
  static_cast<void>(value);
  std::ostringstream out;
  out << value;
  EXPECT_EQ(out.str(), "default ctor Value");
}

TEST(Value, NativeValueIdDebugDeath) {
  Value value;
  static_cast<void>(value);
  EXPECT_DEBUG_DEATH(static_cast<void>(NativeTypeId::Of(value)), _);
}

TEST(Value, GeneratedEnum) {
  EXPECT_EQ(Value::Enum(google::protobuf::NULL_VALUE), NullValue());
  EXPECT_EQ(Value::Enum(google::protobuf::SYNTAX_EDITIONS), IntValue(2));
}

TEST(Value, DynamicEnum) {
  EXPECT_THAT(
      Value::Enum(google::protobuf::GetEnumDescriptor<google::protobuf::NullValue>(), 0),
      test::IsNullValue());
  EXPECT_THAT(
      Value::Enum(google::protobuf::GetEnumDescriptor<google::protobuf::NullValue>()
                      ->FindValueByNumber(0)),
      test::IsNullValue());
  EXPECT_THAT(
      Value::Enum(google::protobuf::GetEnumDescriptor<google::protobuf::Syntax>(), 2),
      test::IntValueIs(2));
  EXPECT_THAT(Value::Enum(google::protobuf::GetEnumDescriptor<google::protobuf::Syntax>()
                              ->FindValueByNumber(2)),
              test::IntValueIs(2));
}

TEST(Value, DynamicClosedEnum) {
  google::protobuf::FileDescriptorProto file_descriptor;
  file_descriptor.set_name("test/closed_enum.proto");
  file_descriptor.set_package("test");
  file_descriptor.set_syntax("editions");
  file_descriptor.set_edition(google::protobuf::EDITION_2023);
  {
    auto* enum_descriptor = file_descriptor.add_enum_type();
    enum_descriptor->set_name("ClosedEnum");
    enum_descriptor->mutable_options()->mutable_features()->set_enum_type(
        google::protobuf::FeatureSet::CLOSED);
    auto* enum_value_descriptor = enum_descriptor->add_value();
    enum_value_descriptor->set_number(1);
    enum_value_descriptor->set_name("FOO");
    enum_value_descriptor = enum_descriptor->add_value();
    enum_value_descriptor->set_number(2);
    enum_value_descriptor->set_name("BAR");
  }
  google::protobuf::DescriptorPool pool;
  ASSERT_THAT(pool.BuildFile(file_descriptor), NotNull());
  const auto* enum_descriptor = pool.FindEnumTypeByName("test.ClosedEnum");
  ASSERT_THAT(enum_descriptor, NotNull());
  EXPECT_THAT(Value::Enum(enum_descriptor, 0),
              test::ErrorValueIs(StatusIs(absl::StatusCode::kInvalidArgument)));
}

}  // namespace
}  // namespace cel
