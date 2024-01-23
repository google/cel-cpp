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

#include "common/values/value_cache.h"

#include <utility>

#include "absl/base/no_destructor.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/types/optional.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/types/type_cache.h"
#include "common/value.h"

namespace cel::common_internal {

ErrorValueView GetDefaultErrorValue() {
  return ProcessLocalValueCache::Get()->GetDefaultErrorValue();
}

ParsedListValueView GetEmptyDynListValue() {
  return ProcessLocalValueCache::Get()->GetEmptyDynListValue();
}

ParsedMapValueView GetEmptyDynDynMapValue() {
  return ProcessLocalValueCache::Get()->GetEmptyDynDynMapValue();
}

OptionalValueView GetEmptyDynOptionalValue() {
  return ProcessLocalValueCache::Get()->GetEmptyDynOptionalValue();
}

const ProcessLocalValueCache* ProcessLocalValueCache::Get() {
  static const absl::NoDestructor<ProcessLocalValueCache> instance;
  return &*instance;
}

ErrorValueView ProcessLocalValueCache::GetDefaultErrorValue() const {
  return default_error_value_;
}

absl::optional<ParsedListValueView> ProcessLocalValueCache::GetEmptyListValue(
    ListTypeView type) const {
  if (auto list_value = list_values_.find(type);
      list_value != list_values_.end()) {
    return list_value->second;
  }
  return absl::nullopt;
}

ParsedListValueView ProcessLocalValueCache::GetEmptyDynListValue() const {
  return *dyn_list_value_;
}

absl::optional<ParsedMapValueView> ProcessLocalValueCache::GetEmptyMapValue(
    MapTypeView type) const {
  if (auto map_value = map_values_.find(type); map_value != map_values_.end()) {
    return map_value->second;
  }
  return absl::nullopt;
}

ParsedMapValueView ProcessLocalValueCache::GetEmptyDynDynMapValue() const {
  return *dyn_dyn_map_value_;
}

ParsedMapValueView ProcessLocalValueCache::GetEmptyStringDynMapValue() const {
  return *string_dyn_map_value_;
}
absl::optional<OptionalValueView> ProcessLocalValueCache::GetEmptyOptionalValue(
    OptionalTypeView type) const {
  if (auto optional_value = optional_values_.find(type);
      optional_value != optional_values_.end()) {
    return optional_value->second;
  }
  return absl::nullopt;
}

OptionalValueView ProcessLocalValueCache::GetEmptyDynOptionalValue() const {
  return *dyn_optional_value_;
}

ProcessLocalValueCache::ProcessLocalValueCache()
    : default_error_value_(absl::UnknownError("unknown error")) {
  MemoryManagerRef memory_manager = MemoryManagerRef::Unmanaged();
  const auto& list_types = ProcessLocalTypeCache::Get()->ListTypes();
  list_values_.reserve(list_types.size());
  for (const auto& list_type : list_types) {
    auto inserted =
        list_values_
            .insert_or_assign(
                list_type,
                ParsedListValue(memory_manager.MakeShared<EmptyListValue>(
                    ListType(list_type))))
            .second;
    ABSL_DCHECK(inserted);
  }
  const auto& map_types = ProcessLocalTypeCache::Get()->MapTypes();
  map_values_.reserve(map_types.size());
  for (const auto& map_type : map_types) {
    auto inserted =
        map_values_
            .insert_or_assign(
                map_type,
                ParsedMapValue(memory_manager.MakeShared<EmptyMapValue>(
                    MapType(map_type))))
            .second;
    ABSL_DCHECK(inserted);
  }
  const auto& optional_types = ProcessLocalTypeCache::Get()->OptionalTypes();
  optional_values_.reserve(optional_types.size());
  for (const auto& optional_type : optional_types) {
    auto inserted =
        optional_values_
            .insert_or_assign(
                optional_type,
                OptionalValue(memory_manager.MakeShared<EmptyOptionalValue>(
                    OptionalType(optional_type))))
            .second;
    ABSL_DCHECK(inserted);
  }
  dyn_list_value_ =
      GetEmptyListValue(ProcessLocalTypeCache::Get()->GetDynListType());
  ABSL_DCHECK(dyn_list_value_.has_value());
  dyn_dyn_map_value_ =
      GetEmptyMapValue(ProcessLocalTypeCache::Get()->GetDynDynMapType());
  ABSL_DCHECK(dyn_dyn_map_value_.has_value());
  string_dyn_map_value_ =
      GetEmptyMapValue(ProcessLocalTypeCache::Get()->GetStringDynMapType());
  ABSL_DCHECK(string_dyn_map_value_.has_value());
  dyn_optional_value_ =
      GetEmptyOptionalValue(ProcessLocalTypeCache::Get()->GetDynOptionalType());
  ABSL_DCHECK(dyn_optional_value_.has_value());
}

}  // namespace cel::common_internal
