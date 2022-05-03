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

#include "internal/testing.h"

namespace cel {
namespace {

struct TriviallyDestructible final {};

TEST(GlobalMemoryManager, TriviallyDestructible) {
  EXPECT_TRUE(std::is_trivially_destructible_v<TriviallyDestructible>);
  auto managed = MemoryManager::Global().New<TriviallyDestructible>();
  EXPECT_NE(managed, nullptr);
  EXPECT_NE(nullptr, managed);
}

struct NotTriviallyDestuctible final {
  ~NotTriviallyDestuctible() { Delete(); }

  MOCK_METHOD(void, Delete, (), ());
};

TEST(GlobalMemoryManager, NotTriviallyDestuctible) {
  EXPECT_FALSE(std::is_trivially_destructible_v<NotTriviallyDestuctible>);
  auto managed = MemoryManager::Global().New<NotTriviallyDestuctible>();
  EXPECT_NE(managed, nullptr);
  EXPECT_NE(nullptr, managed);
  EXPECT_CALL(*managed, Delete());
}

TEST(ManagedMemory, Null) {
  EXPECT_EQ(ManagedMemory<TriviallyDestructible>(), nullptr);
  EXPECT_EQ(nullptr, ManagedMemory<TriviallyDestructible>());
}

struct LargeStruct {
  char padding[4096 - alignof(char)];
};

TEST(DefaultArenaMemoryManager, OddSizes) {
  auto memory_manager = ArenaMemoryManager::Default();
  size_t page_size = base_internal::GetPageSize();
  for (size_t allocated = 0; allocated <= page_size;
       allocated += sizeof(LargeStruct)) {
    static_cast<void>(memory_manager->New<LargeStruct>());
  }
}

}  // namespace
}  // namespace cel
