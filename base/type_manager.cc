// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include "base/type_manager.h"

#include <string>
#include <utility>

#include "absl/status/status.h"
#include "absl/synchronization/mutex.h"
#include "internal/status_macros.h"

namespace cel {

absl::StatusOr<Persistent<const Type>> TypeManager::ResolveType(
    absl::string_view name) {
  {
    // Check for builtin types first.
    CEL_ASSIGN_OR_RETURN(
        auto type, TypeProvider::Builtin().ProvideType(type_factory(), name));
    if (type) {
      return type;
    }
  }
  // Check with the type registry.
  absl::MutexLock lock(&mutex_);
  auto existing = types_.find(name);
  if (existing == types_.end()) {
    // Delegate to TypeRegistry implementation.
    CEL_ASSIGN_OR_RETURN(auto type,
                         type_provider().ProvideType(type_factory(), name));
    ABSL_ASSERT(!type || type->name() == name);
    existing = types_.insert({std::string(name), std::move(type)}).first;
  }
  return existing->second;
}

}  // namespace cel
