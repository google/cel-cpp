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
#include "absl/meta/type_traits.h"
#include "absl/numeric/bits.h"
#include "common/casting.h"
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
  friend struct NativeTypeTraits<Shared<T>>;

  Shared(common_internal::AdoptRef, T* value,
         const common_internal::ReferenceCount* refcount) noexcept
      : value_(value), refcount_(refcount) {}

  bool IsEmpty() const noexcept { return value_ == nullptr; }

  T* value_ = nullptr;
  const common_internal::ReferenceCount* refcount_ = nullptr;
};

template <typename T>
struct NativeTypeTraits<Shared<T>> final {
  static bool SkipDestructor(const Shared<T>& shared) {
    return shared.refcount_ == nullptr;
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
    auto* const derived = static_cast<T*>(this);
    auto* const refcount = common_internal::GetReferenceCountForThat(*this);
    common_internal::StrongRef(refcount);
    return Shared<T>(common_internal::kAdoptRef, derived, refcount);
  }

  Shared<const T> shared_from_this() const noexcept {
    auto* const derived = static_cast<const T*>(this);
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
    return Shared<T>(common_internal::kAdoptRef, static_cast<T*>(ptr), nullptr);
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
  void* Allocate(size_t size, size_t alignment) {
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
      ABSL_DCHECK_EQ(size, 0);
      return false;
    }
    ABSL_DCHECK_GT(size, 0);
    return DeallocateImpl(ptr, size, alignment);
  }

 protected:
  using CustomDestructPtr = void (*)(void*);

 private:
  friend class MemoryManager;
  friend class MemoryManagerRef;
  friend struct NativeTypeTraits<PoolingMemoryManager>;
  template <typename T>
  friend class Allocator;
  template <typename T>
  friend struct base_internal::HandleFactory;

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
  virtual void OwnCustomDestructor(
      absl::Nonnull<void*> object,
      absl::Nonnull<CustomDestructPtr> destruct) = 0;

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
  using CustomDestructPtr = void (*)(void*);
  using OwnCustomDestructorPtr = void (*)(absl::Nonnull<void*>,
                                          absl::Nonnull<void*>,
                                          absl::Nonnull<CustomDestructPtr>);

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
  friend class CompositionTraits<MemoryManagerRef>;
  template <typename T>
  friend class Allocator;
  template <typename T>
  friend struct base_internal::HandleFactory;

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
  void OwnCustomDestructor(absl::Nonnull<void*> object,
                           absl::Nonnull<CustomDestructPtr> destruct) override {
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
  void* Allocate(size_t size, size_t alignment) {
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

  friend void swap(MemoryManager& lhs, MemoryManager& rhs) noexcept {
    using std::swap;
    swap(lhs.pointer_, rhs.pointer_);
  }

 private:
  friend class PoolingMemoryManager;
  friend class MemoryManagerRef;
  friend class NativeTypeTraits<MemoryManager>;
  friend class CompositionTraits<MemoryManager>;

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

  // Returns a `MemoryManagerRef` to a `PoolingMemoryManager` which never
  // deallocates memory and is never destroyed.
  //
  // IMPORTANT: This should only be used for cases where something is
  // initialized and never destructed (e.g. singletons). It should never be used
  // for anything else.
  static MemoryManagerRef Unmanaged();

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
  void* Allocate(size_t size, size_t alignment) {
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

  friend void swap(MemoryManagerRef& lhs, MemoryManagerRef& rhs) noexcept {
    using std::swap;
    swap(lhs.vpointer_, rhs.vpointer_);
    swap(lhs.pointer_, rhs.pointer_);
  }

 private:
  friend class PoolingMemoryManager;
  friend class PoolingMemoryManagerVirtualDispatcher;
  friend struct NativeTypeTraits<MemoryManagerRef>;
  friend class CompositionTraits<MemoryManagerRef>;
  template <typename T>
  friend class Allocator;
  template <typename T>
  friend struct base_internal::HandleFactory;

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

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_MEMORY_H_
