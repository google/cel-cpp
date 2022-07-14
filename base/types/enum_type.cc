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

#include <utility>

#include "absl/base/macros.h"
#include "absl/hash/hash.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"

namespace cel {

CEL_INTERNAL_TYPE_IMPL(EnumType);

EnumType::EnumType() : base_internal::HeapData(kKind) {
  // Ensure `Type*` and `base_internal::HeapData*` are not thunked.
  ABSL_ASSERT(
      reinterpret_cast<uintptr_t>(static_cast<Type*>(this)) ==
      reinterpret_cast<uintptr_t>(static_cast<base_internal::HeapData*>(this)));
}

struct EnumType::FindConstantVisitor final {
  const EnumType& enum_type;

  absl::StatusOr<Constant> operator()(absl::string_view name) const {
    return enum_type.FindConstantByName(name);
  }

  absl::StatusOr<Constant> operator()(int64_t number) const {
    return enum_type.FindConstantByNumber(number);
  }
};

absl::StatusOr<EnumType::Constant> EnumType::FindConstant(ConstantId id) const {
  return absl::visit(FindConstantVisitor{*this}, id.data_);
}

void EnumType::HashValue(absl::HashState state) const {
  absl::HashState::combine(std::move(state), kind(), name(), TypeId());
}

bool EnumType::Equals(const Type& other) const {
  return kind() == other.kind() &&
         name() == static_cast<const EnumType&>(other).name() &&
         TypeId() == static_cast<const EnumType&>(other).TypeId();
}

}  // namespace cel
