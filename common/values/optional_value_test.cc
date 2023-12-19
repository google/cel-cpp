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

#include <sstream>
#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/type_factory.h"
#include "common/value.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::TestParamInfo;
using testing::TestWithParam;
using cel::internal::StatusIs;

class OptionalValueTest : public TestWithParam<MemoryManagement> {
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
    type_factory_ = NewThreadCompatibleTypeFactory(memory_manager());
  }

  OptionalValue OptionalNone() { return OptionalValue::None(); }

  OptionalValue OptionalOf(Value value) {
    return OptionalValue::Of(memory_manager(), std::move(value));
  }

  void TearDown() override { Finish(); }

  void Finish() {
    type_factory_.reset();
    memory_manager_.reset();
  }

  MemoryManagerRef memory_manager() { return *memory_manager_; }

  MemoryManagement memory_management() const { return GetParam(); }

  static std::string ToString(TestParamInfo<MemoryManagement> param) {
    std::ostringstream out;
    out << param.param;
    return out.str();
  }

 private:
  absl::optional<MemoryManager> memory_manager_;
  absl::optional<Shared<TypeFactory>> type_factory_;
};

TEST_P(OptionalValueTest, Kind) {
  auto value = OptionalNone();
  EXPECT_EQ(value.kind(), OptionalValue::kKind);
  EXPECT_EQ(OpaqueValue(value).kind(), OptionalValue::kKind);
  EXPECT_EQ(Value(value).kind(), OptionalValue::kKind);
}

TEST_P(OptionalValueTest, Type) {
  auto value = OptionalNone();
  EXPECT_EQ(value.type(), OptionalType());
  EXPECT_EQ(OpaqueValue(value).type(), OptionalType());
  EXPECT_EQ(Value(value).type(), OptionalType());
}

TEST_P(OptionalValueTest, DebugString) {
  auto value = OptionalNone();
  {
    std::ostringstream out;
    out << value;
    EXPECT_EQ(out.str(), "optional.none()");
  }
  {
    std::ostringstream out;
    out << OpaqueValue(value);
    EXPECT_EQ(out.str(), "optional.none()");
  }
  {
    std::ostringstream out;
    out << Value(value);
    EXPECT_EQ(out.str(), "optional.none()");
  }
  {
    std::ostringstream out;
    out << OptionalOf(IntValue());
    EXPECT_EQ(out.str(), "optional(0)");
  }
}

TEST_P(OptionalValueTest, GetSerializedSize) {
  EXPECT_THAT(OptionalValue().GetSerializedSize(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(OptionalValueTest, SerializeTo) {
  absl::Cord value;
  EXPECT_THAT(OptionalValue().SerializeTo(value),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(OptionalValueTest, Serialize) {
  EXPECT_THAT(OptionalValue().Serialize(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(OptionalValueTest, GetTypeUrl) {
  EXPECT_THAT(OptionalValue().GetTypeUrl(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(OptionalValueTest, ConvertToAny) {
  EXPECT_THAT(OptionalValue().ConvertToAny(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(OptionalValueTest, ConvertToJson) {
  EXPECT_THAT(OptionalValue().ConvertToJson(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(OptionalValueTest, HasValue) {
  auto value = OptionalNone();
  EXPECT_FALSE(value.HasValue());
  value = OptionalOf(IntValue());
  EXPECT_TRUE(value.HasValue());
}

TEST_P(OptionalValueTest, Value) {
  Value scratch;
  auto value = OptionalNone();
  auto element = value.Value(scratch);
  ASSERT_TRUE(InstanceOf<ErrorValueView>(element));
  EXPECT_THAT(Cast<ErrorValueView>(element).NativeValue(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  value = OptionalOf(IntValue());
  element = value.Value(scratch);
  ASSERT_TRUE(InstanceOf<IntValueView>(element));
  EXPECT_EQ(Cast<IntValueView>(element), IntValue());
}

INSTANTIATE_TEST_SUITE_P(
    OptionalValueTest, OptionalValueTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    OptionalValueTest::ToString);

class OptionalValueViewTest : public TestWithParam<MemoryManagement> {
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
    type_factory_ = NewThreadCompatibleTypeFactory(memory_manager());
  }

  OptionalValueView OptionalNone() { return OptionalValueView::None(); }

  OptionalValue OptionalOf(Value value) {
    return OptionalValue::Of(memory_manager(), std::move(value));
  }

  void TearDown() override { Finish(); }

  void Finish() {
    type_factory_.reset();
    memory_manager_.reset();
  }

  MemoryManagerRef memory_manager() { return *memory_manager_; }

  MemoryManagement memory_management() const { return GetParam(); }

  static std::string ToString(TestParamInfo<MemoryManagement> param) {
    std::ostringstream out;
    out << param.param;
    return out.str();
  }

 private:
  absl::optional<MemoryManager> memory_manager_;
  absl::optional<Shared<TypeFactory>> type_factory_;
};

TEST_P(OptionalValueViewTest, Kind) {
  auto value = OptionalNone();
  EXPECT_EQ(value.kind(), OptionalValueView::kKind);
  EXPECT_EQ(OpaqueValueView(value).kind(), OptionalValueView::kKind);
  EXPECT_EQ(ValueView(value).kind(), OptionalValueView::kKind);
}

TEST_P(OptionalValueViewTest, Type) {
  auto value = OptionalNone();
  EXPECT_EQ(value.type(), OptionalType());
  EXPECT_EQ(OpaqueValueView(value).type(), OptionalType());
  EXPECT_EQ(ValueView(value).type(), OptionalType());
}

TEST_P(OptionalValueViewTest, DebugString) {
  auto value = OptionalNone();
  {
    std::ostringstream out;
    out << value;
    EXPECT_EQ(out.str(), "optional.none()");
  }
  {
    std::ostringstream out;
    out << OpaqueValueView(value);
    EXPECT_EQ(out.str(), "optional.none()");
  }
  {
    std::ostringstream out;
    out << ValueView(value);
    EXPECT_EQ(out.str(), "optional.none()");
  }
  {
    std::ostringstream out;
    out << OptionalOf(IntValue());
    EXPECT_EQ(out.str(), "optional(0)");
  }
}

TEST_P(OptionalValueViewTest, GetSerializedSize) {
  EXPECT_THAT(OptionalValueView().GetSerializedSize(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(OptionalValueViewTest, SerializeTo) {
  absl::Cord value;
  EXPECT_THAT(OptionalValueView().SerializeTo(value),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(OptionalValueViewTest, Serialize) {
  EXPECT_THAT(OptionalValueView().Serialize(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(OptionalValueViewTest, GetTypeUrl) {
  EXPECT_THAT(OptionalValueView().GetTypeUrl(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(OptionalValueViewTest, ConvertToAny) {
  EXPECT_THAT(OptionalValueView().ConvertToAny(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(OptionalValueViewTest, ConvertToJson) {
  EXPECT_THAT(OptionalValueView().ConvertToJson(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
}

TEST_P(OptionalValueViewTest, HasValue) {
  auto value_view = OptionalNone();
  EXPECT_FALSE(value_view.HasValue());
  auto value = OptionalOf(IntValue());
  EXPECT_TRUE(OptionalValueView(value).HasValue());
}

TEST_P(OptionalValueViewTest, Value) {
  Value scratch;
  auto value_view = OptionalNone();
  auto element = value_view.Value(scratch);
  ASSERT_TRUE(InstanceOf<ErrorValueView>(element));
  EXPECT_THAT(Cast<ErrorValueView>(element).NativeValue(),
              StatusIs(absl::StatusCode::kFailedPrecondition));
  auto value = OptionalOf(IntValue());
  element = OptionalValueView(value).Value(scratch);
  ASSERT_TRUE(InstanceOf<IntValueView>(element));
  EXPECT_EQ(Cast<IntValueView>(element), IntValue());
}

INSTANTIATE_TEST_SUITE_P(
    OptionalValueViewTest, OptionalValueViewTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    OptionalValueViewTest::ToString);

}  // namespace
}  // namespace cel
