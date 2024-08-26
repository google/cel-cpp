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

#include "common/arena_constant_proto.h"

#include <cstdint>
#include <memory>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "absl/base/nullability.h"
#include "absl/time/time.h"
#include "absl/types/optional.h"
#include "common/arena_bytes.h"
#include "common/arena_bytes_pool.h"
#include "common/arena_constant.h"
#include "common/arena_string.h"
#include "common/arena_string_pool.h"
#include "internal/proto_matchers.h"
#include "internal/testing.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/text_format.h"

namespace cel {
namespace {

using ::cel::internal::test::EqualsProto;
using testing::Eq;
using testing::Test;

using ConstantProto = ::google::api::expr::v1alpha1::Constant;

class ArenaConstantProtoTest : public Test {
 public:
  void SetUp() override {
    arena_.emplace();
    string_pool_ = NewArenaStringPool(arena());
    bytes_pool_ = NewArenaBytesPool(arena());
  }

  void TearDown() override {
    bytes_pool_.reset();
    string_pool_.reset();
    arena_.reset();
  }

  absl::Nonnull<google::protobuf::Arena*> arena() { return &*arena_; }

  absl::Nonnull<ArenaStringPool*> string_pool() { return string_pool_.get(); }

  absl::Nonnull<ArenaBytesPool*> bytes_pool() { return bytes_pool_.get(); }

 private:
  absl::optional<google::protobuf::Arena> arena_;
  std::unique_ptr<ArenaStringPool> string_pool_;
  std::unique_ptr<ArenaBytesPool> bytes_pool_;
};

TEST_F(ArenaConstantProtoTest, Unspecified) {
  ConstantProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb()pb", &expected_proto));
  ASSERT_OK_AND_ASSIGN(
      auto got,
      ArenaConstantFromProto(string_pool(), bytes_pool(), expected_proto));
  EXPECT_THAT(got, Eq(ArenaConstant()));
  ConstantProto got_proto;
  EXPECT_OK(ArenaConstantToProto(got, &got_proto));
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(ArenaConstantProtoTest, Null) {
  ConstantProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(null_value: NULL_VALUE)pb", &expected_proto));
  ASSERT_OK_AND_ASSIGN(
      auto got,
      ArenaConstantFromProto(string_pool(), bytes_pool(), expected_proto));
  EXPECT_THAT(got, Eq(ArenaConstant(nullptr)));
  ConstantProto got_proto;
  EXPECT_OK(ArenaConstantToProto(got, &got_proto));
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(ArenaConstantProtoTest, Bool) {
  ConstantProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(bool_value: true)pb",
                                                  &expected_proto));
  ASSERT_OK_AND_ASSIGN(
      auto got,
      ArenaConstantFromProto(string_pool(), bytes_pool(), expected_proto));
  EXPECT_THAT(got, Eq(ArenaConstant(true)));
  ConstantProto got_proto;
  EXPECT_OK(ArenaConstantToProto(got, &got_proto));
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(ArenaConstantProtoTest, Int) {
  ConstantProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(int64_value: 1)pb",
                                                  &expected_proto));
  ASSERT_OK_AND_ASSIGN(
      auto got,
      ArenaConstantFromProto(string_pool(), bytes_pool(), expected_proto));
  EXPECT_THAT(got, Eq(ArenaConstant(int64_t{1})));
  ConstantProto got_proto;
  EXPECT_OK(ArenaConstantToProto(got, &got_proto));
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(ArenaConstantProtoTest, Uint) {
  ConstantProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(uint64_value: 1)pb",
                                                  &expected_proto));
  ASSERT_OK_AND_ASSIGN(
      auto got,
      ArenaConstantFromProto(string_pool(), bytes_pool(), expected_proto));
  EXPECT_THAT(got, Eq(ArenaConstant(uint64_t{1})));
  ConstantProto got_proto;
  EXPECT_OK(ArenaConstantToProto(got, &got_proto));
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(ArenaConstantProtoTest, Double) {
  ConstantProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(double_value: 1.0)pb",
                                                  &expected_proto));
  ASSERT_OK_AND_ASSIGN(
      auto got,
      ArenaConstantFromProto(string_pool(), bytes_pool(), expected_proto));
  EXPECT_THAT(got, Eq(ArenaConstant(1.0)));
  ConstantProto got_proto;
  EXPECT_OK(ArenaConstantToProto(got, &got_proto));
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(ArenaConstantProtoTest, Bytes) {
  ConstantProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(bytes_value: "foo")pb",
                                                  &expected_proto));
  ASSERT_OK_AND_ASSIGN(
      auto got,
      ArenaConstantFromProto(string_pool(), bytes_pool(), expected_proto));
  EXPECT_THAT(got, Eq(ArenaConstant(ArenaBytes::Static("foo"))));
  ConstantProto got_proto;
  EXPECT_OK(ArenaConstantToProto(got, &got_proto));
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(ArenaConstantProtoTest, String) {
  ConstantProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(string_value: "foo")pb",
                                                  &expected_proto));
  ASSERT_OK_AND_ASSIGN(
      auto got,
      ArenaConstantFromProto(string_pool(), bytes_pool(), expected_proto));
  EXPECT_THAT(got, Eq(ArenaConstant(ArenaString::Static("foo"))));
  ConstantProto got_proto;
  EXPECT_OK(ArenaConstantToProto(got, &got_proto));
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(ArenaConstantProtoTest, Duration) {
  ConstantProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(duration_value: { seconds: 1 nanos: 1 })pb", &expected_proto));
  ASSERT_OK_AND_ASSIGN(
      auto got,
      ArenaConstantFromProto(string_pool(), bytes_pool(), expected_proto));
  EXPECT_THAT(got, Eq(ArenaConstant(absl::Seconds(1) + absl::Nanoseconds(1))));
  ConstantProto got_proto;
  EXPECT_OK(ArenaConstantToProto(got, &got_proto));
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(ArenaConstantProtoTest, Timestamp) {
  ConstantProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(timestamp_value: { seconds: 1 nanos: 1 })pb", &expected_proto));
  ASSERT_OK_AND_ASSIGN(
      auto got,
      ArenaConstantFromProto(string_pool(), bytes_pool(), expected_proto));
  EXPECT_THAT(got, Eq(ArenaConstant(absl::UnixEpoch() + absl::Seconds(1) +
                                    absl::Nanoseconds(1))));
  ConstantProto got_proto;
  EXPECT_OK(ArenaConstantToProto(got, &got_proto));
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

}  // namespace
}  // namespace cel
