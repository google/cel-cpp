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

#ifndef _WIN32
#include <sys/mman.h>
#include <unistd.h>

#include <cstdio>
#else
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN 1
#endif
#ifndef NOMINMAX
#define NOMINMAX 1
#endif
#include <windows.h>
#endif

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <new>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

#include "absl/base/config.h"
#include "absl/base/dynamic_annotations.h"
#include "absl/base/macros.h"
#include "absl/base/thread_annotations.h"
#include "absl/numeric/bits.h"
#include "absl/synchronization/mutex.h"
#include "internal/no_destructor.h"

namespace cel {

namespace {

uintptr_t AlignUp(uintptr_t size, size_t align) {
  ABSL_ASSERT(size != 0);
  ABSL_ASSERT(absl::has_single_bit(align));  // Assert aligned to power of 2.
#if ABSL_HAVE_BUILTIN(__builtin_align_up)
  return __builtin_align_up(size, align);
#else
  return (size + static_cast<uintptr_t>(align) - uintptr_t{1}) &
         ~(static_cast<uintptr_t>(align) - uintptr_t{1});
#endif
}

template <typename T>
T* AlignUp(T* pointer, size_t align) {
  return reinterpret_cast<T*>(
      AlignUp(reinterpret_cast<uintptr_t>(pointer), align));
}

struct ArenaBlock final {
  // The base pointer of the virtual memory, always points to the start of a
  // page.
  uint8_t* begin;
  // The end pointer of the virtual memory, it's 1 past the last byte of the
  // page(s).
  uint8_t* end;
  // The pointer to the first byte that we have not yet allocated.
  uint8_t* current;

  size_t remaining() const { return static_cast<size_t>(end - current); }

  // Aligns the current pointer to `align`.
  ArenaBlock& Align(size_t align) {
    current = std::min(end, AlignUp(current, align));
    return *this;
  }

  // Allocate `size` bytes from this block. This causes the current pointer to
  // advance `size` bytes.
  uint8_t* Allocate(size_t size) {
    uint8_t* pointer = current;
    current += size;
    ABSL_ASSERT(current <= end);
    return pointer;
  }

  size_t capacity() const { return static_cast<size_t>(end - begin); }
};

// Allocate a block of virtual memory from the kernel. `size` must be a multiple
// of `GetArenaPageSize()`. `hint` is a suggestion to the kernel of where we
// would like the virtual memory to be placed.
std::optional<ArenaBlock> ArenaBlockAllocate(size_t size,
                                             void* hint = nullptr) {
  void* pointer;
#ifndef _WIN32
  pointer = mmap(hint, size, PROT_READ | PROT_WRITE,
                 MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
  if (ABSL_PREDICT_FALSE(pointer == MAP_FAILED)) {
    return std::nullopt;
  }
#else
  pointer = VirtualAlloc(hint, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
  if (ABSL_PREDICT_FALSE(pointer == nullptr)) {
    if (hint == nullptr) {
      return std::nullopt;
    }
    // Try again, without the hint.
    pointer =
        VirtualAlloc(nullptr, size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (pointer == nullptr) {
      return std::nullopt;
    }
  }
#endif
  ANNOTATE_MEMORY_IS_UNINITIALIZED(pointer, size);
  return ArenaBlock{static_cast<uint8_t*>(pointer),
                    static_cast<uint8_t*>(pointer) + size,
                    static_cast<uint8_t*>(pointer)};
}

// Free the block of virtual memory with the kernel.
void ArenaBlockFree(void* pointer, size_t size) {
#ifndef _WIN32
  if (ABSL_PREDICT_FALSE(munmap(pointer, size))) {
    // If this happens its likely a bug and its probably corruption. Just bail.
    std::perror("cel: failed to unmap pages from memory");
    std::fflush(stderr);
    std::abort();
  }
#else
  static_cast<void>(size);
  if (ABSL_PREDICT_FALSE(!VirtualFree(pointer, 0, MEM_RELEASE))) {
    // TODO(issues/5): print the error
    std::abort();
  }
#endif
}

class DefaultArenaMemoryManager final : public ArenaMemoryManager {
 public:
  ~DefaultArenaMemoryManager() override {
    absl::MutexLock lock(&mutex_);
    for (const auto& owned : owned_) {
      (*owned.second)(owned.first);
    }
    for (auto& block : blocks_) {
      ArenaBlockFree(block.begin, block.capacity());
    }
  }

 private:
  void* Allocate(size_t size, size_t align) override {
    auto page_size = base_internal::GetPageSize();
    if (align > page_size) {
      // Just, no. We refuse anything that requests alignment over the system
      // page size.
      return nullptr;
    }
    absl::MutexLock lock(&mutex_);
    bool bridge_gap = false;
    if (ABSL_PREDICT_FALSE(blocks_.empty() ||
                           blocks_.back().Align(align).remaining() == 0)) {
      // Currently no allocated blocks or the allocation alignment is large
      // enough that we cannot use any of the last block. Just allocate a block
      // large enough.
      auto maybe_block = ArenaBlockAllocate(AlignUp(size, page_size));
      if (!maybe_block.has_value()) {
        return nullptr;
      }
      blocks_.push_back(std::move(maybe_block).value());
    } else {
      // blocks_.back() was aligned above.
      auto& last_block = blocks_.back();
      size_t remaining = last_block.remaining();
      if (ABSL_PREDICT_FALSE(remaining < size)) {
        auto maybe_block =
            ArenaBlockAllocate(AlignUp(size, page_size), last_block.end);
        if (!maybe_block.has_value()) {
          return nullptr;
        }
        bridge_gap = last_block.end == maybe_block.value().begin;
        blocks_.push_back(std::move(maybe_block).value());
      }
    }
    if (ABSL_PREDICT_FALSE(bridge_gap)) {
      // The last block did not have enough to fit the requested size, so we had
      // to allocate a new block. However the alignment was low enough and the
      // kernel gave us the page immediately after the last. Therefore we can
      // span the allocation across both blocks.
      auto& second_last_block = blocks_[blocks_.size() - 2];
      size_t remaining = second_last_block.remaining();
      void* pointer = second_last_block.Allocate(remaining);
      blocks_.back().Allocate(size - remaining);
      return pointer;
    }
    return blocks_.back().Allocate(size);
  }

  void OwnDestructor(void* pointer, void (*destruct)(void*)) override {
    absl::MutexLock lock(&mutex_);
    owned_.emplace_back(pointer, destruct);
  }

  absl::Mutex mutex_;
  std::vector<ArenaBlock> blocks_ ABSL_GUARDED_BY(mutex_);
  std::vector<std::pair<void*, void (*)(void*)>> owned_ ABSL_GUARDED_BY(mutex_);
  // TODO(issues/5): we could use a priority queue to keep track of any
  // unallocated space at the end blocks.
};

}  // namespace

class GlobalMemoryManager final : public MemoryManager {
 public:
  GlobalMemoryManager() : MemoryManager(false) {}

 private:
  // Never actually called by `MemoryManager`.
  void* Allocate(size_t size, size_t align) override {
    static_cast<void>(size);
    static_cast<void>(align);
    ABSL_INTERNAL_UNREACHABLE;
    return nullptr;
  }

  // Never actually called by `MemoryManager`.
  void OwnDestructor(void* pointer, void (*destructor)(void*)) override {
    static_cast<void>(pointer);
    static_cast<void>(destructor);
    ABSL_INTERNAL_UNREACHABLE;
  }
};

namespace base_internal {

// Returns the platforms page size. When requesting vitual memory from the
// kernel, typically the size requested must be a multiple of the page size.
size_t GetPageSize() {
  static const size_t page_size = []() -> size_t {
#ifndef _WIN32
    auto value = sysconf(_SC_PAGESIZE);
    if (ABSL_PREDICT_FALSE(value == -1)) {
      // This should not happen, if it does bail. There is no other way to
      // determine the page size.
      std::perror("cel: failed to determine system page size");
      std::fflush(stderr);
      std::abort();
    }
    return static_cast<size_t>(value);
#else
    SYSTEM_INFO system_info;
    SecureZeroMemory(&system_info, sizeof(system_info));
    GetSystemInfo(&system_info);
    return static_cast<size_t>(system_info.dwPageSize);
#endif
  }();
  return page_size;
}

}  // namespace base_internal

MemoryManager& MemoryManager::Global() {
  static internal::NoDestructor<GlobalMemoryManager> instance;
  return *instance;
}

std::unique_ptr<ArenaMemoryManager> ArenaMemoryManager::Default() {
  return std::make_unique<DefaultArenaMemoryManager>();
}

}  // namespace cel
