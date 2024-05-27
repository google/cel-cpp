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

#ifndef THIRD_PARTY_CEL_CPP_RUNTIME_RUNTIME_VALUE_MANAGER_H_
#define THIRD_PARTY_CEL_CPP_RUNTIME_RUNTIME_VALUE_MANAGER_H_

#include "absl/base/attributes.h"
#include "common/memory.h"
#include "common/type_reflector.h"
#include "common/value_manager.h"
#include "common/values/legacy_value_manager.h"

namespace cel {

// A convenience class for managing objects associated with a value manager.
//
// Must not outlive the runtime or program that provides the TypeReflector.
class RuntimeValueManager {
 public:
  // type_provider and memory_manager must outlive the ManagedValueFactory.
  RuntimeValueManager(const TypeReflector& type_provider
                          ABSL_ATTRIBUTE_LIFETIME_BOUND,
                      MemoryManagerRef memory_manager)
      : value_manager_(memory_manager, type_provider) {}

  // Neither moveable nor copyable.
  RuntimeValueManager(const RuntimeValueManager& other) = delete;
  RuntimeValueManager& operator=(const RuntimeValueManager& other) = delete;
  RuntimeValueManager(RuntimeValueManager&& other) = delete;
  RuntimeValueManager& operator=(RuntimeValueManager&& other) = delete;

  ValueManager& get() { return value_manager_; }

 private:
  common_internal::LegacyValueManager value_manager_;
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_RUNTIME_RUNTIME_VALUE_MANAGER_H_
