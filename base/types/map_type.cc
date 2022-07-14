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

#include "base/types/map_type.h"

#include <string>
#include <utility>

#include "absl/base/macros.h"
#include "absl/strings/str_cat.h"

namespace cel {

CEL_INTERNAL_TYPE_IMPL(MapType);

MapType::MapType(Persistent<const Type> key, Persistent<const Type> value)
    : base_internal::HeapData(kKind),
      key_(std::move(key)),
      value_(std::move(value)) {
  // Ensure `Type*` and `base_internal::HeapData*` are not thunked.
  ABSL_ASSERT(
      reinterpret_cast<uintptr_t>(static_cast<Type*>(this)) ==
      reinterpret_cast<uintptr_t>(static_cast<base_internal::HeapData*>(this)));
}

std::string MapType::DebugString() const {
  return absl::StrCat(name(), "(", key()->DebugString(), ", ",
                      value()->DebugString(), ")");
}

bool MapType::Equals(const Type& other) const {
  if (kind() != other.kind()) {
    return false;
  }
  return key() == static_cast<const MapType&>(other).key() &&
         value() == static_cast<const MapType&>(other).value();
}

void MapType::HashValue(absl::HashState state) const {
  // We specifically hash the element first and then call the parent method to
  // avoid hash suffix/prefix collisions.
  absl::HashState::combine(std::move(state), key(), value(), kind(), name());
}

}  // namespace cel
