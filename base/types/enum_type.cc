// Copyright 2022 Google LLC
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

#include "base/types/enum_type.h"

#include <string>

#include "absl/base/macros.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"
#include "base/value.h"
#include "internal/overloaded.h"
#include "internal/status_macros.h"

namespace cel {

CEL_INTERNAL_TYPE_IMPL(EnumType);

absl::StatusOr<Handle<Value>> EnumType::NewValueFromAny(
    ValueFactory& value_factory, const absl::Cord& value) const {
  return absl::FailedPreconditionError(
      absl::StrCat("google.protobuf.Any cannot be deserialized as ", name()));
}

bool operator<(const EnumType::ConstantId& lhs,
               const EnumType::ConstantId& rhs) {
  return absl::visit(
      internal::Overloaded{
          [&rhs](absl::string_view lhs_name) {
            return absl::visit(
                internal::Overloaded{// (absl::string_view, absl::string_view)
                                     [lhs_name](absl::string_view rhs_name) {
                                       return lhs_name < rhs_name;
                                     },
                                     // (absl::string_view, int64_t)
                                     [](int64_t rhs_number) { return false; }},
                rhs.data_);
          },
          [&rhs](int64_t lhs_number) {
            return absl::visit(
                internal::Overloaded{
                    // (int64_t, absl::string_view)
                    [](absl::string_view rhs_name) { return true; },
                    // (int64_t, int64_t)
                    [lhs_number](int64_t rhs_number) {
                      return lhs_number < rhs_number;
                    },
                },
                rhs.data_);
          }},
      lhs.data_);
}

std::string EnumType::ConstantId::DebugString() const {
  return absl::visit(
      internal::Overloaded{
          [](absl::string_view name) { return std::string(name); },
          [](int64_t number) { return absl::StrCat(number); }},
      data_);
}

EnumType::EnumType() : base_internal::HeapData(kKind) {
  // Ensure `Type*` and `base_internal::HeapData*` are not thunked.
  ABSL_ASSERT(
      reinterpret_cast<uintptr_t>(static_cast<Type*>(this)) ==
      reinterpret_cast<uintptr_t>(static_cast<base_internal::HeapData*>(this)));
}

struct EnumType::FindConstantVisitor final {
  const EnumType& enum_type;

  absl::StatusOr<absl::optional<Constant>> operator()(
      absl::string_view name) const {
    return enum_type.FindConstantByName(name);
  }

  absl::StatusOr<absl::optional<Constant>> operator()(int64_t number) const {
    return enum_type.FindConstantByNumber(number);
  }
};

absl::StatusOr<absl::optional<EnumType::Constant>> EnumType::FindConstant(
    ConstantId id) const {
  return absl::visit(FindConstantVisitor{*this}, id.data_);
}

absl::StatusOr<absl::string_view> EnumType::ConstantIterator::NextName() {
  CEL_ASSIGN_OR_RETURN(auto constant, Next());
  return constant.name;
}

absl::StatusOr<int64_t> EnumType::ConstantIterator::NextNumber() {
  CEL_ASSIGN_OR_RETURN(auto constant, Next());
  return constant.number;
}

}  // namespace cel
