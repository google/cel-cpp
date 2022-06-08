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

#include "base/types/type_type.h"
#include "internal/casts.h"

namespace cel {

namespace {

using base_internal::PersistentHandleFactory;

}

Persistent<const Type> TypeValue::type() const {
  return PersistentHandleFactory<const Type>::MakeUnmanaged<const TypeType>(
      TypeType::Get());
}

std::string TypeValue::DebugString() const { return value()->DebugString(); }

void TypeValue::CopyTo(Value& address) const {
  CEL_INTERNAL_VALUE_COPY_TO(TypeValue, *this, address);
}

void TypeValue::MoveTo(Value& address) {
  CEL_INTERNAL_VALUE_MOVE_TO(TypeValue, *this, address);
}

bool TypeValue::Equals(const Value& other) const {
  return kind() == other.kind() &&
         value() == internal::down_cast<const TypeValue&>(other).value();
}

void TypeValue::HashValue(absl::HashState state) const {
  absl::HashState::combine(std::move(state), type(), value());
}

}  // namespace cel
