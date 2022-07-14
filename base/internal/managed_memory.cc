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

#include "base/internal/managed_memory.h"

#include <cstddef>
#include <cstdint>
#include <new>

#include "absl/base/config.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/numeric/bits.h"

namespace cel::base_internal {

namespace {

size_t AlignUp(size_t size, size_t align) {
#if ABSL_HAVE_BUILTIN(__builtin_align_up)
  return __builtin_align_up(size, align);
#else
  return (size + align - size_t{1}) & ~(align - size_t{1});
#endif
}

}  // namespace

std::pair<ManagedMemoryState*, void*> ManagedMemoryState::New(
    size_t size, size_t align, ManagedMemoryDestructor destructor) {
  ABSL_ASSERT(size != 0);
  ABSL_ASSERT(absl::has_single_bit(align));  // Assert aligned to power of 2.
  if (ABSL_PREDICT_TRUE(align <= sizeof(ManagedMemoryState))) {
    // Alignment requirements are less than the size of `ManagedMemoryState`, we
    // can place `ManagedMemoryState` in front.
    uint8_t* pointer = reinterpret_cast<uint8_t*>(
        ::operator new(size + sizeof(ManagedMemoryState)));
    ::new (pointer) ManagedMemoryState(destructor);
    return {reinterpret_cast<ManagedMemoryState*>(pointer),
            static_cast<void*>(pointer + sizeof(ManagedMemoryState))};
  }
  // Alignment requirements are greater than the size of `ManagedMemoryState`,
  // we need to place `ManagedMemoryState` at the back and pad to ensure
  // `ManagedMemoryState` itself is aligned.
  size_t adjusted_size = AlignUp(size, alignof(ManagedMemoryState));
  uint8_t* pointer = reinterpret_cast<uint8_t*>(
      ::operator new(adjusted_size + sizeof(ManagedMemoryState)));
  ::new (pointer + adjusted_size) ManagedMemoryState(destructor);
  return {reinterpret_cast<ManagedMemoryState*>(pointer + adjusted_size),
          static_cast<void*>(pointer)};
}

void ManagedMemoryState::Delete(void* pointer) {
  ABSL_ASSERT(pointer != nullptr);
  ABSL_ASSERT(this != pointer);
  if (destructor_ != nullptr) {
    (*destructor_)(pointer);
  }
  this->~ManagedMemoryState();
  ::operator delete(reinterpret_cast<uint8_t*>(this) <
                            static_cast<const uint8_t*>(pointer)
                        ? static_cast<void*>(this)
                        : const_cast<void*>(pointer));
}

}  // namespace cel::base_internal
