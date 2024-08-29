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

#include "common/type_proto.h"

#include <memory>

#include "cel/expr/checked.pb.h"
#include "absl/base/nullability.h"
#include "absl/status/status.h"
#include "absl/status/status_matchers.h"
#include "absl/types/optional.h"
#include "common/arena_string_pool.h"
#include "common/type.h"
#include "common/type_pool.h"
#include "internal/proto_matchers.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "google/protobuf/arena.h"
#include "google/protobuf/text_format.h"

namespace cel {
namespace {

using ::absl_testing::IsOk;
using ::absl_testing::StatusIs;
using ::cel::internal::GetTestingDescriptorPool;
using ::cel::internal::test::EqualsProto;
using ::testing::Eq;
using ::testing::Test;

using TypeProto = ::cel::expr::Type;

class TypeProtoTest : public Test {
 public:
  void SetUp() override {
    arena_.emplace();
    string_pool_ = NewArenaStringPool(arena());
    type_pool_ =
        NewTypePool(arena(), string_pool(), GetTestingDescriptorPool());
  }

  void TearDown() override {
    type_pool_.reset();
    string_pool_.reset();
    arena_.reset();
  }

  absl::Nonnull<google::protobuf::Arena*> arena() { return &*arena_; }

  absl::Nonnull<ArenaStringPool*> string_pool() { return string_pool_.get(); }

  absl::Nonnull<TypePool*> type_pool() { return type_pool_.get(); }

 private:
  absl::optional<google::protobuf::Arena> arena_;
  std::unique_ptr<ArenaStringPool> string_pool_;
  std::unique_ptr<TypePool> type_pool_;
};

TEST_F(TypeProtoTest, Dyn) {
  TypeProto expected_proto;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(R"pb(dyn: {})pb", &expected_proto));
  ASSERT_OK_AND_ASSIGN(auto got, TypeFromProto(type_pool(), expected_proto));
  EXPECT_THAT(got, Eq(DynType()));
  TypeProto got_proto;
  EXPECT_THAT(TypeToProto(got, &got_proto), IsOk());
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(TypeProtoTest, Null) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(null: NULL_VALUE)pb",
                                                  &expected_proto));
  ASSERT_OK_AND_ASSIGN(auto got, TypeFromProto(type_pool(), expected_proto));
  EXPECT_THAT(got, Eq(NullType()));
  TypeProto got_proto;
  EXPECT_THAT(TypeToProto(got, &got_proto), IsOk());
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(TypeProtoTest, Bool) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(primitive: BOOL)pb",
                                                  &expected_proto));
  ASSERT_OK_AND_ASSIGN(auto got, TypeFromProto(type_pool(), expected_proto));
  EXPECT_THAT(got, Eq(BoolType()));
  TypeProto got_proto;
  EXPECT_THAT(TypeToProto(got, &got_proto), IsOk());
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(TypeProtoTest, Int) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(primitive: INT64)pb",
                                                  &expected_proto));
  ASSERT_OK_AND_ASSIGN(auto got, TypeFromProto(type_pool(), expected_proto));
  EXPECT_THAT(got, Eq(IntType()));
  TypeProto got_proto;
  EXPECT_THAT(TypeToProto(got, &got_proto), IsOk());
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(TypeProtoTest, Uint) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(primitive: UINT64)pb",
                                                  &expected_proto));
  ASSERT_OK_AND_ASSIGN(auto got, TypeFromProto(type_pool(), expected_proto));
  EXPECT_THAT(got, Eq(UintType()));
  TypeProto got_proto;
  EXPECT_THAT(TypeToProto(got, &got_proto), IsOk());
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(TypeProtoTest, Double) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(primitive: DOUBLE)pb",
                                                  &expected_proto));
  ASSERT_OK_AND_ASSIGN(auto got, TypeFromProto(type_pool(), expected_proto));
  EXPECT_THAT(got, Eq(DoubleType()));
  TypeProto got_proto;
  EXPECT_THAT(TypeToProto(got, &got_proto), IsOk());
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(TypeProtoTest, String) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(primitive: STRING)pb",
                                                  &expected_proto));
  ASSERT_OK_AND_ASSIGN(auto got, TypeFromProto(type_pool(), expected_proto));
  EXPECT_THAT(got, Eq(StringType()));
  TypeProto got_proto;
  EXPECT_THAT(TypeToProto(got, &got_proto), IsOk());
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(TypeProtoTest, Bytes) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(primitive: BYTES)pb",
                                                  &expected_proto));
  ASSERT_OK_AND_ASSIGN(auto got, TypeFromProto(type_pool(), expected_proto));
  EXPECT_THAT(got, Eq(BytesType()));
  TypeProto got_proto;
  EXPECT_THAT(TypeToProto(got, &got_proto), IsOk());
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(TypeProtoTest, BoolWrapper) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(wrapper: BOOL)pb",
                                                  &expected_proto));
  ASSERT_OK_AND_ASSIGN(auto got, TypeFromProto(type_pool(), expected_proto));
  EXPECT_THAT(got, Eq(BoolWrapperType()));
  TypeProto got_proto;
  EXPECT_THAT(TypeToProto(got, &got_proto), IsOk());
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(TypeProtoTest, IntWrapper) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(wrapper: INT64)pb",
                                                  &expected_proto));
  ASSERT_OK_AND_ASSIGN(auto got, TypeFromProto(type_pool(), expected_proto));
  EXPECT_THAT(got, Eq(IntWrapperType()));
  TypeProto got_proto;
  EXPECT_THAT(TypeToProto(got, &got_proto), IsOk());
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(TypeProtoTest, UintWrapper) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(wrapper: UINT64)pb",
                                                  &expected_proto));
  ASSERT_OK_AND_ASSIGN(auto got, TypeFromProto(type_pool(), expected_proto));
  EXPECT_THAT(got, Eq(UintWrapperType()));
  TypeProto got_proto;
  EXPECT_THAT(TypeToProto(got, &got_proto), IsOk());
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(TypeProtoTest, DoubleWrapper) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(wrapper: DOUBLE)pb",
                                                  &expected_proto));
  ASSERT_OK_AND_ASSIGN(auto got, TypeFromProto(type_pool(), expected_proto));
  EXPECT_THAT(got, Eq(DoubleWrapperType()));
  TypeProto got_proto;
  EXPECT_THAT(TypeToProto(got, &got_proto), IsOk());
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(TypeProtoTest, StringWrapper) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(wrapper: STRING)pb",
                                                  &expected_proto));
  ASSERT_OK_AND_ASSIGN(auto got, TypeFromProto(type_pool(), expected_proto));
  EXPECT_THAT(got, Eq(StringWrapperType()));
  TypeProto got_proto;
  EXPECT_THAT(TypeToProto(got, &got_proto), IsOk());
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(TypeProtoTest, BytesWrapper) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(wrapper: BYTES)pb",
                                                  &expected_proto));
  ASSERT_OK_AND_ASSIGN(auto got, TypeFromProto(type_pool(), expected_proto));
  EXPECT_THAT(got, Eq(BytesWrapperType()));
  TypeProto got_proto;
  EXPECT_THAT(TypeToProto(got, &got_proto), IsOk());
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(TypeProtoTest, Any) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(well_known: ANY)pb",
                                                  &expected_proto));
  ASSERT_OK_AND_ASSIGN(auto got, TypeFromProto(type_pool(), expected_proto));
  EXPECT_THAT(got, Eq(AnyType()));
  TypeProto got_proto;
  EXPECT_THAT(TypeToProto(got, &got_proto), IsOk());
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(TypeProtoTest, Duration) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(well_known: DURATION)pb",
                                                  &expected_proto));
  ASSERT_OK_AND_ASSIGN(auto got, TypeFromProto(type_pool(), expected_proto));
  EXPECT_THAT(got, Eq(DurationType()));
  TypeProto got_proto;
  EXPECT_THAT(TypeToProto(got, &got_proto), IsOk());
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(TypeProtoTest, Timestamp) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(well_known: TIMESTAMP)pb", &expected_proto));
  ASSERT_OK_AND_ASSIGN(auto got, TypeFromProto(type_pool(), expected_proto));
  EXPECT_THAT(got, Eq(TimestampType()));
  TypeProto got_proto;
  EXPECT_THAT(TypeToProto(got, &got_proto), IsOk());
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(TypeProtoTest, List) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(list_type: { elem_type: { primitive: BOOL } })pb", &expected_proto));
  ASSERT_OK_AND_ASSIGN(auto got, TypeFromProto(type_pool(), expected_proto));
  EXPECT_THAT(got, Eq(ListType(arena(), BoolType())));
  TypeProto got_proto;
  EXPECT_THAT(TypeToProto(got, &got_proto), IsOk());
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(TypeProtoTest, Map) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(map_type: {
             key_type: { primitive: INT64 }
             value_type: { primitive: STRING }
           })pb",
      &expected_proto));
  ASSERT_OK_AND_ASSIGN(auto got, TypeFromProto(type_pool(), expected_proto));
  EXPECT_THAT(got, Eq(MapType(arena(), IntType(), StringType())));
  TypeProto got_proto;
  EXPECT_THAT(TypeToProto(got, &got_proto), IsOk());
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(TypeProtoTest, Function) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(function: {
             result_type: { primitive: INT64 }
             arg_types { primitive: STRING }
             arg_types { primitive: INT64 }
             arg_types { primitive: UINT64 }
           })pb",
      &expected_proto));
  ASSERT_OK_AND_ASSIGN(auto got, TypeFromProto(type_pool(), expected_proto));
  EXPECT_THAT(got, Eq(FunctionType(arena(), IntType(),
                                   {StringType(), IntType(), UintType()})));
  TypeProto got_proto;
  EXPECT_THAT(TypeToProto(got, &got_proto), IsOk());
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(TypeProtoTest, Struct) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(message_type: "google.protobuf.Empty")pb", &expected_proto));
  ASSERT_OK_AND_ASSIGN(auto got, TypeFromProto(type_pool(), expected_proto));
  EXPECT_THAT(
      got, Eq(common_internal::MakeBasicStructType("google.protobuf.Empty")));
  TypeProto got_proto;
  EXPECT_THAT(TypeToProto(got, &got_proto), IsOk());
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(TypeProtoTest, BadStruct) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(message_type: "")pb",
                                                  &expected_proto));
  EXPECT_THAT(TypeFromProto(type_pool(), expected_proto),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_F(TypeProtoTest, TypeParam) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(type_param: "T")pb",
                                                  &expected_proto));
  ASSERT_OK_AND_ASSIGN(auto got, TypeFromProto(type_pool(), expected_proto));
  EXPECT_THAT(got, Eq(TypeParamType("T")));
  TypeProto got_proto;
  EXPECT_THAT(TypeToProto(got, &got_proto), IsOk());
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(TypeProtoTest, BadTypeParam) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(type_param: "")pb",
                                                  &expected_proto));
  EXPECT_THAT(TypeFromProto(type_pool(), expected_proto),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

TEST_F(TypeProtoTest, Type) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(R"pb(type: { dyn: {} })pb",
                                                  &expected_proto));
  ASSERT_OK_AND_ASSIGN(auto got, TypeFromProto(type_pool(), expected_proto));
  EXPECT_THAT(got, Eq(TypeType(arena(), DynType())));
  TypeProto got_proto;
  EXPECT_THAT(TypeToProto(got, &got_proto), IsOk());
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(TypeProtoTest, Error) {
  TypeProto expected_proto;
  ASSERT_TRUE(
      google::protobuf::TextFormat::ParseFromString(R"pb(error: {})pb", &expected_proto));
  ASSERT_OK_AND_ASSIGN(auto got, TypeFromProto(type_pool(), expected_proto));
  EXPECT_THAT(got, Eq(ErrorType()));
  TypeProto got_proto;
  EXPECT_THAT(TypeToProto(got, &got_proto), IsOk());
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(TypeProtoTest, Opaque) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(abstract_type: {
             name: "optional_type"
             parameter_types { primitive: STRING }
           })pb",
      &expected_proto));
  ASSERT_OK_AND_ASSIGN(auto got, TypeFromProto(type_pool(), expected_proto));
  EXPECT_THAT(got, Eq(OptionalType(arena(), StringType())));
  TypeProto got_proto;
  EXPECT_THAT(TypeToProto(got, &got_proto), IsOk());
  EXPECT_THAT(got_proto, EqualsProto(expected_proto));
}

TEST_F(TypeProtoTest, BadOpaque) {
  TypeProto expected_proto;
  ASSERT_TRUE(google::protobuf::TextFormat::ParseFromString(
      R"pb(abstract_type: { name: "" })pb", &expected_proto));
  EXPECT_THAT(TypeFromProto(type_pool(), expected_proto),
              StatusIs(absl::StatusCode::kInvalidArgument));
}

}  // namespace
}  // namespace cel
