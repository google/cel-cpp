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

#ifndef THIRD_PARTY_CEL_CPP_BASE_INTERNAL_MANAGED_MEMORY_H_
#define THIRD_PARTY_CEL_CPP_BASE_INTERNAL_MANAGED_MEMORY_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/numeric/bits.h"
#include "base/internal/data.h"

namespace cel {

class MemoryManager;

namespace base_internal {

template <typename T, bool D = std::is_base_of_v<Data, T>>
class ManagedMemory;

template <typename T>
T* ManagedMemoryRelease(ManagedMemory<T>& managed_memory);

// ManagedMemory implementation for T that is derived from Data and HeapData.
template <typename T>
class ManagedMemory<T, true> final {
 private:
  static_assert(std::is_base_of_v<HeapData, T>,
                "T must be derived from HeapData");

 public:
  ManagedMemory() = default;

  explicit ManagedMemory(std::nullptr_t) : ManagedMemory() {}

  ManagedMemory(const ManagedMemory& other) : pointer_(other.pointer_) {
    Ref();
  }

  template <typename F,
            typename = std::enable_if_t<std::is_convertible_v<F*, T*>>>
  ManagedMemory(const ManagedMemory<F, true>& other)  // NOLINT
      : pointer_(other.pointer_) {
    Ref();
  }

  ManagedMemory(ManagedMemory&& other) : ManagedMemory() {
    std::swap(pointer_, other.pointer_);
  }

  template <typename F,
            typename = std::enable_if_t<std::is_convertible_v<F*, T*>>>
  ManagedMemory(ManagedMemory<F, true>&& other)  // NOLINT
      : ManagedMemory() {
    std::swap(pointer_, other.pointer_);
  }

  ~ManagedMemory() { Unref(); }

  ManagedMemory& operator=(const ManagedMemory& other) {
    if (this != &other) {
      other.Ref();
      Unref();
      pointer_ = other.pointer_;
    }
    return *this;
  }

  template <typename F>
  std::enable_if_t<std::is_convertible_v<F*, T*>, ManagedMemory&>  // NOLINT
  operator=(const ManagedMemory<F, true>& other) {
    if (this != &other) {
      other.Ref();
      Unref();
      pointer_ = other.pointer_;
    }
    return *this;
  }

  ManagedMemory& operator=(ManagedMemory&& other) {
    if (this != &other) {
      Unref();
      pointer_ = 0;
      std::swap(pointer_, other.pointer_);
    }
    return *this;
  }

  template <typename F>
  std::enable_if_t<std::is_convertible_v<F*, T*>, ManagedMemory&>  // NOLINT
  operator=(ManagedMemory<F, true>&& other) {
    if (this != &other) {
      Unref();
      pointer_ = 0;
      std::swap(pointer_, other.pointer_);
    }
    return *this;
  }

  T* get() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    return reinterpret_cast<T*>(pointer_ & kPointerMask);
  }

  T& operator*() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_ASSERT(get() != nullptr);
    return *get();
  }

  T* operator->() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_ASSERT(get() != nullptr);
    return get();
  }

  explicit operator bool() const { return get() != nullptr; }

  ABSL_MUST_USE_RESULT T* release() {
    if (pointer_ == 0) {
      return nullptr;
    }
    ABSL_ASSERT((pointer_ & kPointerArenaAllocated) == kPointerArenaAllocated);
    T* pointer = get();
    pointer_ = 0;
    return pointer;
  }

 private:
  friend class cel::MemoryManager;

  template <typename F>
  friend F* ManagedMemoryRelease(ManagedMemory<F>& managed_memory);

  explicit ManagedMemory(T* pointer) {
    ABSL_ASSERT(absl::countr_zero(reinterpret_cast<uintptr_t>(pointer)) >= 2);
    pointer_ =
        reinterpret_cast<uintptr_t>(pointer) |
        (Metadata::IsArenaAllocated(*pointer) ? kPointerArenaAllocated : 0);
  }

  void Ref() const {
    if (pointer_ != 0 && (pointer_ & kPointerArenaAllocated) == 0) {
      Metadata::Ref(**this);
    }
  }

  void Unref() const {
    if (pointer_ != 0 && (pointer_ & kPointerArenaAllocated) == 0 &&
        Metadata::Unref(**this)) {
      delete static_cast<const HeapData*>(get());
    }
  }

  uintptr_t pointer_ = 0;
};

template <typename T>
T* ManagedMemoryRelease(ManagedMemory<T>& managed_memory) {
  T* pointer = managed_memory.get();
  managed_memory.pointer_ = 0;
  return pointer;
}

using ManagedMemoryDestructor = void (*)(void*);

// Shared state used by `ManagedMemory<T, false>` that holds the reference count
// and destructor to call when the reference count hits 0. `MemoryManager`
// places `T` and `ManagedMemoryState` in the same allocation. Whether
// `ManagedMemoryState` is before or after `T` depends on alignment requirements
// of `T`.
class ManagedMemoryState final {
 public:
  static std::pair<ManagedMemoryState*, void*> New(
      size_t size, size_t align, ManagedMemoryDestructor destructor);

  ManagedMemoryState() = delete;
  ManagedMemoryState(const ManagedMemoryState&) = delete;
  ManagedMemoryState(ManagedMemoryState&&) = delete;
  ManagedMemoryState& operator=(const ManagedMemoryState&) = delete;
  ManagedMemoryState& operator=(ManagedMemoryState&&) = delete;

  void Ref() {
    const auto reference_count =
        reference_count_.fetch_add(1, std::memory_order_relaxed);
    ABSL_ASSERT(reference_count > 0);
  }

  ABSL_MUST_USE_RESULT bool Unref() {
    const auto reference_count =
        reference_count_.fetch_sub(1, std::memory_order_seq_cst);
    ABSL_ASSERT(reference_count > 0);
    return reference_count == 1;
  }

  void Delete(void* pointer);

 private:
  explicit ManagedMemoryState(ManagedMemoryDestructor destructor)
      : reference_count_(1), destructor_(destructor) {}

  mutable std::atomic<intptr_t> reference_count_;
  ManagedMemoryDestructor destructor_;
};

// ManagedMemory implementation for T that is not derived from Data. This is
// very similar to `std::shared_ptr`.
template <typename T>
class ManagedMemory<T, false> final {
 public:
  ManagedMemory() = default;

  explicit ManagedMemory(std::nullptr_t) : ManagedMemory() {}

  ManagedMemory(const ManagedMemory& other)
      : pointer_(other.pointer_), state_(other.state_) {
    Ref();
  }

  template <typename F,
            typename = std::enable_if_t<std::is_convertible_v<F*, T*>>>
  ManagedMemory(const ManagedMemory<F, false>& other)  // NOLINT
      : pointer_(static_cast<T*>(other.pointer_)), state_(other.state_) {
    Ref();
  }

  ManagedMemory(ManagedMemory&& other) : ManagedMemory() {
    std::swap(pointer_, other.pointer_);
    std::swap(state_, other.state_);
  }

  template <typename F,
            typename = std::enable_if_t<std::is_convertible_v<F*, T*>>>
  ManagedMemory(ManagedMemory<F, false>&& other)  // NOLINT
      : pointer_(static_cast<T*>(other.pointer_)), state_(other.state_) {
    other.pointer_ = nullptr;
    other.state_ = nullptr;
  }

  ~ManagedMemory() { Unref(); }

  ManagedMemory& operator=(const ManagedMemory& other) {
    if (this != &other) {
      other.Ref();
      Unref();
      pointer_ = other.pointer_;
      state_ = other.state_;
    }
    return *this;
  }

  template <typename F>
  std::enable_if_t<std::is_convertible_v<F*, T*>, ManagedMemory&>  // NOLINT
  operator=(const ManagedMemory<F, false>& other) {
    if (this != &other) {
      other.Ref();
      Unref();
      pointer_ = static_cast<T*>(other.pointer_);
      state_ = other.state_;
    }
    return *this;
  }

  ManagedMemory& operator=(ManagedMemory&& other) {
    if (this != &other) {
      Unref();
      pointer_ = nullptr;
      state_ = nullptr;
      std::swap(pointer_, other.pointer_);
      std::swap(state_, other.state_);
    }
    return *this;
  }

  template <typename F>
  std::enable_if_t<std::is_convertible_v<F*, T*>, ManagedMemory&>  // NOLINT
  operator=(ManagedMemory<F, false>&& other) {
    if (this != &other) {
      Unref();
      pointer_ = static_cast<T*>(other.pointer_);
      state_ = other.state_;
      other.pointer_ = nullptr;
      other.state_ = nullptr;
    }
    return *this;
  }

  T* get() const ABSL_ATTRIBUTE_LIFETIME_BOUND { return pointer_; }

  T& operator*() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_ASSERT(get() != nullptr);
    return *get();
  }

  T* operator->() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_ASSERT(get() != nullptr);
    return get();
  }

  explicit operator bool() const { return get() != nullptr; }

  ABSL_MUST_USE_RESULT T* release() {
    if (pointer_ == nullptr) {
      return nullptr;
    }
    ABSL_ASSERT(state_ == nullptr);
    T* pointer = pointer_;
    pointer_ = nullptr;
    return pointer;
  }

 private:
  friend class cel::MemoryManager;

  ManagedMemory(T* pointer, ManagedMemoryState* state)
      : pointer_(pointer), state_(state) {}

  void Ref() const {
    if (state_ != nullptr) {
      state_->Ref();
    }
  }

  void Unref() const {
    if (state_ != nullptr && state_->Unref()) {
      state_->Delete(const_cast<void*>(static_cast<const void*>(get())));
    }
  }

  T* pointer_ = nullptr;
  ManagedMemoryState* state_ = nullptr;
};

template <typename T, bool D>
constexpr bool operator==(const ManagedMemory<T, D>& lhs, std::nullptr_t) {
  return !static_cast<bool>(lhs);
}

template <typename T, bool D>
constexpr bool operator==(std::nullptr_t, const ManagedMemory<T, D>& rhs) {
  return !static_cast<bool>(rhs);
}

template <typename T, bool D>
constexpr bool operator!=(const ManagedMemory<T, D>& lhs, std::nullptr_t) {
  return !operator==(lhs, nullptr);
}

template <typename T, bool D>
constexpr bool operator!=(std::nullptr_t, const ManagedMemory<T, D>& rhs) {
  return !operator==(nullptr, rhs);
}

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_INTERNAL_MANAGED_MEMORY_H_
