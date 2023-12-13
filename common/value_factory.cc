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

#include "common/value_factory.h"

#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value.h"
#include "common/values/thread_compatible_value_factory.h"
#include "common/values/thread_safe_value_factory.h"
#include "common/values/value_cache.h"

namespace cel {

namespace {

using common_internal::ProcessLocalValueCache;

}  // namespace

ListValue ValueFactory::CreateZeroListValue(ListTypeView type) {
  if (auto list_value = ProcessLocalValueCache::Get()->GetEmptyListValue(type);
      list_value.has_value()) {
    return ListValue(*list_value);
  }
  return CreateZeroListValueImpl(type);
}

MapValue ValueFactory::CreateZeroMapValue(MapTypeView type) {
  if (auto map_value = ProcessLocalValueCache::Get()->GetEmptyMapValue(type);
      map_value.has_value()) {
    return MapValue(*map_value);
  }
  return CreateZeroMapValueImpl(type);
}

OptionalValue ValueFactory::CreateZeroOptionalValue(OptionalTypeView type) {
  if (auto optional_value =
          ProcessLocalValueCache::Get()->GetEmptyOptionalValue(type);
      optional_value.has_value()) {
    return OptionalValue(*optional_value);
  }
  return CreateZeroOptionalValueImpl(type);
}

ListValueView ValueFactory::GetZeroDynListValue() {
  return ProcessLocalValueCache::Get()->GetEmptyDynListValue();
}

MapValueView ValueFactory::GetZeroDynDynMapValue() {
  return ProcessLocalValueCache::Get()->GetEmptyDynDynMapValue();
}

MapValueView ValueFactory::GetZeroStringDynMapValue() {
  return ProcessLocalValueCache::Get()->GetEmptyStringDynMapValue();
}

OptionalValueView ValueFactory::GetZeroDynOptionalValue() {
  return ProcessLocalValueCache::Get()->GetEmptyDynOptionalValue();
}

Shared<ValueFactory> NewThreadCompatibleValueFactory(
    MemoryManagerRef memory_manager) {
  return memory_manager
      .MakeShared<common_internal::ThreadCompatibleValueFactory>(
          memory_manager);
}

Shared<ValueFactory> NewThreadSafeValueFactory(
    MemoryManagerRef memory_manager) {
  return memory_manager.MakeShared<common_internal::ThreadSafeValueFactory>(
      memory_manager);
}

}  // namespace cel
