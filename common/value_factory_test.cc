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

#include "common/value_factory.h"

#include <ostream>
#include <sstream>
#include <string>
#include <tuple>

#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_factory.h"
#include "common/types/type_cache.h"
#include "common/value.h"
#include "internal/testing.h"

namespace cel {
namespace {

using common_internal::ProcessLocalTypeCache;
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

class ValueFactoryTest
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
        value_factory_ = NewThreadCompatibleValueFactory(memory_manager());
        break;
      case ThreadSafety::kSafe:
        type_factory_ = NewThreadSafeTypeFactory(memory_manager());
        value_factory_ = NewThreadSafeValueFactory(memory_manager());
        break;
    }
  }

  void TearDown() override { Finish(); }

  void Finish() {
    value_factory_.reset();
    type_factory_.reset();
    memory_manager_.reset();
  }

  MemoryManagerRef memory_manager() { return *memory_manager_; }

  MemoryManagement memory_management() const { return std::get<0>(GetParam()); }

  TypeFactory& type_factory() const { return **type_factory_; }

  ValueFactory& value_factory() const { return **value_factory_; }

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
  absl::optional<Shared<ValueFactory>> value_factory_;
};

TEST_P(ValueFactoryTest, ListValue) {
  // Primitive zero value types are cached.
  auto list_value1 = value_factory().CreateZeroListValue(
      type_factory().CreateListType(StringTypeView()));
  EXPECT_TRUE(
      Is(list_value1, value_factory().CreateZeroListValue(
                          type_factory().CreateListType(StringTypeView()))));
  EXPECT_FALSE(
      Is(list_value1, value_factory().CreateZeroListValue(
                          type_factory().CreateListType(BoolTypeView()))));
  // Try types which are not cached to exercise other codepath.
  auto struct_type1 = type_factory().CreateStructType("test.Struct1");
  auto struct_type2 = type_factory().CreateStructType("test.Struct2");
  auto list_value2 = value_factory().CreateZeroListValue(
      type_factory().CreateListType(struct_type1));
  EXPECT_TRUE(
      Is(list_value2, value_factory().CreateZeroListValue(
                          type_factory().CreateListType(struct_type1))));
  EXPECT_FALSE(
      Is(list_value2, value_factory().CreateZeroListValue(
                          type_factory().CreateListType(struct_type2))));

  auto zero_list_value = value_factory().GetZeroDynListValue();
  EXPECT_TRUE(zero_list_value.IsEmpty());
  EXPECT_EQ(zero_list_value.Size(), 0);
  EXPECT_EQ(zero_list_value.type(),
            ProcessLocalTypeCache::Get()->GetDynListType());
}

TEST_P(ValueFactoryTest, MapValue) {
  // Primitive zero value types are cached.
  auto map_value1 = value_factory().CreateZeroMapValue(
      type_factory().CreateMapType(StringTypeView(), IntTypeView()));
  EXPECT_TRUE(Is(map_value1, value_factory().CreateZeroMapValue(
                                 type_factory().CreateMapType(StringTypeView(),
                                                              IntTypeView()))));
  EXPECT_FALSE(Is(map_value1, value_factory().CreateZeroMapValue(
                                  type_factory().CreateMapType(
                                      StringTypeView(), BoolTypeView()))));
  // Try types which are not cached to exercise other codepath.
  auto struct_type1 = type_factory().CreateStructType("test.Struct1");
  auto struct_type2 = type_factory().CreateStructType("test.Struct2");
  auto map_value2 = value_factory().CreateZeroMapValue(
      type_factory().CreateMapType(StringTypeView(), struct_type1));
  EXPECT_TRUE(Is(map_value2, value_factory().CreateZeroMapValue(
                                 type_factory().CreateMapType(StringTypeView(),
                                                              struct_type1))));
  EXPECT_FALSE(Is(map_value2, value_factory().CreateZeroMapValue(
                                  type_factory().CreateMapType(StringTypeView(),
                                                               struct_type2))));

  auto zero_map_value = value_factory().GetZeroDynDynMapValue();
  EXPECT_TRUE(zero_map_value.IsEmpty());
  EXPECT_EQ(zero_map_value.Size(), 0);
  EXPECT_EQ(zero_map_value.type(),
            ProcessLocalTypeCache::Get()->GetDynDynMapType());
  zero_map_value = value_factory().GetZeroStringDynMapValue();
  EXPECT_TRUE(zero_map_value.IsEmpty());
  EXPECT_EQ(zero_map_value.Size(), 0);
  EXPECT_EQ(zero_map_value.type(),
            ProcessLocalTypeCache::Get()->GetStringDynMapType());
}

TEST_P(ValueFactoryTest, OptionalType) {
  // Primitive zero value types are cached.
  auto optional_value1 = value_factory().CreateZeroOptionalValue(
      type_factory().CreateOptionalType(StringTypeView()));
  EXPECT_TRUE(Is(optional_value1,
                 value_factory().CreateZeroOptionalValue(
                     type_factory().CreateOptionalType(StringTypeView()))));
  EXPECT_FALSE(Is(optional_value1,
                  value_factory().CreateZeroOptionalValue(
                      type_factory().CreateOptionalType(BoolTypeView()))));
  // Try types which are not cached to exercise other codepath.
  auto struct_type1 = type_factory().CreateStructType("test.Struct1");
  auto struct_type2 = type_factory().CreateStructType("test.Struct2");
  auto optional_value2 = value_factory().CreateZeroOptionalValue(
      type_factory().CreateOptionalType(struct_type1));
  EXPECT_TRUE(Is(optional_value2,
                 value_factory().CreateZeroOptionalValue(
                     type_factory().CreateOptionalType(struct_type1))));
  EXPECT_FALSE(Is(optional_value2,
                  value_factory().CreateZeroOptionalValue(
                      type_factory().CreateOptionalType(struct_type2))));

  auto zero_optional_value = value_factory().GetZeroDynOptionalValue();
  EXPECT_FALSE(zero_optional_value.HasValue());
  EXPECT_EQ(zero_optional_value.type(),
            ProcessLocalTypeCache::Get()->GetDynOptionalType());
}

INSTANTIATE_TEST_SUITE_P(
    ValueFactoryTest, ValueFactoryTest,
    ::testing::Combine(::testing::Values(MemoryManagement::kPooling,
                                         MemoryManagement::kReferenceCounting),
                       ::testing::Values(ThreadSafety::kCompatible,
                                         ThreadSafety::kSafe)),
    ValueFactoryTest::ToString);

}  // namespace
}  // namespace cel
