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

#include "base/values/type_value.h"

#include <string>
#include <utility>

#include "base/internal/data.h"

namespace cel {

CEL_INTERNAL_VALUE_IMPL(TypeValue);

std::string TypeValue::DebugString() const { return std::string(name()); }

bool TypeValue::Equals(const Value& other) const {
  return kind() == other.kind() &&
         name() == static_cast<const TypeValue&>(other).name();
}

void TypeValue::HashValue(absl::HashState state) const {
  absl::HashState::combine(std::move(state), type(), name());
}

absl::string_view TypeValue::name() const {
  if (base_internal::Metadata::IsTriviallyCopyable(*this)) {
    return static_cast<const base_internal::LegacyTypeValue&>(*this).name();
  }
  return static_cast<const base_internal::ModernTypeValue&>(*this).name();
}

}  // namespace cel
