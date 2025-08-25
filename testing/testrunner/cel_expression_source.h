// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_TESTING_TESTRUNNER_CEL_EXPRESSION_SOURCE_H_
#define THIRD_PARTY_CEL_CPP_TESTING_TESTRUNNER_CEL_EXPRESSION_SOURCE_H_

#include <optional>
#include <string>
#include <utility>

#include "cel/expr/checked.pb.h"

namespace cel::test {

// A wrapper class that holds one of three possible sources for a CEL
// expression: a pre-compiled CheckedExpr, a raw string, or a file path.
class CelExpressionSource {
 public:
  enum class Type { kCheckedExpr, kRawExpression, kCelFile };

  // Creates a CelExpressionSource from a compiled
  // cel::expr::CheckedExpr.
  static CelExpressionSource FromCheckedExpr(
      cel::expr::CheckedExpr checked_expr) {
    return CelExpressionSource(Type::kCheckedExpr, std::move(checked_expr));
  }

  // Creates a CelExpressionSource from a raw CEL expression string.
  static CelExpressionSource FromRawExpression(std::string raw_expression) {
    return CelExpressionSource(Type::kRawExpression, std::move(raw_expression));
  }

  // Creates a CelExpressionSource from a file path pointing to a .cel file.
  static CelExpressionSource FromCelFile(std::string cel_file_path) {
    return CelExpressionSource(Type::kCelFile, std::move(cel_file_path));
  }

  // Make non-copyable, but movable.
  // Movable is required for returning this object by value from the factories.
  CelExpressionSource(const CelExpressionSource&) = delete;
  CelExpressionSource& operator=(const CelExpressionSource&) = delete;
  CelExpressionSource(CelExpressionSource&&) = default;
  CelExpressionSource& operator=(CelExpressionSource&&) = default;

  // Getters for the source type and underlying values.
  Type type() const { return type_; }
  const std::optional<cel::expr::CheckedExpr>& checked_expr() const {
    return checked_expr_;
  }
  const std::optional<std::string>& raw_expression() const {
    return raw_expression_;
  }
  const std::optional<std::string>& cel_file_path() const {
    return cel_file_path_;
  }

 private:
  // Private parameterized constructors to enforce the use of the factory
  // methods.
  CelExpressionSource(Type type, cel::expr::CheckedExpr checked_expr)
      : type_(type), checked_expr_(std::move(checked_expr)) {}

  CelExpressionSource(Type type, std::string value) : type_(type) {
    if (type == Type::kRawExpression) {
      raw_expression_ = std::move(value);
    } else {
      cel_file_path_ = std::move(value);
    }
  }

  // Member variables to hold the expression source data.
  Type type_;
  std::optional<cel::expr::CheckedExpr> checked_expr_;
  std::optional<std::string> raw_expression_;
  std::optional<std::string> cel_file_path_;
};
}  // namespace cel::test

#endif  // THIRD_PARTY_CEL_CPP_TESTING_TESTRUNNER_CEL_EXPRESSION_SOURCE_H_
