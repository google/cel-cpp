// Copyright 2022 Google LLC
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

#include "base/memory_manager.h"

#include <type_traits>

#include "base/internal/memory_manager.h"
#include "internal/testing.h"

namespace cel {
namespace {

struct TriviallyDestructible final {};

TEST(GlobalMemoryManager, TriviallyDestructible) {
  EXPECT_TRUE(std::is_trivially_destructible_v<TriviallyDestructible>);
  auto managed = MemoryManager::Global()->New<TriviallyDestructible>();
  EXPECT_FALSE(base_internal::IsEmptyDeleter(managed.get_deleter()));
}

struct NotTriviallyDestuctible final {
  ~NotTriviallyDestuctible() { Delete(); }

  MOCK_METHOD(void, Delete, (), ());
};

TEST(GlobalMemoryManager, NotTriviallyDestuctible) {
  EXPECT_FALSE(std::is_trivially_destructible_v<NotTriviallyDestuctible>);
  auto managed = MemoryManager::Global()->New<NotTriviallyDestuctible>();
  EXPECT_FALSE(base_internal::IsEmptyDeleter(managed.get_deleter()));
  EXPECT_CALL(*managed, Delete());
}

class BadMemoryManager final : public MemoryManager {
 private:
  AllocationResult<void*> Allocate(size_t size, size_t align) override {
    // Return {..., false}, indicating that this was an arena allocation when it
    // is not, causing OwnDestructor to be called and abort.
    return {::operator new(size, static_cast<std::align_val_t>(align)), false};
  }

  void Deallocate(void* pointer, size_t size, size_t align) override {
    ::operator delete(pointer, size, static_cast<std::align_val_t>(align));
  }
};

TEST(BadMemoryManager, OwnDestructorAborts) {
  BadMemoryManager memory_manager;
  EXPECT_EXIT(static_cast<void>(memory_manager.New<NotTriviallyDestuctible>()),
              testing::KilledBySignal(SIGABRT), "");
}

class BadArenaMemoryManager final : public ArenaMemoryManager {
 private:
  AllocationResult<void*> Allocate(size_t size, size_t align) override {
    // Return {..., false}, indicating that this was an arena allocation when it
    // is not, causing OwnDestructor to be called and abort.
    return {::operator new(size, static_cast<std::align_val_t>(align)), true};
  }

  void OwnDestructor(void* pointer, void (*destructor)(void*)) override {}
};

TEST(BadArenaMemoryManager, DeallocateAborts) {
  BadArenaMemoryManager memory_manager;
  EXPECT_EXIT(static_cast<void>(memory_manager.New<NotTriviallyDestuctible>()),
              testing::KilledBySignal(SIGABRT), "");
}

}  // namespace
}  // namespace cel
