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

#ifndef THIRD_PARTY_CEL_CPP_BASE_INTERNAL_DATA_H_
#define THIRD_PARTY_CEL_CPP_BASE_INTERNAL_DATA_H_

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/macros.h"
#include "absl/numeric/bits.h"
#include "base/kind.h"

namespace cel::base_internal {

// Number of bits to shift to store kind.
inline constexpr int kKindShift = sizeof(uintptr_t) * 8 - 8;
// Mask that has all bits set except the most significant bit.
inline constexpr uint8_t kKindMask = (uint8_t{1} << 7) - 1;

// uintptr_t with the least significant bit set.
inline constexpr uintptr_t kStoredInline = uintptr_t{1} << 0;
// uintptr_t with the second to least significant bit set.
inline constexpr uintptr_t kPointerArenaAllocated = uintptr_t{1} << 1;
// Mask that has all bits set except for `kPointerArenaAllocated`.
inline constexpr uintptr_t kPointerMask = ~kPointerArenaAllocated;
// uintptr_t with the most significant bit set.
inline constexpr uintptr_t kArenaAllocated = uintptr_t{1}
                                             << (sizeof(uintptr_t) * 8 - 1);
inline constexpr uintptr_t kReferenceCounted = 1;
// uintptr_t with all bits set except for the most significant byte.
inline constexpr uintptr_t kReferenceCountMask =
    kArenaAllocated | ((uintptr_t{1} << (sizeof(uintptr_t) * 8 - 8)) - 1);
inline constexpr uintptr_t kReferenceCountMax =
    ((uintptr_t{1} << (sizeof(uintptr_t) * 8 - 8)) - 1);

// uintptr_t with the 8th bit set. Used by inline data to indicate it is
// trivially copyable.
inline constexpr uintptr_t kTriviallyCopyable = 1 << 8;
// uintptr_t with the 9th bit set. Used by inline data to indicate it is
// trivially destuctible.
inline constexpr uintptr_t kTriviallyDestructible = 1 << 9;

// We assert some expectations we have around alignment, size, and trivial
// destructability.
static_assert(sizeof(uintptr_t) == sizeof(std::atomic<uintptr_t>),
              "uintptr_t and std::atomic<uintptr_t> must have the same size");
static_assert(sizeof(void*) == sizeof(uintptr_t),
              "void* and uintptr_t must have the same size");
static_assert(std::is_trivially_destructible_v<std::atomic<uintptr_t>>,
              "std::atomic<uintptr_t> must be trivially destructible");

enum class DataLocality {
  kNull = 0,
  kStoredInline,
  kReferenceCounted,
  kArenaAllocated,
};

// Empty base class of all classes that can be managed by handles.
//
// All `Data` implementations have a size of at least `sizeof(uintptr_t)`, have
// a `uintptr_t` at offset 0, and have an alignment that is at most
// `alignof(std::max_align_t)`.
//
// `Data` implementations are split into two categories: those stored inline and
// those allocated separately on the heap. This detail is not exposed to users
// and is managed entirely by the handles. We use a novel approach where given a
// pointer to some instantiated Data we can determine whether it is stored in a
// handle or allocated separately on the heap. If it is allocated on the heap we
// can then determine if it was allocated in an arena or if it is reference
// counted. We can also determine the `Kind` of data.
//
// We can determine whether data is stored directly in a handle by reading a
// `uintptr_t` at offset 0. If the least significant bit is set, this data is
// stored inside a handle. We rely on the fact that C++ places the virtual
// pointer to the virtual function table at offset 0 and it should be aligned to
// at least `sizeof(void*)`.
class Data {};

// Empty base class indicating class must be stored directly in the handle and
// not allocated separately on the heap.
//
// For inline data, Kind is stored in the most significant byte of `metadata`.
class InlineData /* : public Data */ {
  // uintptr_t metadata

 public:
  static void* operator new(size_t) = delete;
  static void* operator new[](size_t) = delete;

  static void operator delete(void*) = delete;
  static void operator delete[](void*) = delete;
};

// Used purely for a static_assert.
constexpr size_t HeapDataMetadataAndReferenceCountOffset();

// Base class indicating class must be allocated on the heap and not stored
// directly in a handle.
//
// For heap data, Kind is stored in the most significant byte of
// `metadata_and_reference_count`. If heap data was arena allocated, the most
// significant bit of the most significant byte is set. This property, combined
// with twos complement integers, allows us to easily detect incorrect reference
// counting as the reference count will be negative.
class HeapData /* : public Data */ {
  // uintptr_t vptr
  // std::atomic<uintptr_t> metadata_and_reference_count

 public:
  HeapData(const HeapData&) = delete;
  HeapData(HeapData&&) = delete;

  virtual ~HeapData() = default;

  HeapData& operator=(const HeapData&) = delete;
  HeapData& operator=(HeapData&&) = delete;

 protected:
  explicit HeapData(Kind kind)
      : metadata_and_reference_count_(static_cast<uintptr_t>(kind)
                                      << kKindShift) {}

 private:
  friend constexpr size_t HeapDataMetadataAndReferenceCountOffset();

  std::atomic<uintptr_t> metadata_and_reference_count_ ABSL_ATTRIBUTE_UNUSED =
      0;
};

inline constexpr size_t HeapDataMetadataAndReferenceCountOffset() {
  return offsetof(HeapData, metadata_and_reference_count_);
}

static_assert(HeapDataMetadataAndReferenceCountOffset() == sizeof(uintptr_t),
              "Expected vptr to be at offset 0");
static_assert(sizeof(HeapData) == sizeof(uintptr_t) * 2,
              "Unexpected class size");

// Provides introspection for `Data`.
class Metadata final {
 public:
  ABSL_ATTRIBUTE_ALWAYS_INLINE static Metadata* For(Data* data) {
    ABSL_ASSERT(data != nullptr);
    return reinterpret_cast<Metadata*>(data);
  }

  ABSL_ATTRIBUTE_ALWAYS_INLINE static const Metadata* For(const Data* data) {
    ABSL_ASSERT(data != nullptr);
    return reinterpret_cast<const Metadata*>(data);
  }

  Kind kind() const {
    ABSL_ASSERT(!IsNull());
    return static_cast<Kind>(
        ((IsStoredInline()
              ? *reinterpret_cast<const uintptr_t*>(this)
              : reference_count()->load(std::memory_order_relaxed)) >>
         kKindShift) &
        kKindMask);
  }

  DataLocality locality() const {
    // We specifically do not use `IsArenaAllocated()` and
    // `IsReferenceCounted()` here due to performance reasons. This code is
    // called often in handle implementations.
    return IsNull()           ? DataLocality::kNull
           : IsStoredInline() ? DataLocality::kStoredInline
           : ((reference_count()->load(std::memory_order_relaxed) &
               kArenaAllocated) != kArenaAllocated)
               ? DataLocality::kReferenceCounted
               : DataLocality::kArenaAllocated;
  }

  ABSL_ATTRIBUTE_ALWAYS_INLINE bool IsNull() const {
    return *reinterpret_cast<const uintptr_t*>(this) == 0;
  }

  ABSL_ATTRIBUTE_ALWAYS_INLINE bool IsStoredInline() const {
    return (*reinterpret_cast<const uintptr_t*>(this) & kStoredInline) ==
           kStoredInline;
  }

  bool IsArenaAllocated() const {
    return !IsNull() && !IsStoredInline() &&
           // We use relaxed because the top 8 bits are never mutated during
           // reference counting and that is all we care about.
           (reference_count()->load(std::memory_order_relaxed) &
            kArenaAllocated) == kArenaAllocated;
  }

  bool IsReferenceCounted() const {
    return !IsNull() && !IsStoredInline() &&
           // We use relaxed because the top 8 bits are never mutated during
           // reference counting and that is all we care about.
           (reference_count()->load(std::memory_order_relaxed) &
            kArenaAllocated) != kArenaAllocated;
  }

  void Ref() const {
    ABSL_ASSERT(IsReferenceCounted());
    const auto count =
        (reference_count()->fetch_add(1, std::memory_order_relaxed)) &
        kReferenceCountMask;
    ABSL_ASSERT(count > 0 && count < kReferenceCountMax);
  }

  bool Unref() const {
    ABSL_ASSERT(IsReferenceCounted());
    const auto count =
        (reference_count()->fetch_sub(1, std::memory_order_seq_cst)) &
        kReferenceCountMask;
    ABSL_ASSERT(count > 0 && count < kReferenceCountMax);
    return count == 1;
  }

  bool IsUnique() const {
    ABSL_ASSERT(IsReferenceCounted());
    return ((reference_count()->fetch_add(1, std::memory_order_acquire)) &
            kReferenceCountMask) == 1;
  }

  bool IsTriviallyCopyable() const {
    ABSL_ASSERT(IsStoredInline());
    return (*reinterpret_cast<const uintptr_t*>(this) & kTriviallyCopyable) ==
           kTriviallyCopyable;
  }

  bool IsTriviallyDestructible() const {
    ABSL_ASSERT(IsStoredInline());
    return (*reinterpret_cast<const uintptr_t*>(this) &
            kTriviallyDestructible) == kTriviallyDestructible;
  }

  // Used by `MemoryManager::New()`.
  void SetArenaAllocated() {
    reference_count()->fetch_or(kArenaAllocated, std::memory_order_relaxed);
  }

  // Used by `MemoryManager::New()`.
  void SetReferenceCounted() {
    reference_count()->fetch_or(kReferenceCounted, std::memory_order_relaxed);
  }

 private:
  std::atomic<uintptr_t>* reference_count() const {
    return reinterpret_cast<std::atomic<uintptr_t>*>(
        const_cast<uintptr_t*>(reinterpret_cast<const uintptr_t*>(this) + 1));
  }

  Metadata() = delete;
};

template <size_t Size, size_t Align>
union alignas(Align) AnyDataStorage final {
  AnyDataStorage() : pointer(0) {}

  uintptr_t pointer;
  char buffer[Size];
};

// Struct capable of storing data directly or a pointer to data. This is used by
// handle implementations. We use an additional bit to determine whether the
// data pointed to is arena allocated. During arena deletion, we cannot
// dereference our stored pointers as it may have already been deleted. Thus we
// need to know if it was arena allocated without dereferencing the pointer.
template <size_t Size, size_t Align>
struct AnyData {
  static_assert(Size >= sizeof(uintptr_t),
                "Size must be at least sizeof(uintptr_t)");
  static_assert(Align >= alignof(uintptr_t),
                "Align must be at least alignof(uintptr_t)");

  using Storage = AnyDataStorage<Size, Align>;

  Kind kind() const {
    ABSL_ASSERT(!IsNull());
    return Metadata::For(get())->kind();
  }

  DataLocality locality() const {
    return storage.pointer == 0 ? DataLocality::kNull
           : (storage.pointer & kStoredInline) == kStoredInline
               ? DataLocality::kStoredInline
           : (storage.pointer & kPointerArenaAllocated) ==
                   kPointerArenaAllocated
               ? DataLocality::kArenaAllocated
               : DataLocality::kReferenceCounted;
  }

  bool IsNull() const { return storage.pointer == 0; }

  bool IsStoredInline() const {
    return (storage.pointer & kStoredInline) == kStoredInline;
  }

  bool IsArenaAllocated() const {
    return (storage.pointer & kPointerArenaAllocated) == kPointerArenaAllocated;
  }

  bool IsReferenceCounted() const {
    return storage.pointer != 0 &&
           (storage.pointer & (kStoredInline | kPointerArenaAllocated)) == 0;
  }

  void Ref() const {
    ABSL_ASSERT(IsReferenceCounted());
    Metadata::For(get())->Ref();
  }

  bool Unref() const {
    ABSL_ASSERT(IsReferenceCounted());
    return Metadata::For(get())->Unref();
  }

  bool IsUnique() const {
    ABSL_ASSERT(IsReferenceCounted());
    return Metadata::For(get())->IsUnique();
  }

  bool IsTriviallyCopyable() const {
    ABSL_ASSERT(IsStoredInline());
    return Metadata::For(get())->IsTriviallyCopyable();
  }

  bool IsTriviallyDestructible() const {
    ABSL_ASSERT(IsStoredInline());
    return Metadata::For(get())->IsTriviallyDestructible();
  }

  // IMPORTANT: Do not use `Metadata::For(get())` unless you know what you are
  // doing, instead us the method of the same name in this class.
  ABSL_ATTRIBUTE_ALWAYS_INLINE Data* get() const {
    return (storage.pointer & kStoredInline) == kStoredInline
               ? reinterpret_cast<Data*>(
                     const_cast<uintptr_t*>(&storage.pointer))
               : reinterpret_cast<Data*>(storage.pointer & kPointerMask);
  }

  // Copy the bytes from other, similar to `std::memcpy`.
  void CopyFrom(const AnyData& other) {
    std::memcpy(&storage.buffer[0], &other.storage.buffer[0], Size);
  }

  // Move the bytes from other, similar to `std::memcpy` and `std::memset`.
  void MoveFrom(AnyData& other) {
    std::memcpy(&storage.buffer[0], &other.storage.buffer[0], Size);
    other.Clear();
  }

  template <typename T>
  void Destruct() {
    ABSL_ASSERT(IsStoredInline());
    static_cast<T*>(get())->~T();
  }

  void Clear() {
    // We only need to clear the first `sizeof(uintptr_t)` bytes as that is
    // consulted to determine locality.
    storage.pointer = 0;
  }

  // Counterpart to `Metadata::SetArenaAllocated()` and
  // `Metadata::SetReferenceCounted()`, also used by `MemoryManager`.
  void ConstructHeap(const Data& data) {
    ABSL_ASSERT(absl::countr_zero(reinterpret_cast<uintptr_t>(&data)) >=
                2);  // Assert pointer alignment results in at least the 2 least
                     // significant bits being unset.
    storage.pointer =
        reinterpret_cast<uintptr_t>(&data) |
        (Metadata::For(&data)->IsArenaAllocated() ? kPointerArenaAllocated : 0);
  }

  template <typename T, typename... Args>
  void ConstructInline(Args&&... args) {
    ::new (&storage.buffer[0]) T(std::forward<Args>(args)...);
    ABSL_ASSERT(absl::countr_zero(storage.pointer) ==
                0);  // Assert the least significant bit is set.
  }

  Storage storage;
};

}  // namespace cel::base_internal

#endif  // THIRD_PARTY_CEL_CPP_BASE_INTERNAL_DATA_H_
