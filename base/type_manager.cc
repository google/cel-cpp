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

#include <utility>

#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "absl/types/span.h"
#include "internal/status_macros.h"

namespace cel {

absl::StatusOr<absl::optional<Handle<Type>>> TypeManager::ResolveType(
    absl::string_view name) {
  // Check the cached types.
  {
    absl::ReaderMutexLock lock(&mutex_);
    auto existing = types_.find(name);
    if (ABSL_PREDICT_TRUE(existing != types_.end())) {
      return existing->second;
    }
  }
  // Check for builtin types.
  TypeProvider& builtin_type_provider = TypeProvider::Builtin();
  {
    CEL_ASSIGN_OR_RETURN(
        auto type, builtin_type_provider.ProvideType(type_factory(), name));
    if (type) {
      absl::string_view provided_name = (*type)->name();
      // We do not check that `provided_name` matches name for the builtin type
      // provider. There are some special types that have aliases.
      return CacheTypeWithAliases(provided_name, std::move(type).value());
    }
  }
  if (ABSL_PREDICT_FALSE(&builtin_type_provider == &type_provider())) {
    return absl::nullopt;
  }
  // Delegate to TypeRegistry implementation.
  CEL_ASSIGN_OR_RETURN(auto type,
                       type_provider().ProvideType(type_factory(), name));
  if (ABSL_PREDICT_FALSE(!type)) {
    return absl::nullopt;
  }
  absl::string_view provided_name = (*type)->name();
  if (ABSL_PREDICT_FALSE(name != provided_name)) {
    return absl::InternalError(
        absl::StrCat("TypeProvider provided ", provided_name, " for ", name));
  }
  return CacheType(provided_name, std::move(type).value());
}

Handle<Type> TypeManager::CacheType(absl::string_view name,
                                    Handle<Type>&& type) {
  ABSL_ASSERT(name == type->name());
  absl::WriterMutexLock lock(&mutex_);
  return types_.insert({name, std::move(type)}).first->second;
}

Handle<Type> TypeManager::CacheTypeWithAliases(absl::string_view name,
                                               Handle<Type>&& type) {
  absl::Span<const absl::string_view> aliases = type->aliases();
  if (aliases.empty()) {
    return CacheType(name, std::move(type));
  }
  absl::WriterMutexLock lock(&mutex_);
  auto insertion = types_.insert({name, type});
  if (insertion.second) {
    // Somebody beat us to caching.
    return insertion.first->second;
  }
  for (const auto& alias : aliases) {
    insertion = types_.insert({alias, type});
    ABSL_ASSERT(insertion.second);
  }
  return std::move(type);
}

}  // namespace cel
