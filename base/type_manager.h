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

#ifndef THIRD_PARTY_CEL_CPP_BASE_TYPE_MANAGER_H_
#define THIRD_PARTY_CEL_CPP_BASE_TYPE_MANAGER_H_

#include "absl/base/attributes.h"
#include "absl/base/thread_annotations.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/synchronization/mutex.h"
#include "base/type.h"
#include "base/type_factory.h"
#include "base/type_provider.h"

namespace cel {

// TypeManager is a union of the TypeFactory and TypeProvider, allowing for both
// the instantiation of type implementations, loading of type implementations,
// and registering type implementations.
//
// TODO(uncreated-issue/8): more comments after solidifying role
class TypeManager final {
 public:
  TypeManager(TypeFactory& type_factory ABSL_ATTRIBUTE_LIFETIME_BOUND,
              const TypeProvider& type_provider ABSL_ATTRIBUTE_LIFETIME_BOUND)
      : type_factory_(type_factory), type_provider_(type_provider) {}

  MemoryManagerRef GetMemoryManager() const {
    return type_factory().GetMemoryManager();
  }

  TypeFactory& type_factory() const { return type_factory_; }

  const TypeProvider& type_provider() const { return type_provider_; }

  absl::StatusOr<absl::optional<Handle<Type>>> ResolveType(
      absl::string_view name);

 private:
  Handle<Type> CacheType(absl::string_view name, Handle<Type>&& type);

  Handle<Type> CacheTypeWithAliases(absl::string_view name,
                                    Handle<Type>&& type);

  TypeFactory& type_factory_;
  const TypeProvider& type_provider_;

  absl::Mutex mutex_;
  // std::string as the key because we also cache types which do not exist.
  absl::flat_hash_map<absl::string_view, Handle<Type>> types_
      ABSL_GUARDED_BY(mutex_);
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_BASE_TYPE_MANAGER_H_
