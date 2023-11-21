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

// This header contains primitives for reference counting, roughly equivalent to
// the primitives used to implement `std::shared_ptr`. These primitives should
// not be used directly in most cases, instead `cel::ManagedMemory` should be
// used instead.

#include "common/memory.h"

#include <memory>
#include <sstream>
#include <string>
#include <utility>

#include "absl/base/config.h"  // IWYU pragma: keep
#include "absl/log/absl_check.h"
#include "absl/types/optional.h"
#include "common/casting.h"
#include "common/native_type.h"
#include "internal/testing.h"

#ifdef ABSL_HAVE_EXCEPTIONS
#include <stdexcept>
#endif

namespace cel {
namespace {

using testing::TestParamInfo;
using testing::TestWithParam;

TEST(MemoryManagement, ostream) {
  {
    std::ostringstream out;
    out << MemoryManagement::kPooling;
    EXPECT_EQ(out.str(), "POOLING");
  }
  {
    std::ostringstream out;
    out << MemoryManagement::kReferenceCounting;
    EXPECT_EQ(out.str(), "REFERENCE_COUNTING");
  }
}

TEST(ReferenceCountingMemoryManager, NativeTypeId) {
  EXPECT_EQ(NativeTypeId::Of(MemoryManager::ReferenceCounting()),
            NativeTypeId::For<ReferenceCountingMemoryManager>());
}

struct TrivialSmallObject {
  uintptr_t ptr;
  char padding[32 - sizeof(uintptr_t)];
};

TEST(RegionalMemoryManager, TrivialSmallSizes) {
  MemoryManager memory_manager(NewThreadCompatiblePoolingMemoryManager());
  for (size_t i = 0; i < 1024; ++i) {
    static_cast<void>(memory_manager.MakeUnique<TrivialSmallObject>());
  }
}

struct TrivialMediumObject {
  uintptr_t ptr;
  char padding[256 - sizeof(uintptr_t)];
};

TEST(RegionalMemoryManager, TrivialMediumSizes) {
  MemoryManager memory_manager(NewThreadCompatiblePoolingMemoryManager());
  for (size_t i = 0; i < 1024; ++i) {
    static_cast<void>(memory_manager.MakeUnique<TrivialMediumObject>());
  }
}

struct TrivialLargeObject {
  uintptr_t ptr;
  char padding[4096 - sizeof(uintptr_t)];
};

TEST(RegionalMemoryManager, TrivialLargeSizes) {
  MemoryManager memory_manager(NewThreadCompatiblePoolingMemoryManager());
  for (size_t i = 0; i < 1024; ++i) {
    static_cast<void>(memory_manager.MakeUnique<TrivialLargeObject>());
  }
}

TEST(RegionalMemoryManager, TrivialMixedSizes) {
  MemoryManager memory_manager(NewThreadCompatiblePoolingMemoryManager());
  for (size_t i = 0; i < 1024; ++i) {
    switch (i % 3) {
      case 0:
        static_cast<void>(memory_manager.MakeUnique<TrivialSmallObject>());
        break;
      case 1:
        static_cast<void>(memory_manager.MakeUnique<TrivialMediumObject>());
        break;
      case 2:
        static_cast<void>(memory_manager.MakeUnique<TrivialLargeObject>());
        break;
    }
  }
}

struct TrivialHugeObject {
  uintptr_t ptr;
  char padding[32768 - sizeof(uintptr_t)];
};

TEST(RegionalMemoryManager, TrivialHugeSizes) {
  MemoryManager memory_manager(NewThreadCompatiblePoolingMemoryManager());
  for (size_t i = 0; i < 1024; ++i) {
    static_cast<void>(memory_manager.MakeUnique<TrivialHugeObject>());
  }
}

class SkippableDestructor {
 public:
  explicit SkippableDestructor(bool& deleted) : deleted_(deleted) {}

  ~SkippableDestructor() { deleted_ = true; }

 private:
  bool& deleted_;
};

}  // namespace

template <>
struct NativeTypeTraits<SkippableDestructor> final {
  static bool SkipDestructor(const SkippableDestructor&) { return true; }
};

namespace {

TEST(RegionalMemoryManager, SkippableDestructor) {
  bool deleted = false;
  {
    MemoryManager memory_manager(NewThreadCompatiblePoolingMemoryManager());
    auto shared = memory_manager.MakeShared<SkippableDestructor>(deleted);
    static_cast<void>(shared);
  }
  EXPECT_FALSE(deleted);
}

class MemoryManagerTest : public TestWithParam<MemoryManagement> {
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

  static std::string ToString(TestParamInfo<MemoryManagement> param) {
    std::ostringstream out;
    out << param.param;
    return out.str();
  }

 private:
  absl::optional<MemoryManager> memory_manager_;
};

class Object {
 public:
  Object() : deleted_(nullptr) {}

  explicit Object(bool& deleted) : deleted_(&deleted) {}

  ~Object() {
    if (deleted_ != nullptr) {
      ABSL_CHECK(!*deleted_);
      *deleted_ = true;
    }
  }

 private:
  bool* deleted_;
};

class Subobject : public Object {
 public:
  using Object::Object;
};

TEST_P(MemoryManagerTest, Shared) {
  bool deleted = false;
  {
    auto object = memory_manager().MakeShared<Object>(deleted);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedCopyConstruct) {
  bool deleted = false;
  {
    auto object = memory_manager().MakeShared<Object>(deleted);
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    Shared<Object> copied_object(object);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedMoveConstruct) {
  bool deleted = false;
  {
    auto object = memory_manager().MakeShared<Object>(deleted);
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    Shared<Object> moved_object(std::move(object));
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedCopyAssign) {
  bool deleted = false;
  {
    auto object = memory_manager().MakeShared<Object>(deleted);
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    Shared<Object> moved_object(std::move(object));
    object = moved_object;
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedMoveAssign) {
  bool deleted = false;
  {
    auto object = memory_manager().MakeShared<Object>(deleted);
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    Shared<Object> moved_object(std::move(object));
    object = std::move(moved_object);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedCopyConstructConvertible) {
  bool deleted = false;
  {
    auto object = memory_manager().MakeShared<Subobject>(deleted);
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    Shared<Object> copied_object(object);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedMoveConstructConvertible) {
  bool deleted = false;
  {
    auto object = memory_manager().MakeShared<Subobject>(deleted);
    // NOLINTNEXTLINE(performance-unnecessary-copy-initialization)
    Shared<Object> moved_object(std::move(object));
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedCopyAssignConvertible) {
  bool deleted = false;
  {
    auto subobject = memory_manager().MakeShared<Subobject>(deleted);
    auto object = memory_manager().MakeShared<Object>();
    object = subobject;
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedMoveAssignConvertible) {
  bool deleted = false;
  {
    auto subobject = memory_manager().MakeShared<Subobject>(deleted);
    auto object = memory_manager().MakeShared<Object>();
    object = std::move(subobject);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedSwap) {
  using std::swap;
  auto object1 = memory_manager().MakeShared<Object>();
  auto object2 = memory_manager().MakeShared<Object>();
  auto* const object1_ptr = object1.operator->();
  auto* const object2_ptr = object2.operator->();
  swap(object1, object2);
  EXPECT_EQ(object1.operator->(), object2_ptr);
  EXPECT_EQ(object2.operator->(), object1_ptr);
}

TEST_P(MemoryManagerTest, SharedPointee) {
  using std::swap;
  auto object = memory_manager().MakeShared<Object>();
  EXPECT_EQ(std::addressof(*object), object.operator->());
}

TEST_P(MemoryManagerTest, SharedViewConstruct) {
  bool deleted = false;
  absl::optional<SharedView<Object>> dangling_object_view;
  {
    auto object = memory_manager().MakeShared<Object>(deleted);
    dangling_object_view.emplace(object);
    {
      auto copied_object = Shared<Object>(*dangling_object_view);
      EXPECT_FALSE(deleted);
    }
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedViewCopyConstruct) {
  bool deleted = false;
  absl::optional<SharedView<Object>> dangling_object_view;
  {
    auto object = memory_manager().MakeShared<Object>(deleted);
    auto object_view = SharedView<Object>(object);
    SharedView<Object> copied_object_view(object_view);
    dangling_object_view.emplace(copied_object_view);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedViewMoveConstruct) {
  bool deleted = false;
  absl::optional<SharedView<Object>> dangling_object_view;
  {
    auto object = memory_manager().MakeShared<Object>(deleted);
    auto object_view = SharedView<Object>(object);
    SharedView<Object> moved_object_view(std::move(object_view));
    dangling_object_view.emplace(moved_object_view);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedViewCopyAssign) {
  bool deleted = false;
  absl::optional<SharedView<Object>> dangling_object_view;
  {
    auto object = memory_manager().MakeShared<Object>(deleted);
    auto object_view1 = SharedView<Object>(object);
    SharedView<Object> object_view2(object);
    object_view1 = object_view2;
    dangling_object_view.emplace(object_view1);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedViewMoveAssign) {
  bool deleted = false;
  absl::optional<SharedView<Object>> dangling_object_view;
  {
    auto object = memory_manager().MakeShared<Object>(deleted);
    auto object_view1 = SharedView<Object>(object);
    SharedView<Object> object_view2(object);
    object_view1 = std::move(object_view2);
    dangling_object_view.emplace(object_view1);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedViewCopyConstructConvertible) {
  bool deleted = false;
  absl::optional<SharedView<Object>> dangling_object_view;
  {
    auto subobject = memory_manager().MakeShared<Subobject>(deleted);
    auto subobject_view = SharedView<Subobject>(subobject);
    SharedView<Object> object_view(subobject_view);
    dangling_object_view.emplace(object_view);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedViewMoveConstructConvertible) {
  bool deleted = false;
  absl::optional<SharedView<Object>> dangling_object_view;
  {
    auto subobject = memory_manager().MakeShared<Subobject>(deleted);
    auto subobject_view = SharedView<Subobject>(subobject);
    SharedView<Object> object_view(std::move(subobject_view));
    dangling_object_view.emplace(object_view);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedViewCopyAssignConvertible) {
  bool deleted = false;
  absl::optional<SharedView<Object>> dangling_object_view;
  {
    auto subobject = memory_manager().MakeShared<Subobject>(deleted);
    auto object_view1 = SharedView<Object>(subobject);
    SharedView<Subobject> subobject_view2(subobject);
    object_view1 = subobject_view2;
    dangling_object_view.emplace(object_view1);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedViewMoveAssignConvertible) {
  bool deleted = false;
  absl::optional<SharedView<Object>> dangling_object_view;
  {
    auto subobject = memory_manager().MakeShared<Subobject>(deleted);
    auto object_view1 = SharedView<Object>(subobject);
    SharedView<Subobject> subobject_view2(subobject);
    object_view1 = std::move(subobject_view2);
    dangling_object_view.emplace(object_view1);
    EXPECT_FALSE(deleted);
  }
  switch (memory_management()) {
    case MemoryManagement::kPooling:
      EXPECT_FALSE(deleted);
      break;
    case MemoryManagement::kReferenceCounting:
      EXPECT_TRUE(deleted);
      break;
  }
  Finish();
}

TEST_P(MemoryManagerTest, SharedViewSwap) {
  using std::swap;
  auto object1 = memory_manager().MakeShared<Object>();
  auto object2 = memory_manager().MakeShared<Object>();
  auto object1_view = SharedView<Object>(object1);
  auto object2_view = SharedView<Object>(object2);
  swap(object1_view, object2_view);
  EXPECT_EQ(object1_view.operator->(), object2.operator->());
  EXPECT_EQ(object2_view.operator->(), object1.operator->());
}

TEST_P(MemoryManagerTest, SharedViewPointee) {
  using std::swap;
  auto object = memory_manager().MakeShared<Object>();
  auto object_view = SharedView<Object>(object);
  EXPECT_EQ(std::addressof(*object_view), object_view.operator->());
}

TEST_P(MemoryManagerTest, Unique) {
  bool deleted = false;
  {
    auto object = memory_manager().MakeUnique<Object>(deleted);
    EXPECT_FALSE(deleted);
  }
  EXPECT_TRUE(deleted);

  Finish();
}

TEST_P(MemoryManagerTest, UniquePointee) {
  using std::swap;
  auto object = memory_manager().MakeUnique<Object>();
  EXPECT_EQ(std::addressof(*object), object.operator->());
}

TEST_P(MemoryManagerTest, UniqueSwap) {
  using std::swap;
  auto object1 = memory_manager().MakeUnique<Object>();
  auto object2 = memory_manager().MakeUnique<Object>();
  auto* const object1_ptr = object1.operator->();
  auto* const object2_ptr = object2.operator->();
  swap(object1, object2);
  EXPECT_EQ(object1.operator->(), object2_ptr);
  EXPECT_EQ(object2.operator->(), object1_ptr);
}

struct EnabledObject : EnableSharedFromThis<EnabledObject> {
  Shared<EnabledObject> This() { return shared_from_this(); }

  Shared<const EnabledObject> This() const { return shared_from_this(); }
};

TEST_P(MemoryManagerTest, EnableSharedFromThis) {
  {
    auto object = memory_manager().MakeShared<EnabledObject>();
    auto this_object = object->This();
    EXPECT_EQ(this_object.operator->(), object.operator->());
  }
  {
    auto object = memory_manager().MakeShared<const EnabledObject>();
    auto this_object = object->This();
    EXPECT_EQ(this_object.operator->(), object.operator->());
  }
  Finish();
}

struct ThrowingConstructorObject {
  ThrowingConstructorObject() {
#ifdef ABSL_HAVE_EXCEPTIONS
    throw std::invalid_argument("ThrowingConstructorObject");
#endif
  }

  char padding[64];
};

TEST_P(MemoryManagerTest, SharedThrowingConstructor) {
#ifdef ABSL_HAVE_EXCEPTIONS
  EXPECT_THROW(static_cast<void>(
                   memory_manager().MakeShared<ThrowingConstructorObject>()),
               std::invalid_argument);
#else
  GTEST_SKIP();
#endif
}

TEST_P(MemoryManagerTest, UniqueThrowingConstructor) {
#ifdef ABSL_HAVE_EXCEPTIONS
  EXPECT_THROW(static_cast<void>(
                   memory_manager().MakeUnique<ThrowingConstructorObject>()),
               std::invalid_argument);
#else
  GTEST_SKIP();
#endif
}

INSTANTIATE_TEST_SUITE_P(
    MemoryManagerTest, MemoryManagerTest,
    ::testing::Values(MemoryManagement::kPooling,
                      MemoryManagement::kReferenceCounting),
    MemoryManagerTest::ToString);

TEST(MemoryManagerCasting, ReferenceCounting) {
  EXPECT_TRUE(InstanceOf<ReferenceCountingMemoryManager>(
      MemoryManager::ReferenceCounting()));
  EXPECT_FALSE(InstanceOf<ReferenceCountingMemoryManager>(
      MemoryManager(NewThreadCompatiblePoolingMemoryManager())));
}

TEST(MemoryManagerCasting, Pooling) {
  EXPECT_FALSE(
      InstanceOf<PoolingMemoryManager>(MemoryManager::ReferenceCounting()));
  EXPECT_TRUE(InstanceOf<PoolingMemoryManager>(
      MemoryManager(NewThreadCompatiblePoolingMemoryManager())));
}

TEST(MemoryManagerRefCasting, ReferenceCounting) {
  EXPECT_TRUE(InstanceOf<ReferenceCountingMemoryManager>(
      MemoryManagerRef::ReferenceCounting()));
  auto pooling = MemoryManager(NewThreadCompatiblePoolingMemoryManager());
  EXPECT_FALSE(
      InstanceOf<ReferenceCountingMemoryManager>(MemoryManagerRef(pooling)));
}

TEST(MemoryManagerRefCasting, Pooling) {
  EXPECT_FALSE(
      InstanceOf<PoolingMemoryManager>(MemoryManagerRef::ReferenceCounting()));
  auto pooling = MemoryManager(NewThreadCompatiblePoolingMemoryManager());
  EXPECT_TRUE(InstanceOf<PoolingMemoryManager>(MemoryManagerRef(pooling)));
}

}  // namespace
}  // namespace cel
