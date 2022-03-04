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

// IWYU pragma: private

#ifndef THIRD_PARTY_CEL_CPP_BASE_INTERNAL_MEMORY_MANAGER_PRE_H_
#define THIRD_PARTY_CEL_CPP_BASE_INTERNAL_MEMORY_MANAGER_PRE_H_

#include <cstddef>
#include <utility>

namespace cel {

template <typename T>
class ManagedMemory;
class MemoryManager;

namespace base_internal {

class Resource;

template <typename T>
constexpr size_t GetManagedMemorySize(const ManagedMemory<T>& managed_memory);

template <typename T>
constexpr size_t GetManagedMemoryAlignment(
    const ManagedMemory<T>& managed_memory);

template <typename T>
constexpr T* ManagedMemoryRelease(ManagedMemory<T>& managed_memory);

template <typename T>
class MemoryManagerDestructor final {
 private:
  friend class cel::MemoryManager;

  static void Destruct(void* pointer) { reinterpret_cast<T*>(pointer)->~T(); }
};

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_INTERNAL_MEMORY_MANAGER_PRE_H_
