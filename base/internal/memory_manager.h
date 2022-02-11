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

#ifndef THIRD_PARTY_CEL_CPP_BASE_INTERNAL_MEMORY_MANAGER_H_
#define THIRD_PARTY_CEL_CPP_BASE_INTERNAL_MEMORY_MANAGER_H_

#include <utility>

namespace cel {

class MemoryManager;

namespace base_internal {

template <typename T>
class MemoryManagerDeleter;

// True if the deleter is no-op, meaning the object was allocated in an arena
// and the arena will perform any deletion upon its own destruction.
template <typename T>
bool IsEmptyDeleter(const MemoryManagerDeleter<T>& deleter);

template <typename T>
class MemoryManagerDeleter final {
 public:
  constexpr MemoryManagerDeleter() noexcept = default;

  MemoryManagerDeleter(const MemoryManagerDeleter<T>&) = delete;

  constexpr MemoryManagerDeleter(MemoryManagerDeleter<T>&& other) noexcept
      : MemoryManagerDeleter() {
    std::swap(memory_manager_, other.memory_manager_);
    std::swap(size_, other.size_);
    std::swap(align_, other.align_);
  }

  void operator()(T* pointer) const;

 private:
  friend class cel::MemoryManager;
  template <typename F>
  friend bool IsEmptyDeleter(const MemoryManagerDeleter<F>& deleter);

  MemoryManagerDeleter(MemoryManager* memory_manager, size_t size, size_t align)
      : memory_manager_(memory_manager), size_(size), align_(align) {}

  MemoryManager* memory_manager_ = nullptr;
  size_t size_ = 0;
  size_t align_ = 0;
};

template <typename T>
bool IsEmptyDeleter(const MemoryManagerDeleter<T>& deleter) {
  return deleter.memory_manager_ == nullptr;
}

template <typename T>
class MemoryManagerDestructor final {
 private:
  friend class cel::MemoryManager;

  static void Destruct(void* pointer) { reinterpret_cast<T*>(pointer)->~T(); }
};

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_INTERNAL_MEMORY_MANAGER_H_
