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

#ifndef THIRD_PARTY_CEL_CPP_TOOLS_NAVIGABLE_AST_H_
#define THIRD_PARTY_CEL_CPP_TOOLS_NAVIGABLE_AST_H_

#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "cel/expr/syntax.pb.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "common/ast/navigable_ast_internal.h"
#include "common/ast/navigable_ast_kinds.h"  // IWYU pragma: export

namespace cel {

class NavigableProtoAst;

// Wrapper around a CEL AST node that exposes traversal information.
class NavigableProtoAstNode {
 public:
  using ExprType = const cel::expr::Expr;

  // A const Span like type that provides pre-order traversal for a sub tree.
  // provides .begin() and .end() returning bidirectional iterators to
  // const NavigableProtoAstNode&.
  using PreorderRange = common_internal::NavigableAstRange<
      common_internal::PreorderTraits<NavigableProtoAstNode>>;

  // A const Span like type that provides post-order traversal for a sub tree.
  // provides .begin() and .end() returning bidirectional iterators to
  // const NavigableProtoAstNode&.
  using PostorderRange = common_internal::NavigableAstRange<
      common_internal::PostorderTraits<NavigableProtoAstNode>>;

  // The parent of this node or nullptr if it is a root.
  const NavigableProtoAstNode* absl_nullable parent() const {
    return data_.parent;
  }

  const cel::expr::Expr* absl_nonnull expr() const {
    return data_.expr;
  }

  // The index of this node in the parent's children. -1 if this is a root.
  int child_index() const { return data_.child_index; }

  // The type of traversal from parent to this node.
  ChildKind parent_relation() const { return data_.parent_relation; }

  // The type of this node, analogous to Expr::ExprKindCase.
  NodeKind node_kind() const { return data_.node_kind; }

  // The number of nodes in the tree rooted at this node (including self).
  size_t tree_size() const { return data_.tree_size; }

  // The height of this node in the tree (the number of descendants including
  // self on the longest path).
  size_t height() const { return data_.height; }

  absl::Span<const NavigableProtoAstNode* const> children() const {
    return absl::MakeConstSpan(data_.children);
  }

  // Range over the descendants of this node (including self) using preorder
  // semantics. Each node is visited immediately before all of its descendants.
  //
  // example:
  //  for (const cel::NavigableProtoAstNode& node :
  //  ast.Root().DescendantsPreorder()) {
  //    ...
  //  }
  //
  // Children are traversed in their natural order:
  //   - call arguments are traversed in order (receiver if present is first)
  //   - list elements are traversed in order
  //   - maps are traversed in order (alternating key, value per entry)
  //   - comprehensions are traversed in the order: range, accu_init, condition,
  //   step, result
  PreorderRange DescendantsPreorder() const {
    return PreorderRange(absl::MakeConstSpan(data_.metadata->nodes)
                             .subspan(data_.index, data_.tree_size));
  }

  // Range over the descendants of this node (including self) using postorder
  // semantics. Each node is visited immediately after all of its descendants.
  PostorderRange DescendantsPostorder() const {
    return PostorderRange(absl::MakeConstSpan(data_.metadata->postorder)
                              .subspan(data_.index, data_.tree_size));
  }

 private:
  friend class NavigableProtoAst;

  NavigableProtoAstNode() = default;
  NavigableProtoAstNode(const NavigableProtoAstNode&) = delete;
  NavigableProtoAstNode& operator=(const NavigableProtoAstNode&) = delete;

  common_internal::NavigableAstNodeData<NavigableProtoAstNode> data_;
};

// NavigableExpr provides a view over a CEL AST that allows for generalized
// traversal. The traversal structures are eagerly built on construction,
// requiring a full traversal of the AST. This is intended for use in tools that
// might require random access or multiple passes over the AST, amortizing the
// cost of building the traversal structures.
//
// Pointers to AstNodes are owned by this instance and must not outlive it.
//
// `NavigableProtoAst` and Navigable nodes are independent of the input Expr and
// may outlive it, but may contain dangling pointers if the input Expr is
// modified or destroyed.
class NavigableProtoAst {
 public:
  static NavigableProtoAst Build(const cel::expr::Expr& expr);

  // Default constructor creates an empty instance.
  //
  // Operations other than equality are undefined on an empty instance.
  //
  // This is intended for composed object construction, a new NavigableProtoAst
  // should be obtained from the Build factory function.
  NavigableProtoAst() = default;

  // Move only.
  NavigableProtoAst(const NavigableProtoAst&) = delete;
  NavigableProtoAst& operator=(const NavigableProtoAst&) = delete;
  NavigableProtoAst(NavigableProtoAst&&) = default;
  NavigableProtoAst& operator=(NavigableProtoAst&&) = default;

  // Return ptr to the AST node with id if present. Otherwise returns nullptr.
  //
  // If ids are non-unique, the first pre-order node encountered with id is
  // returned.
  const NavigableProtoAstNode* absl_nullable FindId(int64_t id) const {
    auto it = metadata_->id_to_node.find(id);
    if (it == metadata_->id_to_node.end()) {
      return nullptr;
    }
    return it->second;
  }

  // Return ptr to the AST node representing the given Expr protobuf node.
  const NavigableProtoAstNode* absl_nullable FindExpr(
      const cel::expr::Expr* expr) const {
    auto it = metadata_->expr_to_node.find(expr);
    if (it == metadata_->expr_to_node.end()) {
      return nullptr;
    }
    return it->second;
  }

  // The root of the AST.
  const NavigableProtoAstNode& Root() const { return *metadata_->nodes[0]; }

  // Check whether the source AST used unique IDs for each node.
  //
  // This is typically the case, but older versions of the parsers didn't
  // guarantee uniqueness for nodes generated by some macros and ASTs modified
  // outside of CEL's parse/type check may not have unique IDs.
  bool IdsAreUnique() const {
    return metadata_->id_to_node.size() == metadata_->nodes.size();
  }

  // Equality operators test for identity. They are intended to distinguish
  // moved-from or uninitialized instances from initialized.
  bool operator==(const NavigableProtoAst& other) const {
    return metadata_ == other.metadata_;
  }

  bool operator!=(const NavigableProtoAst& other) const {
    return metadata_ != other.metadata_;
  }

  // Return true if this instance is initialized.
  explicit operator bool() const { return metadata_ != nullptr; }

 private:
  using AstMetadata =
      common_internal::NavigableAstMetadata<NavigableProtoAstNode>;

  explicit NavigableProtoAst(std::unique_ptr<AstMetadata> metadata)
      : metadata_(std::move(metadata)) {}

  std::unique_ptr<AstMetadata> metadata_;
};

// Type aliases for backwards compatibility.
// To be removed.
using AstNode = NavigableProtoAstNode;
using NavigableAst = NavigableProtoAst;

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_TOOLS_NAVIGABLE_AST_H_
