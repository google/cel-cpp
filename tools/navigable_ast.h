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
#include <string>
#include <utility>
#include <vector>

#include "cel/expr/syntax.pb.h"
#include "absl/base/nullability.h"
#include "absl/container/flat_hash_map.h"
#include "absl/types/span.h"
#include "tools/internal/navigable_ast_internal.h"

namespace cel {

enum class ChildKind {
  kUnspecified,
  kSelectOperand,
  kCallReceiver,
  kCallArg,
  kListElem,
  kMapKey,
  kMapValue,
  kStructValue,
  kComprehensionRange,
  kComprehensionInit,
  kComprehensionCondition,
  kComprehensionLoopStep,
  kComprensionResult
};

enum class NodeKind {
  kUnspecified,
  kConstant,
  kIdent,
  kSelect,
  kCall,
  kList,
  kMap,
  kStruct,
  kComprehension,
};

// Human readable ChildKind name. Provided for test readability -- do not depend
// on the specific values.
std::string ChildKindName(ChildKind kind);

template <typename Sink>
void AbslStringify(Sink& sink, ChildKind kind) {
  absl::Format(&sink, "%s", ChildKindName(kind));
}

// Human readable NodeKind name. Provided for test readability -- do not depend
// on the specific values.
std::string NodeKindName(NodeKind kind);

template <typename Sink>
void AbslStringify(Sink& sink, NodeKind kind) {
  absl::Format(&sink, "%s", NodeKindName(kind));
}

class AstNode;

namespace tools_internal {

struct AstMetadata;

// Internal implementation for data-structures handling cross-referencing nodes.
//
// This is exposed separately to allow building up the AST relationships
// without exposing too much mutable state on the non-internal classes.
struct AstNodeData {
  AstNode* parent;
  const ::cel::expr::Expr* expr;
  ChildKind parent_relation;
  NodeKind node_kind;
  const AstMetadata* metadata;
  size_t index;
  size_t tree_size;
  size_t height;
  std::vector<AstNode*> children;
};

struct AstMetadata {
  // The nodes in the AST in preorder.
  //
  // unique_ptr is used to guarantee pointer stability in the other tables.
  std::vector<std::unique_ptr<AstNode>> nodes;
  std::vector<const AstNode* absl_nonnull> postorder;
  absl::flat_hash_map<int64_t, const AstNode* absl_nonnull> id_to_node;
  absl::flat_hash_map<const cel::expr::Expr*,
                      const AstNode* absl_nonnull>
      expr_to_node;

  AstNodeData& NodeDataAt(size_t index);
  size_t AddNode();
};

struct PostorderTraits {
  using UnderlyingType = const AstNode*;
  static const AstNode& Adapt(const AstNode* const node) { return *node; }
};

struct PreorderTraits {
  using UnderlyingType = std::unique_ptr<AstNode>;
  static const AstNode& Adapt(const std::unique_ptr<AstNode>& node) {
    return *node;
  }
};

}  // namespace tools_internal

// Wrapper around a CEL AST node that exposes traversal information.
class AstNode {
 public:
  // A const Span like type that provides pre-order traversal for a sub tree.
  // provides .begin() and .end() returning bidirectional iterators to
  // const AstNode&.
  using PreorderRange =
      tools_internal::NavigableAstRange<tools_internal::PreorderTraits>;

  // A const Span like type that provides post-order traversal for a sub tree.
  // provides .begin() and .end() returning bidirectional iterators to
  // const AstNode&.
  using PostorderRange =
      tools_internal::NavigableAstRange<tools_internal::PostorderTraits>;

  // The parent of this node or nullptr if it is a root.
  const AstNode* absl_nullable parent() const { return data_.parent; }

  const cel::expr::Expr* absl_nonnull expr() const {
    return data_.expr;
  }

  // The index of this node in the parent's children.
  int child_index() const;

  // The type of traversal from parent to this node.
  ChildKind parent_relation() const { return data_.parent_relation; }

  // The type of this node, analogous to Expr::ExprKindCase.
  NodeKind node_kind() const { return data_.node_kind; }

  // The number of nodes in the tree rooted at this node (including self).
  size_t tree_size() const { return data_.tree_size; }

  // The height of this node in the tree (the number of descendants including
  // self on the longest path).
  size_t height() const { return data_.height; }

  absl::Span<const AstNode* const> children() const {
    return absl::MakeConstSpan(data_.children);
  }

  // Range over the descendants of this node (including self) using preorder
  // semantics. Each node is visited immediately before all of its descendants.
  //
  // example:
  //  for (const cel::AstNode& node : ast.Root().DescendantsPreorder()) {
  //    ...
  //  }
  //
  // Children are traversed in their natural order:
  //   - call arguments are traversed in order (receiver if present is first)
  //   - list elements are traversed in order
  //   - maps are traversed in order (alternating key, value per entry)
  //   - comprehensions are traversed in the order: range, accu_init, condition,
  //   step, result
  PreorderRange DescendantsPreorder() const;

  // Range over the descendants of this node (including self) using postorder
  // semantics. Each node is visited immediately after all of its descendants.
  PostorderRange DescendantsPostorder() const;

 private:
  friend struct tools_internal::AstMetadata;

  AstNode() = default;
  AstNode(const AstNode&) = delete;
  AstNode& operator=(const AstNode&) = delete;

  tools_internal::AstNodeData data_;
};

// NavigableExpr provides a view over a CEL AST that allows for generalized
// traversal. The traversal structures are eagerly built on construction,
// requiring a full traversal of the AST. This is intended for use in tools that
// might require random access or multiple passes over the AST, amortizing the
// cost of building the traversal structures.
//
// Pointers to AstNodes are owned by this instance and must not outlive it.
//
// `NavigableAst` and Navigable nodes are independent of the input Expr and may
// outlive it, but may contain dangling pointers if the input Expr is modified
// or destroyed.
class NavigableAst {
 public:
  static NavigableAst Build(const cel::expr::Expr& expr);

  // Default constructor creates an empty instance.
  //
  // Operations other than equality are undefined on an empty instance.
  //
  // This is intended for composed object construction, a new NavigableAst
  // should be obtained from the Build factory function.
  NavigableAst() = default;

  // Move only.
  NavigableAst(const NavigableAst&) = delete;
  NavigableAst& operator=(const NavigableAst&) = delete;
  NavigableAst(NavigableAst&&) = default;
  NavigableAst& operator=(NavigableAst&&) = default;

  // Return ptr to the AST node with id if present. Otherwise returns nullptr.
  //
  // If ids are non-unique, the first pre-order node encountered with id is
  // returned.
  const AstNode* absl_nullable FindId(int64_t id) const {
    auto it = metadata_->id_to_node.find(id);
    if (it == metadata_->id_to_node.end()) {
      return nullptr;
    }
    return it->second;
  }

  // Return ptr to the AST node representing the given Expr protobuf node.
  const AstNode* absl_nullable FindExpr(
      const cel::expr::Expr* expr) const {
    auto it = metadata_->expr_to_node.find(expr);
    if (it == metadata_->expr_to_node.end()) {
      return nullptr;
    }
    return it->second;
  }

  // The root of the AST.
  const AstNode& Root() const { return *metadata_->nodes[0]; }

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
  bool operator==(const NavigableAst& other) const {
    return metadata_ == other.metadata_;
  }

  bool operator!=(const NavigableAst& other) const {
    return metadata_ != other.metadata_;
  }

  // Return true if this instance is initialized.
  explicit operator bool() const { return metadata_ != nullptr; }

 private:
  explicit NavigableAst(std::unique_ptr<tools_internal::AstMetadata> metadata)
      : metadata_(std::move(metadata)) {}

  std::unique_ptr<tools_internal::AstMetadata> metadata_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_TOOLS_NAVIGABLE_AST_H_
