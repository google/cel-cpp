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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_MEMORY_H_
#define THIRD_PARTY_CEL_CPP_COMMON_MEMORY_H_

#include <cstddef>
#include <memory>
#include <new>
#include <ostream>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/config.h"  // IWYU pragma: keep
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/log/die_if_null.h"
#include "absl/meta/type_traits.h"
#include "absl/numeric/bits.h"
#include "common/casting.h"
#include "common/internal/reference_count.h"
#include "common/native_type.h"
#include "internal/exceptions.h"
#include "internal/new.h"
#include "google/protobuf/arena.h"

namespace cel {

// MemoryManagement is an enumeration of supported memory management forms
// underlying `cel::MemoryManager`.
enum class MemoryManagement {
  // Region-based (a.k.a. arena). Memory is allocated in fixed size blocks and
  // deallocated all at once upon destruction of the `cel::MemoryManager`.
  kPooling = 1,
  // Reference counting. Memory is allocated with an associated reference
  // counter. When the reference counter hits 0, it is deallocated.
  kReferenceCounting,
};

std::ostream& operator<<(std::ostream& out, MemoryManagement memory_management);

template <typename T>
using IsArenaConstructible = google::protobuf::Arena::is_arena_constructable<T>;
template <typename T>
using IsArenaDestructorSkippable =
    absl::conjunction<IsArenaConstructible<T>,
                      google::protobuf::Arena::is_destructor_skippable<T>>;

template <typename T = void>
class Allocator;
template <typename T>
class ABSL_ATTRIBUTE_TRIVIAL_ABI Shared;
template <typename T>
class ABSL_ATTRIBUTE_TRIVIAL_ABI SharedView;
template <typename T>
class ABSL_ATTRIBUTE_TRIVIAL_ABI Unique;
template <typename T>
struct EnableSharedFromThis;

class ABSL_ATTRIBUTE_TRIVIAL_ABI MemoryManager;
class ABSL_ATTRIBUTE_TRIVIAL_ABI MemoryManagerRef;
class ReferenceCountingMemoryManager;
class PoolingMemoryManager;
struct PoolingMemoryManagerVirtualTable;
class PoolingMemoryManagerVirtualDispatcher;

namespace common_internal {
template <typename T>
T* GetPointer(const Shared<T>& shared);
template <typename T>
const ReferenceCount* GetReferenceCount(const Shared<T>& shared);
template <typename T>
Shared<T> MakeShared(AdoptRef, T* value, const ReferenceCount* refcount);
template <typename T>
Shared<T> MakeShared(T* value, const ReferenceCount* refcount);
template <typename T>
T* GetPointer(SharedView<T> shared);
template <typename T>
const ReferenceCount* GetReferenceCount(SharedView<T> shared);
template <typename T>
SharedView<T> MakeSharedView(T* value, const ReferenceCount* refcount);
}  // namespace common_internal

template <typename To, typename From>
Shared<To> StaticCast(const Shared<From>& from);
template <typename To, typename From>
Shared<To> StaticCast(Shared<From>&& from);
template <typename To, typename From>
SharedView<To> StaticCast(SharedView<From> from);

// `Allocator<>` is a type-erased vocabulary type capable of performing
// allocation/deallocation and construction/destruction using memory owned by
// `google::protobuf::Arena` or `operator new`.
template <>
class Allocator<void> {
 public:
  using size_type = size_t;
  using difference_type = ptrdiff_t;
  using propagate_on_container_copy_assignment = std::true_type;
  using propagate_on_container_move_assignment = std::true_type;
  using propagate_on_container_swap = std::true_type;

  Allocator() = delete;

  Allocator(const Allocator&) = default;
  Allocator& operator=(const Allocator&) = delete;

  Allocator(std::nullptr_t) = delete;

  template <typename U, typename = std::enable_if_t<!std::is_void_v<U>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr Allocator(const Allocator<U>& other) noexcept
      : arena_(other.arena()) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  constexpr Allocator(absl::Nullable<google::protobuf::Arena*> arena) noexcept
      : arena_(arena) {}

  absl::Nullable<google::protobuf::Arena*> arena() const noexcept { return arena_; }

  // Allocates at least `nbytes` bytes with a minimum alignment of `alignment`
  // from the underlying memory resource. When the underlying memory resource is
  // `operator new`, `deallocate_bytes` must be called at some point, otherwise
  // calling `deallocate_bytes` is optional. The caller must not pass an object
  // constructed in the return memory to `delete_object`, doing so is undefined
  // behavior.
  ABSL_MUST_USE_RESULT void* allocate_bytes(
      size_t nbytes, size_t alignment = alignof(std::max_align_t)) {
    ABSL_DCHECK(absl::has_single_bit(alignment));
    if (nbytes == 0) {
      return nullptr;
    }
    if (auto* arena = this->arena(); arena != nullptr) {
      return arena->AllocateAligned(nbytes, alignment);
    }
    return internal::AlignedNew(nbytes,
                                static_cast<std::align_val_t>(alignment));
  }

  // Deallocates memory previously returned by `allocate_bytes`.
  void deallocate_bytes(void* p, size_t nbytes,
                        size_t alignment = alignof(std::max_align_t)) noexcept {
    ABSL_DCHECK((p == nullptr && nbytes == 0) || (p != nullptr && nbytes != 0));
    ABSL_DCHECK(absl::has_single_bit(alignment));
    if (arena() == nullptr) {
      internal::SizedAlignedDelete(p, nbytes,
                                   static_cast<std::align_val_t>(alignment));
    }
  }

  // Allocates memory suitable for an object of type `T` and constructs the
  // object by forwarding the provided arguments. If the underlying memory
  // resource is `operator new` is false, `delete_object` must eventually be
  // called.
  template <typename T, typename... Args>
  ABSL_MUST_USE_RESULT T* new_object(Args&&... args) {
    T* object = google::protobuf::Arena::Create<std::remove_const_t<T>>(
        arena(), std::forward<Args>(args)...);
    if constexpr (IsArenaConstructible<T>::value) {
      ABSL_DCHECK_EQ(object->GetArena(), arena());
    }
    return object;
  }

  // Destructs the object of type `T` located at address `p` and deallocates the
  // memory, `p` must have been previously returned by `new_object`.
  template <typename T>
  void delete_object(T* p) noexcept {
    ABSL_DCHECK(p != nullptr);
    if constexpr (IsArenaConstructible<T>::value) {
      ABSL_DCHECK_EQ(p->GetArena(), arena());
    }
    if (arena() == nullptr) {
      delete p;
    }
  }

  void delete_object(std::nullptr_t) = delete;

 private:
  template <typename U>
  friend class Allocator;

  absl::Nullable<google::protobuf::Arena*> arena_;
};

// `Allocator<T>` is an extension of `Allocator<>` which adheres to the named
// C++ requirements for `Allocator`, allowing it to be used in places which
// accept custom STL allocators.
template <typename T>
class Allocator : public Allocator<void> {
 public:
  static_assert(!std::is_const_v<T>, "T must not be const qualified");
  static_assert(!std::is_volatile_v<T>, "T must not be volatile qualified");
  static_assert(std::is_object_v<T>, "T must be an object type");

  using value_type = T;
  using pointer = value_type*;
  using const_pointer = const value_type*;
  using reference = value_type&;
  using const_reference = const value_type&;

  using Allocator<void>::Allocator;

  pointer allocate(size_type n, const void* /*hint*/ = nullptr) {
    return arena() != nullptr
               ? static_cast<pointer>(
                     arena()->AllocateAligned(n * sizeof(T), alignof(T)))
               : static_cast<pointer>(internal::AlignedNew(
                     n * sizeof(T), static_cast<std::align_val_t>(alignof(T))));
  }

  void deallocate(pointer p, size_type n) noexcept {
    if (arena() == nullptr) {
      internal::SizedAlignedDelete(p, n * sizeof(T),
                                   static_cast<std::align_val_t>(alignof(T)));
    }
  }

  template <typename U, typename... Args>
  void construct(U* p, Args&&... args) {
    static_assert(std::is_same_v<T*, U*>);
    static_assert(!IsArenaConstructible<T>::value);
    ::new (static_cast<void*>(p)) T(std::forward<Args>(args)...);
  }

  template <typename U>
  void destroy(U* p) noexcept {
    static_assert(std::is_same_v<T*, U*>);
    static_assert(!IsArenaConstructible<T>::value);
    p->~T();
  }
};

template <typename T, typename U>
inline bool operator==(Allocator<T> lhs, Allocator<U> rhs) noexcept {
  return lhs.arena() == rhs.arena();
}

template <typename T, typename U>
inline bool operator!=(Allocator<T> lhs, Allocator<U> rhs) noexcept {
  return !operator==(lhs, rhs);
}

inline Allocator<> NewDeleteAllocator() noexcept {
  return Allocator<>(static_cast<google::protobuf::Arena*>(nullptr));
}

template <typename T>
inline Allocator<T> NewDeleteAllocatorFor() noexcept {
  static_assert(!std::is_void_v<T>);
  return Allocator<T>(static_cast<google::protobuf::Arena*>(nullptr));
}

inline Allocator<> ArenaAllocator(
    absl::Nonnull<google::protobuf::Arena*> arena) noexcept {
  return Allocator<>(arena);
}

template <typename T>
inline Allocator<T> ArenaAllocatorFor(
    absl::Nonnull<google::protobuf::Arena*> arena) noexcept {
  static_assert(!std::is_void_v<T>);
  return Allocator<T>(arena);
}

// `Shared` points to an object allocated in memory which is managed by a
// `MemoryManager`. The pointed to object is valid so long as the managing
// `MemoryManager` is alive and one or more valid `Shared` exist pointing to the
// object.
//
// IMPLEMENTATION DETAILS:
// `Shared` is similar to `std::shared_ptr`, except that it works for
// region-based memory management as well. In that case the pointer to the
// reference count is `nullptr`.
template <typename T>
class ABSL_ATTRIBUTE_TRIVIAL_ABI Shared final {
 public:
  Shared() = default;

  Shared(const Shared& other)
      : value_(other.value_), refcount_(other.refcount_) {
    common_internal::StrongRef(refcount_);
  }

  Shared(Shared&& other) noexcept
      : value_(other.value_), refcount_(other.refcount_) {
    other.value_ = nullptr;
    other.refcount_ = nullptr;
  }

  template <
      typename U,
      typename = std::enable_if_t<std::conjunction_v<
          std::negation<std::is_same<U, T>>, std::is_convertible<U*, T*>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Shared(const Shared<U>& other)
      : value_(other.value_), refcount_(other.refcount_) {
    common_internal::StrongRef(refcount_);
  }

  template <
      typename U,
      typename = std::enable_if_t<std::conjunction_v<
          std::negation<std::is_same<U, T>>, std::is_convertible<U*, T*>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Shared(Shared<U>&& other) noexcept
      : value_(other.value_), refcount_(other.refcount_) {
    other.value_ = nullptr;
    other.refcount_ = nullptr;
  }

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  explicit Shared(SharedView<U> other);

  // An aliasing constructor. The resulting `Shared` shares ownership
  // information with `alias`, but holds an unmanaged pointer to `T`.
  //
  // Usage:
  //   Shared<Object> object;
  //   Shared<Member> member = Shared<Member>(object, &object->member);
  template <typename U>
  Shared(const Shared<U>& alias, T* ptr)
      : value_(ptr), refcount_(alias.refcount_) {
    common_internal::StrongRef(refcount_);
  }

  // An aliasing constructor. The resulting `Shared` shares ownership
  // information with `alias`, but holds an unmanaged pointer to `T`.
  template <typename U>
  Shared(Shared<U>&& alias, T* ptr) noexcept
      : value_(ptr), refcount_(alias.refcount_) {
    alias.value_ = nullptr;
    alias.refcount_ = nullptr;
  }

  ~Shared() { common_internal::StrongUnref(refcount_); }

  Shared& operator=(const Shared& other) {
    common_internal::StrongRef(other.refcount_);
    common_internal::StrongUnref(refcount_);
    value_ = other.value_;
    refcount_ = other.refcount_;
    return *this;
  }

  Shared& operator=(Shared&& other) noexcept {
    common_internal::StrongUnref(refcount_);
    value_ = other.value_;
    refcount_ = other.refcount_;
    other.value_ = nullptr;
    other.refcount_ = nullptr;
    return *this;
  }

  template <
      typename U,
      typename = std::enable_if_t<std::conjunction_v<
          std::negation<std::is_same<U, T>>, std::is_convertible<U*, T*>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Shared& operator=(const Shared<U>& other) {
    common_internal::StrongRef(other.refcount_);
    common_internal::StrongUnref(refcount_);
    value_ = other.value_;
    refcount_ = other.refcount_;
    return *this;
  }

  template <
      typename U,
      typename = std::enable_if_t<std::conjunction_v<
          std::negation<std::is_same<U, T>>, std::is_convertible<U*, T*>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Shared& operator=(Shared<U>&& other) noexcept {
    common_internal::StrongUnref(refcount_);
    value_ = other.value_;
    refcount_ = other.refcount_;
    other.value_ = nullptr;
    other.refcount_ = nullptr;
    return *this;
  }

  template <typename U = T, typename = std::enable_if_t<!std::is_void_v<U>>>
  U& operator*() const noexcept ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(!IsEmpty());
    return *value_;
  }

  absl::Nonnull<T*> operator->() const noexcept ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(!IsEmpty());
    return value_;
  }

  explicit operator bool() const { return !IsEmpty(); }

  friend constexpr void swap(Shared& lhs, Shared& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
    swap(lhs.refcount_, rhs.refcount_);
  }

 private:
  template <typename U>
  friend class Shared;
  template <typename U>
  friend class SharedView;
  template <typename To, typename From>
  friend Shared<To> StaticCast(Shared<From>&& from);
  template <typename U>
  friend U* common_internal::GetPointer(const Shared<U>& shared);
  template <typename U>
  friend const common_internal::ReferenceCount*
  common_internal::GetReferenceCount(const Shared<U>& shared);
  template <typename U>
  friend Shared<U> common_internal::MakeShared(
      common_internal::AdoptRef, U* value,
      const common_internal::ReferenceCount* refcount);

  Shared(common_internal::AdoptRef, T* value,
         const common_internal::ReferenceCount* refcount) noexcept
      : value_(value), refcount_(refcount) {}

  Shared(T* value, const common_internal::ReferenceCount* refcount) noexcept
      : value_(value), refcount_(refcount) {
    common_internal::StrongRef(refcount_);
  }

  bool IsEmpty() const noexcept { return value_ == nullptr; }

  T* value_ = nullptr;
  const common_internal::ReferenceCount* refcount_ = nullptr;
};

template <typename To, typename From>
inline Shared<To> StaticCast(const Shared<From>& from) {
  return common_internal::MakeShared(
      static_cast<To*>(common_internal::GetPointer(from)),
      common_internal::GetReferenceCount(from));
}

template <typename To, typename From>
inline Shared<To> StaticCast(Shared<From>&& from) {
  To* value = static_cast<To*>(from.value_);
  const auto* refcount = from.refcount_;
  from.value_ = nullptr;
  from.refcount_ = nullptr;
  return Shared<To>(common_internal::kAdoptRef, value, refcount);
}

template <typename T>
struct NativeTypeTraits<Shared<T>> final {
  static bool SkipDestructor(const Shared<T>& shared) {
    return common_internal::GetReferenceCount(shared) == nullptr;
  }
};

// `SharedView` is a wrapper on top of `Shared`. It is roughly equivalent to
// `const Shared<T>&` and can be used in places where it is not feasible to use
// `const Shared<T>&` directly. This is also analygous to
// `std::reference_wrapper<const Shared<T>>>` and is intended to be used under
// the same cirumstances.
template <typename T>
class ABSL_ATTRIBUTE_TRIVIAL_ABI SharedView final {
 public:
  SharedView() = default;
  SharedView(const SharedView&) = default;
  SharedView& operator=(const SharedView&) = default;

  template <
      typename U,
      typename = std::enable_if_t<std::conjunction_v<
          std::negation<std::is_same<U, T>>, std::is_convertible<U*, T*>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  SharedView(const SharedView<U>& other)
      : value_(other.value_), refcount_(other.refcount_) {}

  template <
      typename U,
      typename = std::enable_if_t<std::conjunction_v<
          std::negation<std::is_same<U, T>>, std::is_convertible<U*, T*>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  SharedView(SharedView<U>&& other) noexcept
      : value_(other.value_), refcount_(other.refcount_) {}

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  SharedView(const Shared<U>& other ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept
      : value_(other.value_), refcount_(other.refcount_) {}

  template <typename U>
  SharedView(SharedView<U> alias, T* ptr)
      : value_(ptr), refcount_(alias.refcount_) {}

  template <
      typename U,
      typename = std::enable_if_t<std::conjunction_v<
          std::negation<std::is_same<U, T>>, std::is_convertible<U*, T*>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  SharedView& operator=(const SharedView<U>& other) {
    value_ = other.value_;
    refcount_ = other.refcount_;
    return *this;
  }

  template <
      typename U,
      typename = std::enable_if_t<std::conjunction_v<
          std::negation<std::is_same<U, T>>, std::is_convertible<U*, T*>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  SharedView& operator=(SharedView<U>&& other) noexcept {
    value_ = other.value_;
    refcount_ = other.refcount_;
    return *this;
  }

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  SharedView& operator=(
      const Shared<U>& other ABSL_ATTRIBUTE_LIFETIME_BOUND) noexcept {
    value_ = other.value_;
    refcount_ = other.refcount_;
    return *this;
  }

  template <typename U,
            typename = std::enable_if_t<std::is_convertible_v<U*, T*>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  SharedView& operator=(Shared<U>&&) = delete;

  template <typename U = T, typename = std::enable_if_t<!std::is_void_v<U>>>
  U& operator*() const noexcept ABSL_ATTRIBUTE_LIFETIME_BOUND {
    ABSL_DCHECK(!IsEmpty());
    return *value_;
  }

  absl::Nonnull<T*> operator->() const noexcept {
    ABSL_DCHECK(!IsEmpty());
    return value_;
  }

  explicit operator bool() const { return !IsEmpty(); }

  friend constexpr void swap(SharedView& lhs, SharedView& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
    swap(lhs.refcount_, rhs.refcount_);
  }

 private:
  template <typename U>
  friend class Shared;
  template <typename U>
  friend class SharedView;
  template <typename U>
  friend U* common_internal::GetPointer(SharedView<U> shared);
  template <typename U>
  friend const common_internal::ReferenceCount*
  common_internal::GetReferenceCount(SharedView<U> shared);
  template <typename U>
  friend SharedView<U> common_internal::MakeSharedView(
      U* value, const common_internal::ReferenceCount* refcount);

  SharedView(T* value, const common_internal::ReferenceCount* refcount)
      : value_(value), refcount_(refcount) {}

  bool IsEmpty() const noexcept { return value_ == nullptr; }

  T* value_ = nullptr;
  const common_internal::ReferenceCount* refcount_ = nullptr;
};

template <typename T>
template <typename U, typename>
Shared<T>::Shared(SharedView<U> other)
    : value_(other.value_), refcount_(other.refcount_) {
  StrongRef(refcount_);
}

template <typename To, typename From>
SharedView<To> StaticCast(SharedView<From> from) {
  return common_internal::MakeSharedView(
      static_cast<To*>(common_internal::GetPointer(from)),
      common_internal::GetReferenceCount(from));
}

template <typename T>
struct EnableSharedFromThis
    : public virtual common_internal::ReferenceCountFromThis {
 protected:
  Shared<T> shared_from_this() noexcept {
    auto* const derived = static_cast<T*>(this);
    auto* const refcount = common_internal::GetReferenceCountForThat(*this);
    return common_internal::MakeShared(derived, refcount);
  }

  Shared<const T> shared_from_this() const noexcept {
    auto* const derived = static_cast<const T*>(this);
    auto* const refcount = common_internal::GetReferenceCountForThat(*this);
    return common_internal::MakeShared(derived, refcount);
  }
};

// `Unique` points to an object allocated in memory managed by a
// `MemoryManager`. The pointed to object is valid so long as the managing
// `MemoryManager` is alive and `Unique` is alive.
template <typename T>
class ABSL_ATTRIBUTE_TRIVIAL_ABI Unique final {
 public:
  static_assert(!std::is_array_v<T>);

  Unique() = default;
  Unique(const Unique&) = delete;
  Unique& operator=(const Unique&) = delete;

  Unique(Unique&& other) noexcept
      : ptr_(other.ptr_), memory_management_(other.memory_management_) {
    other.ptr_ = nullptr;
  }

  template <typename U,
            typename = std::enable_if_t<std::conjunction_v<
                std::negation<std::is_same<U, T>>, std::is_convertible<U*, T*>,
                std::is_polymorphic<std::remove_const_t<U>>,
                std::is_polymorphic<std::remove_const_t<T>>,
                std::is_base_of<std::remove_const_t<T>, std::remove_const_t<U>>,
                std::has_virtual_destructor<std::remove_const_t<T>>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Unique(Unique<U>&& other) noexcept
      : ptr_(other.ptr_), memory_management_(other.memory_management_) {
    other.ptr_ = nullptr;
  }

  ~Unique() { Delete(); }

  Unique& operator=(Unique&& other) noexcept {
    Delete();
    ptr_ = other.ptr_;
    memory_management_ = other.memory_management_;
    other.ptr_ = nullptr;
    return *this;
  }

  template <typename U, typename = std::enable_if_t<std::conjunction_v<
                            std::negation<std::is_same<U, T>>,
                            std::is_same<U, std::remove_const_t<T>>,
                            std::negation<std::is_const<U>>>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  Unique& operator=(Unique<U>&& other) noexcept {
    Delete();
    ptr_ = other.ptr_;
    memory_management_ = other.memory_management_;
    other.ptr_ = nullptr;
    return *this;
  }

  T& operator*() const noexcept {
    ABSL_DCHECK(!IsEmpty());
    return *ptr_;
  }

  absl::Nonnull<T*> operator->() const noexcept {
    ABSL_DCHECK(!IsEmpty());
    return ptr_;
  }

  explicit operator bool() const { return !IsEmpty(); }

  friend void swap(Unique& lhs, Unique& rhs) noexcept {
    using std::swap;
    swap(lhs.ptr_, rhs.ptr_);
    swap(lhs.memory_management_, rhs.memory_management_);
  }

 private:
  friend class ReferenceCountingMemoryManager;
  friend class PoolingMemoryManager;
  template <typename U>
  friend class Unique;

  Unique(T* ptr, MemoryManagement memory_management) noexcept
      : ptr_(ptr), memory_management_(memory_management) {}

  void Delete() noexcept {
    if (ptr_ != nullptr) {
      switch (memory_management_) {
        case MemoryManagement::kPooling:
          ptr_->~T();
          break;
        case MemoryManagement::kReferenceCounting:
          delete ptr_;
          break;
      }
    }
  }

  bool IsEmpty() const noexcept { return ptr_ == nullptr; }

  T* ptr_ = nullptr;
  MemoryManagement memory_management_ = MemoryManagement::kPooling;
};

// `ReferenceCountingMemoryManager` is a `MemoryManager` which employs automatic
// memory management through reference counting.
class ReferenceCountingMemoryManager final {
 public:
  ReferenceCountingMemoryManager(const ReferenceCountingMemoryManager&) =
      delete;
  ReferenceCountingMemoryManager(ReferenceCountingMemoryManager&&) = delete;
  ReferenceCountingMemoryManager& operator=(
      const ReferenceCountingMemoryManager&) = delete;
  ReferenceCountingMemoryManager& operator=(ReferenceCountingMemoryManager&&) =
      delete;

 private:
  template <typename T, typename... Args>
  static ABSL_MUST_USE_RESULT Shared<T> MakeShared(Args&&... args) {
    using U = std::remove_const_t<T>;
    U* ptr;
    common_internal::ReferenceCount* refcount;
    std::tie(ptr, refcount) =
        common_internal::MakeReferenceCount<U>(std::forward<Args>(args)...);
    return common_internal::MakeShared(common_internal::kAdoptRef,
                                       static_cast<T*>(ptr), refcount);
  }

  template <typename T, typename... Args>
  static ABSL_MUST_USE_RESULT Unique<T> MakeUnique(Args&&... args) {
    using U = std::remove_const_t<T>;
    return Unique<T>(static_cast<T*>(new U(std::forward<Args>(args)...)),
                     MemoryManagement::kReferenceCounting);
  }

  static void* Allocate(size_t size, size_t alignment);

  static bool Deallocate(void* ptr, size_t size, size_t alignment) noexcept;

  explicit ReferenceCountingMemoryManager() = default;

  friend class MemoryManager;
  friend class MemoryManagerRef;
};

// `PoolingMemoryManager` is a `MemoryManager` which employs automatic
// memory management through memory pooling.
class PoolingMemoryManager {
 public:
  virtual ~PoolingMemoryManager() = default;

  template <typename T, typename... Args>
  ABSL_MUST_USE_RESULT Shared<T> MakeShared(Args&&... args) {
    using U = std::remove_const_t<T>;
    U* ptr = nullptr;
    void* addr = Allocate(sizeof(U), alignof(U));
    CEL_INTERNAL_TRY {
      ptr = ::new (addr) U(std::forward<Args>(args)...);
      if constexpr (!std::is_trivially_destructible_v<U>) {
        if (!NativeType::SkipDestructor(*ptr)) {
          CEL_INTERNAL_TRY { OwnCustomDestructor(ptr, &DefaultDestructor<U>); }
          CEL_INTERNAL_CATCH_ANY {
            ptr->~U();
            CEL_INTERNAL_RETHROW;
          }
        }
      }
      if constexpr (std::is_base_of_v<common_internal::ReferenceCountFromThis,
                                      U>) {
        common_internal::SetReferenceCountForThat(*ptr, nullptr);
      }
    }
    CEL_INTERNAL_CATCH_ANY {
      Deallocate(addr, sizeof(U), alignof(U));
      CEL_INTERNAL_RETHROW;
    }
    return common_internal::MakeShared(common_internal::kAdoptRef,
                                       static_cast<T*>(ptr), nullptr);
  }

  template <typename T, typename... Args>
  ABSL_MUST_USE_RESULT Unique<T> MakeUnique(Args&&... args) {
    using U = std::remove_const_t<T>;
    U* ptr = nullptr;
    void* addr = Allocate(sizeof(U), alignof(U));
    CEL_INTERNAL_TRY { ptr = ::new (addr) U(std::forward<Args>(args)...); }
    CEL_INTERNAL_CATCH_ANY {
      Deallocate(addr, sizeof(U), alignof(U));
      CEL_INTERNAL_RETHROW;
    }
    return Unique<T>(static_cast<T*>(ptr), MemoryManagement::kPooling);
  }

  // Allocates memory directly from the allocator used by this memory manager.
  // If `memory_management()` returns `MemoryManagement::kReferenceCounting`,
  // this allocation *must* be explicitly deallocated at some point via
  // `Deallocate`. Otherwise deallocation is optional.
  ABSL_MUST_USE_RESULT void* Allocate(size_t size, size_t alignment) {
    ABSL_DCHECK(absl::has_single_bit(alignment))
        << "alignment must be a power of 2";
    if (size == 0) {
      return nullptr;
    }
    return AllocateImpl(size, alignment);
  }

  // Attempts to deallocate memory previously allocated via `Allocate`, `size`
  // and `alignment` must match the values from the previous call to `Allocate`.
  // Returns `true` if the deallocation was successful and additional calls to
  // `Allocate` may re-use the memory, `false` otherwise. Returns `false` if
  // given `nullptr`.
  bool Deallocate(void* ptr, size_t size, size_t alignment) noexcept {
    ABSL_DCHECK(absl::has_single_bit(alignment))
        << "alignment must be a power of 2";
    if (ptr == nullptr) {
      ABSL_DCHECK_EQ(size, size_t(0));
      return false;
    }
    ABSL_DCHECK_GT(size, size_t(0));
    return DeallocateImpl(ptr, size, alignment);
  }

  // Registers a custom destructor to be run upon destruction of the memory
  // management implementation. Return value is always `true`, indicating that
  // the destructor may be called at some point in the future.
  bool OwnCustomDestructor(void* object,
                           absl::Nonnull<void (*)(void*)> destruct) {
    ABSL_DCHECK(destruct != nullptr);
    OwnCustomDestructorImpl(object, destruct);
    return true;
  }

 private:
  friend class MemoryManager;
  friend class MemoryManagerRef;
  friend struct NativeTypeTraits<PoolingMemoryManager>;

  template <typename T>
  static void DefaultDestructor(void* ptr) {
    static_assert(!std::is_trivially_destructible_v<T>);
    static_cast<T*>(ptr)->~T();
  }

  // These are virtual private, ensuring only `MemoryManager` calls these.

  virtual absl::Nonnull<void*> AllocateImpl(size_t size, size_t align) = 0;

  virtual bool DeallocateImpl(absl::Nonnull<void*>, size_t, size_t) noexcept {
    return false;
  }

  // Registers a destructor to be run upon destruction of the memory management
  // implementation.
  virtual void OwnCustomDestructorImpl(
      absl::Nonnull<void*> object, absl::Nonnull<void (*)(void*)> destruct) = 0;

  virtual NativeTypeId GetNativeTypeId() const noexcept = 0;
};

template <>
struct NativeTypeTraits<PoolingMemoryManager> final {
  static NativeTypeId Id(const PoolingMemoryManager& memory_manager) {
    return memory_manager.GetNativeTypeId();
  }
};

template <typename T>
struct NativeTypeTraits<
    T, std::enable_if_t<std::conjunction_v<
           std::is_base_of<PoolingMemoryManager, T>,
           std::negation<std::is_same<T, PoolingMemoryManager>>>>>
    final {
  static NativeTypeId Id(const PoolingMemoryManager& memory_manager) {
    return NativeTypeTraits<PoolingMemoryManager>::Id(memory_manager);
  }
};

template <typename To, typename From>
struct CastTraits<To, From,
                  EnableIfSubsumptionCastable<To, From, PoolingMemoryManager>>
    : SubsumptionCastTraits<To, From> {};

// Creates a new `PoolingMemoryManager` which is thread-compatible.
absl::Nonnull<std::unique_ptr<PoolingMemoryManager>>
NewThreadCompatiblePoolingMemoryManager();

// `PoolingMemoryManagerVirtualTable` describes an implementation of
// `PoolingMemoryManager` without inheriting from it. This allows adapting
// other implementations to the `PoolingMemoryManager` interface without having
// to directly inherit from it, thus avoiding an unnecessary heap allocation.
struct PoolingMemoryManagerVirtualTable final {
  using AllocatePtr = absl::Nonnull<void*> (*)(absl::Nonnull<void*>, size_t,
                                               size_t);
  using DeallocatePtr = bool (*)(absl::Nonnull<void*>, absl::Nonnull<void*>,
                                 size_t, size_t) noexcept;
  using OwnCustomDestructorPtr = void (*)(absl::Nonnull<void*>,
                                          absl::Nonnull<void*>,
                                          absl::Nonnull<void (*)(void*)>);

  // NOLINTBEGIN(google3-readability-class-member-naming)
  const cel::NativeTypeId NativeTypeId;
  const AllocatePtr Allocate;
  const DeallocatePtr Deallocate;
  const OwnCustomDestructorPtr OwnCustomDestructor;
  // NOLINTEND(google3-readability-class-member-naming)
};

// `PoolingMemoryManagerVirtualDispatcher` adapts
// `PoolingMemoryManagerVirtualTable` and the instance it describes to
// implement `PoolingMemoryManager`. It should only be used by
// `MemoryManagerRef`.
class PoolingMemoryManagerVirtualDispatcher final
    : public PoolingMemoryManager {
 public:
  absl::Nonnull<const PoolingMemoryManagerVirtualTable*> vtable() const {
    return vtable_;
  }

  absl::Nonnull<void*> callee() const { return callee_; }

 private:
  friend class MemoryManagerRef;
  friend struct CompositionTraits<MemoryManagerRef>;

  explicit PoolingMemoryManagerVirtualDispatcher(
      absl::Nonnull<const PoolingMemoryManagerVirtualTable*> vtable,
      absl::Nonnull<void*> callee)
      : vtable_(ABSL_DIE_IF_NULL(vtable)),   // Crash OK
        callee_(ABSL_DIE_IF_NULL(callee)) {  // Crash OK
    ABSL_DCHECK(vtable_->NativeTypeId != NativeTypeId());
    ABSL_DCHECK(vtable_->Allocate != nullptr);
    ABSL_DCHECK(vtable_->Deallocate != nullptr);
    ABSL_DCHECK(vtable_->OwnCustomDestructor != nullptr);
  }

  absl::Nonnull<void*> AllocateImpl(size_t size, size_t align) override {
    return vtable()->Allocate(callee(), size, align);
  }

  bool DeallocateImpl(absl::Nonnull<void*> pointer, size_t size,
                      size_t align) noexcept override {
    return vtable()->Deallocate(callee(), pointer, size, align);
  }

  // Registers a destructor to be run upon destruction of the memory management
  // implementation.
  void OwnCustomDestructorImpl(
      void* object, absl::Nonnull<void (*)(void*)> destruct) override {
    vtable()->OwnCustomDestructor(callee(), object, destruct);
  }

  NativeTypeId GetNativeTypeId() const noexcept override {
    return vtable()->NativeTypeId;
  }

  absl::Nonnull<const PoolingMemoryManagerVirtualTable*> vtable_;
  absl::Nonnull<void*> callee_;
};

// `MemoryManager` is an abstraction for supporting automatic memory management.
// All objects created by the `MemoryManager` have a lifetime governed by the
// underlying memory management strategy. Currently `MemoryManager` is a
// composed type that holds either a reference to
// `ReferenceCountingMemoryManager` or owns a `PoolingMemoryManager`.
//
// ============================ Reference Counting ============================
// `Unique`: The object is valid until destruction of the `Unique`.
//
// `Shared`: The object is valid so long as one or more `Shared` managing the
// object exist.
//
// ================================= Pooling ==================================
// `Unique`: The object is valid until destruction of the underlying memory
// resources or of the `Unique`.
//
// `Shared`: The object is valid until destruction of the underlying memory
// resources.
class ABSL_ATTRIBUTE_TRIVIAL_ABI MemoryManager final {
 private:
  ABSL_ATTRIBUTE_PURE_FUNCTION
  static absl::Nonnull<PoolingMemoryManager*> UnreachablePooling() noexcept;

 public:
  ABSL_MUST_USE_RESULT static MemoryManager ReferenceCounting() {
    MemoryManager memory_manager(nullptr);
    ABSL_ASSUME(memory_manager.pointer_ == nullptr);
    return memory_manager;
  }

  ABSL_MUST_USE_RESULT static MemoryManager Pooling(
      absl::Nonnull<std::unique_ptr<PoolingMemoryManager>> pooling) {
    return MemoryManager(std::move(pooling));
  }

  MemoryManager() = delete;
  MemoryManager(const MemoryManager&) = delete;
  MemoryManager& operator=(const MemoryManager&) = delete;

  template <typename T, typename = std::enable_if_t<
                            std::is_base_of_v<PoolingMemoryManager, T>>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  MemoryManager(absl::Nonnull<std::unique_ptr<T>> pooling)
      : pointer_(ABSL_DIE_IF_NULL(pooling).release()) {  // Crash OK
    ABSL_ASSUME(pointer_ != nullptr);
  }

  MemoryManager(MemoryManager&& other) noexcept : pointer_(other.pointer_) {
    if (other.pointer_ != nullptr) {
      other.pointer_ = UnreachablePooling();
    }
  }

  ~MemoryManager() { Delete(); }

  MemoryManager& operator=(MemoryManager&& other) noexcept {
    Delete();
    pointer_ = other.pointer_;
    if (other.pointer_ != nullptr) {
      other.pointer_ = UnreachablePooling();
    }
    return *this;
  }

  MemoryManagement memory_management() const noexcept {
    return pointer_ == nullptr ? MemoryManagement::kReferenceCounting
                               : MemoryManagement::kPooling;
  }

  template <typename T, typename... Args>
  ABSL_MUST_USE_RESULT Shared<T> MakeShared(Args&&... args) {
    if (pointer_ == nullptr) {
      return ReferenceCountingMemoryManager::MakeShared<T>(
          std::forward<Args>(args)...);
    } else {
      return pointer_->MakeShared<T>(std::forward<Args>(args)...);
    }
  }

  template <typename T, typename... Args>
  ABSL_MUST_USE_RESULT Unique<T> MakeUnique(Args&&... args) {
    if (pointer_ == nullptr) {
      return ReferenceCountingMemoryManager::MakeUnique<T>(
          std::forward<Args>(args)...);
    } else {
      return pointer_->MakeUnique<T>(std::forward<Args>(args)...);
    }
  }

  // Allocates memory directly from the allocator used by this memory manager.
  // If `memory_management()` returns `MemoryManagement::kReferenceCounting`,
  // this allocation *must* be explicitly deallocated at some point via
  // `Deallocate`. Otherwise deallocation is optional.
  ABSL_MUST_USE_RESULT void* Allocate(size_t size, size_t alignment) {
    if (pointer_ == nullptr) {
      return ReferenceCountingMemoryManager::Allocate(size, alignment);
    } else {
      return pointer_->Allocate(size, alignment);
    }
  }

  // Attempts to deallocate memory previously allocated via `Allocate`, `size`
  // and `alignment` must match the values from the previous call to `Allocate`.
  // Returns `true` if the deallocation was successful and additional calls to
  // `Allocate` may re-use the memory, `false` otherwise. Returns `false` if
  // given `nullptr`.
  bool Deallocate(void* ptr, size_t size, size_t alignment) noexcept {
    if (pointer_ == nullptr) {
      return ReferenceCountingMemoryManager::Deallocate(ptr, size, alignment);
    } else {
      return pointer_->Deallocate(ptr, size, alignment);
    }
  }

  // Registers a custom destructor to be run upon destruction of the memory
  // management implementation. A return of `true` indicates the destructor may
  // be called at some point in the future, `false` if will definitely not be
  // called. All pooling memory managers return `true` while the reference
  // counting memory manager returns `false`.
  bool OwnCustomDestructor(void* object,
                           absl::Nonnull<void (*)(void*)> destruct) {
    ABSL_DCHECK(destruct != nullptr);
    if (pointer_ == nullptr) {
      return false;
    } else {
      return static_cast<PoolingMemoryManager*>(pointer_)->OwnCustomDestructor(
          object, destruct);
    }
  }

  friend void swap(MemoryManager& lhs, MemoryManager& rhs) noexcept {
    using std::swap;
    swap(lhs.pointer_, rhs.pointer_);
  }

 private:
  friend class PoolingMemoryManager;
  friend class MemoryManagerRef;
  friend struct NativeTypeTraits<MemoryManager>;
  friend struct CompositionTraits<MemoryManager>;

  explicit MemoryManager(PoolingMemoryManager* pointer) : pointer_(pointer) {}

  void Delete() {
    if (pointer_ != nullptr && pointer_ != UnreachablePooling()) {
      std::default_delete<PoolingMemoryManager>{}(pointer_);
    }
  }

  // If `nullptr`, we are using reference counting. Otherwise we are using
  // Pooling. We use `UnreachablePooling()` as a sentinel to detect use after
  // move otherwise the moved-from `MemoryManager` would be in a valid state and
  // utilize reference counting.
  PoolingMemoryManager* pointer_;
};

template <>
struct NativeTypeTraits<MemoryManager> final {
  static NativeTypeId Id(const MemoryManager& memory_manager) {
    return memory_manager.pointer_ == nullptr
               ? NativeTypeId::For<ReferenceCountingMemoryManager>()
               : NativeTypeId::Of(*memory_manager.pointer_);
  }
};

template <>
struct CompositionTraits<MemoryManager> final {
  template <typename U>
  static std::enable_if_t<std::is_same_v<ReferenceCountingMemoryManager, U>,
                          bool>
  HasA(const MemoryManager& memory_manager) {
    return memory_manager.memory_management() ==
           MemoryManagement::kReferenceCounting;
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<PoolingMemoryManager, U>, bool> HasA(
      const MemoryManager& memory_manager) {
    return memory_manager.memory_management() == MemoryManagement::kPooling;
  }

  template <typename U>
  static std::enable_if_t<
      std::conjunction_v<std::is_base_of<PoolingMemoryManager, U>,
                         std::negation<std::is_same<PoolingMemoryManager, U>>>,
      bool>
  HasA(const MemoryManager& memory_manager) {
    return memory_manager.memory_management() == MemoryManagement::kPooling &&
           SubsumptionTraits<U>::IsA(*static_cast<const PoolingMemoryManager*>(
               memory_manager.pointer_));
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<PoolingMemoryManager, U>, const U&>
  Get(const MemoryManager& memory_manager) {
    ABSL_DCHECK(HasA<U>(memory_manager));
    return *static_cast<const PoolingMemoryManager*>(memory_manager.pointer_);
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<PoolingMemoryManager, U>, U&> Get(
      MemoryManager& memory_manager) {
    ABSL_DCHECK(HasA<U>(memory_manager));
    return *static_cast<PoolingMemoryManager*>(memory_manager.pointer_);
  }

  template <typename U>
  static std::enable_if_t<
      std::conjunction_v<std::is_base_of<PoolingMemoryManager, U>,
                         std::negation<std::is_same<PoolingMemoryManager, U>>>,
      const U&>
  Get(const MemoryManager& memory_manager) {
    ABSL_DCHECK(HasA<U>(memory_manager));
    return Cast<U>(
        *static_cast<const PoolingMemoryManager*>(memory_manager.pointer_));
  }

  template <typename U>
  static std::enable_if_t<
      std::conjunction_v<std::is_base_of<PoolingMemoryManager, U>,
                         std::negation<std::is_same<PoolingMemoryManager, U>>>,
      U&>
  Get(MemoryManager& memory_manager) {
    ABSL_DCHECK(HasA<U>(memory_manager));
    return Cast<U>(
        *static_cast<PoolingMemoryManager*>(memory_manager.pointer_));
  }
};

template <typename To, typename From>
struct CastTraits<
    To, From,
    std::enable_if_t<std::is_same_v<MemoryManager, absl::remove_cvref_t<From>>>>
    : CompositionCastTraits<To, From> {};

// `MemoryManagerRef` is similar to `MemoryManager` except it is more flexible.
// In most cases you should accept and pass around `MemoryManagerRef` instead of
// `MemoryManager`.
class ABSL_ATTRIBUTE_TRIVIAL_ABI MemoryManagerRef final {
 public:
  ABSL_MUST_USE_RESULT static MemoryManagerRef ReferenceCounting() {
    MemoryManagerRef memory_manager(nullptr, nullptr);
    ABSL_ASSUME(memory_manager.vpointer_ == nullptr &&
                memory_manager.pointer_ == nullptr);
    return memory_manager;
  }

  template <typename T>
  ABSL_MUST_USE_RESULT static MemoryManagerRef Pooling(
      const PoolingMemoryManagerVirtualTable& vtable
          ABSL_ATTRIBUTE_LIFETIME_BOUND,
      T& self ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    MemoryManagerRef memory_manager(
        const_cast<PoolingMemoryManagerVirtualTable*>(std::addressof(vtable)),
        std::addressof(self));
    ABSL_ASSUME(memory_manager.vpointer_ != nullptr &&
                memory_manager.pointer_ != nullptr);
    return memory_manager;
  }

  // Returns a `MemoryManagerRef` to a `PoolingMemoryManager` which never
  // deallocates memory and is never destroyed.
  //
  // IMPORTANT: This should only be used for cases where something is
  // initialized and never destructed (e.g. singletons). It should never be used
  // for anything else.
  ABSL_MUST_USE_RESULT static MemoryManagerRef Unmanaged();

  MemoryManagerRef() = delete;
  MemoryManagerRef(const MemoryManagerRef&) = default;
  MemoryManagerRef(MemoryManagerRef&&) = default;

  MemoryManagerRef& operator=(const MemoryManagerRef&) = default;
  MemoryManagerRef& operator=(MemoryManagerRef&&) = default;

  // NOLINTNEXTLINE(google-explicit-constructor)
  MemoryManagerRef(MemoryManager& memory_manager ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : vpointer_(memory_manager.pointer_), pointer_(nullptr) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  MemoryManagerRef(PoolingMemoryManager& pooling ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : vpointer_(std::addressof(pooling)), pointer_(nullptr) {}

  MemoryManagement memory_management() const noexcept {
    return vpointer_ == nullptr ? MemoryManagement::kReferenceCounting
                                : MemoryManagement::kPooling;
  }

  template <typename T, typename... Args>
  ABSL_MUST_USE_RESULT Shared<T> MakeShared(Args&&... args) {
    if (vpointer_ == nullptr) {
      return ReferenceCountingMemoryManager::MakeShared<T>(
          std::forward<Args>(args)...);
    } else if (pointer_ == nullptr) {
      return static_cast<PoolingMemoryManager*>(vpointer_)->MakeShared<T>(
          std::forward<Args>(args)...);
    } else {
      return PoolingMemoryManagerVirtualDispatcher(
                 static_cast<const PoolingMemoryManagerVirtualTable*>(
                     vpointer_),
                 pointer_)
          .MakeShared<T>(std::forward<Args>(args)...);
    }
  }

  template <typename T, typename... Args>
  ABSL_MUST_USE_RESULT Unique<T> MakeUnique(Args&&... args) {
    if (vpointer_ == nullptr) {
      return ReferenceCountingMemoryManager::MakeUnique<T>(
          std::forward<Args>(args)...);
    } else if (pointer_ == nullptr) {
      return static_cast<PoolingMemoryManager*>(vpointer_)->MakeUnique<T>(
          std::forward<Args>(args)...);
    } else {
      return PoolingMemoryManagerVirtualDispatcher(
                 static_cast<const PoolingMemoryManagerVirtualTable*>(
                     vpointer_),
                 pointer_)
          .MakeUnique<T>(std::forward<Args>(args)...);
    }
  }

  // Allocates memory directly from the allocator used by this memory manager.
  // If `memory_management()` returns `MemoryManagement::kReferenceCounting`,
  // this allocation *must* be explicitly deallocated at some point via
  // `Deallocate`. Otherwise deallocation is optional.
  ABSL_MUST_USE_RESULT void* Allocate(size_t size, size_t alignment) {
    if (vpointer_ == nullptr) {
      return ReferenceCountingMemoryManager::Allocate(size, alignment);
    } else if (pointer_ == nullptr) {
      return static_cast<PoolingMemoryManager*>(vpointer_)->Allocate(size,
                                                                     alignment);
    } else {
      return PoolingMemoryManagerVirtualDispatcher(
                 static_cast<const PoolingMemoryManagerVirtualTable*>(
                     vpointer_),
                 pointer_)
          .Allocate(size, alignment);
    }
  }

  // Attempts to deallocate memory previously allocated via `Allocate`, `size`
  // and `alignment` must match the values from the previous call to `Allocate`.
  // Returns `true` if the deallocation was successful and additional calls to
  // `Allocate` may re-use the memory, `false` otherwise. Returns `false` if
  // given `nullptr`.
  bool Deallocate(void* ptr, size_t size, size_t alignment) noexcept {
    if (vpointer_ == nullptr) {
      return ReferenceCountingMemoryManager::Deallocate(ptr, size, alignment);
    } else if (pointer_ == nullptr) {
      return static_cast<PoolingMemoryManager*>(vpointer_)->Deallocate(
          ptr, size, alignment);
    } else {
      return PoolingMemoryManagerVirtualDispatcher(
                 static_cast<const PoolingMemoryManagerVirtualTable*>(
                     vpointer_),
                 pointer_)
          .Deallocate(ptr, size, alignment);
    }
  }

  // Registers a custom destructor to be run upon destruction of the memory
  // management implementation. A return of `true` indicates the destructor may
  // be called at some point in the future, `false` if will definitely not be
  // called. All pooling memory managers return `true` while the reference
  // counting memory manager returns `false`.
  bool OwnCustomDestructor(void* object,
                           absl::Nonnull<void (*)(void*)> destruct) {
    ABSL_DCHECK(destruct != nullptr);
    if (vpointer_ == nullptr) {
      return false;
    } else if (pointer_ == nullptr) {
      return static_cast<PoolingMemoryManager*>(vpointer_)->OwnCustomDestructor(
          object, destruct);
    } else {
      return PoolingMemoryManagerVirtualDispatcher(
                 static_cast<const PoolingMemoryManagerVirtualTable*>(
                     vpointer_),
                 pointer_)
          .OwnCustomDestructor(object, destruct);
    }
  }

  friend void swap(MemoryManagerRef& lhs, MemoryManagerRef& rhs) noexcept {
    using std::swap;
    swap(lhs.vpointer_, rhs.vpointer_);
    swap(lhs.pointer_, rhs.pointer_);
  }

 private:
  friend class PoolingMemoryManager;
  friend class PoolingMemoryManagerVirtualDispatcher;
  friend struct NativeTypeTraits<MemoryManagerRef>;
  friend struct CompositionTraits<MemoryManagerRef>;

  explicit MemoryManagerRef(void* vpointer, void* pointer)
      : vpointer_(vpointer), pointer_(pointer) {}

  void* vpointer_;
  void* pointer_;
};

template <>
struct NativeTypeTraits<MemoryManagerRef> final {
  static NativeTypeId Id(MemoryManagerRef memory_manager) {
    return memory_manager.vpointer_ == nullptr
               ? NativeTypeId::For<ReferenceCountingMemoryManager>()
           : memory_manager.pointer_ == nullptr
               ? NativeTypeId::Of(*static_cast<PoolingMemoryManager*>(
                     memory_manager.vpointer_))
               : static_cast<const PoolingMemoryManagerVirtualTable*>(
                     memory_manager.vpointer_)
                     ->NativeTypeId;
  }
};

template <>
struct CompositionTraits<MemoryManagerRef> final {
  template <typename U>
  static std::enable_if_t<std::is_same_v<ReferenceCountingMemoryManager, U>,
                          bool>
  HasA(MemoryManagerRef memory_manager) {
    return memory_manager.memory_management() ==
           MemoryManagement::kReferenceCounting;
  }

  template <typename U>
  static std::enable_if_t<std::is_same_v<PoolingMemoryManager, U>, bool> HasA(
      MemoryManagerRef memory_manager) {
    return memory_manager.memory_management() == MemoryManagement::kPooling;
  }

  template <typename U>
  static std::enable_if_t<
      std::is_same_v<PoolingMemoryManagerVirtualDispatcher, U>, bool>
  HasA(MemoryManagerRef memory_manager) {
    return memory_manager.vpointer_ != nullptr &&
           memory_manager.pointer_ != nullptr;
  }

  template <typename U>
  static std::enable_if_t<
      std::conjunction_v<std::is_base_of<PoolingMemoryManager, U>,
                         std::negation<std::is_same<PoolingMemoryManager, U>>,
                         std::negation<std::is_same<
                             PoolingMemoryManagerVirtualDispatcher, U>>>,
      bool>
  HasA(MemoryManagerRef memory_manager) {
    return memory_manager.vpointer_ != nullptr &&
           memory_manager.pointer_ == nullptr &&
           SubsumptionTraits<U>::IsA(*static_cast<const PoolingMemoryManager*>(
               memory_manager.vpointer_));
  }

  template <typename U>
  static std::enable_if_t<
      std::is_same_v<PoolingMemoryManagerVirtualDispatcher, U>,
      PoolingMemoryManagerVirtualDispatcher>
  Get(MemoryManagerRef memory_manager) {
    ABSL_DCHECK(HasA<U>(memory_manager));
    return PoolingMemoryManagerVirtualDispatcher(
        static_cast<const PoolingMemoryManagerVirtualTable*>(
            memory_manager.vpointer_),
        memory_manager.pointer_);
  }

  template <typename U>
  static std::enable_if_t<
      std::conjunction_v<std::is_base_of<PoolingMemoryManager, U>,
                         std::negation<std::is_same<
                             PoolingMemoryManagerVirtualDispatcher, U>>>,
      U&>
  Get(MemoryManagerRef memory_manager) {
    ABSL_DCHECK(HasA<U>(memory_manager));
    return Cast<U>(
        *static_cast<PoolingMemoryManager*>(memory_manager.vpointer_));
  }
};

template <typename To, typename From>
struct CastTraits<To, From,
                  std::enable_if_t<std::is_same_v<MemoryManagerRef,
                                                  absl::remove_cvref_t<From>>>>
    : CompositionCastTraits<To, From> {};

namespace common_internal {

template <typename T>
inline T* GetPointer(const Shared<T>& shared) {
  return shared.value_;
}

template <typename T>
inline const ReferenceCount* GetReferenceCount(const Shared<T>& shared) {
  return shared.refcount_;
}

template <typename T>
inline Shared<T> MakeShared(T* value, const ReferenceCount* refcount) {
  StrongRef(refcount);
  return MakeShared(kAdoptRef, value, refcount);
}

template <typename T>
inline Shared<T> MakeShared(AdoptRef, T* value,
                            const ReferenceCount* refcount) {
  return Shared<T>(kAdoptRef, value, refcount);
}

template <typename T>
inline T* GetPointer(SharedView<T> shared) {
  return shared.value_;
}

template <typename T>
inline const ReferenceCount* GetReferenceCount(SharedView<T> shared) {
  return shared.refcount_;
}

template <typename T>
inline SharedView<T> MakeSharedView(T* value, const ReferenceCount* refcount) {
  return SharedView<T>(value, refcount);
}

}  // namespace common_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_MEMORY_H_
