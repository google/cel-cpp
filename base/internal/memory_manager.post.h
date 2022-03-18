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

// IWYU pragma: private, include "base/memory_manager.h"

#ifndef THIRD_PARTY_CEL_CPP_BASE_INTERNAL_MEMORY_MANAGER_POST_H_
#define THIRD_PARTY_CEL_CPP_BASE_INTERNAL_MEMORY_MANAGER_POST_H_

namespace cel::base_internal {

template <typename T>
constexpr size_t GetManagedMemorySize(const ManagedMemory<T>& managed_memory) {
  return managed_memory.size_;
}

template <typename T>
constexpr size_t GetManagedMemoryAlignment(
    const ManagedMemory<T>& managed_memory) {
  return managed_memory.align_;
}

template <typename T>
constexpr T* ManagedMemoryRelease(ManagedMemory<T>& managed_memory) {
  // Like ManagedMemory<T>::release except there is no assert. For use during
  // handle creation.
  T* ptr = managed_memory.ptr_;
  managed_memory.ptr_ = nullptr;
  managed_memory.size_ = managed_memory.align_ = 0;
  return ptr;
}

inline MemoryManager& GetMemoryManager(const void* pointer, size_t size,
                                       size_t align) {
  return MemoryManager::Get(pointer, size, align);
}

}  // namespace cel::base_internal

#endif  // THIRD_PARTY_CEL_CPP_BASE_INTERNAL_MEMORY_MANAGER_POST_H_
