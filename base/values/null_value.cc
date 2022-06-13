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

#include "base/values/null_value.h"

#include <string>
#include <utility>

#include "base/types/null_type.h"
#include "internal/no_destructor.h"

namespace cel {

namespace {

using base_internal::PersistentHandleFactory;

}

Persistent<const Type> NullValue::type() const {
  return PersistentHandleFactory<const Type>::MakeUnmanaged<const NullType>(
      NullType::Get());
}

std::string NullValue::DebugString() const { return "null"; }

const NullValue& NullValue::Get() {
  static const internal::NoDestructor<NullValue> instance;
  return *instance;
}

void NullValue::CopyTo(Value& address) const {
  CEL_INTERNAL_VALUE_COPY_TO(NullValue, *this, address);
}

void NullValue::MoveTo(Value& address) {
  CEL_INTERNAL_VALUE_MOVE_TO(NullValue, *this, address);
}

bool NullValue::Equals(const Value& other) const {
  return kind() == other.kind();
}

void NullValue::HashValue(absl::HashState state) const {
  absl::HashState::combine(std::move(state), type(), 0);
}

}  // namespace cel
