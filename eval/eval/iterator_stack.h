// Copyright 2025 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//       https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_EVAL_EVAL_ITERATOR_STACK_H_
#define THIRD_PARTY_CEL_CPP_EVAL_EVAL_ITERATOR_STACK_H_

#include <algorithm>
#include <cstddef>
#include <memory>
#include <utility>

#include "absl/base/dynamic_annotations.h"
#include "absl/base/nullability.h"
#include "absl/log/absl_check.h"
#include "common/value.h"
#include "internal/new.h"

namespace cel::runtime_internal {

class IteratorStack final {
 public:
  explicit IteratorStack(size_t max_size) { Reserve(max_size); }

  IteratorStack(const IteratorStack&) = delete;
  IteratorStack(IteratorStack&&) = delete;

  ~IteratorStack() {
    if (max_size() > 0) {
      const size_t n = size();
      std::destroy_n(iterators_begin_, n);
      cel::internal::SizedDelete(iterators_begin_,
                                 sizeof(ValueIteratorPtr) * max_size_);
    }
  }

  IteratorStack& operator=(const IteratorStack&) = delete;
  IteratorStack& operator=(IteratorStack&&) = delete;

  size_t size() const { return iterators_ - iterators_begin_; }

  bool empty() const { return iterators_ == iterators_begin_; }

  bool full() const { return iterators_ == iterators_begin_ + max_size_; }

  size_t max_size() const { return max_size_; }

  void Clear() {
    if (max_size() > 0) {
      const size_t n = size();
      std::destroy_n(iterators_begin_, n);
      iterators_ = iterators_begin_;

      ABSL_ANNOTATE_CONTIGUOUS_CONTAINER(iterators_begin_,
                                         iterators_begin_ + max_size_,
                                         iterators_, iterators_begin_);
    }
  }

  void Push(absl::Nonnull<ValueIteratorPtr> iterator) {
    ABSL_DCHECK(!full());
    ABSL_DCHECK(iterator != nullptr);

    ABSL_ANNOTATE_CONTIGUOUS_CONTAINER(iterators_begin_,
                                       iterators_begin_ + max_size_, iterators_,
                                       iterators_ + 1);

    ::new (static_cast<void*>(iterators_))
        ValueIteratorPtr(std::move(iterator));
    ++iterators_;
  }

  absl::Nonnull<ValueIterator*> Peek() {
    ABSL_DCHECK(!empty());
    ABSL_DCHECK(*(iterators_ - 1) != nullptr);

    return (iterators_ - 1)->get();
  }

  void Pop() {
    ABSL_DCHECK(!empty());

    --iterators_;
    iterators_->~ValueIteratorPtr();

    ABSL_ANNOTATE_CONTIGUOUS_CONTAINER(iterators_begin_,
                                       iterators_begin_ + max_size_,
                                       iterators_ + 1, iterators_);
  }

  void SetMaxSize(size_t max_size) { Reserve(max_size); }

 private:
  void Reserve(size_t max_size) {
    if (max_size_ >= max_size) {
      return;
    }

    absl::NullabilityUnknown<absl::NullabilityUnknown<ValueIteratorPtr>*>
        iterators = reinterpret_cast<ValueIteratorPtr*>(
            cel::internal::New(sizeof(ValueIteratorPtr) * max_size));
    absl::NullabilityUnknown<absl::NullabilityUnknown<ValueIteratorPtr>*>
        iterators_begin = iterators;

    if (this->max_size() > 0) {
      const size_t n = size();
      const size_t m = std::min(n, max_size);
      for (size_t i = 0; i < m; ++i) {
        ::new (static_cast<void*>(iterators++))
            ValueIteratorPtr(std::move(iterators_begin_[i]));
      }
      std::destroy_n(iterators_begin_, n);
      ABSL_ANNOTATE_CONTIGUOUS_CONTAINER(
          iterators_begin_, iterators_begin_ + max_size_, iterators_,
          iterators_begin_ + max_size_);
      cel::internal::SizedDelete(iterators_begin_,
                                 sizeof(ValueIteratorPtr) * max_size_);
      ABSL_ANNOTATE_CONTIGUOUS_CONTAINER(iterators_begin,
                                         iterators_begin + max_size,
                                         iterators_begin + max_size, iterators);
    } else {
      ABSL_ANNOTATE_CONTIGUOUS_CONTAINER(
          iterators_begin, iterators_begin + max_size,
          iterators_begin + max_size, iterators_begin);
    }

    iterators_ = iterators;
    iterators_begin_ = iterators_begin;
    max_size_ = max_size;
  }

  absl::NullabilityUnknown<absl::NullabilityUnknown<ValueIteratorPtr>*>
      iterators_ = nullptr;
  absl::NullabilityUnknown<absl::NullabilityUnknown<ValueIteratorPtr>*>
      iterators_begin_ = nullptr;
  size_t max_size_ = 0;
};

}  // namespace cel::runtime_internal

#endif  // THIRD_PARTY_CEL_CPP_EVAL_EVAL_ITERATOR_STACK_H_
