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

#include "testutil/expr_printer.h"

#include <algorithm>
#include <string>

#include "absl/strings/str_format.h"
#include "internal/strings.h"

namespace google {
namespace api {
namespace expr {
namespace testutil {
namespace {

using ::google::api::expr::v1alpha1::Expr;

class EmptyAdorner : public ExpressionAdorner {
 public:
  ~EmptyAdorner() override {}

  std::string adorn(const Expr& e) const override { return ""; }

  std::string adorn(const Expr::CreateStruct::Entry& e) const override {
    return "";
  }
};

const EmptyAdorner the_empty_adorner;

class Writer {
 public:
  explicit Writer(const ExpressionAdorner& adorner)
      : adorner_(adorner), line_start_(true), indent_(0) {}

  void appendExpr(const Expr& e) {
    switch (e.expr_kind_case()) {
      case Expr::kConstExpr:
        append(formatLiteral(e.const_expr()));
        break;
      case Expr::kIdentExpr:
        append(e.ident_expr().name());
        break;
      case Expr::kSelectExpr:
        appendSelect(e.select_expr());
        break;
      case Expr::kCallExpr:
        appendCall(e.call_expr());
        break;
      case Expr::kListExpr:
        appendList(e.list_expr());
        break;
      case Expr::kStructExpr:
        appendStruct(e.struct_expr());
        break;
      case Expr::kComprehensionExpr:
        appendComprehension(e.comprehension_expr());
        break;
      default:
        break;
    }
    appendAdorn(e);
  }

  void appendSelect(const Expr::Select& sel) {
    appendExpr(sel.operand());
    append(".");
    append(sel.field());
    if (sel.test_only()) {
      append("~test-only~");
    }
  }

  void appendCall(const Expr::Call& call) {
    if (call.has_target()) {
      appendExpr(call.target());
      s_ += ".";
    }
    append(call.function());
    append("(");
    if (call.args_size() > 0) {
      addIndent();
      appendLine();
      for (int i = 0; i < call.args_size(); ++i) {
        const auto& arg = call.args(i);
        if (i > 0) {
          append(",");
          appendLine();
        }
        appendExpr(arg);
      }
      removeIndent();
      appendLine();
    }
    append(")");
  }

  void appendList(const Expr::CreateList& list) {
    append("[");
    if (list.elements_size() > 0) {
      appendLine();
      addIndent();
      for (int i = 0; i < list.elements_size(); ++i) {
        const auto& elem = list.elements(i);
        if (i > 0) {
          append(",");
          appendLine();
        }
        appendExpr(elem);
      }
      removeIndent();
      appendLine();
    }
    append("]");
  }

  void appendStruct(const Expr::CreateStruct& obj) {
    if (obj.message_name().empty()) {
      appendMap(obj);
    } else {
      appendObject(obj);
    }
  }

  void appendMap(const Expr::CreateStruct& obj) {
    append("{");
    if (obj.entries_size() > 0) {
      appendLine();
      addIndent();
      for (int i = 0; i < obj.entries_size(); ++i) {
        const auto& entry = obj.entries(i);
        if (i > 0) {
          append(",");
          appendLine();
        }
        appendExpr(entry.map_key());
        append(":");
        appendExpr(entry.value());
        appendAdorn(entry);
      }
      removeIndent();
      appendLine();
    }
    append("}");
  }

  void appendObject(const Expr::CreateStruct& obj) {
    append(obj.message_name());
    append("{");
    if (obj.entries_size() > 0) {
      appendLine();
      addIndent();
      for (int i = 0; i < obj.entries_size(); ++i) {
        const auto& entry = obj.entries(i);
        if (i > 0) {
          append(",");
          appendLine();
        }
        append(entry.field_key());
        append(":");
        appendExpr(entry.value());
        appendAdorn(entry);
      }
      removeIndent();
      appendLine();
    }
    append("}");
  }

  void appendComprehension(const Expr::Comprehension& comprehension) {
    append("__comprehension__(");
    addIndent();
    appendLine();
    append("// Variable");
    appendLine();
    append(comprehension.iter_var());
    append(",");
    appendLine();
    append("// Target");
    appendLine();
    appendExpr(comprehension.iter_range());
    append(",");
    appendLine();
    append("// Accumulator");
    appendLine();
    append(comprehension.accu_var());
    append(",");
    appendLine();
    append("// Init");
    appendLine();
    appendExpr(comprehension.accu_init());
    append(",");
    appendLine();
    append("// LoopCondition");
    appendLine();
    appendExpr(comprehension.loop_condition());
    append(",");
    appendLine();
    append("// LoopStep");
    appendLine();
    appendExpr(comprehension.loop_step());
    append(",");
    appendLine();
    append("// Result");
    appendLine();
    appendExpr(comprehension.result());
    append(")");
    removeIndent();
  }

  void appendAdorn(const Expr& e) { append(adorner_.adorn(e)); }

  void appendAdorn(const Expr::CreateStruct::Entry& e) {
    append(adorner_.adorn(e));
  }

  void append(const std::string& s) {
    if (line_start_) {
      line_start_ = false;
      for (int i = 0; i < indent_; ++i) {
        s_ += "  ";
      }
    }
    s_ += s;
  }

  void appendLine() {
    s_ += "\n";
    line_start_ = true;
  }

  void addIndent() { indent_ += 1; }

  void removeIndent() {
    if (indent_ > 0) {
      indent_ -= 1;
    }
  }

  std::string formatLiteral(const google::api::expr::v1alpha1::Constant& c) {
    switch (c.constant_kind_case()) {
      case google::api::expr::v1alpha1::Constant::kBoolValue:
        return absl::StrFormat("%s", c.bool_value() ? "true" : "false");
      case google::api::expr::v1alpha1::Constant::kBytesValue:
        return cel::internal::FormatDoubleQuotedBytesLiteral(c.bytes_value());
      case google::api::expr::v1alpha1::Constant::kDoubleValue: {
        std::string s = absl::StrFormat("%f", c.double_value());
        // remove trailing zeros, i.e., convert 1.600000 to just 1.6 without
        // forcing a specific precision. There seems to be no flag to get this
        // directly from absl::StrFormat.
        auto idx = std::find_if_not(s.rbegin(), s.rend(),
                                    [](const char c) { return c == '0'; });
        s.erase(idx.base(), s.end());
        return s;
      }
      case google::api::expr::v1alpha1::Constant::kInt64Value:
        return absl::StrFormat("%d", c.int64_value());
      case google::api::expr::v1alpha1::Constant::kStringValue:
        return cel::internal::FormatDoubleQuotedStringLiteral(c.string_value());
      case google::api::expr::v1alpha1::Constant::kUint64Value:
        return absl::StrFormat("%uu", c.uint64_value());
      case google::api::expr::v1alpha1::Constant::kNullValue:
        return "null";
      default:
        return "<<ERROR>>";
    }
  }

  std::string print(const Expr& expr) {
    appendExpr(expr);
    return s_;
  }

 private:
  std::string s_;
  const ExpressionAdorner& adorner_;
  bool line_start_;
  int indent_;
};

}  // namespace

const ExpressionAdorner& empty_adorner() {
  return the_empty_adorner;
}

std::string ExprPrinter::print(const Expr& expr) const {
  Writer w(adorner_);
  return w.print(expr);
}

}  // namespace testutil
}  // namespace expr
}  // namespace api
}  // namespace google
