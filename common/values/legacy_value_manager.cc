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

#include "common/values/legacy_value_manager.h"

#include "common/type.h"
#include "common/value.h"
#include "common/values/value_cache.h"

namespace cel::common_internal {

ListValue LegacyValueManager::CreateZeroListValueImpl(ListTypeView type) {
  return ListValue(
      GetMemoryManager().MakeShared<EmptyListValue>(ListType(type)));
}

MapValue LegacyValueManager::CreateZeroMapValueImpl(MapTypeView type) {
  return MapValue(GetMemoryManager().MakeShared<EmptyMapValue>(MapType(type)));
}

OptionalValue LegacyValueManager::CreateZeroOptionalValueImpl(
    OptionalTypeView type) {
  return OptionalValue(
      GetMemoryManager().MakeShared<EmptyOptionalValue>(OptionalType(type)));
}

}  // namespace cel::common_internal
