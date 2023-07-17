// Copyright 2023 Google LLC
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

#include "common/json.h"

#include "internal/copy_on_write.h"
#include "internal/no_destructor.h"

namespace cel {

internal::CopyOnWrite<typename JsonArray::Container> JsonArray::Empty() {
  static const internal::NoDestructor<internal::CopyOnWrite<Container>> empty;
  return empty.get();
}

internal::CopyOnWrite<typename JsonObject::Container> JsonObject::Empty() {
  static const internal::NoDestructor<internal::CopyOnWrite<Container>> empty;
  return empty.get();
}

}  // namespace cel
