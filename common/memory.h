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
#include <ostream>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/config.h"  // IWYU pragma: keep
#include "absl/base/nullability.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"
#include "absl/log/die_if_null.h"
#include "common/internal/reference_count.h"
#include "common/native_type.h"
#include "internal/exceptions.h"

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

template <typename T>
class Allocator;

namespace base_internal {
template <typename T>
struct HandleFactory;
}  // namespace base_internal

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
  Shared() = delete;

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

  T& operator*() const noexcept {
    ABSL_DCHECK(!IsEmpty());
    return *value_;
  }

  absl::Nonnull<T*> operator->() const noexcept {
    ABSL_DCHECK(!IsEmpty());
    return value_;
  }

  friend void swap(Shared& lhs, Shared& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
    swap(lhs.refcount_, rhs.refcount_);
  }

 private:
  friend class ReferenceCountingMemoryManager;
  friend class PoolingMemoryManager;
  template <typename U>
  friend class Shared;
  template <typename U>
  friend class SharedView;
  template <typename U>
  friend struct EnableSharedFromThis;

  Shared(common_internal::AdoptRef, T* value,
         const common_internal::ReferenceCount* refcount) noexcept
      : value_(value), refcount_(refcount) {}

  bool IsEmpty() const noexcept { return value_ == nullptr; }

  T* value_ = nullptr;
  const common_internal::ReferenceCount* refcount_ = nullptr;
};

// `SharedView` is a wrapper on top of `Shared`. It is roughly equivalent to
// `const Shared<T>&` and can be used in places where it is not feasible to use
// `const Shared<T>&` directly. This is also analygous to
// `std::reference_wrapper<const Shared<T>>>` and is intended to be used under
// the same cirumstances.
template <typename T>
class ABSL_ATTRIBUTE_TRIVIAL_ABI SharedView final {
 public:
  SharedView() = delete;
  SharedView(const SharedView&) = default;
  SharedView(SharedView&&) = default;
  SharedView& operator=(const SharedView&) = default;
  SharedView& operator=(SharedView&&) = default;

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

  T& operator*() const noexcept {
    ABSL_DCHECK(!IsEmpty());
    return *value_;
  }

  absl::Nonnull<T*> operator->() const noexcept {
    ABSL_DCHECK(!IsEmpty());
    return value_;
  }

  friend void swap(SharedView& lhs, SharedView& rhs) noexcept {
    using std::swap;
    swap(lhs.value_, rhs.value_);
    swap(lhs.refcount_, rhs.refcount_);
  }

 private:
  template <typename U>
  friend class Shared;
  template <typename U>
  friend class SharedView;

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

template <typename T>
struct EnableSharedFromThis
    : public virtual common_internal::ReferenceCountFromThis {
 protected:
  Shared<T> shared_from_this() noexcept {
    auto* const derived = reinterpret_cast<T*>(this);
    auto* const refcount = common_internal::GetReferenceCountForThat(*this);
    common_internal::StrongRef(refcount);
    return Shared<T>(common_internal::kAdoptRef, derived, refcount);
  }

  Shared<const T> shared_from_this() const noexcept {
    auto* const derived = reinterpret_cast<const T*>(this);
    auto* const refcount = common_internal::GetReferenceCountForThat(*this);
    common_internal::StrongRef(refcount);
    return Shared<const T>(common_internal::kAdoptRef, derived, refcount);
  }
};

// `Unique` points to an object allocated in memory managed by a
// `MemoryManager`. The pointed to object is valid so long as the managing
// `MemoryManager` is alive and `Unique` is alive.
template <typename T>
class ABSL_ATTRIBUTE_TRIVIAL_ABI Unique final {
 public:
  static_assert(!std::is_array_v<T>);

  Unique() = delete;
  Unique(const Unique&) = delete;
  Unique& operator=(const Unique&) = delete;

  Unique(Unique&& other) noexcept
      : ptr_(other.ptr_), memory_management_(other.memory_management_) {
    other.ptr_ = nullptr;
  }

  template <typename U, typename = std::enable_if_t<std::conjunction_v<
                            std::negation<std::is_same<U, T>>,
                            std::is_same<U, std::remove_const_t<T>>,
                            std::negation<std::is_const<U>>>>>
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

  T* ptr_;
  MemoryManagement memory_management_;
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
    return Shared<T>(common_internal::kAdoptRef, static_cast<T*>(ptr),
                     refcount);
  }

  template <typename T, typename... Args>
  static ABSL_MUST_USE_RESULT Unique<T> MakeUnique(Args&&... args) {
    using U = std::remove_const_t<T>;
    return Unique<T>(static_cast<T*>(new U(std::forward<Args>(args)...)),
                     MemoryManagement::kReferenceCounting);
  }

  explicit ReferenceCountingMemoryManager() = default;

  friend class MemoryManager;
  friend class MemoryManagerRef;
};

// `PoolingMemoryManager` is a `MemoryManager` which employs automatic
// memory management through memory pooling.
class PoolingMemoryManager {
 public:
  static PoolingMemoryManager& DownCast(
      MemoryManager& memory_manager ABSL_ATTRIBUTE_LIFETIME_BOUND);

  static PoolingMemoryManager& DownCast(MemoryManagerRef memory_manager);

  virtual ~PoolingMemoryManager() = default;

  friend NativeTypeId CelNativeTypeIdOf(
      const PoolingMemoryManager& memory_manager) noexcept {
    return memory_manager.GetNativeTypeId();
  }

 protected:
  using CustomDestructPtr = void (*)(void*);

 private:
  friend class MemoryManager;
  friend class MemoryManagerRef;
  template <typename T>
  friend class Allocator;
  template <typename T>
  friend struct base_internal::HandleFactory;

  template <typename, typename = void>
  struct HasCelIsDestructorSkippable : std::false_type {};

  template <typename T>
  struct HasCelIsDestructorSkippable<
      T,
      std::void_t<decltype(CelIsDestructorSkippable(std::declval<const T&>()))>>
      : std::true_type {};

  template <typename T>
  static void DefaultDestructor(void* ptr) {
    static_assert(!std::is_trivially_destructible_v<T>);
    static_cast<T*>(ptr)->~T();
  }

  template <typename T, typename... Args>
  static ABSL_MUST_USE_RESULT Shared<T> MakeShared(
      PoolingMemoryManager& memory_manager, Args&&... args) {
    using U = std::remove_const_t<T>;
    U* ptr = nullptr;
    void* addr = memory_manager.Allocate(sizeof(U), alignof(U));
    CEL_INTERNAL_TRY {
      ptr = ::new (addr) U(std::forward<Args>(args)...);
      if constexpr (!std::is_trivially_destructible_v<U>) {
        if constexpr (HasCelIsDestructorSkippable<U>::value) {
          CEL_INTERNAL_TRY {
            if (!CelIsDestructorSkippable(
                    *static_cast<std::add_const_t<U>*>(ptr))) {
              memory_manager.OwnCustomDestructor(ptr, &DefaultDestructor<U>);
            }
          }
          CEL_INTERNAL_CATCH_ANY {
            ptr->~U();
            CEL_INTERNAL_RETHROW;
          }
        } else {
          CEL_INTERNAL_TRY {
            memory_manager.OwnCustomDestructor(ptr, &DefaultDestructor<U>);
          }
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
      memory_manager.Deallocate(addr, sizeof(U), alignof(U));
      CEL_INTERNAL_RETHROW;
    }
    return Shared<T>(common_internal::kAdoptRef, static_cast<T*>(ptr), nullptr);
  }

  template <typename T, typename... Args>
  static ABSL_MUST_USE_RESULT Unique<T> MakeUnique(
      PoolingMemoryManager& memory_manager, Args&&... args) {
    using U = std::remove_const_t<T>;
    U* ptr = nullptr;
    void* addr = memory_manager.Allocate(sizeof(U), alignof(U));
    CEL_INTERNAL_TRY { ptr = ::new (addr) U(std::forward<Args>(args)...); }
    CEL_INTERNAL_CATCH_ANY {
      memory_manager.Deallocate(addr, sizeof(U), alignof(U));
      CEL_INTERNAL_RETHROW;
    }
    return Unique<T>(static_cast<T*>(ptr), MemoryManagement::kPooling);
  }

  // These are virtual private, ensuring only `MemoryManager` calls these.

  // Allocates memory of at least size `size` in bytes that is at least as
  // aligned as `align`.
  virtual absl::Nonnull<void*> Allocate(size_t size, size_t align) = 0;

  // Attempts to deallocate memory previously returned from `Allocate`. This is
  // only used for manual memory management.
  virtual bool Deallocate(absl::Nonnull<void*> pointer, size_t size,
                          size_t align) noexcept {
    return false;
  }

  // Registers a destructor to be run upon destruction of the memory management
  // implementation.
  virtual void OwnCustomDestructor(
      absl::Nonnull<void*> object,
      absl::Nonnull<CustomDestructPtr> destruct) = 0;

  virtual NativeTypeId GetNativeTypeId() const noexcept = 0;
};

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
  using CustomDestructPtr = void (*)(void*);
  using OwnCustomDestructorPtr = void (*)(absl::Nonnull<void*>,
                                          absl::Nonnull<void*>,
                                          absl::Nonnull<CustomDestructPtr>);

  // NOLINTBEGIN(google3-readability-class-member-naming)
  const cel::NativeTypeId NativeTypeId;
  const absl::Nonnull<AllocatePtr> Allocate;
  const absl::Nonnull<DeallocatePtr> Deallocate;
  const absl::Nonnull<OwnCustomDestructorPtr> OwnCustomDestructor;
  // NOLINTEND(google3-readability-class-member-naming)
};

// `PoolingMemoryManagerVirtualDispatcher` adapts
// `PoolingMemoryManagerVirtualTable` and the instance it describes to
// implement `PoolingMemoryManager`. It should only be used by
// `MemoryManagerRef`.
class PoolingMemoryManagerVirtualDispatcher final
    : public PoolingMemoryManager {
 public:
  static PoolingMemoryManagerVirtualDispatcher DownCast(
      MemoryManagerRef memory_manager);

  absl::Nonnull<const PoolingMemoryManagerVirtualTable*> vtable() const {
    return vtable_;
  }

  absl::Nonnull<void*> callee() const { return context_; }

 private:
  friend class MemoryManagerRef;
  template <typename T>
  friend class Allocator;
  template <typename T>
  friend struct base_internal::HandleFactory;

  explicit PoolingMemoryManagerVirtualDispatcher(
      absl::Nonnull<const PoolingMemoryManagerVirtualTable*> vtable,
      absl::Nonnull<void*> context)
      : vtable_(vtable), context_(context) {
    ABSL_DCHECK(vtable_ != nullptr);
    ABSL_DCHECK(context_ != nullptr);
    ABSL_DCHECK(vtable_->NativeTypeId != NativeTypeId());
    ABSL_DCHECK(vtable_->Allocate != nullptr);
    ABSL_DCHECK(vtable_->Deallocate != nullptr);
    ABSL_DCHECK(vtable_->OwnCustomDestructor != nullptr);
  }

  // These are virtual private, ensuring only `MemoryManager` calls these.

  // Allocates memory of at least size `size` in bytes that is at least as
  // aligned as `align`.
  absl::Nonnull<void*> Allocate(size_t size, size_t align) override {
    return vtable_->Allocate(context_, size, align);
  }

  // Attempts to deallocate memory previously returned from `Allocate`. This is
  // only used for manual memory management.
  bool Deallocate(absl::Nonnull<void*> pointer, size_t size,
                  size_t align) noexcept override {
    return vtable_->Deallocate(context_, pointer, size, align);
  }

  // Registers a destructor to be run upon destruction of the memory management
  // implementation.
  void OwnCustomDestructor(absl::Nonnull<void*> object,
                           absl::Nonnull<CustomDestructPtr> destruct) override {
    vtable_->OwnCustomDestructor(context_, object, destruct);
  }

  NativeTypeId GetNativeTypeId() const noexcept override {
    return vtable_->NativeTypeId;
  }

  absl::Nonnull<const PoolingMemoryManagerVirtualTable*> vtable_;
  absl::Nonnull<void*> context_;
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
  static MemoryManager ReferenceCounting() {
    MemoryManager memory_manager(nullptr);
    ABSL_ASSUME(memory_manager.pointer_ == nullptr);
    return memory_manager;
  }

  static MemoryManager Pooling(
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
      return PoolingMemoryManager::MakeShared<T>(*pointer_,
                                                 std::forward<Args>(args)...);
    }
  }

  template <typename T, typename... Args>
  ABSL_MUST_USE_RESULT Unique<T> MakeUnique(Args&&... args) {
    if (pointer_ == nullptr) {
      return ReferenceCountingMemoryManager::MakeUnique<T>(
          std::forward<Args>(args)...);
    } else {
      return PoolingMemoryManager::MakeUnique<T>(*pointer_,
                                                 std::forward<Args>(args)...);
    }
  }

  friend void swap(MemoryManager& lhs, MemoryManager& rhs) noexcept {
    using std::swap;
    swap(lhs.pointer_, rhs.pointer_);
  }

  friend NativeTypeId CelNativeTypeIdOf(
      const MemoryManager& memory_manager) noexcept {
    return memory_manager.pointer_ == nullptr
               ? NativeTypeId::For<ReferenceCountingMemoryManager>()
               : NativeTypeId::Of(*memory_manager.pointer_);
  }

 private:
  friend class MemoryManagerRef;

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

// `MemoryManagerRef` is similar to `MemoryManager` except it is more flexible.
// In most cases you should accept and pass around `MemoryManagerRef` instead of
// `MemoryManager`.
class ABSL_ATTRIBUTE_TRIVIAL_ABI MemoryManagerRef final {
 public:
  static MemoryManagerRef ReferenceCounting() {
    MemoryManagerRef memory_manager(nullptr, nullptr);
    ABSL_ASSUME(memory_manager.vpointer_ == nullptr &&
                memory_manager.pointer_ == nullptr);
    return memory_manager;
  }

  template <typename T>
  static MemoryManagerRef Pooling(const PoolingMemoryManagerVirtualTable& vtable
                                      ABSL_ATTRIBUTE_LIFETIME_BOUND,
                                  T& self ABSL_ATTRIBUTE_LIFETIME_BOUND) {
    MemoryManagerRef memory_manager(
        const_cast<PoolingMemoryManagerVirtualTable*>(std::addressof(vtable)),
        std::addressof(self));
    ABSL_ASSUME(memory_manager.vpointer_ != nullptr &&
                memory_manager.pointer_ != nullptr);
    return memory_manager;
  }

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
      return PoolingMemoryManager::MakeShared<T>(
          *static_cast<PoolingMemoryManager*>(vpointer_),
          std::forward<Args>(args)...);
    } else {
      PoolingMemoryManagerVirtualDispatcher pooling(
          static_cast<const PoolingMemoryManagerVirtualTable*>(vpointer_),
          pointer_);
      return PoolingMemoryManager::MakeShared<T>(pooling,
                                                 std::forward<Args>(args)...);
    }
  }

  template <typename T, typename... Args>
  ABSL_MUST_USE_RESULT Unique<T> MakeUnique(Args&&... args) {
    if (vpointer_ == nullptr) {
      return ReferenceCountingMemoryManager::MakeUnique<T>(
          std::forward<Args>(args)...);
    } else if (pointer_ == nullptr) {
      return PoolingMemoryManager::MakeUnique<T>(
          *static_cast<PoolingMemoryManager*>(vpointer_),
          std::forward<Args>(args)...);
    } else {
      PoolingMemoryManagerVirtualDispatcher pooling(
          static_cast<const PoolingMemoryManagerVirtualTable*>(vpointer_),
          pointer_);
      return PoolingMemoryManager::MakeUnique<T>(pooling,
                                                 std::forward<Args>(args)...);
    }
  }

  friend void swap(MemoryManagerRef& lhs, MemoryManagerRef& rhs) noexcept {
    using std::swap;
    swap(lhs.vpointer_, rhs.vpointer_);
    swap(lhs.pointer_, rhs.pointer_);
  }

  friend NativeTypeId CelNativeTypeIdOf(
      const MemoryManagerRef& memory_manager) noexcept {
    return memory_manager.vpointer_ == nullptr
               ? NativeTypeId::For<ReferenceCountingMemoryManager>()
           : memory_manager.pointer_ == nullptr
               ? NativeTypeId::Of(*static_cast<PoolingMemoryManager*>(
                     memory_manager.vpointer_))
               : static_cast<const PoolingMemoryManagerVirtualTable*>(
                     memory_manager.vpointer_)
                     ->NativeTypeId;
  }

 private:
  friend class PoolingMemoryManager;
  friend class PoolingMemoryManagerVirtualDispatcher;
  template <typename T>
  friend class Allocator;
  template <typename T>
  friend struct base_internal::HandleFactory;

  explicit MemoryManagerRef(void* vpointer, void* pointer)
      : vpointer_(vpointer), pointer_(pointer) {}

  void* vpointer_;
  void* pointer_;
};

inline PoolingMemoryManager& PoolingMemoryManager::DownCast(
    MemoryManager& memory_manager) {
  return DownCast(MemoryManagerRef(memory_manager));
}

inline PoolingMemoryManager& PoolingMemoryManager::DownCast(
    MemoryManagerRef memory_manager) {
  ABSL_DCHECK(memory_manager.vpointer_ != nullptr &&
              memory_manager.pointer_ == nullptr)
      << NativeTypeId::Of(memory_manager);
  return *static_cast<PoolingMemoryManager*>(memory_manager.vpointer_);
}

inline PoolingMemoryManagerVirtualDispatcher
PoolingMemoryManagerVirtualDispatcher::DownCast(
    MemoryManagerRef memory_manager) {
  ABSL_DCHECK(memory_manager.vpointer_ != nullptr &&
              memory_manager.pointer_ != nullptr)
      << NativeTypeId::Of(memory_manager);
  return PoolingMemoryManagerVirtualDispatcher(
      static_cast<const PoolingMemoryManagerVirtualTable*>(
          memory_manager.vpointer_),
      memory_manager.pointer_);
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_MEMORY_H_
