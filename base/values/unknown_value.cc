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

#include "base/values/unknown_value.h"

#include <memory>
#include <string>
#include <utility>

#include "absl/base/macros.h"

namespace cel {

CEL_INTERNAL_VALUE_IMPL(UnknownValue);

UnknownValue::UnknownValue(std::shared_ptr<base_internal::UnknownSetImpl> impl)
    : base_internal::HeapData(kKind), impl_(std::move(impl)) {
  // Ensure `Value*` and `base_internal::HeapData*` are not thunked.
  ABSL_ASSERT(
      reinterpret_cast<uintptr_t>(static_cast<Value*>(this)) ==
      reinterpret_cast<uintptr_t>(static_cast<base_internal::HeapData*>(this)));
}

std::string UnknownValue::DebugString() const { return "*unknown*"; }

void UnknownValue::HashValue(absl::HashState state) const {
  absl::HashState::combine(std::move(state), type());
}

bool UnknownValue::Equals(const Value& other) const {
  return kind() == other.kind();
}

}  // namespace cel
