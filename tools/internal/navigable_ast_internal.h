// Copyright 2023 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
#ifndef THIRD_PARTY_CEL_CPP_TOOLS_INTERNAL_NAVIGABLE_AST_INTERNAL_H_
#define THIRD_PARTY_CEL_CPP_TOOLS_INTERNAL_NAVIGABLE_AST_INTERNAL_H_

#include <cstddef>
#include <iterator>

#include "absl/log/absl_check.h"
#include "absl/types/span.h"

namespace cel::tools_internal {

// Implementation for range used for traversals backed by an absl::Span.
//
// This is intended to abstract the metadata layout from clients using the
// traversal methods in navigable_expr.h
//
// RangeTraits provide type info needed to construct the span and adapt to the
// range element type.
template <class RangeTraits>
class NavigableAstRange {
 private:
  using UnderlyingType = typename RangeTraits::UnderlyingType;
  using PtrType = const UnderlyingType*;
  using SpanType = absl::Span<const UnderlyingType>;

 public:
  class Iterator {
   public:
    using difference_type = ptrdiff_t;
    using value_type = decltype(RangeTraits::Adapt(*PtrType()));
    using iterator_category = std::bidirectional_iterator_tag;

    Iterator() : ptr_(nullptr), span_() {}
    Iterator(SpanType span, size_t i) : ptr_(span.data() + i), span_(span) {}

    value_type operator*() const {
      ABSL_DCHECK(ptr_ != nullptr);
      ABSL_DCHECK(span_.data() != nullptr);
      ABSL_DCHECK_GE(ptr_, span_.data());
      ABSL_DCHECK_LT(ptr_, span_.data() + span_.size());
      return RangeTraits::Adapt(*ptr_);
    }

    Iterator& operator++() {
      ++ptr_;
      return *this;
    }

    Iterator operator++(int) {
      Iterator tmp = *this;
      ++ptr_;
      return tmp;
    }

    Iterator& operator--() {
      --ptr_;
      return *this;
    }

    Iterator operator--(int) {
      Iterator tmp = *this;
      --ptr_;
      return tmp;
    }

    bool operator==(const Iterator& other) const {
      return ptr_ == other.ptr_ && span_ == other.span_;
    }

    bool operator!=(const Iterator& other) const { return !(*this == other); }

   private:
    PtrType ptr_;
    SpanType span_;
  };

  explicit NavigableAstRange(SpanType span) : span_(span) {}

  Iterator begin() { return Iterator(span_, 0); }
  Iterator end() { return Iterator(span_, span_.size()); }

  explicit operator bool() const { return !span_.empty(); }

 private:
  SpanType span_;
};

}  // namespace cel::tools_internal

#endif  // THIRD_PARTY_CEL_CPP_TOOLS_INTERNAL_NAVIGABLE_AST_INTERNAL_H_
