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

#include "common/type_pool.h"

#include <memory>

#include "absl/base/nullability.h"
#include "absl/types/optional.h"
#include "common/arena_string_pool.h"
#include "common/type.h"
#include "internal/testing.h"
#include "internal/testing_descriptor_pool.h"
#include "google/protobuf/arena.h"

namespace cel {
namespace {

using ::cel::internal::GetTestingDescriptorPool;
using ::testing::_;
using ::testing::Test;

class TypePoolTest : public Test {
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

TEST_F(TypePoolTest, MakeStructType) {
  EXPECT_EQ(type_pool()->MakeStructType("foo.Bar"),
            common_internal::MakeBasicStructType("foo.Bar"));
  EXPECT_TRUE(
      type_pool()
          ->MakeStructType("google.api.expr.test.v1.proto3.TestAllTypes")
          .IsMessage());
  EXPECT_DEBUG_DEATH(static_cast<void>(type_pool()->MakeStructType(
                         "google.protobuf.BoolValue")),
                     _);
}

TEST_F(TypePoolTest, MakeFunctionType) {
  EXPECT_EQ(type_pool()->MakeFunctionType(BoolType(), {IntType(), IntType()}),
            FunctionType(arena(), BoolType(), {IntType(), IntType()}));
}

TEST_F(TypePoolTest, MakeListType) {
  EXPECT_EQ(type_pool()->MakeListType(DynType()), ListType());
  EXPECT_EQ(type_pool()->MakeListType(DynType()), JsonListType());
  EXPECT_EQ(type_pool()->MakeListType(StringType()),
            ListType(arena(), StringType()));
}

TEST_F(TypePoolTest, MakeMapType) {
  EXPECT_EQ(type_pool()->MakeMapType(DynType(), DynType()), MapType());
  EXPECT_EQ(type_pool()->MakeMapType(StringType(), DynType()), JsonMapType());
  EXPECT_EQ(type_pool()->MakeMapType(StringType(), StringType()),
            MapType(arena(), StringType(), StringType()));
}

TEST_F(TypePoolTest, MakeOpaqueType) {
  EXPECT_EQ(type_pool()->MakeOpaqueType("custom_type", {DynType(), DynType()}),
            OpaqueType(arena(), "custom_type", {DynType(), DynType()}));
}

TEST_F(TypePoolTest, MakeOptionalType) {
  EXPECT_EQ(type_pool()->MakeOptionalType(DynType()), OptionalType());
  EXPECT_EQ(type_pool()->MakeOptionalType(StringType()),
            OptionalType(arena(), StringType()));
}

TEST_F(TypePoolTest, MakeTypeParamType) {
  EXPECT_EQ(type_pool()->MakeTypeParamType("T"), TypeParamType("T"));
}

TEST_F(TypePoolTest, MakeTypeType) {
  EXPECT_EQ(type_pool()->MakeTypeType(BoolType()),
            TypeType(arena(), BoolType()));
}

}  // namespace
}  // namespace cel
