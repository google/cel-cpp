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

#include "base/type_provider.h"

#include <memory>

#include "base/internal/memory_manager_testing.h"
#include "base/memory.h"
#include "base/type_factory.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::Eq;
using testing::Optional;
using cel::internal::IsOkAndHolds;

class BuiltinTypeProviderTest
    : public testing::TestWithParam<base_internal::MemoryManagerTestMode> {
 protected:
  void SetUp() override {
    if (GetParam() == base_internal::MemoryManagerTestMode::kArena) {
      memory_manager_ = ArenaMemoryManager::Default();
    }
  }

  void TearDown() override {
    if (GetParam() == base_internal::MemoryManagerTestMode::kArena) {
      memory_manager_.reset();
    }
  }

  MemoryManager& memory_manager() const {
    switch (GetParam()) {
      case base_internal::MemoryManagerTestMode::kGlobal:
        return MemoryManager::Global();
      case base_internal::MemoryManagerTestMode::kArena:
        return *memory_manager_;
    }
  }

 private:
  std::unique_ptr<ArenaMemoryManager> memory_manager_;
};

TEST_P(BuiltinTypeProviderTest, ProvidesBoolWrapperType) {
  TypeFactory type_factory(memory_manager());
  ASSERT_THAT(TypeProvider::Builtin().ProvideType(type_factory,
                                                  "google.protobuf.BoolValue"),
              IsOkAndHolds(Optional(Eq(type_factory.GetBoolWrapperType()))));
}

TEST_P(BuiltinTypeProviderTest, ProvidesBytesWrapperType) {
  TypeFactory type_factory(memory_manager());
  ASSERT_THAT(TypeProvider::Builtin().ProvideType(type_factory,
                                                  "google.protobuf.BytesValue"),
              IsOkAndHolds(Optional(Eq(type_factory.GetBytesWrapperType()))));
}

TEST_P(BuiltinTypeProviderTest, ProvidesDoubleWrapperType) {
  TypeFactory type_factory(memory_manager());
  ASSERT_THAT(TypeProvider::Builtin().ProvideType(type_factory,
                                                  "google.protobuf.FloatValue"),
              IsOkAndHolds(Optional(Eq(type_factory.GetDoubleWrapperType()))));
  ASSERT_THAT(TypeProvider::Builtin().ProvideType(
                  type_factory, "google.protobuf.DoubleValue"),
              IsOkAndHolds(Optional(Eq(type_factory.GetDoubleWrapperType()))));
}

TEST_P(BuiltinTypeProviderTest, ProvidesIntWrapperType) {
  TypeFactory type_factory(memory_manager());
  ASSERT_THAT(TypeProvider::Builtin().ProvideType(type_factory,
                                                  "google.protobuf.Int32Value"),
              IsOkAndHolds(Optional(Eq(type_factory.GetIntWrapperType()))));
  ASSERT_THAT(TypeProvider::Builtin().ProvideType(type_factory,
                                                  "google.protobuf.Int64Value"),
              IsOkAndHolds(Optional(Eq(type_factory.GetIntWrapperType()))));
}

TEST_P(BuiltinTypeProviderTest, ProvidesStringWrapperType) {
  TypeFactory type_factory(memory_manager());
  ASSERT_THAT(TypeProvider::Builtin().ProvideType(
                  type_factory, "google.protobuf.StringValue"),
              IsOkAndHolds(Optional(Eq(type_factory.GetStringWrapperType()))));
}

TEST_P(BuiltinTypeProviderTest, ProvidesUintWrapperType) {
  TypeFactory type_factory(memory_manager());
  ASSERT_THAT(TypeProvider::Builtin().ProvideType(
                  type_factory, "google.protobuf.UInt32Value"),
              IsOkAndHolds(Optional(Eq(type_factory.GetUintWrapperType()))));
  ASSERT_THAT(TypeProvider::Builtin().ProvideType(
                  type_factory, "google.protobuf.UInt64Value"),
              IsOkAndHolds(Optional(Eq(type_factory.GetUintWrapperType()))));
}

TEST_P(BuiltinTypeProviderTest, ProvidesValueWrapperType) {
  TypeFactory type_factory(memory_manager());
  ASSERT_THAT(TypeProvider::Builtin().ProvideType(type_factory,
                                                  "google.protobuf.Value"),
              IsOkAndHolds(Optional(Eq(type_factory.GetDynType()))));
}

TEST_P(BuiltinTypeProviderTest, ProvidesListWrapperType) {
  TypeFactory type_factory(memory_manager());
  ASSERT_OK_AND_ASSIGN(auto list_type,
                       TypeProvider::Builtin().ProvideType(
                           type_factory, "google.protobuf.ListValue"));
  ASSERT_TRUE(list_type.has_value());
  EXPECT_TRUE((*list_type)->Is<ListType>());
  EXPECT_TRUE(list_type->As<ListType>()->element()->Is<DynType>());
}

TEST_P(BuiltinTypeProviderTest, ProvidesStructWrapperType) {
  TypeFactory type_factory(memory_manager());
  ASSERT_OK_AND_ASSIGN(auto struct_type,
                       TypeProvider::Builtin().ProvideType(
                           type_factory, "google.protobuf.Struct"));
  ASSERT_TRUE(struct_type.has_value());
  EXPECT_TRUE((*struct_type)->Is<MapType>());
  EXPECT_TRUE(struct_type->As<MapType>()->key()->Is<StringType>());
  EXPECT_TRUE(struct_type->As<MapType>()->value()->Is<DynType>());
}

TEST_P(BuiltinTypeProviderTest, DoesNotProvide) {
  TypeFactory type_factory(memory_manager());
  ASSERT_THAT(
      TypeProvider::Builtin().ProvideType(type_factory, "google.protobuf.Api"),
      IsOkAndHolds(Eq(absl::nullopt)));
}

INSTANTIATE_TEST_SUITE_P(BuiltinTypeProviderTest, BuiltinTypeProviderTest,
                         base_internal::MemoryManagerTestModeAll(),
                         base_internal::MemoryManagerTestModeName);

}  // namespace
}  // namespace cel
