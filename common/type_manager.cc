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

#include "common/type_manager.h"

#include <utility>

#include "common/memory.h"
#include "common/type_factory.h"
#include "common/type_provider.h"

namespace cel {

namespace {

// Currently `TypeManagerImpl` is really a wrapper around `TypeFactory` and
// `TypeProvider`, so the same implementation is used regardless of thread
// safety.
class TypeManagerImpl final : public TypeManager {
 public:
  TypeManagerImpl(MemoryManagerRef memory_manager,
                  Shared<TypeFactory> type_factory,
                  Shared<TypeProvider> type_provider)
      : memory_manager_(memory_manager),
        type_factory_(std::move(type_factory)),
        type_provider_(std::move(type_provider)) {}

  MemoryManagerRef GetMemoryManager() const override { return memory_manager_; }

  TypeFactory& GetTypeFactory() const override { return *type_factory_; }

  TypeProvider& GetTypeProvider() const override { return *type_provider_; }

 private:
  MemoryManagerRef memory_manager_;
  Shared<TypeFactory> type_factory_;
  Shared<TypeProvider> type_provider_;
};

Shared<TypeManager> NewThreadCompatibleTypeManager(
    MemoryManagerRef memory_manager, Shared<TypeFactory> type_factory,
    Shared<TypeProvider> type_provider) {
  return memory_manager.MakeShared<TypeManagerImpl>(
      memory_manager, std::move(type_factory), std::move(type_provider));
}

Shared<TypeManager> NewThreadSafeTypeManager(
    MemoryManagerRef memory_manager, Shared<TypeFactory> type_factory,
    Shared<TypeProvider> type_provider) {
  return memory_manager.MakeShared<TypeManagerImpl>(
      memory_manager, std::move(type_factory), std::move(type_provider));
}

}  // namespace

}  // namespace cel
