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

#include "base/type.h"

#include <string>
#include <utility>

#include "absl/hash/hash.h"

namespace cel {

CEL_INTERNAL_TYPE_IMPL(Type);

std::string Type::DebugString() const { return std::string(name()); }

std::pair<size_t, size_t> Type::SizeAndAlignment() const {
  // Currently no implementation of Type is reference counted. However once we
  // introduce Struct it likely will be. Using 0 here will trigger runtime
  // asserts in case of undefined behavior. Struct should force this to be pure.
  return std::pair<size_t, size_t>(0, 0);
}

bool Type::Equals(const Type& other) const { return kind() == other.kind(); }

void Type::HashValue(absl::HashState state) const {
  absl::HashState::combine(std::move(state), kind(), name());
}

}  // namespace cel
