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
#ifndef THIRD_PARTY_CEL_CPP_COMMON_AST_NAVIGABLE_AST_INTERNAL_H_
#define THIRD_PARTY_CEL_CPP_COMMON_AST_NAVIGABLE_AST_INTERNAL_H_

#include <cstddef>
#include <cstdint>
#include <iterator>
#include <memory>
#include <type_traits>
#include <vector>

#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/log/absl_check.h"
#include "absl/types/span.h"
#include "common/ast/navigable_ast_kinds.h"  // IWYU pragma: keep

namespace cel::common_internal {

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

    template <int... Barrier, typename T = value_type>
    std::enable_if_t<std::is_lvalue_reference<T>::value,
                     std::add_pointer_t<std::remove_reference_t<T>>>
    operator->() const {
      return &operator*();
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

template <typename AstNode>
struct NavigableAstMetadata;

// Internal implementation for data-structures handling cross-referencing nodes.
//
// This is exposed separately to allow building up the AST relationships
// without exposing too much mutable state on the client facing classes.
template <typename AstNode>
struct NavigableAstNodeData {
  AstNode* parent;
  const typename AstNode::ExprType* expr;
  ChildKind parent_relation;
  NodeKind node_kind;
  const NavigableAstMetadata<AstNode>* absl_nonnull metadata;
  size_t index;
  size_t tree_size;
  size_t height;
  int child_index;
  std::vector<AstNode*> children;
};

template <typename AstNode>
struct NavigableAstMetadata {
  // The nodes in the AST in preorder.
  //
  // unique_ptr is used to guarantee pointer stability in the other tables.
  std::vector<std::unique_ptr<AstNode>> nodes;
  std::vector<const AstNode* absl_nonnull> postorder;
  absl::flat_hash_map<int64_t, const AstNode* absl_nonnull> id_to_node;
  absl::flat_hash_map<const typename AstNode::ExprType*,
                      const AstNode* absl_nonnull>
      expr_to_node;
};

template <typename AstNode>
struct PostorderTraits {
  using UnderlyingType = const AstNode*;

  static const AstNode& Adapt(const AstNode* const node) { return *node; }
};

template <typename AstNode>
struct PreorderTraits {
  using UnderlyingType = std::unique_ptr<AstNode>;
  static const AstNode& Adapt(const std::unique_ptr<AstNode>& node) {
    return *node;
  }
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_AST_NAVIGABLE_AST_INTERNAL_H_
