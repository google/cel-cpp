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

// This header contains primitives for reference counting, roughly equivalent to
// the primitives used to implement `std::shared_ptr`. These primitives should
// not be used directly in most cases, instead `cel::Shared` should be
// used instead.

#ifndef THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_REFERENCE_COUNT_H_
#define THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_REFERENCE_COUNT_H_

#include <atomic>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"

namespace cel::common_internal {

struct AdoptRef final {
  explicit AdoptRef() = default;
};

inline constexpr AdoptRef kAdoptRef{};

class ReferenceCount;
struct ReferenceCountFromThis;

void SetReferenceCountForThat(
    ReferenceCountFromThis& that,
    absl::Nullable<ReferenceCount*> refcount) noexcept;

ABSL_ATTRIBUTE_PURE_FUNCTION
absl::Nullable<ReferenceCount*> GetReferenceCountForThat(
    const ReferenceCountFromThis& that) noexcept;

// `ReferenceCountFromThis` is similar to `std::enable_shared_from_this`. It
// allows the derived object to inspect its own reference count. It should not
// be used directly, but should be used through
// `cel::EnableManagedMemoryFromThis`.
struct ReferenceCountFromThis {
 private:
  friend void SetReferenceCountForThat(
      ReferenceCountFromThis& that,
      absl::Nullable<ReferenceCount*> refcount) noexcept;
  friend absl::Nullable<ReferenceCount*> GetReferenceCountForThat(
      const ReferenceCountFromThis& that) noexcept;

  static constexpr uintptr_t kNullPtr = uintptr_t{0};
  static constexpr uintptr_t kSentinelPtr = ~kNullPtr;

  absl::Nullable<void*> refcount = reinterpret_cast<void*>(kSentinelPtr);
};

inline void SetReferenceCountForThat(
    ReferenceCountFromThis& that,
    absl::Nullable<ReferenceCount*> refcount) noexcept {
  ABSL_DCHECK_EQ(that.refcount,
                 reinterpret_cast<void*>(ReferenceCountFromThis::kSentinelPtr));
  that.refcount = static_cast<void*>(refcount);
}

inline absl::Nullable<ReferenceCount*> GetReferenceCountForThat(
    const ReferenceCountFromThis& that) noexcept {
  ABSL_DCHECK_NE(that.refcount,
                 reinterpret_cast<void*>(ReferenceCountFromThis::kSentinelPtr));
  return static_cast<ReferenceCount*>(that.refcount);
}

void StrongRef(const ReferenceCount& refcount) noexcept;

void StrongRef(absl::Nullable<const ReferenceCount*> refcount) noexcept;

void StrongUnref(const ReferenceCount& refcount) noexcept;

void StrongUnref(absl::Nullable<const ReferenceCount*> refcount) noexcept;

ABSL_MUST_USE_RESULT
bool StrengthenRef(const ReferenceCount& refcount) noexcept;

ABSL_MUST_USE_RESULT
bool StrengthenRef(absl::Nullable<const ReferenceCount*> refcount) noexcept;

void WeakRef(const ReferenceCount& refcount) noexcept;

void WeakRef(absl::Nullable<const ReferenceCount*> refcount) noexcept;

void WeakUnref(const ReferenceCount& refcount) noexcept;

void WeakUnref(absl::Nullable<const ReferenceCount*> refcount) noexcept;

ABSL_MUST_USE_RESULT
bool IsUniqueRef(const ReferenceCount& refcount) noexcept;

ABSL_MUST_USE_RESULT
bool IsUniqueRef(absl::Nullable<const ReferenceCount*> refcount) noexcept;

ABSL_MUST_USE_RESULT
bool IsExpiredRef(const ReferenceCount& refcount) noexcept;

ABSL_MUST_USE_RESULT
bool IsExpiredRef(absl::Nullable<const ReferenceCount*> refcount) noexcept;

// `ReferenceCount` is similar to the control block used by `std::shared_ptr`.
// It is not meant to be interacted with directly in most cases, instead
// `cel::Shared` should be used.
class ReferenceCount {
 public:
  ReferenceCount() = default;

  ReferenceCount(const ReferenceCount&) = delete;
  ReferenceCount(ReferenceCount&&) = delete;
  ReferenceCount& operator=(const ReferenceCount&) = delete;
  ReferenceCount& operator=(ReferenceCount&&) = delete;

  virtual ~ReferenceCount() = default;

 private:
  friend void StrongRef(const ReferenceCount& refcount) noexcept;
  friend void StrongUnref(const ReferenceCount& refcount) noexcept;
  friend bool StrengthenRef(const ReferenceCount& refcount) noexcept;
  friend void WeakRef(const ReferenceCount& refcount) noexcept;
  friend void WeakUnref(const ReferenceCount& refcount) noexcept;
  friend bool IsUniqueRef(const ReferenceCount& refcount) noexcept;
  friend bool IsExpiredRef(const ReferenceCount& refcount) noexcept;

  virtual void Finalize() const noexcept = 0;

  mutable std::atomic<int32_t> strong_refcount_ = 1;
  mutable std::atomic<int32_t> weak_refcount_ = 1;
};

template <typename T>
class InlinedReferenceCount final : public ReferenceCount {
 public:
  template <typename... Args>
  explicit InlinedReferenceCount(std::in_place_t, Args&&... args)
      : ReferenceCount() {
    ::new (static_cast<void*>(value())) T(std::forward<Args>(args)...);
  }

  ABSL_ATTRIBUTE_ALWAYS_INLINE absl::Nonnull<T*> value() {
    return reinterpret_cast<T*>(&value_[0]);
  }

  ABSL_ATTRIBUTE_ALWAYS_INLINE absl::Nonnull<const T*> value() const {
    return reinterpret_cast<const T*>(&value_[0]);
  }

 private:
  void Finalize() const noexcept override { value()->~T(); }

  // We store the instance of `T` in a char buffer and use placement new and
  // direct calls to the destructor. The reason for this is `Finalize()` is
  // called when the strong reference count hits 0. This allows us to destroy
  // our instance of `T` once we are no longer strongly reachable and deallocate
  // the memory once we are no longer weakly reachable.
  alignas(T) char value_[sizeof(T)];
};

template <typename T, typename... Args>
std::pair<absl::Nonnull<T*>, absl::Nonnull<ReferenceCount*>> MakeReferenceCount(
    Args&&... args) {
  using U = std::remove_const_t<T>;
  auto* const refcount =
      new InlinedReferenceCount<U>(std::in_place, std::forward<Args>(args)...);
  auto* const pointer = refcount->value();
  if constexpr (std::is_base_of_v<ReferenceCountFromThis, U>) {
    SetReferenceCountForThat(*pointer, refcount);
  }
  return std::make_pair(static_cast<T*>(pointer),
                        static_cast<ReferenceCount*>(refcount));
}

inline void StrongRef(const ReferenceCount& refcount) noexcept {
  const auto count =
      refcount.strong_refcount_.fetch_add(1, std::memory_order_relaxed);
  ABSL_DCHECK_GT(count, 0);
}

inline void StrongRef(absl::Nullable<const ReferenceCount*> refcount) noexcept {
  if (refcount != nullptr) {
    StrongRef(*refcount);
  }
}

inline void StrongUnref(const ReferenceCount& refcount) noexcept {
  const auto count =
      refcount.strong_refcount_.fetch_sub(1, std::memory_order_acq_rel);
  ABSL_DCHECK_GT(count, 0);
  if (count == 1) {
    const_cast<ReferenceCount&>(refcount).Finalize();
    WeakUnref(refcount);
  }
}

inline void StrongUnref(
    absl::Nullable<const ReferenceCount*> refcount) noexcept {
  if (refcount != nullptr) {
    StrongUnref(*refcount);
  }
}

inline bool StrengthenRef(const ReferenceCount& refcount) noexcept {
  auto count = refcount.strong_refcount_.load(std::memory_order_relaxed);
  while (true) {
    ABSL_DCHECK_GE(count, 0);
    if (count == 0) {
      return false;
    }
    if (refcount.strong_refcount_.compare_exchange_weak(
            count, count + 1, std::memory_order_release,
            std::memory_order_relaxed)) {
      return true;
    }
  }
}

inline bool StrengthenRef(
    absl::Nullable<const ReferenceCount*> refcount) noexcept {
  return refcount != nullptr ? StrengthenRef(*refcount) : false;
}

inline void WeakRef(const ReferenceCount& refcount) noexcept {
  const auto count =
      refcount.weak_refcount_.fetch_add(1, std::memory_order_relaxed);
  ABSL_DCHECK_GT(count, 0);
}

inline void WeakRef(absl::Nullable<const ReferenceCount*> refcount) noexcept {
  if (refcount != nullptr) {
    WeakRef(*refcount);
  }
}

inline void WeakUnref(const ReferenceCount& refcount) noexcept {
  const auto count =
      refcount.weak_refcount_.fetch_sub(1, std::memory_order_acq_rel);
  ABSL_DCHECK_GT(count, 0);
  if (count == 1) {
    delete std::addressof(refcount);
  }
}

inline void WeakUnref(absl::Nullable<const ReferenceCount*> refcount) noexcept {
  if (refcount != nullptr) {
    WeakUnref(*refcount);
  }
}

inline bool IsUniqueRef(const ReferenceCount& refcount) noexcept {
  const auto count = refcount.strong_refcount_.load(std::memory_order_acquire);
  ABSL_DCHECK_GT(count, 0);
  return count == 1;
}

inline bool IsUniqueRef(
    absl::Nullable<const ReferenceCount*> refcount) noexcept {
  return refcount != nullptr ? IsUniqueRef(*refcount) : false;
}

inline bool IsExpiredRef(const ReferenceCount& refcount) noexcept {
  const auto count = refcount.strong_refcount_.load(std::memory_order_acquire);
  ABSL_DCHECK_GE(count, 0);
  return count == 0;
}

inline bool IsExpiredRef(
    absl::Nullable<const ReferenceCount*> refcount) noexcept {
  return refcount != nullptr ? IsExpiredRef(*refcount) : false;
}

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_INTERNAL_REFERENCE_COUNT_H_
