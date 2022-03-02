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

#include <cstddef>
#include <new>

#include "absl/base/macros.h"
#include "absl/base/optimization.h"

namespace cel::extensions {

MemoryManager::AllocationResult<void*> ProtoMemoryManager::Allocate(
    size_t size, size_t align) {
  void* pointer;
  if (arena_ != nullptr) {
    pointer = arena_->AllocateAligned(size, align);
  } else {
    if (ABSL_PREDICT_TRUE(align <= alignof(std::max_align_t))) {
      pointer = ::operator new(size, std::nothrow);
    } else {
      pointer = ::operator new(size, static_cast<std::align_val_t>(align),
                               std::nothrow);
    }
  }
  return {pointer};
}

void ProtoMemoryManager::Deallocate(void* pointer, size_t size, size_t align) {
  // Only possible when `arena_` is nullptr.
  ABSL_HARDENING_ASSERT(arena_ == nullptr);
  if (ABSL_PREDICT_TRUE(align <= alignof(std::max_align_t))) {
    ::operator delete(pointer, size);
  } else {
    ::operator delete(pointer, size, static_cast<std::align_val_t>(align));
  }
}

void ProtoMemoryManager::OwnDestructor(void* pointer, void (*destruct)(void*)) {
  ABSL_HARDENING_ASSERT(arena_ != nullptr);
  arena_->OwnCustomDestructor(pointer, destruct);
}

}  // namespace cel::extensions
