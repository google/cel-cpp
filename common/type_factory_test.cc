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

#include "common/type_factory.h"

#include <ostream>
#include <sstream>
#include <string>
#include <tuple>

#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/type.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::Eq;
using testing::Ne;
using testing::TestParamInfo;
using testing::TestWithParam;
using cel::internal::IsOkAndHolds;

enum class ThreadSafety {
  kCompatible,
  kSafe,
};

std::ostream& operator<<(std::ostream& out, ThreadSafety thread_safety) {
  switch (thread_safety) {
    case ThreadSafety::kCompatible:
      return out << "THREAD_SAFE";
    case ThreadSafety::kSafe:
      return out << "THREAD_COMPATIBLE";
  }
}

class TypeFactoryTest
    : public TestWithParam<std::tuple<MemoryManagement, ThreadSafety>> {
 public:
  void SetUp() override {
    switch (memory_management()) {
      case MemoryManagement::kPooling:
        memory_manager_ =
            MemoryManager::Pooling(NewThreadCompatiblePoolingMemoryManager());
        break;
      case MemoryManagement::kReferenceCounting:
        memory_manager_ = MemoryManager::ReferenceCounting();
        break;
    }
    switch (thread_safety()) {
      case ThreadSafety::kCompatible:
        type_factory_ = NewThreadCompatibleTypeFactory(memory_manager());
        break;
      case ThreadSafety::kSafe:
        type_factory_ = NewThreadSafeTypeFactory(memory_manager());
        break;
    }
  }

  void TearDown() override { Finish(); }

  void Finish() {
    type_factory_.reset();
    memory_manager_.reset();
  }

  MemoryManagerRef memory_manager() { return *memory_manager_; }

  MemoryManagement memory_management() const { return std::get<0>(GetParam()); }

  TypeFactory& type_factory() const { return **type_factory_; }

  ThreadSafety thread_safety() const { return std::get<1>(GetParam()); }

  static std::string ToString(
      TestParamInfo<std::tuple<MemoryManagement, ThreadSafety>> param) {
    std::ostringstream out;
    out << std::get<0>(param.param) << "_" << std::get<1>(param.param);
    return out.str();
  }

 private:
  absl::optional<MemoryManager> memory_manager_;
  absl::optional<Shared<TypeFactory>> type_factory_;
};

TEST_P(TypeFactoryTest, ListType) {
  // Primitive types are cached.
  ASSERT_OK_AND_ASSIGN(auto list_type1,
                       type_factory().CreateListType(StringType()));
  EXPECT_THAT(type_factory().CreateListType(StringType()),
              IsOkAndHolds(Eq(list_type1)));
  EXPECT_THAT(type_factory().CreateListType(BytesType()),
              IsOkAndHolds(Ne(list_type1)));
  // Try types which are not cached to exercise other codepath.
  ASSERT_OK_AND_ASSIGN(auto struct_type1,
                       type_factory().CreateStructType("test.Struct1"));
  ASSERT_OK_AND_ASSIGN(auto struct_type2,
                       type_factory().CreateStructType("test.Struct2"));
  ASSERT_OK_AND_ASSIGN(auto list_type2,
                       type_factory().CreateListType(struct_type1));
  EXPECT_THAT(type_factory().CreateListType(struct_type1),
              IsOkAndHolds(Eq(list_type2)));
  EXPECT_THAT(type_factory().CreateListType(struct_type2),
              IsOkAndHolds(Ne(list_type2)));
}

TEST_P(TypeFactoryTest, MapType) {
  // Primitive types are cached.
  ASSERT_OK_AND_ASSIGN(auto map_type1,
                       type_factory().CreateMapType(StringType(), BytesType()));
  EXPECT_THAT(type_factory().CreateMapType(StringType(), BytesType()),
              IsOkAndHolds(Eq(map_type1)));
  EXPECT_THAT(type_factory().CreateMapType(StringType(), StringType()),
              IsOkAndHolds(Ne(map_type1)));
  // Try types which are not cached to exercise other codepath.
  ASSERT_OK_AND_ASSIGN(auto struct_type1,
                       type_factory().CreateStructType("test.Struct1"));
  ASSERT_OK_AND_ASSIGN(auto struct_type2,
                       type_factory().CreateStructType("test.Struct2"));
  ASSERT_OK_AND_ASSIGN(
      auto map_type2, type_factory().CreateMapType(StringType(), struct_type1));
  EXPECT_THAT(type_factory().CreateMapType(StringType(), struct_type1),
              IsOkAndHolds(Eq(map_type2)));
  EXPECT_THAT(type_factory().CreateMapType(StringType(), struct_type2),
              IsOkAndHolds(Ne(map_type2)));
}

TEST_P(TypeFactoryTest, StructType) {
  ASSERT_OK_AND_ASSIGN(auto struct_type1,
                       type_factory().CreateStructType("test.Struct1"));
  EXPECT_THAT(type_factory().CreateStructType("test.Struct1"),
              IsOkAndHolds(Eq(struct_type1)));
  EXPECT_THAT(type_factory().CreateStructType("test.Struct2"),
              IsOkAndHolds(Ne(struct_type1)));
}

TEST_P(TypeFactoryTest, OpaqueType) {
  ASSERT_OK_AND_ASSIGN(auto opaque_type1, type_factory().CreateOpaqueType(
                                              "test.Struct1", {BytesType()}));
  EXPECT_THAT(type_factory().CreateOpaqueType("test.Struct1", {BytesType()}),
              IsOkAndHolds(Eq(opaque_type1)));
  EXPECT_THAT(type_factory().CreateOpaqueType("test.Struct2", {}),
              IsOkAndHolds(Ne(opaque_type1)));
}

TEST_P(TypeFactoryTest, OptionalType) {
  // Primitive types are cached.
  ASSERT_OK_AND_ASSIGN(auto optional_type1,
                       type_factory().CreateOptionalType(StringType()));
  EXPECT_THAT(type_factory().CreateOptionalType(StringType()),
              IsOkAndHolds(Eq(optional_type1)));
  EXPECT_THAT(type_factory().CreateOptionalType(BytesType()),
              IsOkAndHolds(Ne(optional_type1)));
  // Try types which are not cached to exercise other codepath.
  ASSERT_OK_AND_ASSIGN(auto struct_type1,
                       type_factory().CreateStructType("test.Struct1"));
  ASSERT_OK_AND_ASSIGN(auto struct_type2,
                       type_factory().CreateStructType("test.Struct2"));
  ASSERT_OK_AND_ASSIGN(auto optional_type2,
                       type_factory().CreateOptionalType(struct_type1));
  EXPECT_THAT(type_factory().CreateOptionalType(struct_type1),
              IsOkAndHolds(Eq(optional_type2)));
  EXPECT_THAT(type_factory().CreateOptionalType(struct_type2),
              IsOkAndHolds(Ne(optional_type2)));
}

INSTANTIATE_TEST_SUITE_P(
    TypeFactoryTest, TypeFactoryTest,
    ::testing::Combine(::testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting),
                       ::testing::Values(ThreadSafety::kCompatible,
                                         ThreadSafety::kSafe)),
    TypeFactoryTest::ToString);

}  // namespace
}  // namespace cel
