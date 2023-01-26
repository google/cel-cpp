// Copyright 2023 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_BASE_ALLOCATOR_H_
#define THIRD_PARTY_CEL_CPP_BASE_ALLOCATOR_H_

#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "base/memory_manager.h"

namespace cel {

// STL allocator implementation which is backed by MemoryManager.
template <typename T>
class Allocator final {
 public:
  using value_type = T;
  using pointer = T*;
  using const_pointer = const T*;
  using reference = T&;
  using const_reference = const T&;
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using propagate_on_container_move_assignment = std::true_type;
  using is_always_equal = std::false_type;

  template <typename U>
  struct rebind final {
    using other = Allocator<U>;
  };

  explicit Allocator(
      ABSL_ATTRIBUTE_LIFETIME_BOUND MemoryManager& memory_manager)
      : memory_manager_(memory_manager) {}

  pointer allocate(size_type n) {
    if (!memory_manager_.allocation_only_) {
      return static_cast<pointer>(::operator new(
          n * sizeof(T), static_cast<std::align_val_t>(alignof(T))));
    }
    return static_cast<T*>(memory_manager_.Allocate(n * sizeof(T), alignof(T)));
  }

  pointer allocate(size_type n, const void* hint) {
    static_cast<void>(hint);
    return allocate(n);
  }

  void deallocate(pointer p, size_type n) {
    if (!memory_manager_.allocation_only_) {
      ::operator delete(static_cast<void*>(p), n * sizeof(T),
                        static_cast<std::align_val_t>(alignof(T)));
    }
  }

  constexpr size_type max_size() const noexcept {
    return std::numeric_limits<size_type>::max() / sizeof(value_type);
  }

  pointer address(reference x) const noexcept { return std::addressof(x); }

  const_pointer address(const_reference x) const noexcept {
    return std::addressof(x);
  }

  void construct(pointer p, const_reference val) {
    ::new (static_cast<void*>(p)) T(val);
  }

  template <typename U, typename... Args>
  void construct(U* p, Args&&... args) {
    ::new (static_cast<void*>(p)) U(std::forward<Args>(args)...);
  }

  void destroy(pointer p) { p->~T(); }

  template <typename U>
  void destroy(U* p) {
    p->~U();
  }

  template <typename U, typename V>
  friend bool operator==(const Allocator<U>& lhs, const Allocator<V>& rhs) {
    return &lhs.memory_manager_ == &rhs.memory_manager_;
  }

  template <typename U, typename V>
  friend bool operator!=(const Allocator<U>& lhs, const Allocator<V>& rhs) {
    return &lhs.memory_manager_ != &rhs.memory_manager_;
  }

 private:
  MemoryManager& memory_manager_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_ALLOCATOR_H_
