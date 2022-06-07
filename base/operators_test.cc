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

#include "base/operators.h"

#include <type_traits>

#include "absl/hash/hash_testing.h"
#include "absl/status/status.h"
#include "internal/testing.h"

namespace cel {
namespace {

using cel::internal::StatusIs;

TEST(Operator, TypeTraits) {
  EXPECT_FALSE(std::is_default_constructible_v<Operator>);
  EXPECT_TRUE(std::is_copy_constructible_v<Operator>);
  EXPECT_TRUE(std::is_move_constructible_v<Operator>);
  EXPECT_TRUE(std::is_copy_assignable_v<Operator>);
  EXPECT_TRUE(std::is_move_assignable_v<Operator>);
}

TEST(Operator, Conditional) {
  EXPECT_EQ(Operator::Conditional().id(), OperatorId::kConditional);
  EXPECT_EQ(Operator::Conditional().name(), "_?_:_");
  EXPECT_EQ(Operator::Conditional().display_name(), "");
  EXPECT_EQ(Operator::Conditional().precedence(), 8);
  EXPECT_EQ(Operator::Conditional().arity(), 3);
}

TEST(Operator, LogicalAnd) {
  EXPECT_EQ(Operator::LogicalAnd().id(), OperatorId::kLogicalAnd);
  EXPECT_EQ(Operator::LogicalAnd().name(), "_&&_");
  EXPECT_EQ(Operator::LogicalAnd().display_name(), "&&");
  EXPECT_EQ(Operator::LogicalAnd().precedence(), 6);
  EXPECT_EQ(Operator::LogicalAnd().arity(), 2);
}

TEST(Operator, LogicalOr) {
  EXPECT_EQ(Operator::LogicalOr().id(), OperatorId::kLogicalOr);
  EXPECT_EQ(Operator::LogicalOr().name(), "_||_");
  EXPECT_EQ(Operator::LogicalOr().display_name(), "||");
  EXPECT_EQ(Operator::LogicalOr().precedence(), 7);
  EXPECT_EQ(Operator::LogicalOr().arity(), 2);
}

TEST(Operator, LogicalNot) {
  EXPECT_EQ(Operator::LogicalNot().id(), OperatorId::kLogicalNot);
  EXPECT_EQ(Operator::LogicalNot().name(), "!_");
  EXPECT_EQ(Operator::LogicalNot().display_name(), "!");
  EXPECT_EQ(Operator::LogicalNot().precedence(), 2);
  EXPECT_EQ(Operator::LogicalNot().arity(), 1);
}

TEST(Operator, Equals) {
  EXPECT_EQ(Operator::Equals().id(), OperatorId::kEquals);
  EXPECT_EQ(Operator::Equals().name(), "_==_");
  EXPECT_EQ(Operator::Equals().display_name(), "==");
  EXPECT_EQ(Operator::Equals().precedence(), 5);
  EXPECT_EQ(Operator::Equals().arity(), 2);
}

TEST(Operator, NotEquals) {
  EXPECT_EQ(Operator::NotEquals().id(), OperatorId::kNotEquals);
  EXPECT_EQ(Operator::NotEquals().name(), "_!=_");
  EXPECT_EQ(Operator::NotEquals().display_name(), "!=");
  EXPECT_EQ(Operator::NotEquals().precedence(), 5);
  EXPECT_EQ(Operator::NotEquals().arity(), 2);
}

TEST(Operator, Less) {
  EXPECT_EQ(Operator::Less().id(), OperatorId::kLess);
  EXPECT_EQ(Operator::Less().name(), "_<_");
  EXPECT_EQ(Operator::Less().display_name(), "<");
  EXPECT_EQ(Operator::Less().precedence(), 5);
  EXPECT_EQ(Operator::Less().arity(), 2);
}

TEST(Operator, LessEquals) {
  EXPECT_EQ(Operator::LessEquals().id(), OperatorId::kLessEquals);
  EXPECT_EQ(Operator::LessEquals().name(), "_<=_");
  EXPECT_EQ(Operator::LessEquals().display_name(), "<=");
  EXPECT_EQ(Operator::LessEquals().precedence(), 5);
  EXPECT_EQ(Operator::LessEquals().arity(), 2);
}

TEST(Operator, Greater) {
  EXPECT_EQ(Operator::Greater().id(), OperatorId::kGreater);
  EXPECT_EQ(Operator::Greater().name(), "_>_");
  EXPECT_EQ(Operator::Greater().display_name(), ">");
  EXPECT_EQ(Operator::Greater().precedence(), 5);
  EXPECT_EQ(Operator::Greater().arity(), 2);
}

TEST(Operator, GreaterEquals) {
  EXPECT_EQ(Operator::GreaterEquals().id(), OperatorId::kGreaterEquals);
  EXPECT_EQ(Operator::GreaterEquals().name(), "_>=_");
  EXPECT_EQ(Operator::GreaterEquals().display_name(), ">=");
  EXPECT_EQ(Operator::GreaterEquals().precedence(), 5);
  EXPECT_EQ(Operator::GreaterEquals().arity(), 2);
}

TEST(Operator, Add) {
  EXPECT_EQ(Operator::Add().id(), OperatorId::kAdd);
  EXPECT_EQ(Operator::Add().name(), "_+_");
  EXPECT_EQ(Operator::Add().display_name(), "+");
  EXPECT_EQ(Operator::Add().precedence(), 4);
  EXPECT_EQ(Operator::Add().arity(), 2);
}

TEST(Operator, Subtract) {
  EXPECT_EQ(Operator::Subtract().id(), OperatorId::kSubtract);
  EXPECT_EQ(Operator::Subtract().name(), "_-_");
  EXPECT_EQ(Operator::Subtract().display_name(), "-");
  EXPECT_EQ(Operator::Subtract().precedence(), 4);
  EXPECT_EQ(Operator::Subtract().arity(), 2);
}

TEST(Operator, Multiply) {
  EXPECT_EQ(Operator::Multiply().id(), OperatorId::kMultiply);
  EXPECT_EQ(Operator::Multiply().name(), "_*_");
  EXPECT_EQ(Operator::Multiply().display_name(), "*");
  EXPECT_EQ(Operator::Multiply().precedence(), 3);
  EXPECT_EQ(Operator::Multiply().arity(), 2);
}

TEST(Operator, Divide) {
  EXPECT_EQ(Operator::Divide().id(), OperatorId::kDivide);
  EXPECT_EQ(Operator::Divide().name(), "_/_");
  EXPECT_EQ(Operator::Divide().display_name(), "/");
  EXPECT_EQ(Operator::Divide().precedence(), 3);
  EXPECT_EQ(Operator::Divide().arity(), 2);
}

TEST(Operator, Modulo) {
  EXPECT_EQ(Operator::Modulo().id(), OperatorId::kModulo);
  EXPECT_EQ(Operator::Modulo().name(), "_%_");
  EXPECT_EQ(Operator::Modulo().display_name(), "%");
  EXPECT_EQ(Operator::Modulo().precedence(), 3);
  EXPECT_EQ(Operator::Modulo().arity(), 2);
}

TEST(Operator, Negate) {
  EXPECT_EQ(Operator::Negate().id(), OperatorId::kNegate);
  EXPECT_EQ(Operator::Negate().name(), "-_");
  EXPECT_EQ(Operator::Negate().display_name(), "-");
  EXPECT_EQ(Operator::Negate().precedence(), 2);
  EXPECT_EQ(Operator::Negate().arity(), 1);
}

TEST(Operator, Index) {
  EXPECT_EQ(Operator::Index().id(), OperatorId::kIndex);
  EXPECT_EQ(Operator::Index().name(), "_[_]");
  EXPECT_EQ(Operator::Index().display_name(), "");
  EXPECT_EQ(Operator::Index().precedence(), 1);
  EXPECT_EQ(Operator::Index().arity(), 2);
}

TEST(Operator, In) {
  EXPECT_EQ(Operator::In().id(), OperatorId::kIn);
  EXPECT_EQ(Operator::In().name(), "@in");
  EXPECT_EQ(Operator::In().display_name(), "in");
  EXPECT_EQ(Operator::In().precedence(), 5);
  EXPECT_EQ(Operator::In().arity(), 2);
}

TEST(Operator, NotStrictlyFalse) {
  EXPECT_EQ(Operator::NotStrictlyFalse().id(), OperatorId::kNotStrictlyFalse);
  EXPECT_EQ(Operator::NotStrictlyFalse().name(), "@not_strictly_false");
  EXPECT_EQ(Operator::NotStrictlyFalse().display_name(), "");
  EXPECT_EQ(Operator::NotStrictlyFalse().precedence(), 0);
  EXPECT_EQ(Operator::NotStrictlyFalse().arity(), 1);
}

TEST(Operator, OldIn) {
  EXPECT_EQ(Operator::OldIn().id(), OperatorId::kOldIn);
  EXPECT_EQ(Operator::OldIn().name(), "_in_");
  EXPECT_EQ(Operator::OldIn().display_name(), "in");
  EXPECT_EQ(Operator::OldIn().precedence(), 5);
  EXPECT_EQ(Operator::OldIn().arity(), 2);
}

TEST(Operator, OldNotStrictlyFalse) {
  EXPECT_EQ(Operator::OldNotStrictlyFalse().id(),
            OperatorId::kOldNotStrictlyFalse);
  EXPECT_EQ(Operator::OldNotStrictlyFalse().name(), "__not_strictly_false__");
  EXPECT_EQ(Operator::OldNotStrictlyFalse().display_name(), "");
  EXPECT_EQ(Operator::OldNotStrictlyFalse().precedence(), 0);
  EXPECT_EQ(Operator::OldNotStrictlyFalse().arity(), 1);
}

TEST(Operator, FindByName) {
  auto status_or_operator = Operator::FindByName("@in");
  EXPECT_OK(status_or_operator);
  EXPECT_EQ(status_or_operator.value(), Operator::In());
  status_or_operator = Operator::FindByName("_in_");
  EXPECT_OK(status_or_operator);
  EXPECT_EQ(status_or_operator.value(), Operator::OldIn());
  status_or_operator = Operator::FindByName("in");
  EXPECT_THAT(status_or_operator, StatusIs(absl::StatusCode::kNotFound));
}

TEST(Operator, FindByDisplayName) {
  auto status_or_operator = Operator::FindByDisplayName("-");
  EXPECT_OK(status_or_operator);
  EXPECT_EQ(status_or_operator.value(), Operator::Subtract());
  status_or_operator = Operator::FindByDisplayName("@in");
  EXPECT_THAT(status_or_operator, StatusIs(absl::StatusCode::kNotFound));
}

TEST(Operator, FindUnaryByDisplayName) {
  auto status_or_operator = Operator::FindUnaryByDisplayName("-");
  EXPECT_OK(status_or_operator);
  EXPECT_EQ(status_or_operator.value(), Operator::Negate());
  status_or_operator = Operator::FindUnaryByDisplayName("&&");
  EXPECT_THAT(status_or_operator, StatusIs(absl::StatusCode::kNotFound));
}

TEST(Operator, FindBinaryByDisplayName) {
  auto status_or_operator = Operator::FindBinaryByDisplayName("-");
  EXPECT_OK(status_or_operator);
  EXPECT_EQ(status_or_operator.value(), Operator::Subtract());
  status_or_operator = Operator::FindBinaryByDisplayName("!");
  EXPECT_THAT(status_or_operator, StatusIs(absl::StatusCode::kNotFound));
}

TEST(Type, SupportsAbslHash) {
  EXPECT_TRUE(absl::VerifyTypeImplementsAbslHashCorrectly({
      Operator::Conditional(),
      Operator::LogicalAnd(),
      Operator::LogicalOr(),
      Operator::LogicalNot(),
      Operator::Equals(),
      Operator::NotEquals(),
      Operator::Less(),
      Operator::LessEquals(),
      Operator::Greater(),
      Operator::GreaterEquals(),
      Operator::Add(),
      Operator::Subtract(),
      Operator::Multiply(),
      Operator::Divide(),
      Operator::Modulo(),
      Operator::Negate(),
      Operator::Index(),
      Operator::In(),
      Operator::NotStrictlyFalse(),
      Operator::OldIn(),
      Operator::OldNotStrictlyFalse(),
  }));
}

}  // namespace
}  // namespace cel
