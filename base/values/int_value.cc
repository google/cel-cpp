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

#include "base/values/int_value.h"

#include <string>
#include <utility>

#include "absl/strings/str_cat.h"
#include "base/types/int_type.h"
#include "internal/casts.h"

namespace cel {

namespace {

using base_internal::PersistentHandleFactory;

}

Persistent<const Type> IntValue::type() const {
  return PersistentHandleFactory<const Type>::MakeUnmanaged<const IntType>(
      IntType::Get());
}

std::string IntValue::DebugString() const { return absl::StrCat(value()); }

void IntValue::CopyTo(Value& address) const {
  CEL_INTERNAL_VALUE_COPY_TO(IntValue, *this, address);
}

void IntValue::MoveTo(Value& address) {
  CEL_INTERNAL_VALUE_MOVE_TO(IntValue, *this, address);
}

bool IntValue::Equals(const Value& other) const {
  return kind() == other.kind() &&
         value() == internal::down_cast<const IntValue&>(other).value();
}

void IntValue::HashValue(absl::HashState state) const {
  absl::HashState::combine(std::move(state), type(), value());
}

}  // namespace cel
