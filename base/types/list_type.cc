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

#include "base/types/list_type.h"

#include <string>
#include <utility>

#include "absl/base/macros.h"
#include "absl/strings/str_cat.h"

namespace cel {

CEL_INTERNAL_TYPE_IMPL(ListType);

ListType::ListType(Persistent<const Type> element)
    : base_internal::HeapData(kKind), element_(std::move(element)) {
  // Ensure `Type*` and `base_internal::HeapData*` are not thunked.
  ABSL_ASSERT(
      reinterpret_cast<uintptr_t>(static_cast<Type*>(this)) ==
      reinterpret_cast<uintptr_t>(static_cast<base_internal::HeapData*>(this)));
}

std::string ListType::DebugString() const {
  return absl::StrCat(name(), "(", element()->DebugString(), ")");
}

bool ListType::Equals(const Type& other) const {
  if (kind() != other.kind()) {
    return false;
  }
  return element() == static_cast<const ListType&>(other).element();
}

void ListType::HashValue(absl::HashState state) const {
  // We specifically hash the element first and then call the parent method to
  // avoid hash suffix/prefix collisions.
  absl::HashState::combine(std::move(state), element(), kind(), name());
}

}  // namespace cel
