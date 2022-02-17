#include "base/memory_manager.h"

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

#include <cstdlib>
#include <new>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"

namespace cel {

namespace {

class GlobalMemoryManager final : public MemoryManager {
 private:
  AllocationResult<void*> Allocate(size_t size, size_t align) override {
    return {::operator new(size, static_cast<std::align_val_t>(align),
                           std::nothrow),
            true};
  }

  void Deallocate(void* pointer, size_t size, size_t align) override {
    ::operator delete(pointer, size, static_cast<std::align_val_t>(align));
  }
};

}  // namespace

MemoryManager& MemoryManager::Global() {
  static MemoryManager* const instance = new GlobalMemoryManager();
  return *instance;
}

void MemoryManager::OwnDestructor(void* pointer, void (*destruct)(void*)) {
  static_cast<void>(pointer);
  static_cast<void>(destruct);
  // OwnDestructor is only called for arena-based memory managers by `New`. If
  // we got here, something is seriously wrong so crashing is okay.
  std::abort();
}

void ArenaMemoryManager::Deallocate(void* pointer, size_t size, size_t align) {
  static_cast<void>(pointer);
  static_cast<void>(size);
  static_cast<void>(align);
  // Most arena-based allocators will not deallocate individual allocations, so
  // we default the implementation to std::abort().
  std::abort();
}

}  // namespace cel
