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

#include "base/value.h"

#include <cstddef>
#include <utility>

namespace cel {

std::pair<size_t, size_t> Value::SizeAndAlignment() const {
  // Currently most implementations of Value are not reference counted, so those
  // that are override this and those that do not inherit this. Using 0 here
  // will trigger runtime asserts in case of undefined behavior.
  return std::pair<size_t, size_t>(0, 0);
}

void Value::CopyTo(Value& address) const {}

void Value::MoveTo(Value& address) {}

}  // namespace cel
