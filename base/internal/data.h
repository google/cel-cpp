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
#include <memory>
#include <type_traits>

#include "absl/base/attributes.h"
#include "absl/base/casts.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/numeric/bits.h"
#include "base/kind.h"

namespace cel {

class Type;
class Value;
class MemoryManager;

namespace base_internal {

// Number of bits to shift to store kind.
inline constexpr int kKindShift = sizeof(uintptr_t) * 8 - 8;
// Mask that has all bits set except the two most significant bits.
inline constexpr uint8_t kKindMask = (uint8_t{1} << 6) - 1;

// uintptr_t with the least significant bit set.
inline constexpr uintptr_t kPointerArenaAllocated = uintptr_t{1} << 0;
// uintptr_t with the second to least significant bit set.
inline constexpr uintptr_t kPointerReferenceCounted = uintptr_t{1} << 1;
// uintptr_t with the least and second to to least significant bits set.
inline constexpr uintptr_t kStoredInline =
    kPointerArenaAllocated | kPointerReferenceCounted;
// uintptr_t which is the bitwise OR of kPointerArenaAllocated,
// kPointerReferenceCounted, and kStoredInline.
inline constexpr uintptr_t kPointerBits =
    kPointerArenaAllocated | kPointerReferenceCounted | kStoredInline;
// Mask that has all bits set except for `kPointerBits`.
inline constexpr uintptr_t kPointerMask = ~kPointerBits;
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
// trivially copyable/moveable/destructible.
inline constexpr uintptr_t kTrivial = 1 << 8;

inline constexpr int kInlineVariantShift = 12;
inline constexpr uintptr_t kInlineVariantBits = uintptr_t{0xf}
                                                << kInlineVariantShift;

// We assert some expectations we have around alignment, size, and trivial
// destructibility.
static_assert(sizeof(uintptr_t) == sizeof(std::atomic<uintptr_t>),
              "uintptr_t and std::atomic<uintptr_t> must have the same size");
static_assert(sizeof(void*) == sizeof(uintptr_t),
              "void* and uintptr_t must have the same size");
static_assert(std::is_trivially_destructible_v<std::atomic<uintptr_t>>,
              "std::atomic<uintptr_t> must be trivially destructible");

template <typename E>
constexpr uintptr_t AsInlineVariant(E value) {
  ABSL_ASSERT(static_cast<uintptr_t>(value) <= 15);
  return static_cast<uintptr_t>(value) << kInlineVariantShift;
}

enum class DataLocality {
  kNull = 0,
  kArenaAllocated = 1,
  kReferenceCounted = 2,
  kStoredInline = 3,
};

static_assert(static_cast<uintptr_t>(DataLocality::kArenaAllocated) ==
              kPointerArenaAllocated);
static_assert(static_cast<uintptr_t>(DataLocality::kReferenceCounted) ==
              kPointerReferenceCounted);
static_assert(static_cast<uintptr_t>(DataLocality::kStoredInline) ==
              kStoredInline);

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

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wattributes"

// Empty base class indicating class must be stored directly in the handle and
// not allocated separately on the heap.
//
// For inline data, Kind is stored in the most significant byte of `metadata`.
class InlineData /* : public Data */ {
 public:
  static void* operator new(size_t) = delete;
  static void* operator new[](size_t) = delete;

  static void operator delete(void*) = delete;
  static void operator delete[](void*) = delete;

  InlineData(const InlineData&) = default;
  InlineData(InlineData&&) = default;

  InlineData& operator=(const InlineData&) = default;
  InlineData& operator=(InlineData&&) = default;

 protected:
  constexpr explicit InlineData(uintptr_t metadata) : metadata_(metadata) {}

 private:
  uintptr_t metadata_ ABSL_ATTRIBUTE_UNUSED = 0;
};

static_assert(std::is_trivially_copyable_v<InlineData>,
              "InlineData must be trivially copyable");
static_assert(std::is_trivially_destructible_v<InlineData>,
              "InlineData must be trivially destructible");
static_assert(sizeof(InlineData) == sizeof(uintptr_t),
              "InlineData has unexpected padding");

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

  explicit HeapData(TypeKind kind) : HeapData(TypeKindToKind(kind)) {}

 private:
  // Called by Arena-based memory managers to determine whether we actually need
  // our destructor called. Subclasses should override this if they want their
  // destructor to be skippable, by default it is not.
  static bool IsDestructorSkippable(
      const HeapData& data ABSL_ATTRIBUTE_UNUSED) {
    return false;
  }

  friend class cel::MemoryManager;
  friend constexpr size_t HeapDataMetadataAndReferenceCountOffset();

  std::atomic<uintptr_t> metadata_and_reference_count_ ABSL_ATTRIBUTE_UNUSED =
      0;
};

#pragma GCC diagnostic pop

// Provides introspection for `Data`.
class Metadata final {
 public:
  static ::cel::Kind Kind(const Data& data) {
    ABSL_ASSERT(!IsNull(data));
    return static_cast<cel::Kind>(
        ((IsStoredInline(data)
              ? VirtualPointer(data)
              : ReferenceCount(data).load(std::memory_order_relaxed)) >>
         kKindShift) &
        kKindMask);
  }

  static ::cel::Kind KindHeap(const Data& data) {
    ABSL_ASSERT(!IsNull(data) && !IsStoredInline(data));
    return static_cast<cel::Kind>(
        (ReferenceCount(data).load(std::memory_order_relaxed) >> kKindShift) &
        kKindMask);
  }

  static DataLocality Locality(const Data& data) {
    // We specifically do not use `IsArenaAllocated()` and
    // `IsReferenceCounted()` here due to performance reasons. This code is
    // called often in handle implementations.
    ABSL_ASSERT(!IsNull(data));
    return IsStoredInline(data) ? DataLocality::kStoredInline
           : ((ReferenceCount(data).load(std::memory_order_relaxed) &
               kArenaAllocated) != kArenaAllocated)
               ? DataLocality::kReferenceCounted
               : DataLocality::kArenaAllocated;
  }

  static bool IsNull(const Data& data) { return VirtualPointer(data) == 0; }

  static bool IsStoredInline(const Data& data) {
    return (VirtualPointer(data) & kPointerBits) == kStoredInline;
  }

  static bool IsArenaAllocated(const Data& data) {
    ABSL_ASSERT(!IsNull(data));
    return !IsStoredInline(data) &&
           // We use relaxed because the top 8 bits are never mutated during
           // reference counting and that is all we care about.
           (ReferenceCount(data).load(std::memory_order_relaxed) &
            kArenaAllocated) == kArenaAllocated;
  }

  static bool IsReferenceCounted(const Data& data) {
    ABSL_ASSERT(!IsNull(data));
    return !IsStoredInline(data) &&
           // We use relaxed because the top 8 bits are never mutated during
           // reference counting and that is all we care about.
           (ReferenceCount(data).load(std::memory_order_relaxed) &
            kArenaAllocated) != kArenaAllocated;
  }

  static void Ref(const Data& data) {
    ABSL_ASSERT(IsReferenceCounted(data));
    const auto count = (ReferenceCount(const_cast<Data&>(data))
                            .fetch_add(1, std::memory_order_relaxed)) &
                       kReferenceCountMask;
    ABSL_ASSERT(count > 0 && count < kReferenceCountMax);
  }

  ABSL_MUST_USE_RESULT static bool Unref(const Data& data) {
    ABSL_ASSERT(IsReferenceCounted(data));
    const auto count = (ReferenceCount(const_cast<Data&>(data))
                            .fetch_sub(1, std::memory_order_seq_cst)) &
                       kReferenceCountMask;
    ABSL_ASSERT(count > 0 && count < kReferenceCountMax);
    return count == 1;
  }

  template <typename E>
  static E GetInlineVariant(const Data& data) {
    ABSL_ASSERT(IsStoredInline(data));
    return static_cast<E>((VirtualPointer(data) & kInlineVariantBits) >>
                          kInlineVariantShift);
  }

  static bool IsUnique(const Data& data) {
    ABSL_ASSERT(IsReferenceCounted(data));
    return (ReferenceCount(data).load(std::memory_order_acquire) &
            kReferenceCountMask) == 1;
  }

  static bool IsTrivial(const Data& data) {
    ABSL_ASSERT(IsStoredInline(data));
    return (VirtualPointer(data) & kTrivial) == kTrivial;
  }

  // Used by `MemoryManager::New()`.
  static void SetArenaAllocated(Data& data) {
    ReferenceCount(data).fetch_or(kArenaAllocated, std::memory_order_relaxed);
  }

  // Used by `MemoryManager::New()`.
  static void SetReferenceCounted(Data& data) {
    ReferenceCount(data).fetch_or(kReferenceCounted, std::memory_order_relaxed);
  }

  // Used by `MemoryManager::New()` and `T::IsDestructorSkippable()`. This is
  // used by `T::IsDestructorSkippable()` to query whether a member `Handle<F>`
  // needs its destructor called for an arena-based memory manager.
  static bool IsDestructorSkippable(const Data& data) {
    // We can skip the destructor for any data which is stored inline and
    // trivial, or is arena-allocated.
    switch (Locality(data)) {
      case DataLocality::kStoredInline:
        return IsTrivial(data);
      case DataLocality::kReferenceCounted:
        return false;
      case DataLocality::kArenaAllocated:
        return true;
      case DataLocality::kNull:
        // Locality() never returns kNull.
        ABSL_UNREACHABLE();
    }
  }

 private:
  static uintptr_t VirtualPointer(const Data& data) {
    // The vptr, or equivalent, is stored at offset 0. Inform the compiler that
    // `data` is aligned to at least `uintptr_t`.
    return *absl::bit_cast<const uintptr_t*>(std::addressof(data));
  }

  static const std::atomic<uintptr_t>& ReferenceCount(const Data& data) {
    // For arena allocated and reference counted, the reference count
    // immediately follows the vptr, or equivalent, at offset 0. So its offset
    // is `sizeof(uintptr_t)`.
    return *absl::bit_cast<const std::atomic<uintptr_t>*>(
        absl::bit_cast<uintptr_t>(std::addressof(data)) + sizeof(uintptr_t));
  }

  static std::atomic<uintptr_t>& ReferenceCount(Data& data) {
    // For arena allocated and reference counted, the reference count
    // immediately follows the vptr, or equivalent, at offset 0. So its offset
    // is `sizeof(uintptr_t)`.
    return const_cast<std::atomic<uintptr_t>&>(
        ReferenceCount(static_cast<const Data&>(data)));
  }

  Metadata() = delete;
  Metadata(const Metadata&) = delete;
  Metadata(Metadata&&) = delete;
  Metadata& operator=(const Metadata&) = delete;
  Metadata& operator=(Metadata&&) = delete;
};

class TypeMetadata;
class ValueMetadata;

template <typename T, typename = void>
struct SelectMetadataImpl;

template <typename T>
struct SelectMetadataImpl<T,
                          std::enable_if_t<std::is_base_of_v<cel::Type, T>>> {
  using type = TypeMetadata;
};

template <typename T>
struct SelectMetadataImpl<T,
                          std::enable_if_t<std::is_base_of_v<cel::Value, T>>> {
  using type = ValueMetadata;
};

template <typename T>
using SelectMetadata = typename SelectMetadataImpl<T>::type;

template <size_t Size, size_t Align>
union alignas(Align) AnyDataStorage final {
#ifdef NDEBUG
  // Only need to clear the pointer for this to appear as empty.
  AnyDataStorage() : pointer(0) {}
#else
  // In debug builds we clear the entire storage to help identify misuse.
  AnyDataStorage() { std::memset(buffer, '\0', sizeof(buffer)); }
#endif

  uintptr_t pointer;
  uint8_t buffer[Size];
};

// Struct capable of storing data directly or a pointer to data. This is used by
// handle implementations. We use an additional bit to determine whether the
// data pointed to is arena allocated. During arena deletion, we cannot
// dereference our stored pointers as it may have already been deleted. Thus we
// need to know if it was arena allocated without dereferencing the pointer.
template <size_t Size, size_t Align>
struct AnyData final {
  static_assert(Size >= sizeof(uintptr_t),
                "Size must be at least sizeof(uintptr_t)");
  static_assert(Align >= alignof(uintptr_t),
                "Align must be at least alignof(uintptr_t)");

  static constexpr size_t kSize = Size;
  static constexpr size_t kAlign = Align;

  using Storage = AnyDataStorage<kSize, kAlign>;

  Kind kind_inline() const {
    // We do not need apply the mask as the upper bits are only used by heap
    // allocated data.
    return static_cast<Kind>(pointer() >> kKindShift);
  }

  Kind kind_heap() const {
    return static_cast<Kind>(
        ((absl::bit_cast<std::atomic<uintptr_t>*>((pointer() & kPointerMask) +
                                                  sizeof(uintptr_t))
              ->load(std::memory_order_relaxed)) >>
         kKindShift) &
        kKindMask);
  }

  DataLocality locality() const {
    return static_cast<DataLocality>(pointer() & kPointerBits);
  }

  template <typename E>
  E inline_variant() const {
    return static_cast<E>((pointer() & kInlineVariantBits) >>
                          kInlineVariantShift);
  }

  bool IsNull() const { return pointer() == 0; }

  bool IsStoredInline() const {
    return locality() == DataLocality::kStoredInline;
  }

  bool IsArenaAllocated() const {
    return locality() == DataLocality::kArenaAllocated;
  }

  bool IsReferenceCounted() const {
    return locality() == DataLocality::kReferenceCounted;
  }

  void Ref() const {
    ABSL_ASSERT(IsReferenceCounted());
    // We do not need to apply the pointer mask, we know this is reference
    // counted.
    Metadata::Ref(*get_heap());
  }

  ABSL_MUST_USE_RESULT bool Unref() const {
    ABSL_ASSERT(IsReferenceCounted());
    // We do not need to apply the pointer mask, we know this is reference
    // counted.
    return Metadata::Unref(*get_heap());
  }

  bool IsUnique() const {
    ABSL_ASSERT(IsReferenceCounted());
    // We do not need to apply the pointer mask, we know this is reference
    // counted.
    return Metadata::IsUnique(*get_heap());
  }

  bool IsTrivial() const {
    ABSL_ASSERT(IsStoredInline());
    return (pointer() & kTrivial) == kTrivial;
  }

  // IMPORTANT: Do not use `Metadata::For(get())` unless you know what you are
  // doing, instead us the method of the same name in this class.
  Data* get() const {
    return (pointer() & kPointerBits) == kStoredInline ? get_inline()
                                                       : get_heap();
  }

  Data* get_inline() const {
    return absl::bit_cast<Data*>(const_cast<void*>(buffer()));
  }

  Data* get_heap() const {
    return absl::bit_cast<Data*>(pointer() & kPointerMask);
  }

  // Copy the bytes from other, similar to `std::memcpy`.
  void CopyFrom(const AnyData& other) {
    std::memcpy(buffer(), other.buffer(), kSize);
  }

  // Move the bytes from other, similar to `std::memcpy` and `std::memset`.
  void MoveFrom(AnyData& other) {
    CopyFrom(other);
    other.Clear();
  }

  template <typename T>
  void Destruct() {
    static_assert(sizeof(T) <= kSize);
    static_assert(alignof(T) <= kAlign);
    ABSL_ASSERT(IsStoredInline());
    static_cast<T*>(get_inline())->~T();
  }

  void Clear() {
#ifdef NDEBUG
    // We only need to clear the first `sizeof(uintptr_t)` bytes as that is
    // consulted to determine locality.
    set_pointer(0);
#else
    // In debug builds, we clear all the storage to help identify misuse.
    std::memset(buffer(), '\0', kSize);
#endif
  }

  // Counterpart to `Metadata::SetArenaAllocated()` and
  // `Metadata::SetReferenceCounted()`, also used by `MemoryManager`.
  void ConstructReferenceCounted(const Data& data) {
    uintptr_t pointer = absl::bit_cast<uintptr_t>(std::addressof(data));
    ABSL_ASSERT(absl::countr_zero(pointer) >=
                2);  // Assert pointer alignment results in at least the 2 least
                     // significant bits being unset.
    set_pointer(pointer | kPointerReferenceCounted);
    ABSL_ASSERT(IsReferenceCounted());
  }

  // Counterpart to `Metadata::SetArenaAllocated()` and
  // `Metadata::SetReferenceCounted()`, also used by `MemoryManager`.
  void ConstructArenaAllocated(const Data& data) {
    uintptr_t pointer = absl::bit_cast<uintptr_t>(std::addressof(data));
    ABSL_ASSERT(absl::countr_zero(pointer) >=
                2);  // Assert pointer alignment results in at least the 2 least
                     // significant bits being unset.
    set_pointer(pointer | kPointerArenaAllocated);
    ABSL_ASSERT(IsArenaAllocated());
  }

  template <typename T, typename... Args>
  void ConstructInline(Args&&... args) {
    static_assert(sizeof(T) <= kSize);
    static_assert(alignof(T) <= kAlign);
    ::new (buffer()) T(std::forward<Args>(args)...);
    ABSL_ASSERT(IsStoredInline());
  }

  void* buffer() { return &storage.buffer[0]; }

  const void* buffer() const { return &storage.buffer[0]; }

  uintptr_t pointer() const { return storage.pointer; }

  void set_pointer(uintptr_t pointer) { storage.pointer = pointer; }

  Storage storage;
};

template <typename T>
struct IsData
    : public std::integral_constant<bool, std::is_base_of_v<Data, T>> {};

template <typename T>
inline constexpr bool IsDataV = IsData<T>::value;

template <typename T>
struct IsDerivedData
    : public std::integral_constant<
          bool, std::conjunction_v<
                    std::is_base_of<Data, T>,
                    std::negation<std::is_same<Data, std::remove_cv_t<T>>>>> {};

template <typename T>
inline constexpr bool IsDerivedDataV = IsDerivedData<T>::value;

template <typename T>
struct IsInlineData
    : public std::integral_constant<
          bool, std::conjunction_v<IsData<T>, std::is_base_of<InlineData, T>>> {
};

template <typename T>
inline constexpr bool IsInlineDataV = IsInlineData<T>::value;

template <typename T>
struct IsDerivedInlineData
    : public std::integral_constant<
          bool,
          std::conjunction_v<
              IsInlineData<T>, IsDerivedData<T>,
              std::negation<std::is_same<InlineData, std::remove_cv_t<T>>>>> {};

template <typename T>
inline constexpr bool IsDerivedInlineDataV = IsDerivedInlineData<T>::value;

template <typename T>
struct IsHeapData
    : public std::integral_constant<
          bool, std::conjunction_v<IsData<T>, std::is_base_of<HeapData, T>>> {};

template <typename T>
inline constexpr bool IsHeapDataV = IsHeapData<T>::value;

template <typename T>
struct IsDerivedHeapData
    : public std::integral_constant<
          bool,
          std::conjunction_v<
              IsHeapData<T>, IsDerivedData<T>,
              std::negation<std::is_same<HeapData, std::remove_cv_t<T>>>>> {};

template <typename T>
inline constexpr bool IsDerivedHeapDataV = IsDerivedHeapData<T>::value;

}  // namespace base_internal

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_INTERNAL_DATA_H_
