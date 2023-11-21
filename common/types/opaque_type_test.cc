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

#include "absl/container/inlined_vector.h"
#include "absl/hash/hash.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/memory.h"
#include "common/native_type.h"
#include "common/sized_input_view.h"
#include "common/type.h"
#include "internal/testing.h"

namespace cel {
namespace {

using testing::An;
using testing::Ne;
using testing::TestParamInfo;
using testing::TestWithParam;

class CustomTypeInterface;

using OpaqueTypeParameterVector = absl::InlinedVector<Type, 1>;

class CustomTypeInterface final
    : public ExtendOpaqueTypeInterface<CustomTypeInterface> {
 public:
  explicit CustomTypeInterface(OpaqueTypeParameterVector parameters)
      : parameters_(std::move(parameters)) {}

  absl::string_view name() const override { return "custom_type"; }

  SizedInputView<TypeView> parameters() const override { return parameters_; }

 private:
  bool Equals(const OpaqueTypeInterface&) const override { return true; }

  void HashValue(absl::HashState) const override {}

  const OpaqueTypeParameterVector parameters_;
};

using CustomType = OpaqueTypeFor<CustomTypeInterface>;

using CustomTypeView = OpaqueTypeViewFor<CustomTypeInterface>;

class EmptyOpaqueTypeInterface final
    : public ExtendOpaqueTypeInterface<EmptyOpaqueTypeInterface> {
 public:
  using ExtendOpaqueTypeInterface::ExtendOpaqueTypeInterface;

  absl::string_view name() const override { return "empty"; }

 private:
  bool Equals(const OpaqueTypeInterface&) const override { return true; }

  void HashValue(absl::HashState) const override {}
};

using EmptyOpaqueType = OpaqueTypeFor<EmptyOpaqueTypeInterface>;

using EmptyOpaqueTypeView = OpaqueTypeViewFor<EmptyOpaqueTypeInterface>;

class OpaqueTypeTest : public TestWithParam<MemoryManagement> {
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
  }

  void TearDown() override { Finish(); }

  void Finish() { memory_manager_.reset(); }

  MemoryManagerRef memory_manager() { return *memory_manager_; }

  MemoryManagement memory_management() const { return GetParam(); }

  CustomType MakeCustomType() {
    return memory_manager().MakeShared<CustomTypeInterface>(
        OpaqueTypeParameterVector{StringType()});
  }

  static std::string ToString(TestParamInfo<MemoryManagement> param) {
    std::ostringstream out;
    out << param.param;
    return out.str();
  }

 private:
  absl::optional<MemoryManager> memory_manager_;
};

TEST_P(OpaqueTypeTest, Kind) {
  EXPECT_EQ(MakeCustomType().kind(), OpaqueType::kKind);
  EXPECT_EQ(OpaqueType(MakeCustomType()).kind(), OpaqueType::kKind);
  EXPECT_EQ(Type(OpaqueType(MakeCustomType())).kind(), OpaqueType::kKind);
}

TEST_P(OpaqueTypeTest, Name) {
  EXPECT_EQ(MakeCustomType().name(), "custom_type");
  EXPECT_EQ(OpaqueType(MakeCustomType()).name(), "custom_type");
  EXPECT_EQ(Type(OpaqueType(MakeCustomType())).name(), "custom_type");
}

TEST_P(OpaqueTypeTest, DebugString) {
  {
    std::ostringstream out;
    out << MakeCustomType();
    EXPECT_EQ(out.str(), "custom_type<string>");
  }
  {
    std::ostringstream out;
    out << OpaqueType(MakeCustomType());
    EXPECT_EQ(out.str(), "custom_type<string>");
  }
  {
    std::ostringstream out;
    out << Type(OpaqueType(MakeCustomType()));
    EXPECT_EQ(out.str(), "custom_type<string>");
  }
  {
    std::ostringstream out;
    out << EmptyOpaqueType(
        memory_manager().MakeShared<EmptyOpaqueTypeInterface>());
    EXPECT_EQ(out.str(), "empty");
  }
}

TEST_P(OpaqueTypeTest, Hash) {
  const auto expected_hash = absl::HashOf(MakeCustomType());
  EXPECT_EQ(absl::HashOf(MakeCustomType()), expected_hash);
  EXPECT_EQ(absl::HashOf(OpaqueType(MakeCustomType())), expected_hash);
  EXPECT_EQ(absl::HashOf(Type(OpaqueType(MakeCustomType()))), expected_hash);
}

TEST_P(OpaqueTypeTest, Equal) {
  EXPECT_EQ(MakeCustomType(), MakeCustomType());
  EXPECT_EQ(OpaqueType(MakeCustomType()), MakeCustomType());
  EXPECT_EQ(MakeCustomType(), OpaqueType(MakeCustomType()));
  EXPECT_EQ(OpaqueType(MakeCustomType()), OpaqueType(MakeCustomType()));
  EXPECT_EQ(Type(OpaqueType(MakeCustomType())), OpaqueType(MakeCustomType()));
  EXPECT_EQ(OpaqueType(MakeCustomType()), Type(OpaqueType(MakeCustomType())));
  EXPECT_EQ(Type(OpaqueType(MakeCustomType())),
            Type(OpaqueType(MakeCustomType())));
}

TEST_P(OpaqueTypeTest, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(MakeCustomType()),
            NativeTypeId::For<CustomTypeInterface>());
  EXPECT_EQ(NativeTypeId::Of(OpaqueType(MakeCustomType())),
            NativeTypeId::For<CustomTypeInterface>());
  EXPECT_EQ(NativeTypeId::Of(Type(OpaqueType(MakeCustomType()))),
            NativeTypeId::For<CustomTypeInterface>());
}

TEST_P(OpaqueTypeTest, InstanceOf) {
  EXPECT_TRUE(InstanceOf<OpaqueType>(MakeCustomType()));
  EXPECT_TRUE(InstanceOf<OpaqueType>(OpaqueType(MakeCustomType())));
  EXPECT_TRUE(InstanceOf<OpaqueType>(Type(OpaqueType(MakeCustomType()))));
  EXPECT_TRUE(InstanceOf<CustomType>(MakeCustomType()));
  EXPECT_TRUE(InstanceOf<CustomType>(OpaqueType(MakeCustomType())));
  EXPECT_TRUE(InstanceOf<CustomType>(Type(OpaqueType(MakeCustomType()))));
}

TEST_P(OpaqueTypeTest, Cast) {
  EXPECT_THAT(Cast<CustomType>(MakeCustomType()), An<CustomType>());
  EXPECT_THAT(Cast<CustomType>(OpaqueType(MakeCustomType())), An<CustomType>());
  EXPECT_THAT(Cast<CustomType>(Type(OpaqueType(MakeCustomType()))),
              An<CustomType>());
}

TEST_P(OpaqueTypeTest, As) {
  EXPECT_THAT(As<CustomType>(MakeCustomType()), Ne(absl::nullopt));
  EXPECT_THAT(As<CustomType>(OpaqueType(MakeCustomType())), Ne(absl::nullopt));
  EXPECT_THAT(As<CustomType>(Type(OpaqueType(MakeCustomType()))),
              Ne(absl::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    OpaqueTypeTest, OpaqueTypeTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    OpaqueTypeTest::ToString);

class OpaqueTypeViewTest : public TestWithParam<MemoryManagement> {
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
  }

  void TearDown() override { Finish(); }

  void Finish() { memory_manager_.reset(); }

  MemoryManagerRef memory_manager() { return *memory_manager_; }

  MemoryManagement memory_management() const { return GetParam(); }

  CustomType MakeCustomType() {
    return memory_manager().MakeShared<CustomTypeInterface>(
        OpaqueTypeParameterVector{StringType()});
  }

  static std::string ToString(TestParamInfo<MemoryManagement> param) {
    std::ostringstream out;
    out << param.param;
    return out.str();
  }

 private:
  absl::optional<MemoryManager> memory_manager_;
};

TEST_P(OpaqueTypeViewTest, Kind) {
  auto type = MakeCustomType();
  EXPECT_EQ(CustomTypeView(type).kind(), OpaqueTypeView::kKind);
  EXPECT_EQ(OpaqueTypeView(CustomTypeView(type)).kind(), OpaqueTypeView::kKind);
  EXPECT_EQ(TypeView(OpaqueTypeView(CustomTypeView(type))).kind(),
            OpaqueTypeView::kKind);
}

TEST_P(OpaqueTypeViewTest, Name) {
  auto type = MakeCustomType();
  EXPECT_EQ(CustomTypeView(type).name(), "custom_type");
  EXPECT_EQ(OpaqueTypeView(CustomTypeView(type)).name(), "custom_type");
  EXPECT_EQ(TypeView(OpaqueTypeView(CustomTypeView(type))).name(),
            "custom_type");
}

TEST_P(OpaqueTypeViewTest, DebugString) {
  auto type = MakeCustomType();
  {
    std::ostringstream out;
    out << CustomTypeView(type);
    EXPECT_EQ(out.str(), "custom_type<string>");
  }
  {
    std::ostringstream out;
    out << OpaqueTypeView(CustomTypeView(type));
    EXPECT_EQ(out.str(), "custom_type<string>");
  }
  {
    std::ostringstream out;
    out << TypeView(OpaqueTypeView(CustomTypeView(type)));
    EXPECT_EQ(out.str(), "custom_type<string>");
  }
}

TEST_P(OpaqueTypeViewTest, Hash) {
  auto type = MakeCustomType();
  const auto expected_hash = absl::HashOf(CustomTypeView(type));
  EXPECT_EQ(absl::HashOf(CustomTypeView(type)), expected_hash);
  EXPECT_EQ(absl::HashOf(OpaqueTypeView(CustomTypeView(type))), expected_hash);
  EXPECT_EQ(absl::HashOf(TypeView(OpaqueTypeView(CustomTypeView(type)))),
            expected_hash);
}

TEST_P(OpaqueTypeViewTest, Equal) {
  auto type = MakeCustomType();
  EXPECT_EQ(CustomTypeView(type), CustomTypeView(type));
  EXPECT_EQ(OpaqueTypeView(CustomTypeView(type)), CustomTypeView(type));
  EXPECT_EQ(CustomTypeView(type), OpaqueTypeView(CustomTypeView(type)));
  EXPECT_EQ(OpaqueTypeView(CustomTypeView(type)),
            OpaqueTypeView(CustomTypeView(type)));
  EXPECT_EQ(TypeView(OpaqueTypeView(CustomTypeView(type))),
            OpaqueTypeView(CustomTypeView(type)));
  EXPECT_EQ(OpaqueTypeView(CustomTypeView(type)),
            TypeView(OpaqueTypeView(CustomTypeView(type))));
  EXPECT_EQ(TypeView(OpaqueTypeView(CustomTypeView(type))),
            TypeView(OpaqueTypeView(CustomTypeView(type))));
}

TEST_P(OpaqueTypeViewTest, NativeTypeId) {
  auto type = MakeCustomType();
  EXPECT_EQ(NativeTypeId::Of(CustomTypeView(type)),
            NativeTypeId::For<CustomTypeInterface>());
  EXPECT_EQ(NativeTypeId::Of(OpaqueTypeView(CustomTypeView(type))),
            NativeTypeId::For<CustomTypeInterface>());
  EXPECT_EQ(NativeTypeId::Of(TypeView(OpaqueTypeView(CustomTypeView(type)))),
            NativeTypeId::For<CustomTypeInterface>());
}

TEST_P(OpaqueTypeViewTest, InstanceOf) {
  auto type = MakeCustomType();
  EXPECT_TRUE(InstanceOf<OpaqueTypeView>(CustomTypeView(type)));
  EXPECT_TRUE(InstanceOf<OpaqueTypeView>(OpaqueType(CustomTypeView(type))));
  EXPECT_TRUE(InstanceOf<OpaqueTypeView>(
      TypeView(OpaqueTypeView(CustomTypeView(type)))));
  EXPECT_TRUE(InstanceOf<CustomTypeView>(CustomTypeView(type)));
  EXPECT_TRUE(InstanceOf<CustomTypeView>(OpaqueTypeView(CustomTypeView(type))));
  EXPECT_TRUE(InstanceOf<CustomTypeView>(
      TypeView(OpaqueTypeView(CustomTypeView(type)))));
}

TEST_P(OpaqueTypeViewTest, Cast) {
  auto type = MakeCustomType();
  EXPECT_THAT(Cast<CustomTypeView>(CustomTypeView(type)), An<CustomTypeView>());
  EXPECT_THAT(Cast<CustomTypeView>(OpaqueTypeView(CustomTypeView(type))),
              An<CustomTypeView>());
  EXPECT_THAT(
      Cast<CustomTypeView>(TypeView(OpaqueTypeView(CustomTypeView(type)))),
      An<CustomTypeView>());
}

TEST_P(OpaqueTypeViewTest, As) {
  auto type = MakeCustomType();
  EXPECT_THAT(As<CustomTypeView>(CustomTypeView(type)), Ne(absl::nullopt));
  EXPECT_THAT(As<CustomTypeView>(OpaqueTypeView(CustomTypeView(type))),
              Ne(absl::nullopt));
  EXPECT_THAT(
      As<CustomTypeView>(TypeView(OpaqueTypeView(CustomTypeView(type)))),
      Ne(absl::nullopt));
}

INSTANTIATE_TEST_SUITE_P(
    OpaqueTypeViewTest, OpaqueTypeViewTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    OpaqueTypeViewTest::ToString);

}  // namespace
}  // namespace cel
