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
}

struct NotTriviallyDestuctible final {
  ~NotTriviallyDestuctible() { Delete(); }

  MOCK_METHOD(void, Delete, (), ());
};

TEST(GlobalMemoryManager, NotTriviallyDestuctible) {
  EXPECT_FALSE(std::is_trivially_destructible_v<NotTriviallyDestuctible>);
  auto managed = MemoryManager::Global().New<NotTriviallyDestuctible>();
  EXPECT_CALL(*managed, Delete());
}

}  // namespace
}  // namespace cel
