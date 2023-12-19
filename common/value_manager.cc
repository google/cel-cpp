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

#include "common/value_manager.h"

#include <utility>

#include "common/memory.h"
#include "common/value_factory.h"
#include "common/value_provider.h"

namespace cel {

namespace {

class ValueManagerImpl final : public ValueManager {
 public:
  ValueManagerImpl(MemoryManagerRef memory_manager,
                   Shared<ValueFactory> value_factory,
                   Shared<ValueProvider> value_provider)
      : memory_manager_(memory_manager),
        value_factory_(std::move(value_factory)),
        value_provider_(std::move(value_provider)) {}

  MemoryManagerRef GetMemoryManager() const override { return memory_manager_; }

  ValueFactory& GetValueFactory() const override { return *value_factory_; }

  ValueProvider& GetValueProvider() const override { return *value_provider_; }

 private:
  MemoryManagerRef memory_manager_;
  Shared<ValueFactory> value_factory_;
  Shared<ValueProvider> value_provider_;
};

}  // namespace

Shared<ValueManager> NewThreadCompatibleValueManager(
    MemoryManagerRef memory_manager, Shared<ValueFactory> value_factory,
    Shared<ValueProvider> value_provider) {
  return memory_manager.MakeShared<ValueManagerImpl>(
      memory_manager, std::move(value_factory), std::move(value_provider));
}

Shared<ValueManager> NewThreadSafeValueManager(
    MemoryManagerRef memory_manager, Shared<ValueFactory> value_factory,
    Shared<ValueProvider> value_provider) {
  return memory_manager.MakeShared<ValueManagerImpl>(
      memory_manager, std::move(value_factory), std::move(value_provider));
}

}  // namespace cel
