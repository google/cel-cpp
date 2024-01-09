// Copyright 2024 Google LLC
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

// IWYU pragma: private

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_LEGACY_VALUE_MANAGER_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_LEGACY_VALUE_MANAGER_H_

#include "common/memory.h"
#include "common/type.h"
#include "common/types/legacy_type_manager.h"
#include "common/value.h"
#include "common/value_manager.h"
#include "common/value_provider.h"
#include "common/values/legacy_value_provider.h"

namespace cel::common_internal {

class LegacyValueManager : public LegacyTypeManager, public ValueManager {
 public:
  LegacyValueManager(MemoryManagerRef memory_manager,
                     LegacyValueProvider& value_provider)
      : LegacyTypeManager(memory_manager, value_provider),
        value_provider_(value_provider) {}

  using LegacyTypeManager::GetMemoryManager;

 protected:
  ValueProvider& GetValueProvider() const final { return value_provider_; }

 private:
  ListValue CreateZeroListValueImpl(ListTypeView type) override;

  MapValue CreateZeroMapValueImpl(MapTypeView type) override;

  OptionalValue CreateZeroOptionalValueImpl(OptionalTypeView type) override;

  LegacyValueProvider& value_provider_;
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_LEGACY_VALUE_MANAGER_H_
