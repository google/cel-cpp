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

#ifndef THIRD_PARTY_CEL_CPP_BASE_MEMORY_H_
#define THIRD_PARTY_CEL_CPP_BASE_MEMORY_H_

#include <cstddef>
#include <limits>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/log/die_if_null.h"
#include "base/handle.h"
#include "base/internal/data.h"
#include "base/internal/memory_manager.h"
#include "internal/rtti.h"

namespace cel {

template <typename T>
class Allocator;
class MemoryManager;
class GlobalMemoryManager;
class ArenaMemoryManager;

namespace extensions {
class ProtoMemoryManager;
}

// `UniqueRef` is similar to `std::unique_ptr`, but works with `MemoryManager`
// and is more strict to help prevent misuse. It is undefined behavior to access
// `UniqueRef` after being moved.
template <typename T>
class UniqueRef final {
 public:
  UniqueRef() = delete;

  UniqueRef(const UniqueRef<T>&) = delete;

  UniqueRef(UniqueRef<T>&& other) : ref_(other.ref_), owned_(other.owned_) {
    other.ref_ = nullptr;
    other.owned_ = false;
  }

  template <typename F,
            typename = std::enable_if_t<std::is_convertible_v<F*, T*>>>
  UniqueRef(UniqueRef<F>&& other)  // NOLINT
      : ref_(other.ref_), owned_(other.owned_) {
    other.ref_ = nullptr;
    other.owned_ = false;
  }

  ~UniqueRef() {
    if (ref_ != nullptr) {
      if (owned_) {
        delete ref_;
      } else {
        ref_->~T();
      }
    }
  }

  UniqueRef<T>& operator=(const UniqueRef<T>&) = delete;

  UniqueRef<T>& operator=(UniqueRef<T>&& other) {
    if (ABSL_PREDICT_TRUE(this != &other)) {
      if (ref_ != nullptr) {
        if (owned_) {
          delete ref_;
        } else {
          ref_->~T();
        }
      }
      ref_ = other.ref_;
      owned_ = other.owned_;
      other.ref_ = nullptr;
      other.owned_ = false;
    }
    return *this;
  }

  template <typename F,
            typename = std::enable_if_t<std::is_convertible_v<F*, T*>>>
  UniqueRef<T>& operator=(UniqueRef<F>&& other) {  // NOLINT
    if (ABSL_PREDICT_TRUE(this != &other)) {
      if (ref_ != nullptr) {
        if (owned_) {
          delete ref_;
        } else {
          ref_->~T();
        }
      }
      ref_ = other.ref_;
      owned_ = other.owned_;
      other.ref_ = nullptr;
      other.owned_ = false;
    }
    return *this;
  }

  T* operator->() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_ASSERT(ref_ != nullptr);
    ABSL_ASSUME(ref_ != nullptr);
    return ref_;
  }

  T& operator*() const ABSL_ATTRIBUTE_LIFETIME_BOUND { return get(); }

  T& get() const ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_ASSERT(ref_ != nullptr);
    ABSL_ASSUME(ref_ != nullptr);
    return *ref_;
  }

 private:
  template <typename F>
  friend class UniqueRef;
  friend class MemoryManager;

  UniqueRef(T* ref, bool owned) ABSL_ATTRIBUTE_NONNULL()
      : ref_(ABSL_DIE_IF_NULL(ref)),  // Crash OK
        owned_(owned) {}

  T* ref_;
  bool owned_;
};

template <typename T, typename... Args>
ABSL_MUST_USE_RESULT UniqueRef<T> MakeUnique(MemoryManager& memory_manager
                                                 ABSL_ATTRIBUTE_LIFETIME_BOUND,
                                             Args&&... args);

// `MemoryManager` is an abstraction over memory management that supports
// different allocation strategies.
class MemoryManager {
 public:
  ABSL_ATTRIBUTE_PURE_FUNCTION static MemoryManager& Global();

  MemoryManager(const MemoryManager&) = delete;
  MemoryManager(MemoryManager&&) = delete;

  virtual ~MemoryManager() = default;

  MemoryManager& operator=(const MemoryManager&) = delete;
  MemoryManager& operator=(MemoryManager&&) = delete;

 private:
  friend class GlobalMemoryManager;
  friend class ArenaMemoryManager;
  friend class extensions::ProtoMemoryManager;
  template <typename T>
  friend class Allocator;
  template <typename T>
  friend struct base_internal::HandleFactory;
  template <typename T, typename... Args>
  friend UniqueRef<T> MakeUnique(MemoryManager&, Args&&...);

  // Only for use by GlobalMemoryManager and ArenaMemoryManager.
  explicit MemoryManager(bool allocation_only)
      : allocation_only_(allocation_only) {}

  // Allocates and constructs `T`.
  template <typename T, typename... Args>
  Handle<T> AllocateHandle(Args&&... args)
      ABSL_ATTRIBUTE_LIFETIME_BOUND ABSL_MUST_USE_RESULT {
    static_assert(base_internal::IsDerivedHeapDataV<T>);
    if (allocation_only_) {
      T* pointer = ::new (Allocate(sizeof(T), alignof(T)))
          T(std::forward<Args>(args)...);
      if constexpr (!std::is_trivially_destructible_v<T>) {
        if constexpr (base_internal::HasIsDestructorSkippable<T>::value) {
          if (!T::IsDestructorSkippable(*pointer)) {
            OwnDestructor(pointer,
                          &base_internal::MemoryManagerDestructor<T>::Destruct);
          }
        } else {
          OwnDestructor(pointer,
                        &base_internal::MemoryManagerDestructor<T>::Destruct);
        }
      }
      base_internal::Metadata::SetArenaAllocated(*pointer);
      return Handle<T>(base_internal::kInPlaceArenaAllocated, *pointer);
    }
    T* pointer = new T(std::forward<Args>(args)...);
    base_internal::Metadata::SetReferenceCounted(*pointer);
    return Handle<T>(base_internal::kInPlaceReferenceCounted, *pointer);
  }

  template <typename T, typename... Args>
  UniqueRef<T> AllocateUnique(Args&&... args)
      ABSL_ATTRIBUTE_LIFETIME_BOUND ABSL_MUST_USE_RESULT {
    static_assert(!base_internal::IsDataV<T>);
    T* ptr;
    if (allocation_only_) {
      ptr = ::new (Allocate(sizeof(T), alignof(T)))
          T(std::forward<Args>(args)...);
    } else {
      ptr = new T(std::forward<Args>(args)...);
    }
    return UniqueRef<T>(ptr, !allocation_only_);
  }

  // These are virtual private, ensuring only `MemoryManager` calls these.

  // Allocates memory of at least size `size` in bytes that is at least as
  // aligned as `align`.
  virtual void* Allocate(size_t size, size_t align) = 0;

  // Registers a destructor to be run upon destruction of the memory management
  // implementation.
  virtual void OwnDestructor(void* pointer, void (*destruct)(void*)) = 0;

  virtual internal::TypeInfo TypeId() const { return internal::TypeInfo(); }

  const bool allocation_only_;
};

// Allocates and constructs `T`.
template <typename T, typename... Args>
UniqueRef<T> MakeUnique(MemoryManager& memory_manager, Args&&... args) {
  return memory_manager.AllocateUnique<T>(std::forward<Args>(args)...);
}

// Base class for all arena-based memory managers.
class ArenaMemoryManager : public MemoryManager {
 public:
  // Returns the default implementation of an arena-based memory manager. In
  // most cases it should be good enough, however you should not rely on its
  // performance characteristics.
  static std::unique_ptr<ArenaMemoryManager> Default();

 protected:
  ArenaMemoryManager() : ArenaMemoryManager(true) {}

 private:
  friend class extensions::ProtoMemoryManager;

  // Private so that only ProtoMemoryManager can use it for legacy reasons. All
  // other derivations of ArenaMemoryManager should be allocation-only.
  explicit ArenaMemoryManager(bool allocation_only)
      : MemoryManager(allocation_only) {}
};

// STL allocator implementation which is backed by MemoryManager.
template <typename T>
class Allocator {
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
      : memory_manager_(memory_manager),
        allocation_only_(memory_manager.allocation_only_) {}

  Allocator(const Allocator&) = default;

  template <typename U>
  Allocator(const Allocator<U>& other)  // NOLINT(google-explicit-constructor)
      : memory_manager_(other.memory_manager_),
        allocation_only_(other.allocation_only_) {}

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
    if (!allocation_only_) {
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

  template <typename U>
  bool operator==(const Allocator<U>& rhs) const {
    return &memory_manager_ == &rhs.memory_manager_;
  }

  template <typename U>
  bool operator!=(const Allocator<U>& rhs) const {
    return &memory_manager_ != &rhs.memory_manager_;
  }

 private:
  template <typename U>
  friend class Allocator;

  MemoryManager& memory_manager_;
  // Ugh. This is here because of legacy behavior. MemoryManager& is guaranteed
  // to exist during allocation, but not necessarily during deallocation. So we
  // store the member variable from MemoryManager. This can go away once
  // CelValue and friends are entirely gone and everybody is instantiating their
  // own MemoryManager.
  bool allocation_only_;
};

namespace base_internal {

template <typename T>
template <typename F, typename... Args>
std::enable_if_t<IsDerivedHeapDataV<F>, Handle<T>> HandleFactory<T>::Make(
    MemoryManager& memory_manager, Args&&... args) {
  static_assert(std::is_base_of_v<T, F>, "F is not derived from T");
#if defined(__cpp_lib_is_pointer_interconvertible) && \
    __cpp_lib_is_pointer_interconvertible >= 201907L
  // Only available in C++20.
  static_assert(std::is_pointer_interconvertible_base_of_v<Data, F>,
                "F must be pointer interconvertible to Data");
#endif
  return memory_manager.AllocateHandle<F>(std::forward<Args>(args)...);
}

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_MEMORY_H_
