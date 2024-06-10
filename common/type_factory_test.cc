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
#include "common/memory_testing.h"
#include "common/type.h"
#include "common/type_introspector.h"
#include "common/type_manager.h"
#include "common/types/type_cache.h"
#include "internal/testing.h"

namespace cel {
namespace {

using common_internal::ProcessLocalTypeCache;
using testing::_;
using testing::Eq;
using testing::Ne;
using testing::TestParamInfo;
using testing::TestWithParam;

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
    : public common_internal::ThreadCompatibleMemoryTest<ThreadSafety> {
 public:
  void SetUp() override {
    ThreadCompatibleMemoryTest::SetUp();
    switch (thread_safety()) {
      case ThreadSafety::kCompatible:
        type_manager_ = NewThreadCompatibleTypeManager(
            memory_manager(),
            NewThreadCompatibleTypeIntrospector(memory_manager()));
        break;
      case ThreadSafety::kSafe:
        type_manager_ = NewThreadSafeTypeManager(
            memory_manager(), NewThreadSafeTypeIntrospector(memory_manager()));
        break;
    }
  }

  void TearDown() override { Finish(); }

  void Finish() {
    type_manager_.reset();
    ThreadCompatibleMemoryTest::Finish();
  }

  TypeFactory& type_factory() const { return **type_manager_; }

  ThreadSafety thread_safety() const { return std::get<1>(GetParam()); }

  static std::string ToString(
      TestParamInfo<std::tuple<MemoryManagement, ThreadSafety>> param) {
    std::ostringstream out;
    out << std::get<0>(param.param) << "_" << std::get<1>(param.param);
    return out.str();
  }

 private:
  absl::optional<Shared<TypeManager>> type_manager_;
};

TEST_P(TypeFactoryTest, ListType) {
  // Primitive types are cached.
  auto list_type1 = type_factory().CreateListType(StringType());
  EXPECT_THAT(type_factory().CreateListType(StringType()), Eq(list_type1));
  EXPECT_THAT(type_factory().CreateListType(BytesType()), Ne(list_type1));
  // Try types which are not cached to exercise other codepath.
  auto struct_type1 = type_factory().CreateStructType("test.Struct1");
  auto struct_type2 = type_factory().CreateStructType("test.Struct2");
  auto list_type2 = type_factory().CreateListType(struct_type1);
  EXPECT_THAT(type_factory().CreateListType(struct_type1), Eq(list_type2));
  EXPECT_THAT(type_factory().CreateListType(struct_type2), Ne(list_type2));

  EXPECT_EQ(type_factory().GetDynListType(),
            ProcessLocalTypeCache::Get()->GetDynListType());
}

TEST_P(TypeFactoryTest, MapType) {
  // Primitive types are cached.
  auto map_type1 = type_factory().CreateMapType(StringType(), BytesType());
  EXPECT_THAT(type_factory().CreateMapType(StringType(), BytesType()),
              Eq(map_type1));
  EXPECT_THAT(type_factory().CreateMapType(StringType(), StringType()),
              Ne(map_type1));
  // Try types which are not cached to exercise other codepath.
  auto struct_type1 = type_factory().CreateStructType("test.Struct1");
  auto struct_type2 = type_factory().CreateStructType("test.Struct2");

  auto map_type2 = type_factory().CreateMapType(StringType(), struct_type1);
  EXPECT_THAT(type_factory().CreateMapType(StringType(), struct_type1),
              Eq(map_type2));
  EXPECT_THAT(type_factory().CreateMapType(StringType(), struct_type2),
              Ne(map_type2));

  EXPECT_EQ(type_factory().GetDynDynMapType(),
            ProcessLocalTypeCache::Get()->GetDynDynMapType());
  EXPECT_EQ(type_factory().GetStringDynMapType(),
            ProcessLocalTypeCache::Get()->GetStringDynMapType());
}

TEST_P(TypeFactoryTest, MapTypeInvalidKeyType) {
  EXPECT_DEBUG_DEATH(type_factory().CreateMapType(DoubleType(), BytesType()),
                     _);
}

TEST_P(TypeFactoryTest, StructType) {
  auto struct_type1 = type_factory().CreateStructType("test.Struct1");
  EXPECT_THAT(type_factory().CreateStructType("test.Struct1"),
              Eq(struct_type1));
  EXPECT_THAT(type_factory().CreateStructType("test.Struct2"),
              Ne(struct_type1));
}

TEST_P(TypeFactoryTest, StructTypeBadName) {
  EXPECT_DEBUG_DEATH(type_factory().CreateStructType("test.~"), _);
}

TEST_P(TypeFactoryTest, OpaqueType) {
  auto opaque_type1 =
      type_factory().CreateOpaqueType("test.Struct1", {BytesType()});
  EXPECT_THAT(type_factory().CreateOpaqueType("test.Struct1", {BytesType()}),
              Eq(opaque_type1));
  EXPECT_THAT(type_factory().CreateOpaqueType("test.Struct2", {}),
              Ne(opaque_type1));
}

TEST_P(TypeFactoryTest, OpaqueTypeBadName) {
  EXPECT_DEBUG_DEATH(type_factory().CreateOpaqueType("test.~", {}), _);
}

TEST_P(TypeFactoryTest, OptionalType) {
  // Primitive types are cached.
  auto optional_type1 = type_factory().CreateOptionalType(StringType());
  EXPECT_THAT(type_factory().CreateOptionalType(StringType()),
              Eq(optional_type1));
  EXPECT_THAT(type_factory().CreateOptionalType(BytesType()),
              Ne(optional_type1));
  // Try types which are not cached to exercise other codepath.
  auto struct_type1 = type_factory().CreateStructType("test.Struct1");
  auto struct_type2 = type_factory().CreateStructType("test.Struct2");
  auto optional_type2 = type_factory().CreateOptionalType(struct_type1);
  EXPECT_THAT(type_factory().CreateOptionalType(struct_type1),
              Eq(optional_type2));
  EXPECT_THAT(type_factory().CreateOptionalType(struct_type2),
              Ne(optional_type2));

  EXPECT_EQ(type_factory().GetDynOptionalType(),
            ProcessLocalTypeCache::Get()->GetDynOptionalType());
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
