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

#ifndef THIRD_PARTY_CEL_CPP_BASE_OPERATORS_H_
#define THIRD_PARTY_CEL_CPP_BASE_OPERATORS_H_

#include <utility>

#include "absl/base/attributes.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "base/internal/operators.h"

namespace cel {

enum class OperatorId {
  kConditional = 1,
  kLogicalAnd,
  kLogicalOr,
  kLogicalNot,
  kEquals,
  kNotEquals,
  kLess,
  kLessEquals,
  kGreater,
  kGreaterEquals,
  kAdd,
  kSubtract,
  kMultiply,
  kDivide,
  kModulo,
  kNegate,
  kIndex,
  kIn,
  kNotStrictlyFalse,
  kOldIn,
  kOldNotStrictlyFalse,
};

class Operator final {
 public:
  ABSL_ATTRIBUTE_PURE_FUNCTION static Operator Conditional();
  ABSL_ATTRIBUTE_PURE_FUNCTION static Operator LogicalAnd();
  ABSL_ATTRIBUTE_PURE_FUNCTION static Operator LogicalOr();
  ABSL_ATTRIBUTE_PURE_FUNCTION static Operator LogicalNot();
  ABSL_ATTRIBUTE_PURE_FUNCTION static Operator Equals();
  ABSL_ATTRIBUTE_PURE_FUNCTION static Operator NotEquals();
  ABSL_ATTRIBUTE_PURE_FUNCTION static Operator Less();
  ABSL_ATTRIBUTE_PURE_FUNCTION static Operator LessEquals();
  ABSL_ATTRIBUTE_PURE_FUNCTION static Operator Greater();
  ABSL_ATTRIBUTE_PURE_FUNCTION static Operator GreaterEquals();
  ABSL_ATTRIBUTE_PURE_FUNCTION static Operator Add();
  ABSL_ATTRIBUTE_PURE_FUNCTION static Operator Subtract();
  ABSL_ATTRIBUTE_PURE_FUNCTION static Operator Multiply();
  ABSL_ATTRIBUTE_PURE_FUNCTION static Operator Divide();
  ABSL_ATTRIBUTE_PURE_FUNCTION static Operator Modulo();
  ABSL_ATTRIBUTE_PURE_FUNCTION static Operator Negate();
  ABSL_ATTRIBUTE_PURE_FUNCTION static Operator Index();
  ABSL_ATTRIBUTE_PURE_FUNCTION static Operator In();
  ABSL_ATTRIBUTE_PURE_FUNCTION static Operator NotStrictlyFalse();
  ABSL_ATTRIBUTE_PURE_FUNCTION static Operator OldIn();
  ABSL_ATTRIBUTE_PURE_FUNCTION static Operator OldNotStrictlyFalse();

  static absl::StatusOr<Operator> FindByName(absl::string_view input);

  static absl::StatusOr<Operator> FindByDisplayName(absl::string_view input);

  static absl::StatusOr<Operator> FindUnaryByDisplayName(
      absl::string_view input);

  static absl::StatusOr<Operator> FindBinaryByDisplayName(
      absl::string_view input);

  Operator() = delete;

  Operator(const Operator&) = default;

  Operator(Operator&&) = default;

  Operator& operator=(const Operator&) = default;

  Operator& operator=(Operator&&) = default;

  constexpr OperatorId id() const { return data_->id; }

  // Returns the name of the operator. This is the managed representation of the
  // operator, for example "_&&_".
  constexpr absl::string_view name() const { return data_->name; }

  // Returns the source text representation of the operator. This is the
  // unmanaged text representation of the operator, for example "&&".
  //
  // Note that this will be empty for operators like Conditional() and Index().
  constexpr absl::string_view display_name() const {
    return data_->display_name;
  }

  constexpr int precedence() const { return data_->precedence; }

  constexpr int arity() const { return data_->arity; }

 private:
  constexpr explicit Operator(const base_internal::OperatorData* data)
      : data_(data) {}

  const base_internal::OperatorData* data_;
};

constexpr bool operator==(const Operator& lhs, const Operator& rhs) {
  return lhs.id() == rhs.id();
}

constexpr bool operator==(OperatorId lhs, const Operator& rhs) {
  return lhs == rhs.id();
}

constexpr bool operator==(const Operator& lhs, OperatorId rhs) {
  return operator==(rhs, lhs);
}

constexpr bool operator!=(const Operator& lhs, const Operator& rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(OperatorId lhs, const Operator& rhs) {
  return !operator==(lhs, rhs);
}

constexpr bool operator!=(const Operator& lhs, OperatorId rhs) {
  return !operator==(lhs, rhs);
}

template <typename H>
H AbslHashValue(H state, const Operator& op) {
  return H::combine(std::move(state), op.id());
}

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_OPERATORS_H_
