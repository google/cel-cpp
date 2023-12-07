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

#ifndef THIRD_PARTY_CEL_CPP_COMMON_SIZED_INPUT_VIEW_H_
#define THIRD_PARTY_CEL_CPP_COMMON_SIZED_INPUT_VIEW_H_

#include <cstddef>
#include <initializer_list>
#include <iterator>
#include <memory>
#include <new>
#include <type_traits>
#include <utility>

#include "absl/base/attributes.h"
#include "absl/base/optimization.h"
#include "absl/log/absl_check.h"

namespace cel {

// `SizedInputView` is a type-erased, read-only view for forward sized iterable
// ranges. This should be useful for handling different container types when
// the alternatives are cumbersome or impossible.
template <typename T>
class SizedInputView;

namespace sized_input_view_internal {

struct ForwardingTransformer {
  template <typename T>
  decltype(auto) operator()(T&& to_transformer) const {
    return std::forward<T>(to_transformer);
  }
};

template <typename C>
using ConstIterator =
    std::decay_t<decltype(std::begin(std::declval<const C&>()))>;

template <typename C>
using SizeType = std::decay_t<decltype(std::size(std::declval<const C&>()))>;

template <typename Transformer, typename T, typename Iter>
constexpr bool CanIterateAsType() {
  return std::is_convertible_v<
             std::add_pointer_t<decltype((std::declval<Transformer>())(
                 *std::declval<Iter>()))>,
             const T*> ||
         std::is_convertible_v<
             decltype((std::declval<Transformer>())(*std::declval<Iter>())), T>;
}

inline constexpr size_t kSmallSize = sizeof(void*) * 2;

union Storage {
  char small[kSmallSize];
  void* large;
};

template <typename T>
constexpr bool IsStoredInline() {
  return alignof(T) <= alignof(Storage) && sizeof(T) <= kSmallSize;
}

template <typename T>
T* StorageCast(Storage& storage) {
  if constexpr (IsStoredInline<T>()) {
    return std::launder(reinterpret_cast<T*>(&storage.small[0]));
  } else {
    return static_cast<T*>(storage.large);
  }
}

template <typename T>
const T* StorageCast(const Storage& storage) {
  if constexpr (IsStoredInline<T>()) {
    return std::launder(reinterpret_cast<const T*>(&storage.small[0]));
  } else {
    return static_cast<const T*>(storage.large);
  }
}

template <typename T, typename Iter, typename Transformer>
constexpr bool IsValueStashRequired() {
  return !std::is_convertible_v<
      std::add_pointer_t<decltype((std::declval<Transformer>())(
          *std::declval<Iter>()))>,
      const T*>;
}

template <typename Iter>
struct LargeIteratorStorage {
  alignas(Iter) char begin[sizeof(Iter)];
  alignas(Iter) char end[sizeof(Iter)];
};

template <typename Transformer>
struct LargeTransformerStorage {
  alignas(Transformer) char value[sizeof(Transformer)];
};

template <typename Iter, typename Transformer>
struct LargeIteratorTransformerStorage : LargeIteratorStorage<Iter>,
                                         LargeTransformerStorage<Transformer> {
};

template <typename T>
struct LargeValueStorage {
  alignas(T) char value[sizeof(T)];
};

template <typename Iter, typename T>
struct LargeIteratorValueStorage : LargeIteratorStorage<Iter>,
                                   LargeValueStorage<T> {};

template <typename Transformer, typename T>
struct LargeTransformerValueStorage : LargeTransformerStorage<Transformer>,
                                      LargeValueStorage<T> {};

template <typename T, typename Iter, typename Transformer>
struct LargeStorage : LargeIteratorStorage<Iter>,
                      LargeTransformerStorage<Transformer>,
                      LargeValueStorage<T> {};

struct RangeStorage {
  Storage begin;
  Storage end;
  Storage transformer;
  Storage value_stash;
};

template <typename T>
char* Allocate() {
  return static_cast<char*>(
      ::operator new(sizeof(T), static_cast<std::align_val_t>(alignof(T))));
}

template <typename T>
void Deallocate(void* address) {
#if defined(__cpp_sized_deallocation) && __cpp_sized_deallocation >= 201309L
  ::operator delete(address, sizeof(T),
                    static_cast<std::align_val_t>(alignof(T)));
#else
  ::operator delete(address, static_cast<std::align_val_t>(alignof(T)));
#endif
}

template <typename T, typename Iter, typename Transformer>
void CreateRangeStorage(RangeStorage* range) {
  constexpr bool value_stash_required =
      IsValueStashRequired<T, Iter, Transformer>() && !IsStoredInline<T>();
  if constexpr (!value_stash_required && IsStoredInline<Iter>() &&
                IsStoredInline<Transformer>()) {
    // Nothing.
  } else if constexpr (!value_stash_required && !IsStoredInline<Iter>() &&
                       IsStoredInline<Transformer>()) {
    using StorageType = LargeIteratorStorage<Iter>;
    auto* storage = Allocate<StorageType>();
    range->begin.large = storage + offsetof(StorageType, begin);
    range->end.large = storage + offsetof(StorageType, end);
  } else if constexpr (!value_stash_required && IsStoredInline<Iter>() &&
                       !IsStoredInline<Transformer>()) {
    using StorageType = LargeTransformerStorage<Transformer>;
    auto* storage = Allocate<StorageType>();
    range->transformer.large = storage + offsetof(StorageType, transformer);
  } else if constexpr (!value_stash_required && !IsStoredInline<Iter>() &&
                       !IsStoredInline<Transformer>()) {
    using StorageType = LargeIteratorTransformerStorage<Iter, Transformer>;
    auto* storage = Allocate<StorageType>();
    range->begin.large = storage + offsetof(StorageType, begin);
    range->end.large = storage + offsetof(StorageType, end);
    range->transformer.large = storage + offsetof(StorageType, transformer);
  } else if constexpr (value_stash_required && IsStoredInline<Iter>() &&
                       !IsStoredInline<Transformer>()) {
    using StorageType = LargeTransformerValueStorage<Transformer, T>;
    auto* storage = Allocate<StorageType>();
    range->transformer.large = storage + offsetof(StorageType, transformer);
    range->value_stash.large = storage + offsetof(StorageType, value);
  } else if constexpr (value_stash_required && !IsStoredInline<Iter>() &&
                       IsStoredInline<Transformer>()) {
    using StorageType = LargeIteratorValueStorage<Transformer, T>;
    auto* storage = Allocate<StorageType>();
    range->begin.large = storage + offsetof(StorageType, begin);
    range->end.large = storage + offsetof(StorageType, end);
    range->value_stash.large = storage + offsetof(StorageType, value);
  } else if constexpr (value_stash_required && IsStoredInline<Iter>() &&
                       IsStoredInline<Transformer>()) {
    using StorageType = LargeValueStorage<T>;
    auto* storage = Allocate<StorageType>();
    range->value_stash.large = storage + offsetof(StorageType, value);
  } else {
    static_assert(value_stash_required);
    static_assert(!IsStoredInline<Iter>());
    static_assert(!IsStoredInline<Transformer>());
    using StorageType = LargeStorage<T, Iter, Transformer>;
    auto* storage = Allocate<StorageType>();
    range->begin.large = storage + offsetof(StorageType, begin);
    range->end.large = storage + offsetof(StorageType, end);
    range->transformer.large = storage + offsetof(StorageType, transformer);
    range->value_stash.large = storage + offsetof(StorageType, value);
  }
}

template <typename T, typename Iter, typename Transformer>
void DestroyRangeStorage(RangeStorage* range) {
  constexpr bool value_stash_required =
      IsValueStashRequired<T, Iter, Transformer>() && !IsStoredInline<T>();
  if constexpr (!value_stash_required && IsStoredInline<Iter>() &&
                IsStoredInline<Transformer>()) {
    // Nothing.
  } else if constexpr (!value_stash_required && !IsStoredInline<Iter>() &&
                       IsStoredInline<Transformer>()) {
    using StorageType = LargeIteratorStorage<Iter>;
    auto* storage =
        static_cast<char*>(range->begin.large) - offsetof(StorageType, begin);
    Deallocate<StorageType>(storage);
  } else if constexpr (!value_stash_required && IsStoredInline<Iter>() &&
                       !IsStoredInline<Transformer>()) {
    using StorageType = LargeTransformerStorage<Transformer>;
    auto* storage = static_cast<char*>(range->transformer.large) -
                    offsetof(StorageType, transformer);
    Deallocate<StorageType>(storage);
  } else if constexpr (!value_stash_required && !IsStoredInline<Iter>() &&
                       !IsStoredInline<Transformer>()) {
    using StorageType = LargeIteratorTransformerStorage<Iter, Transformer>;
    auto* storage =
        static_cast<char*>(range->begin.large) - offsetof(StorageType, begin);
    Deallocate<StorageType>(storage);
  } else if constexpr (value_stash_required && IsStoredInline<Iter>() &&
                       !IsStoredInline<Transformer>()) {
    using StorageType = LargeTransformerValueStorage<Transformer, T>;
    auto* storage = static_cast<char*>(range->transformer.large) -
                    offsetof(StorageType, transformer);
    Deallocate<StorageType>(storage);
  } else if constexpr (value_stash_required && !IsStoredInline<Iter>() &&
                       IsStoredInline<Transformer>()) {
    using StorageType = LargeIteratorValueStorage<Transformer, T>;
    auto* storage =
        static_cast<char*>(range->begin.large) - offsetof(StorageType, begin);
    Deallocate<StorageType>(storage);
  } else if constexpr (value_stash_required && IsStoredInline<Iter>() &&
                       IsStoredInline<Transformer>()) {
    using StorageType = LargeValueStorage<T>;
    auto* storage = static_cast<char*>(range->value_stash.large) -
                    offsetof(StorageType, value);
    Deallocate<StorageType>(storage);
  } else {
    static_assert(value_stash_required);
    static_assert(!IsStoredInline<Iter>());
    static_assert(!IsStoredInline<Transformer>());
    using StorageType = LargeStorage<T, Iter, Transformer>;
    auto* storage =
        static_cast<char*>(range->begin.large) - offsetof(StorageType, begin);
    Deallocate<StorageType>(storage);
  }
}

enum class Operation {
  kCreate,
  kAdvanceOne,
  kCopy,
  kMove,
  kDestroy,
};

union OperationInput {
  struct {
    RangeStorage* storage;
    void* begin;
    void* end;
    void* transformer;
  } create;
  RangeStorage* advance_one;
  struct {
    const RangeStorage* src;
    RangeStorage* dest;
  } copy;
  struct {
    RangeStorage* src;
    RangeStorage* dest;
  } move;
  RangeStorage* destroy;
};

union OperationOutput {
  const void* value;
};

using RangeManagerFn = OperationOutput (*)(Operation, const OperationInput&);

template <typename T, typename Iter, typename Transformer>
void RangeManagerDestroy(RangeStorage* range) {
  if constexpr (IsValueStashRequired<T, Iter, Transformer>()) {
    StorageCast<T>(range->value_stash)->~T();
  }
  StorageCast<Transformer>(range->transformer)->~Transformer();
  StorageCast<Iter>(range->end)->~Iter();
  StorageCast<Iter>(range->begin)->~Iter();
  DestroyRangeStorage<T, Iter, Transformer>(range);
}

template <typename T, typename Iter, typename Transformer>
const void* RangeManagerAdvanceOne(RangeStorage* range) {
  auto* begin = StorageCast<Iter>(range->begin);
  auto* end = StorageCast<Iter>(range->end);
  if (++(*begin) == *end) {
    RangeManagerDestroy<T, Iter, Transformer>(range);
    return nullptr;
  } else {
    auto* transformer = StorageCast<Transformer>(range->transformer);
    if constexpr (IsValueStashRequired<T, Iter, Transformer>()) {
      auto* value_stash = StorageCast<T>(range->value_stash);
      value_stash->~T();
      ::new (static_cast<void*>(value_stash)) T((*transformer)(**begin));
      return value_stash;
    } else {
      return static_cast<const T*>(std::addressof((*transformer)(**begin)));
    }
  }
}

template <typename T, typename Iter, typename Transformer>
const void* RangeManagerCreate(RangeStorage* range, Iter begin, Iter end,
                               Transformer transformer) {
  CreateRangeStorage<T, Iter, Transformer>(range);
  ::new (static_cast<void*>(StorageCast<Iter>(range->begin)))
      Iter(std::move(begin));
  ::new (static_cast<void*>(StorageCast<Iter>(range->end)))
      Iter(std::move(end));
  auto* transformer_ptr =
      ::new (static_cast<void*>(StorageCast<Transformer>(range->transformer)))
          Transformer(std::move(transformer));
  if constexpr (IsValueStashRequired<T, Iter, Transformer>()) {
    auto* value_stash = StorageCast<T>(range->value_stash);
    ::new (static_cast<void*>(value_stash))
        T((*transformer_ptr)(**StorageCast<Iter>(range->begin)));
    return value_stash;
  } else {
    return static_cast<const T*>(
        std::addressof((*transformer_ptr)(**StorageCast<Iter>(range->begin))));
  }
}

template <typename T, typename Iter, typename Transformer>
const void* RangeManagerCopy(const RangeStorage* src, RangeStorage* dest) {
  CreateRangeStorage<T, Iter, Transformer>(dest);
  ::new (static_cast<void*>(StorageCast<Iter>(dest->begin)))
      Iter(*StorageCast<Iter>(src->begin));
  ::new (static_cast<void*>(StorageCast<Iter>(dest->end)))
      Iter(*StorageCast<Iter>(src->end));
  auto* transformer_ptr =
      ::new (static_cast<void*>(StorageCast<Transformer>(dest->transformer)))
          Transformer(*StorageCast<Transformer>(src->transformer));
  if constexpr (IsValueStashRequired<T, Iter, Transformer>()) {
    auto* value_stash = StorageCast<T>(dest->value_stash);
    ::new (static_cast<void*>(value_stash))
        T((*transformer_ptr)(**StorageCast<Iter>(dest->begin)));
    return value_stash;
  } else {
    return static_cast<const T*>(
        std::addressof((*transformer_ptr)(**StorageCast<Iter>(dest->begin))));
  }
}

template <typename T, typename Iter, typename Transformer>
const void* RangeManagerMove(RangeStorage* src, RangeStorage* dest) {
  if constexpr (IsValueStashRequired<T, Iter, Transformer>()) {
    if constexpr (IsStoredInline<T>()) {
      ::new (static_cast<void*>(&dest->value_stash.small[0]))
          T(std::move(*StorageCast<T>(src->value_stash)));
      StorageCast<T>(src->value_stash)->~T();
    } else {
      dest->value_stash.large = src->value_stash.large;
    }
  }
  if constexpr (IsStoredInline<Transformer>()) {
    ::new (static_cast<void*>(&dest->transformer.small[0]))
        Transformer(std::move(*StorageCast<Transformer>(src->transformer)));
    StorageCast<Transformer>(src->transformer)->~Transformer();
  } else {
    dest->transformer.large = src->transformer.large;
  }
  if constexpr (IsStoredInline<Iter>()) {
    ::new (static_cast<void*>(&dest->begin.small[0]))
        Iter(std::move(*StorageCast<Iter>(src->begin)));
    ::new (static_cast<void*>(&dest->end.small[0]))
        Iter(std::move(*StorageCast<Iter>(src->end)));
    StorageCast<Iter>(src->end)->~Iter();
    StorageCast<Iter>(src->begin)->~Iter();
  } else {
    dest->begin.large = src->begin.large;
    dest->end.large = src->end.large;
  }
  if constexpr (IsValueStashRequired<T, Iter, Transformer>()) {
    return StorageCast<T>(dest->value_stash);
  } else {
    return static_cast<const T*>(
        std::addressof(**StorageCast<Iter>(dest->begin)));
  }
}

template <typename T, typename Iter, typename Transformer>
OperationOutput RangeManager(Operation op, const OperationInput& input) {
  OperationOutput output;
  switch (op) {
    case Operation::kCreate: {
      output.value = RangeManagerCreate<T, Iter, Transformer>(
          input.create.storage,
          std::move(*static_cast<Iter*>(input.create.begin)),
          std::move(*static_cast<Iter*>(input.create.end)),
          std::move(*static_cast<Transformer*>(input.create.transformer)));
    } break;
    case Operation::kAdvanceOne: {
      output.value =
          RangeManagerAdvanceOne<T, Iter, Transformer>(input.advance_one);
    } break;
    case Operation::kDestroy: {
      RangeManagerDestroy<T, Iter, Transformer>(input.destroy);
      output.value = nullptr;
    } break;
    case Operation::kCopy: {
      output.value = RangeManagerCopy<T, Iter, Transformer>(input.copy.src,
                                                            input.copy.dest);
    } break;
    case Operation::kMove: {
      output.value = RangeManagerMove<T, Iter, Transformer>(input.move.src,
                                                            input.move.dest);
    } break;
  }
  return output;
}

template <typename T>
class Iterator final {
 public:
  using iterator_category = std::input_iterator_tag;
  using value_type = T;
  using pointer = const value_type*;
  using reference = const value_type&;
  using difference_type = ptrdiff_t;

  Iterator() = default;

  template <typename Iter, typename Transformer>
  Iterator(Iter first, Iter last, Transformer transformer) {
    if (first != last) {
      manager_ = &RangeManager<T, Iter, Transformer>;
      value_ = static_cast<pointer>(
          ((*manager_)(Operation::kCreate,
                       OperationInput{.create = {.storage = &range_,
                                                 .begin = std::addressof(first),
                                                 .end = std::addressof(last),
                                                 .transformer = std::addressof(
                                                     transformer)}}))
              .value);
    }
  }

  Iterator(const Iterator& other) { Copy(other); }

  Iterator(Iterator&& other) noexcept { Move(other); }

  ~Iterator() { Destroy(); }

  Iterator& operator=(const Iterator& other) {
    if (ABSL_PREDICT_TRUE(this != std::addressof(other))) {
      Destroy();
      Copy(other);
    }
    return *this;
  }

  Iterator& operator=(Iterator&& other) noexcept {
    if (ABSL_PREDICT_TRUE(this != std::addressof(other))) {
      Destroy();
      Move(other);
    }
    return *this;
  }

  reference operator*() const {
    ABSL_DCHECK(value_ != nullptr) << "SizedInputIterator is at end";
    return *value_;
  }

  pointer operator->() const {
    ABSL_DCHECK(value_ != nullptr) << "SizedInputIterator is at end";
    return value_;
  }

  Iterator& operator++() {
    ABSL_DCHECK(value_ != nullptr) << "SizedInputIterator is at end";
    value_ = static_cast<pointer>(
        ((*manager_)(Operation::kAdvanceOne,
                     OperationInput{.advance_one = &range_}))
            .value);
    if (value_ == nullptr) {
      manager_ = nullptr;
    }
    return *this;
  }

  friend bool operator==(const Iterator& lhs, const Iterator& rhs) {
    ABSL_DCHECK(lhs.manager_ == rhs.manager_ || lhs.manager_ == nullptr ||
                rhs.manager_ == nullptr);
    ABSL_DCHECK(lhs.value_ == nullptr || rhs.value_ == nullptr ||
                lhs.value_ == rhs.value_);
    return lhs.value_ == rhs.value_;
  }

 private:
  void Destroy() noexcept {
    if (manager_ != nullptr) {
      (*manager_)(Operation::kDestroy, OperationInput{.destroy = &range_});
    }
  }

  void Copy(const Iterator& other) {
    manager_ = other.manager_;
    if (manager_ != nullptr) {
      value_ = static_cast<pointer>(
          ((*manager_)(
               Operation::kCopy,
               OperationInput{.copy = {.src = &other.range_, .dest = &range_}}))
              .value);
    } else {
      value_ = nullptr;
    }
  }

  void Move(Iterator& other) noexcept {
    manager_ = other.manager_;
    other.manager_ = nullptr;
    if (manager_ != nullptr) {
      value_ = static_cast<pointer>(
          ((*manager_)(
               Operation::kMove,
               OperationInput{.move = {.src = &other.range_, .dest = &range_}}))
              .value);
    } else {
      value_ = nullptr;
    }
  }

  pointer value_ = nullptr;
  RangeManagerFn manager_ = nullptr;
  RangeStorage range_;
};

template <typename T>
inline bool operator!=(const Iterator<T>& lhs, const Iterator<T>& rhs) {
  return !operator==(lhs, rhs);
}

}  // namespace sized_input_view_internal

template <typename T>
class SizedInputView final {
 public:
  using iterator = sized_input_view_internal::Iterator<T>;
  using const_iterator = iterator;
  using value_type = T;
  using reference = const value_type&;
  using const_reference = reference;
  using pointer = const value_type*;
  using const_pointer = pointer;
  using size_type = size_t;

  SizedInputView() = default;
  SizedInputView(const SizedInputView&) = default;
  SizedInputView& operator=(const SizedInputView&) = default;

  SizedInputView(SizedInputView&& other) noexcept
      : begin_(std::move(other.begin_)), size_(other.size_) {
    other.size_ = 0;
  }

  SizedInputView& operator=(SizedInputView&& other) noexcept {
    if (ABSL_PREDICT_TRUE(this != std::addressof(other))) {
      begin_ = std::move(other.begin_);
      size_ = other.size_;
      other.size_ = 0;
    }
    return *this;
  }

  template <typename C,
            typename IterType = sized_input_view_internal::ConstIterator<C>,
            typename SizeType = sized_input_view_internal::SizeType<C>,
            typename = std::enable_if_t<
                (sized_input_view_internal::CanIterateAsType<
                     sized_input_view_internal::ForwardingTransformer, T,
                     IterType>() &&
                 std::is_convertible_v<SizeType, size_type> &&
                 !std::is_same_v<SizedInputView<T>, C>)>>
  // NOLINTNEXTLINE(google-explicit-constructor)
  SizedInputView(const C& c ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : SizedInputView(c, sized_input_view_internal::ForwardingTransformer{}) {}

  template <
      typename C, typename Transformer,
      typename IterType = sized_input_view_internal::ConstIterator<C>,
      typename SizeType = sized_input_view_internal::SizeType<C>,
      typename = std::enable_if_t<(sized_input_view_internal::CanIterateAsType<
                                       Transformer, T, IterType>() &&
                                   std::is_convertible_v<SizeType, size_type> &&
                                   !std::is_same_v<SizedInputView<T>, C>)>>
  SizedInputView(const C& c ABSL_ATTRIBUTE_LIFETIME_BOUND,
                 Transformer&& transformer)
      : begin_(std::begin(c), std::end(c),
               std::forward<Transformer>(transformer)),
        size_(std::size(c)) {}

  // NOLINTNEXTLINE(google-explicit-constructor)
  template <
      typename U,
      typename = std::enable_if_t<sized_input_view_internal::CanIterateAsType<
          sized_input_view_internal::ForwardingTransformer, T,
          typename std::initializer_list<U>::const_iterator>()>>
  SizedInputView(
      const std::initializer_list<U>& c ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : SizedInputView(c, sized_input_view_internal::ForwardingTransformer{}) {}

  template <
      typename U, typename Transformer,
      typename = std::enable_if_t<sized_input_view_internal::CanIterateAsType<
          Transformer, T, typename std::initializer_list<U>::const_iterator>()>>
  SizedInputView(const std::initializer_list<U>& c
                     ABSL_ATTRIBUTE_LIFETIME_BOUND,
                 Transformer&& transformer)
      : begin_(c.begin(), c.end(), std::forward<Transformer>(transformer)),
        size_(c.size()) {}

  const iterator& begin() const ABSL_ATTRIBUTE_LIFETIME_BOUND { return begin_; }

  iterator end() const { return iterator(); }

  bool empty() const { return size() == 0; }

  size_type size() const { return size_; }

 private:
  iterator begin_;
  size_type size_ = 0;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_SIZED_INPUT_VIEW_H_
