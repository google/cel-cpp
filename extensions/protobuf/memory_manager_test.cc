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

#include "extensions/protobuf/memory_manager.h"

#include "google/protobuf/arena.h"
#include "internal/testing.h"

namespace cel::extensions {
namespace {

struct NotTriviallyDestuctible final {
  ~NotTriviallyDestuctible() { Delete(); }

  MOCK_METHOD(void, Delete, (), ());
};

TEST(ProtoMemoryManager, NotTriviallyDestuctible) {
  google::protobuf::Arena arena;
  ProtoMemoryManager memory_manager(&arena);
  {
    // Destructor is called when UniqueRef is destructed, not on MemoryManager
    // destruction.
    auto managed = MakeUnique<NotTriviallyDestuctible>(memory_manager);
    EXPECT_CALL(*managed, Delete());
  }
}

}  // namespace
}  // namespace cel::extensions
