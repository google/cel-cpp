// Copyright 2021 Google LLC
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

#include "parser/parser.h"

#include <algorithm>
#include <any>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "google/api/expr/v1alpha1/syntax.pb.h"
#include "google/protobuf/struct.pb.h"
#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/memory/memory.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/numbers.h"
#include "absl/strings/str_format.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_replace.h"
#include "absl/strings/string_view.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "antlr4-runtime.h"
#include "common/operators.h"
#include "internal/status_macros.h"
#include "internal/strings.h"
#include "internal/unicode.h"
#include "internal/utf8.h"
#include "parser/internal/CelBaseVisitor.h"
#include "parser/internal/CelLexer.h"
#include "parser/internal/CelParser.h"
#include "parser/macro.h"
#include "parser/options.h"
#include "parser/source_factory.h"

namespace google::api::expr::parser {

namespace {

using ::antlr4::CharStream;
using ::antlr4::CommonTokenStream;
using ::antlr4::DefaultErrorStrategy;
using ::antlr4::ParseCancellationException;
using ::antlr4::Parser;
using ::antlr4::ParserRuleContext;
using ::antlr4::Token;
using ::antlr4::misc::IntervalSet;
using ::antlr4::tree::ErrorNode;
using ::antlr4::tree::ParseTreeListener;
using ::antlr4::tree::TerminalNode;
using ::cel_parser_internal::CelBaseVisitor;
using ::cel_parser_internal::CelLexer;
using ::cel_parser_internal::CelParser;
using common::CelOperator;
using common::ReverseLookupOperator;
using ::google::api::expr::v1alpha1::Expr;
using ::google::api::expr::v1alpha1::ParsedExpr;

class CodePointBuffer final {
 public:
  explicit CodePointBuffer(absl::string_view data)
      : storage_(absl::in_place_index<0>, data) {}

  explicit CodePointBuffer(std::string data)
      : storage_(absl::in_place_index<1>, std::move(data)) {}

  explicit CodePointBuffer(std::u16string data)
      : storage_(absl::in_place_index<2>, std::move(data)) {}

  explicit CodePointBuffer(std::u32string data)
      : storage_(absl::in_place_index<3>, std::move(data)) {}

  size_t size() const { return absl::visit(SizeVisitor{}, storage_); }

  char32_t at(size_t index) const {
    ABSL_ASSERT(index < size());
    return absl::visit(AtVisitor{index}, storage_);
  }

  std::string ToString(size_t begin, size_t end) const {
    ABSL_ASSERT(begin <= end);
    ABSL_ASSERT(begin < size());
    ABSL_ASSERT(end <= size());
    return absl::visit(ToStringVisitor{begin, end}, storage_);
  }

 private:
  struct SizeVisitor final {
    size_t operator()(absl::string_view ascii) const { return ascii.size(); }

    size_t operator()(const std::string& latin1) const { return latin1.size(); }

    size_t operator()(const std::u16string& basic) const {
      return basic.size();
    }

    size_t operator()(const std::u32string& supplemental) const {
      return supplemental.size();
    }
  };

  struct AtVisitor final {
    const size_t index;

    size_t operator()(absl::string_view ascii) const {
      return static_cast<uint8_t>(ascii[index]);
    }

    size_t operator()(const std::string& latin1) const {
      return static_cast<uint8_t>(latin1[index]);
    }

    size_t operator()(const std::u16string& basic) const {
      return basic[index];
    }

    size_t operator()(const std::u32string& supplemental) const {
      return supplemental[index];
    }
  };

  struct ToStringVisitor final {
    const size_t begin;
    const size_t end;

    std::string operator()(absl::string_view ascii) const {
      return std::string(ascii.substr(begin, end - begin));
    }

    std::string operator()(const std::string& latin1) const {
      std::string result;
      result.reserve((end - begin) *
                     2);  // Worst case is 2 code units per code point.
      for (size_t index = begin; index < end; index++) {
        cel::internal::Utf8Encode(
            &result,
            static_cast<char32_t>(static_cast<uint8_t>(latin1[index])));
      }
      result.shrink_to_fit();
      return result;
    }

    std::string operator()(const std::u16string& basic) const {
      std::string result;
      result.reserve((end - begin) *
                     3);  // Worst case is 3 code units per code point.
      for (size_t index = begin; index < end; index++) {
        cel::internal::Utf8Encode(&result, static_cast<char32_t>(basic[index]));
      }
      result.shrink_to_fit();
      return result;
    }

    std::string operator()(const std::u32string& supplemental) const {
      std::string result;
      result.reserve((end - begin) *
                     4);  // Worst case is 4 code units per code point.
      for (size_t index = begin; index < end; index++) {
        cel::internal::Utf8Encode(&result, supplemental[index]);
      }
      result.shrink_to_fit();
      return result;
    }
  };

  absl::variant<absl::string_view, std::string, std::u16string, std::u32string>
      storage_;
};

// Given a UTF-8 encoded string and produces a CodePointBuffer which provides
// constant time indexing to each code point. If all code points fall in the
// ASCII range then the view is used as is. If all code points fall in the
// Latin-1 range then the text is represented as std::string. If all code points
// fall in the BMP then the text is represented as std::u16string. Otherwise the
// text is represented as std::u32string. This is much more efficient than the
// default ANTLRv4 implementation which unconditionally converts to
// std::u32string.
absl::StatusOr<CodePointBuffer> MakeCodePointBuffer(absl::string_view text) {
  size_t index = 0;
  char32_t code_point;
  size_t code_units;
  std::string data8;
  std::u16string data16;
  std::u32string data32;
  while (index < text.size()) {
    std::tie(code_point, code_units) =
        cel::internal::Utf8Decode(text.substr(index));
    if (code_point <= 0x7f) {
      index += code_units;
      continue;
    }
    if (code_point <= 0xff) {
      data8.reserve(text.size());
      data8.append(text.data(), index);
      data8.push_back(static_cast<char>(static_cast<uint8_t>(code_point)));
      index += code_units;
      goto latin1;
    }
    if (code_point == cel::internal::kUnicodeReplacementCharacter &&
        code_units == 1) {
      // Thats an invalid UTF-8 encoding.
      return absl::InvalidArgumentError("Cannot parse malformed UTF-8 input");
    }
    if (code_point <= 0xffff) {
      data16.reserve(text.size());
      for (size_t offset = 0; offset < index; offset++) {
        data16.push_back(static_cast<uint8_t>(text[offset]));
      }
      data16.push_back(static_cast<char16_t>(code_point));
      index += code_units;
      goto basic;
    }
    data32.reserve(text.size());
    for (size_t offset = 0; offset < index; offset++) {
      data32.push_back(static_cast<char32_t>(text[offset]));
    }
    data32.push_back(code_point);
    index += code_units;
    goto supplemental;
  }
  return CodePointBuffer(text);
latin1:
  while (index < text.size()) {
    std::tie(code_point, code_units) =
        cel::internal::Utf8Decode(text.substr(index));
    if (code_point <= 0xff) {
      data8.push_back(static_cast<char>(static_cast<uint8_t>(code_point)));
      index += code_units;
      continue;
    }
    if (code_point == cel::internal::kUnicodeReplacementCharacter &&
        code_units == 1) {
      // Thats an invalid UTF-8 encoding.
      return absl::InvalidArgumentError("Cannot parse malformed UTF-8 input");
    }
    if (code_point <= 0xffff) {
      data16.reserve(text.size());
      for (const auto& value : data8) {
        data16.push_back(static_cast<uint8_t>(value));
      }
      std::string().swap(data8);
      data16.push_back(static_cast<char16_t>(code_point));
      index += code_units;
      goto basic;
    }
    data32.reserve(text.size());
    for (const auto& value : data8) {
      data32.push_back(static_cast<uint8_t>(value));
    }
    std::string().swap(data8);
    data32.push_back(code_point);
    index += code_units;
    goto supplemental;
  }
  return CodePointBuffer(std::move(data8));
basic:
  while (index < text.size()) {
    std::tie(code_point, code_units) =
        cel::internal::Utf8Decode(text.substr(index));
    if (code_point == cel::internal::kUnicodeReplacementCharacter &&
        code_units == 1) {
      // Thats an invalid UTF-8 encoding.
      return absl::InvalidArgumentError("Cannot parse malformed UTF-8 input");
    }
    if (code_point <= 0xffff) {
      data16.push_back(static_cast<char16_t>(code_point));
      index += code_units;
      continue;
    }
    data32.reserve(text.size());
    for (const auto& value : data16) {
      data32.push_back(static_cast<char32_t>(value));
    }
    std::u16string().swap(data16);
    data32.push_back(code_point);
    index += code_units;
    goto supplemental;
  }
  return CodePointBuffer(std::move(data16));
supplemental:
  while (index < text.size()) {
    std::tie(code_point, code_units) =
        cel::internal::Utf8Decode(text.substr(index));
    if (code_point == cel::internal::kUnicodeReplacementCharacter &&
        code_units == 1) {
      // Thats an invalid UTF-8 encoding.
      return absl::InvalidArgumentError("Cannot parse malformed UTF-8 input");
    }
    data32.push_back(code_point);
    index += code_units;
  }
  return CodePointBuffer(std::move(data32));
}

class CodePointStream final : public CharStream {
 public:
  CodePointStream(CodePointBuffer* buffer, absl::string_view source_name)
      : buffer_(buffer),
        source_name_(source_name),
        size_(buffer_->size()),
        index_(0) {}

  void consume() override {
    if (ABSL_PREDICT_FALSE(index_ >= size_)) {
      ABSL_ASSERT(LA(1) == IntStream::EOF);
      throw antlr4::IllegalStateException("cannot consume EOF");
    }
    index_++;
  }

  size_t LA(ssize_t i) override {
    if (ABSL_PREDICT_FALSE(i == 0)) {
      return 0;
    }
    auto p = static_cast<ssize_t>(index_);
    if (i < 0) {
      i++;
      if (p + i - 1 < 0) {
        return IntStream::EOF;
      }
    }
    if (p + i - 1 >= static_cast<ssize_t>(size_)) {
      return IntStream::EOF;
    }
    return buffer_->at(static_cast<size_t>(p + i - 1));
  }

  ssize_t mark() override { return -1; }

  void release(ssize_t marker) override {}

  size_t index() override { return index_; }

  void seek(size_t index) override { index_ = std::min(index, size_); }

  size_t size() override { return size_; }

  std::string getSourceName() const override {
    return source_name_.empty() ? IntStream::UNKNOWN_SOURCE_NAME
                                : std::string(source_name_);
  }

  std::string getText(const antlr4::misc::Interval& interval) override {
    if (ABSL_PREDICT_FALSE(interval.a < 0 || interval.b < 0)) {
      return std::string();
    }
    size_t start = static_cast<size_t>(interval.a);
    if (ABSL_PREDICT_FALSE(start >= size_)) {
      return std::string();
    }
    size_t stop = static_cast<size_t>(interval.b);
    if (ABSL_PREDICT_FALSE(stop >= size_)) {
      stop = size_ - 1;
    }
    return buffer_->ToString(start, stop + 1);
  }

  std::string toString() const override { return buffer_->ToString(0, size_); }

 private:
  CodePointBuffer* const buffer_;
  const absl::string_view source_name_;
  const size_t size_;
  size_t index_;
};

// Scoped helper for incrementing the parse recursion count.
// Increments on creation, decrements on destruction (stack unwind).
class ScopedIncrement final {
 public:
  explicit ScopedIncrement(int& recursion_depth)
      : recursion_depth_(recursion_depth) {
    ++recursion_depth_;
  }

  ~ScopedIncrement() { --recursion_depth_; }

 private:
  int& recursion_depth_;
};

// balancer performs tree balancing on operators whose arguments are of equal
// precedence.
//
// The purpose of the balancer is to ensure a compact serialization format for
// the logical &&, || operators which have a tendency to create long DAGs which
// are skewed in one direction. Since the operators are commutative re-ordering
// the terms *must not* affect the evaluation result.
//
// Based on code from //third_party/cel/go/parser/helper.go
class ExpressionBalancer final {
 public:
  ExpressionBalancer(std::shared_ptr<SourceFactory> sf, std::string function,
                     Expr expr);

  // addTerm adds an operation identifier and term to the set of terms to be
  // balanced.
  void AddTerm(int64_t op, Expr term);

  // balance creates a balanced tree from the sub-terms and returns the final
  // Expr value.
  Expr Balance();

 private:
  // balancedTree recursively balances the terms provided to a commutative
  // operator.
  Expr BalancedTree(int lo, int hi);

 private:
  std::shared_ptr<SourceFactory> sf_;
  std::string function_;
  std::vector<Expr> terms_;
  std::vector<int64_t> ops_;
};

ExpressionBalancer::ExpressionBalancer(std::shared_ptr<SourceFactory> sf,
                                       std::string function, Expr expr)
    : sf_(std::move(sf)),
      function_(std::move(function)),
      terms_{std::move(expr)},
      ops_{} {}

void ExpressionBalancer::AddTerm(int64_t op, Expr term) {
  terms_.push_back(std::move(term));
  ops_.push_back(op);
}

Expr ExpressionBalancer::Balance() {
  if (terms_.size() == 1) {
    return terms_[0];
  }
  return BalancedTree(0, ops_.size() - 1);
}

Expr ExpressionBalancer::BalancedTree(int lo, int hi) {
  int mid = (lo + hi + 1) / 2;

  Expr left;
  if (mid == lo) {
    left = terms_[mid];
  } else {
    left = BalancedTree(lo, mid - 1);
  }

  Expr right;
  if (mid == hi) {
    right = terms_[mid + 1];
  } else {
    right = BalancedTree(mid + 1, hi);
  }
  return sf_->NewGlobalCall(ops_[mid], function_,
                            {std::move(left), std::move(right)});
}

class ParserVisitor final : public CelBaseVisitor,
                            public antlr4::BaseErrorListener {
 public:
  ParserVisitor(absl::string_view description, absl::string_view expression,
                const int max_recursion_depth,
                const std::vector<Macro>& macros = {},
                const bool add_macro_calls = false);
  ~ParserVisitor() override;

  antlrcpp::Any visit(antlr4::tree::ParseTree* tree) override;

  antlrcpp::Any visitStart(CelParser::StartContext* ctx) override;
  antlrcpp::Any visitExpr(CelParser::ExprContext* ctx) override;
  antlrcpp::Any visitConditionalOr(
      CelParser::ConditionalOrContext* ctx) override;
  antlrcpp::Any visitConditionalAnd(
      CelParser::ConditionalAndContext* ctx) override;
  antlrcpp::Any visitRelation(CelParser::RelationContext* ctx) override;
  antlrcpp::Any visitCalc(CelParser::CalcContext* ctx) override;
  antlrcpp::Any visitUnary(CelParser::UnaryContext* ctx);
  antlrcpp::Any visitLogicalNot(CelParser::LogicalNotContext* ctx) override;
  antlrcpp::Any visitNegate(CelParser::NegateContext* ctx) override;
  antlrcpp::Any visitSelectOrCall(CelParser::SelectOrCallContext* ctx) override;
  antlrcpp::Any visitIndex(CelParser::IndexContext* ctx) override;
  antlrcpp::Any visitCreateMessage(
      CelParser::CreateMessageContext* ctx) override;
  antlrcpp::Any visitFieldInitializerList(
      CelParser::FieldInitializerListContext* ctx) override;
  antlrcpp::Any visitIdentOrGlobalCall(
      CelParser::IdentOrGlobalCallContext* ctx) override;
  antlrcpp::Any visitNested(CelParser::NestedContext* ctx) override;
  antlrcpp::Any visitCreateList(CelParser::CreateListContext* ctx) override;
  std::vector<google::api::expr::v1alpha1::Expr> visitList(
      CelParser::ExprListContext* ctx);
  antlrcpp::Any visitCreateStruct(CelParser::CreateStructContext* ctx) override;
  antlrcpp::Any visitConstantLiteral(
      CelParser::ConstantLiteralContext* ctx) override;
  antlrcpp::Any visitPrimaryExpr(CelParser::PrimaryExprContext* ctx) override;
  antlrcpp::Any visitMemberExpr(CelParser::MemberExprContext* ctx) override;

  antlrcpp::Any visitMapInitializerList(
      CelParser::MapInitializerListContext* ctx) override;
  antlrcpp::Any visitInt(CelParser::IntContext* ctx) override;
  antlrcpp::Any visitUint(CelParser::UintContext* ctx) override;
  antlrcpp::Any visitDouble(CelParser::DoubleContext* ctx) override;
  antlrcpp::Any visitString(CelParser::StringContext* ctx) override;
  antlrcpp::Any visitBytes(CelParser::BytesContext* ctx) override;
  antlrcpp::Any visitBoolTrue(CelParser::BoolTrueContext* ctx) override;
  antlrcpp::Any visitBoolFalse(CelParser::BoolFalseContext* ctx) override;
  antlrcpp::Any visitNull(CelParser::NullContext* ctx) override;
  google::api::expr::v1alpha1::SourceInfo source_info() const;
  EnrichedSourceInfo enriched_source_info() const;
  void syntaxError(antlr4::Recognizer* recognizer,
                   antlr4::Token* offending_symbol, size_t line, size_t col,
                   const std::string& msg, std::exception_ptr e) override;
  bool HasErrored() const;

  std::string ErrorMessage() const;

 private:
  Expr GlobalCallOrMacro(int64_t expr_id, const std::string& function,
                         const std::vector<Expr>& args);
  Expr ReceiverCallOrMacro(int64_t expr_id, const std::string& function,
                           const Expr& target, const std::vector<Expr>& args);
  bool ExpandMacro(int64_t expr_id, const std::string& function,
                   const Expr& target, const std::vector<Expr>& args,
                   Expr* macro_expr);
  std::string ExtractQualifiedName(antlr4::ParserRuleContext* ctx,
                                   const Expr* e);

 private:
  absl::string_view description_;
  absl::string_view expression_;
  std::shared_ptr<SourceFactory> sf_;
  std::map<std::string, Macro> macros_;
  int recursion_depth_;
  const int max_recursion_depth_;
  const bool add_macro_calls_;
};

ParserVisitor::ParserVisitor(absl::string_view description,
                             absl::string_view expression,
                             const int max_recursion_depth,
                             const std::vector<Macro>& macros,
                             const bool add_macro_calls)
    : description_(description),
      expression_(expression),
      sf_(std::make_shared<SourceFactory>(expression)),
      recursion_depth_(0),
      max_recursion_depth_(max_recursion_depth),
      add_macro_calls_(add_macro_calls) {
  for (const auto& m : macros) {
    macros_.emplace(m.macroKey(), m);
  }
}

ParserVisitor::~ParserVisitor() {}

template <typename T, typename = std::enable_if_t<
                          std::is_base_of<antlr4::tree::ParseTree, T>::value>>
T* tree_as(antlr4::tree::ParseTree* tree) {
  return dynamic_cast<T*>(tree);
}

antlrcpp::Any ParserVisitor::visit(antlr4::tree::ParseTree* tree) {
  ScopedIncrement inc(recursion_depth_);
  if (recursion_depth_ > max_recursion_depth_) {
    return sf_->ReportError(
        SourceFactory::NoLocation(),
        absl::StrFormat("Exceeded max recursion depth of %d when parsing.",
                        max_recursion_depth_));
  }
  if (auto* ctx = tree_as<CelParser::StartContext>(tree)) {
    return visitStart(ctx);
  } else if (auto* ctx = tree_as<CelParser::ExprContext>(tree)) {
    return visitExpr(ctx);
  } else if (auto* ctx = tree_as<CelParser::ConditionalAndContext>(tree)) {
    return visitConditionalAnd(ctx);
  } else if (auto* ctx = tree_as<CelParser::ConditionalOrContext>(tree)) {
    return visitConditionalOr(ctx);
  } else if (auto* ctx = tree_as<CelParser::RelationContext>(tree)) {
    return visitRelation(ctx);
  } else if (auto* ctx = tree_as<CelParser::CalcContext>(tree)) {
    return visitCalc(ctx);
  } else if (auto* ctx = tree_as<CelParser::LogicalNotContext>(tree)) {
    return visitLogicalNot(ctx);
  } else if (auto* ctx = tree_as<CelParser::PrimaryExprContext>(tree)) {
    return visitPrimaryExpr(ctx);
  } else if (auto* ctx = tree_as<CelParser::MemberExprContext>(tree)) {
    return visitMemberExpr(ctx);
  } else if (auto* ctx = tree_as<CelParser::SelectOrCallContext>(tree)) {
    return visitSelectOrCall(ctx);
  } else if (auto* ctx = tree_as<CelParser::MapInitializerListContext>(tree)) {
    return visitMapInitializerList(ctx);
  } else if (auto* ctx = tree_as<CelParser::NegateContext>(tree)) {
    return visitNegate(ctx);
  } else if (auto* ctx = tree_as<CelParser::IndexContext>(tree)) {
    return visitIndex(ctx);
  } else if (auto* ctx = tree_as<CelParser::UnaryContext>(tree)) {
    return visitUnary(ctx);
  } else if (auto* ctx = tree_as<CelParser::CreateListContext>(tree)) {
    return visitCreateList(ctx);
  } else if (auto* ctx = tree_as<CelParser::CreateMessageContext>(tree)) {
    return visitCreateMessage(ctx);
  } else if (auto* ctx = tree_as<CelParser::CreateStructContext>(tree)) {
    return visitCreateStruct(ctx);
  }

  if (tree) {
    return sf_->ReportError(tree_as<antlr4::ParserRuleContext>(tree),
                            "unknown parsetree type");
  }
  return sf_->ReportError(SourceFactory::NoLocation(), "<<nil>> parsetree");
}

antlrcpp::Any ParserVisitor::visitPrimaryExpr(
    CelParser::PrimaryExprContext* pctx) {
  CelParser::PrimaryContext* primary = pctx->primary();
  if (auto* ctx = tree_as<CelParser::NestedContext>(primary)) {
    return visitNested(ctx);
  } else if (auto* ctx =
                 tree_as<CelParser::IdentOrGlobalCallContext>(primary)) {
    return visitIdentOrGlobalCall(ctx);
  } else if (auto* ctx = tree_as<CelParser::CreateListContext>(primary)) {
    return visitCreateList(ctx);
  } else if (auto* ctx = tree_as<CelParser::CreateStructContext>(primary)) {
    return visitCreateStruct(ctx);
  } else if (auto* ctx = tree_as<CelParser::ConstantLiteralContext>(primary)) {
    return visitConstantLiteral(ctx);
  }
  return sf_->ReportError(pctx, "invalid primary expression");
}

antlrcpp::Any ParserVisitor::visitMemberExpr(
    CelParser::MemberExprContext* mctx) {
  CelParser::MemberContext* member = mctx->member();
  if (auto* ctx = tree_as<CelParser::PrimaryExprContext>(member)) {
    return visitPrimaryExpr(ctx);
  } else if (auto* ctx = tree_as<CelParser::SelectOrCallContext>(member)) {
    return visitSelectOrCall(ctx);
  } else if (auto* ctx = tree_as<CelParser::IndexContext>(member)) {
    return visitIndex(ctx);
  } else if (auto* ctx = tree_as<CelParser::CreateMessageContext>(member)) {
    return visitCreateMessage(ctx);
  }
  return sf_->ReportError(mctx, "unsupported simple expression");
}

antlrcpp::Any ParserVisitor::visitStart(CelParser::StartContext* ctx) {
  return visit(ctx->expr());
}

antlrcpp::Any ParserVisitor::visitExpr(CelParser::ExprContext* ctx) {
  auto result = std::any_cast<Expr>(visit(ctx->e));
  if (!ctx->op) {
    return result;
  }
  int64_t op_id = sf_->Id(ctx->op);
  Expr if_true = std::any_cast<Expr>(visit(ctx->e1));
  Expr if_false = std::any_cast<Expr>(visit(ctx->e2));

  return GlobalCallOrMacro(op_id, CelOperator::CONDITIONAL,
                           {result, if_true, if_false});
}

antlrcpp::Any ParserVisitor::visitConditionalOr(
    CelParser::ConditionalOrContext* ctx) {
  auto result = std::any_cast<Expr>(visit(ctx->e));
  if (ctx->ops.empty()) {
    return result;
  }
  ExpressionBalancer b(sf_, CelOperator::LOGICAL_OR, result);
  for (size_t i = 0; i < ctx->ops.size(); ++i) {
    auto op = ctx->ops[i];
    if (i >= ctx->e1.size()) {
      return sf_->ReportError(ctx, "unexpected character, wanted '||'");
    }
    auto next = std::any_cast<Expr>(visit(ctx->e1[i]));
    int64_t op_id = sf_->Id(op);
    b.AddTerm(op_id, next);
  }
  return b.Balance();
}

antlrcpp::Any ParserVisitor::visitConditionalAnd(
    CelParser::ConditionalAndContext* ctx) {
  auto result = std::any_cast<Expr>(visit(ctx->e));
  if (ctx->ops.empty()) {
    return result;
  }
  ExpressionBalancer b(sf_, CelOperator::LOGICAL_AND, result);
  for (size_t i = 0; i < ctx->ops.size(); ++i) {
    auto op = ctx->ops[i];
    if (i >= ctx->e1.size()) {
      return sf_->ReportError(ctx, "unexpected character, wanted '&&'");
    }
    auto next = std::any_cast<Expr>(visit(ctx->e1[i]));
    int64_t op_id = sf_->Id(op);
    b.AddTerm(op_id, next);
  }
  return b.Balance();
}

antlrcpp::Any ParserVisitor::visitRelation(CelParser::RelationContext* ctx) {
  if (ctx->calc()) {
    return visit(ctx->calc());
  }
  std::string op_text;
  if (ctx->op) {
    op_text = ctx->op->getText();
  }
  auto op = ReverseLookupOperator(op_text);
  if (op) {
    auto lhs = std::any_cast<Expr>(visit(ctx->relation(0)));
    int64_t op_id = sf_->Id(ctx->op);
    auto rhs = std::any_cast<Expr>(visit(ctx->relation(1)));
    return GlobalCallOrMacro(op_id, *op, {lhs, rhs});
  }
  return sf_->ReportError(ctx, "operator not found");
}

antlrcpp::Any ParserVisitor::visitCalc(CelParser::CalcContext* ctx) {
  if (ctx->unary()) {
    return visit(ctx->unary());
  }
  std::string op_text;
  if (ctx->op) {
    op_text = ctx->op->getText();
  }
  auto op = ReverseLookupOperator(op_text);
  if (op) {
    auto lhs = std::any_cast<Expr>(visit(ctx->calc(0)));
    int64_t op_id = sf_->Id(ctx->op);
    auto rhs = std::any_cast<Expr>(visit(ctx->calc(1)));
    return GlobalCallOrMacro(op_id, *op, {lhs, rhs});
  }
  return sf_->ReportError(ctx, "operator not found");
}

antlrcpp::Any ParserVisitor::visitUnary(CelParser::UnaryContext* ctx) {
  return sf_->NewLiteralString(ctx, "<<error>>");
}

antlrcpp::Any ParserVisitor::visitLogicalNot(
    CelParser::LogicalNotContext* ctx) {
  if (ctx->ops.size() % 2 == 0) {
    return visit(ctx->member());
  }
  int64_t op_id = sf_->Id(ctx->ops[0]);
  auto target = std::any_cast<Expr>(visit(ctx->member()));
  return GlobalCallOrMacro(op_id, CelOperator::LOGICAL_NOT, {target});
}

antlrcpp::Any ParserVisitor::visitNegate(CelParser::NegateContext* ctx) {
  if (ctx->ops.size() % 2 == 0) {
    return visit(ctx->member());
  }
  int64_t op_id = sf_->Id(ctx->ops[0]);
  auto target = std::any_cast<Expr>(visit(ctx->member()));
  return GlobalCallOrMacro(op_id, CelOperator::NEGATE, {target});
}

antlrcpp::Any ParserVisitor::visitSelectOrCall(
    CelParser::SelectOrCallContext* ctx) {
  auto operand = std::any_cast<Expr>(visit(ctx->member()));
  // Handle the error case where no valid identifier is specified.
  if (!ctx->id) {
    return sf_->NewExpr(ctx);
  }
  auto id = ctx->id->getText();
  if (ctx->open) {
    int64_t op_id = sf_->Id(ctx->open);
    return ReceiverCallOrMacro(op_id, id, operand, visitList(ctx->args));
  }
  return sf_->NewSelect(ctx, operand, id);
}

antlrcpp::Any ParserVisitor::visitIndex(CelParser::IndexContext* ctx) {
  auto target = std::any_cast<Expr>(visit(ctx->member()));
  int64_t op_id = sf_->Id(ctx->op);
  auto index = std::any_cast<Expr>(visit(ctx->index));
  return GlobalCallOrMacro(op_id, CelOperator::INDEX, {target, index});
}

antlrcpp::Any ParserVisitor::visitCreateMessage(
    CelParser::CreateMessageContext* ctx) {
  auto target = std::any_cast<Expr>(visit(ctx->member()));
  int64_t obj_id = sf_->Id(ctx->op);
  std::string message_name = ExtractQualifiedName(ctx, &target);
  if (!message_name.empty()) {
    auto entries = std::any_cast<std::vector<Expr::CreateStruct::Entry>>(
        visitFieldInitializerList(ctx->entries));
    return sf_->NewObject(obj_id, message_name, entries);
  } else {
    return sf_->NewExpr(obj_id);
  }
}

antlrcpp::Any ParserVisitor::visitFieldInitializerList(
    CelParser::FieldInitializerListContext* ctx) {
  std::vector<Expr::CreateStruct::Entry> res;
  if (!ctx || ctx->fields.empty()) {
    return res;
  }

  res.resize(ctx->fields.size());
  for (size_t i = 0; i < ctx->fields.size(); ++i) {
    if (i >= ctx->cols.size() || i >= ctx->values.size()) {
      // This is the result of a syntax error detected elsewhere.
      return res;
    }
    const auto& f = ctx->fields[i];
    int64_t init_id = sf_->Id(ctx->cols[i]);
    auto value = std::any_cast<Expr>(visit(ctx->values[i]));
    auto field = sf_->NewObjectField(init_id, f->getText(), value);
    res[i] = field;
  }

  return res;
}

antlrcpp::Any ParserVisitor::visitIdentOrGlobalCall(
    CelParser::IdentOrGlobalCallContext* ctx) {
  std::string ident_name;
  if (ctx->leadingDot) {
    ident_name = ".";
  }
  if (!ctx->id) {
    return sf_->NewExpr(ctx);
  }
  if (sf_->IsReserved(ctx->id->getText())) {
    return sf_->ReportError(
        ctx, absl::StrFormat("reserved identifier: %s", ctx->id->getText()));
  }
  // check if ID is in reserved identifiers
  ident_name += ctx->id->getText();
  if (ctx->op) {
    int64_t op_id = sf_->Id(ctx->op);
    return GlobalCallOrMacro(op_id, ident_name, visitList(ctx->args));
  }
  return sf_->NewIdent(ctx->id, ident_name);
}

antlrcpp::Any ParserVisitor::visitNested(CelParser::NestedContext* ctx) {
  return visit(ctx->e);
}

antlrcpp::Any ParserVisitor::visitCreateList(
    CelParser::CreateListContext* ctx) {
  int64_t list_id = sf_->Id(ctx->op);
  return sf_->NewList(list_id, visitList(ctx->elems));
}

std::vector<Expr> ParserVisitor::visitList(CelParser::ExprListContext* ctx) {
  std::vector<Expr> rv;
  if (!ctx) return rv;
  std::transform(ctx->e.begin(), ctx->e.end(), std::back_inserter(rv),
                 [this](CelParser::ExprContext* expr_ctx) {
                   return std::any_cast<Expr>(visitExpr(expr_ctx));
                 });
  return rv;
}

antlrcpp::Any ParserVisitor::visitCreateStruct(
    CelParser::CreateStructContext* ctx) {
  int64_t struct_id = sf_->Id(ctx->op);
  std::vector<Expr::CreateStruct::Entry> entries;
  if (ctx->entries) {
    entries = std::any_cast<std::vector<Expr::CreateStruct::Entry>>(
        visitMapInitializerList(ctx->entries));
  }
  return sf_->NewMap(struct_id, entries);
}

antlrcpp::Any ParserVisitor::visitConstantLiteral(
    CelParser::ConstantLiteralContext* clctx) {
  CelParser::LiteralContext* literal = clctx->literal();
  if (auto* ctx = tree_as<CelParser::IntContext>(literal)) {
    return visitInt(ctx);
  } else if (auto* ctx = tree_as<CelParser::UintContext>(literal)) {
    return visitUint(ctx);
  } else if (auto* ctx = tree_as<CelParser::DoubleContext>(literal)) {
    return visitDouble(ctx);
  } else if (auto* ctx = tree_as<CelParser::StringContext>(literal)) {
    return visitString(ctx);
  } else if (auto* ctx = tree_as<CelParser::BytesContext>(literal)) {
    return visitBytes(ctx);
  } else if (auto* ctx = tree_as<CelParser::BoolFalseContext>(literal)) {
    return visitBoolFalse(ctx);
  } else if (auto* ctx = tree_as<CelParser::BoolTrueContext>(literal)) {
    return visitBoolTrue(ctx);
  } else if (auto* ctx = tree_as<CelParser::NullContext>(literal)) {
    return visitNull(ctx);
  }
  return sf_->ReportError(clctx, "invalid constant literal expression");
}

antlrcpp::Any ParserVisitor::visitMapInitializerList(
    CelParser::MapInitializerListContext* ctx) {
  std::vector<Expr::CreateStruct::Entry> res;
  if (!ctx || ctx->keys.empty()) {
    return res;
  }

  res.resize(ctx->cols.size());
  for (size_t i = 0; i < ctx->cols.size(); ++i) {
    int64_t col_id = sf_->Id(ctx->cols[i]);
    auto key = std::any_cast<Expr>(visit(ctx->keys[i]));
    auto value = std::any_cast<Expr>(visit(ctx->values[i]));
    res[i] = sf_->NewMapEntry(col_id, key, value);
  }
  return res;
}

antlrcpp::Any ParserVisitor::visitInt(CelParser::IntContext* ctx) {
  std::string value;
  if (ctx->sign) {
    value = ctx->sign->getText();
  }
  value += ctx->tok->getText();
  int64_t int_value;
  if (absl::StartsWith(ctx->tok->getText(), "0x")) {
    if (absl::SimpleHexAtoi(value, &int_value)) {
      return sf_->NewLiteralInt(ctx, int_value);
    } else {
      return sf_->ReportError(ctx, "invalid hex int literal");
    }
  }
  if (absl::SimpleAtoi(value, &int_value)) {
    return sf_->NewLiteralInt(ctx, int_value);
  } else {
    return sf_->ReportError(ctx, "invalid int literal");
  }
}

antlrcpp::Any ParserVisitor::visitUint(CelParser::UintContext* ctx) {
  std::string value = ctx->tok->getText();
  // trim the 'u' designator included in the uint literal.
  if (!value.empty()) {
    value.resize(value.size() - 1);
  }
  uint64_t uint_value;
  if (absl::StartsWith(ctx->tok->getText(), "0x")) {
    if (absl::SimpleHexAtoi(value, &uint_value)) {
      return sf_->NewLiteralUint(ctx, uint_value);
    } else {
      return sf_->ReportError(ctx, "invalid hex uint literal");
    }
  }
  if (absl::SimpleAtoi(value, &uint_value)) {
    return sf_->NewLiteralUint(ctx, uint_value);
  } else {
    return sf_->ReportError(ctx, "invalid uint literal");
  }
}

antlrcpp::Any ParserVisitor::visitDouble(CelParser::DoubleContext* ctx) {
  std::string value;
  if (ctx->sign) {
    value = ctx->sign->getText();
  }
  value += ctx->tok->getText();
  double double_value;
  if (absl::SimpleAtod(value, &double_value)) {
    return sf_->NewLiteralDouble(ctx, double_value);
  } else {
    return sf_->ReportError(ctx, "invalid double literal");
  }
}

antlrcpp::Any ParserVisitor::visitString(CelParser::StringContext* ctx) {
  auto status_or_value = cel::internal::ParseStringLiteral(ctx->tok->getText());
  if (!status_or_value.ok()) {
    return sf_->ReportError(ctx, status_or_value.status().message());
  }
  return sf_->NewLiteralString(ctx, status_or_value.value());
}

antlrcpp::Any ParserVisitor::visitBytes(CelParser::BytesContext* ctx) {
  auto status_or_value = cel::internal::ParseBytesLiteral(ctx->tok->getText());
  if (!status_or_value.ok()) {
    return sf_->ReportError(ctx, status_or_value.status().message());
  }
  return sf_->NewLiteralBytes(ctx, status_or_value.value());
}

antlrcpp::Any ParserVisitor::visitBoolTrue(CelParser::BoolTrueContext* ctx) {
  return sf_->NewLiteralBool(ctx, true);
}

antlrcpp::Any ParserVisitor::visitBoolFalse(CelParser::BoolFalseContext* ctx) {
  return sf_->NewLiteralBool(ctx, false);
}

antlrcpp::Any ParserVisitor::visitNull(CelParser::NullContext* ctx) {
  return sf_->NewLiteralNull(ctx);
}

google::api::expr::v1alpha1::SourceInfo ParserVisitor::source_info() const {
  return sf_->source_info();
}

EnrichedSourceInfo ParserVisitor::enriched_source_info() const {
  return sf_->enriched_source_info();
}

void ParserVisitor::syntaxError(antlr4::Recognizer* recognizer,
                                antlr4::Token* offending_symbol, size_t line,
                                size_t col, const std::string& msg,
                                std::exception_ptr e) {
  sf_->ReportError(line, col, "Syntax error: " + msg);
}

bool ParserVisitor::HasErrored() const { return !sf_->errors().empty(); }

std::string ParserVisitor::ErrorMessage() const {
  return sf_->ErrorMessage(description_, expression_);
}

Expr ParserVisitor::GlobalCallOrMacro(int64_t expr_id,
                                      const std::string& function,
                                      const std::vector<Expr>& args) {
  Expr macro_expr;
  if (ExpandMacro(expr_id, function, Expr::default_instance(), args,
                  &macro_expr)) {
    return macro_expr;
  }

  return sf_->NewGlobalCall(expr_id, function, args);
}

Expr ParserVisitor::ReceiverCallOrMacro(int64_t expr_id,
                                        const std::string& function,
                                        const Expr& target,
                                        const std::vector<Expr>& args) {
  Expr macro_expr;
  if (ExpandMacro(expr_id, function, target, args, &macro_expr)) {
    return macro_expr;
  }

  return sf_->NewReceiverCall(expr_id, function, target, args);
}

bool ParserVisitor::ExpandMacro(int64_t expr_id, const std::string& function,
                                const Expr& target,
                                const std::vector<Expr>& args,
                                Expr* macro_expr) {
  std::string macro_key = absl::StrFormat("%s:%d:%s", function, args.size(),
                                          target.id() != 0 ? "true" : "false");
  auto m = macros_.find(macro_key);
  if (m == macros_.end()) {
    std::string var_arg_macro_key = absl::StrFormat(
        "%s:*:%s", function, target.id() != 0 ? "true" : "false");
    m = macros_.find(var_arg_macro_key);
    if (m == macros_.end()) {
      return false;
    }
  }

  Expr expr = m->second.expand(sf_, expr_id, target, args);
  if (expr.expr_kind_case() != Expr::EXPR_KIND_NOT_SET) {
    *macro_expr = std::move(expr);
    if (add_macro_calls_) {
      // If the macro is nested, the full expression id is used as an argument
      // id in the tree. Using this ID instead of expr_id allows argument id
      // lookups in macro_calls when building the map and iterating
      // the AST.
      sf_->AddMacroCall(macro_expr->id(), target, args, function);
    }
    return true;
  }
  return false;
}

std::string ParserVisitor::ExtractQualifiedName(antlr4::ParserRuleContext* ctx,
                                                const Expr* e) {
  if (!e) {
    return "";
  }

  switch (e->expr_kind_case()) {
    case Expr::kIdentExpr:
      return e->ident_expr().name();
    case Expr::kSelectExpr: {
      auto& s = e->select_expr();
      std::string prefix = ExtractQualifiedName(ctx, &s.operand());
      if (!prefix.empty()) {
        return prefix + "." + s.field();
      }
    } break;
    default:
      break;
  }
  sf_->ReportError(sf_->GetSourceLocation(e->id()),
                   "expected a qualified name");
  return "";
}

// Replacements for absl::StrReplaceAll for escaping standard whitespace
// characters.
static constexpr auto kStandardReplacements =
    std::array<std::pair<absl::string_view, absl::string_view>, 3>{
        std::make_pair("\n", "\\n"),
        std::make_pair("\r", "\\r"),
        std::make_pair("\t", "\\t"),
    };

static constexpr absl::string_view kSingleQuote = "'";

// ExprRecursionListener extends the standard ANTLR CelParser to ensure that
// recursive entries into the 'expr' rule are limited to a configurable depth so
// as to prevent stack overflows.
class ExprRecursionListener : public ParseTreeListener {
 public:
  explicit ExprRecursionListener(
      const int max_recursion_depth = kDefaultMaxRecursionDepth)
      : max_recursion_depth_(max_recursion_depth), recursion_depth_(0) {}
  ~ExprRecursionListener() override {}

  void visitTerminal(TerminalNode* node) override{};
  void visitErrorNode(ErrorNode* error) override{};
  void enterEveryRule(ParserRuleContext* ctx) override;
  void exitEveryRule(ParserRuleContext* ctx) override;

 private:
  const int max_recursion_depth_;
  int recursion_depth_;
};

void ExprRecursionListener::enterEveryRule(ParserRuleContext* ctx) {
  // Throw a ParseCancellationException since the parsing would otherwise
  // continue if this were treated as a syntax error and the problem would
  // continue to manifest.
  if (ctx->getRuleIndex() == CelParser::RuleExpr) {
    if (recursion_depth_ >= max_recursion_depth_) {
      throw ParseCancellationException(
          absl::StrFormat("Expression recursion limit exceeded. limit: %d",
                          max_recursion_depth_));
    }
    recursion_depth_++;
  }
}

void ExprRecursionListener::exitEveryRule(ParserRuleContext* ctx) {
  if (ctx->getRuleIndex() == CelParser::RuleExpr) {
    recursion_depth_--;
  }
}

class RecoveryLimitErrorStrategy : public DefaultErrorStrategy {
 public:
  explicit RecoveryLimitErrorStrategy(
      int recovery_limit = kDefaultErrorRecoveryLimit,
      int recovery_token_lookahead_limit =
          kDefaultErrorRecoveryTokenLookaheadLimit)
      : recovery_limit_(recovery_limit),
        recovery_attempts_(0),
        recovery_token_lookahead_limit_(recovery_token_lookahead_limit) {}

  void recover(Parser* recognizer, std::exception_ptr e) override {
    checkRecoveryLimit(recognizer);
    DefaultErrorStrategy::recover(recognizer, e);
  }

  Token* recoverInline(Parser* recognizer) override {
    checkRecoveryLimit(recognizer);
    return DefaultErrorStrategy::recoverInline(recognizer);
  }

  // Override the ANTLR implementation to introduce a token lookahead limit as
  // this prevents pathologically constructed, yet small (< 16kb) inputs from
  // consuming inordinate amounts of compute.
  //
  // This method is only called on error recovery paths.
  void consumeUntil(Parser* recognizer, const IntervalSet& set) override {
    size_t ttype = recognizer->getInputStream()->LA(1);
    int recovery_search_depth = 0;
    while (ttype != Token::EOF && !set.contains(ttype) &&
           recovery_search_depth++ < recovery_token_lookahead_limit_) {
      recognizer->consume();
      ttype = recognizer->getInputStream()->LA(1);
    }
    // Halt all parsing if the lookahead limit is reached during error recovery.
    if (recovery_search_depth == recovery_token_lookahead_limit_) {
      throw ParseCancellationException("Unable to find a recovery token");
    }
  }

 protected:
  std::string escapeWSAndQuote(const std::string& s) const override {
    std::string result;
    result.reserve(s.size() + 2);
    absl::StrAppend(&result, kSingleQuote, s, kSingleQuote);
    absl::StrReplaceAll(kStandardReplacements, &result);
    return result;
  }

 private:
  void checkRecoveryLimit(Parser* recognizer) {
    if (recovery_attempts_++ >= recovery_limit_) {
      std::string too_many_errors =
          absl::StrFormat("More than %d parse errors.", recovery_limit_);
      recognizer->notifyErrorListeners(too_many_errors);
      throw ParseCancellationException(too_many_errors);
    }
  }

  int recovery_limit_;
  int recovery_attempts_;
  int recovery_token_lookahead_limit_;
};

}  // namespace

absl::StatusOr<ParsedExpr> Parse(absl::string_view expression,
                                 absl::string_view description,
                                 const ParserOptions& options) {
  return ParseWithMacros(expression, Macro::AllMacros(), description, options);
}

absl::StatusOr<ParsedExpr> ParseWithMacros(absl::string_view expression,
                                           const std::vector<Macro>& macros,
                                           absl::string_view description,
                                           const ParserOptions& options) {
  CEL_ASSIGN_OR_RETURN(auto verbose_parsed_expr,
                       EnrichedParse(expression, macros, description, options));
  return verbose_parsed_expr.parsed_expr();
}

absl::StatusOr<VerboseParsedExpr> EnrichedParse(
    absl::string_view expression, const std::vector<Macro>& macros,
    absl::string_view description, const ParserOptions& options) {
  try {
    CEL_ASSIGN_OR_RETURN(auto buffer, MakeCodePointBuffer(expression));
    CodePointStream input(&buffer, description);
    if (input.size() > options.expression_size_codepoint_limit) {
      return absl::InvalidArgumentError(absl::StrCat(
          "expression size exceeds codepoint limit.", " input size: ",
          input.size(), ", limit: ", options.expression_size_codepoint_limit));
    }
    CelLexer lexer(&input);
    CommonTokenStream tokens(&lexer);
    CelParser parser(&tokens);
    ExprRecursionListener listener(options.max_recursion_depth);
    ParserVisitor visitor(description, expression, options.max_recursion_depth,
                          macros, options.add_macro_calls);

    lexer.removeErrorListeners();
    parser.removeErrorListeners();
    lexer.addErrorListener(&visitor);
    parser.addErrorListener(&visitor);
    parser.addParseListener(&listener);

    // Limit the number of error recovery attempts to prevent bad expressions
    // from consuming lots of cpu / memory.
    parser.setErrorHandler(std::make_shared<RecoveryLimitErrorStrategy>(
        options.error_recovery_limit,
        options.error_recovery_token_lookahead_limit));

    Expr expr;
    try {
      expr = std::any_cast<Expr>(visitor.visit(parser.start()));
    } catch (const ParseCancellationException& e) {
      if (visitor.HasErrored()) {
        return absl::InvalidArgumentError(visitor.ErrorMessage());
      }
      return absl::CancelledError(e.what());
    }

    if (visitor.HasErrored()) {
      return absl::InvalidArgumentError(visitor.ErrorMessage());
    }

    // root is deleted as part of the parser context
    ParsedExpr parsed_expr;
    *(parsed_expr.mutable_expr()) = std::move(expr);
    auto enriched_source_info = visitor.enriched_source_info();
    *(parsed_expr.mutable_source_info()) = visitor.source_info();
    return VerboseParsedExpr(std::move(parsed_expr),
                             std::move(enriched_source_info));
  } catch (const std::exception& e) {
    return absl::AbortedError(e.what());
  } catch (const char* what) {
    // ANTLRv4 has historically thrown C string literals.
    return absl::AbortedError(what);
  } catch (...) {
    // We guarantee to never throw and always return a status.
    return absl::UnknownError("An unknown exception occurred");
  }
}

}  // namespace google::api::expr::parser
