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

// IWYU pragma: private

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_VALUE_CACHE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_VALUE_CACHE_H_

#include "absl/base/attributes.h"
#include "absl/container/flat_hash_map.h"
#include "absl/types/optional.h"
#include "common/type.h"
#include "common/types/optional_type.h"
#include "common/value.h"
#include "internal/no_destructor.h"

namespace cel::common_internal {

class ProcessLocalValueCache final {
 public:
  ABSL_ATTRIBUTE_PURE_FUNCTION static const ProcessLocalValueCache* Get();

  ErrorValueView GetDefaultErrorValue() const;

  absl::optional<ListValueView> GetEmptyListValue(ListTypeView type) const;

  ListValueView GetEmptyDynListValue() const;

  absl::optional<MapValueView> GetEmptyMapValue(MapTypeView type) const;

  MapValueView GetEmptyDynDynMapValue() const;

  absl::optional<OptionalValueView> GetEmptyOptionalValue(
      OptionalTypeView type) const;

  OptionalValueView GetEmptyDynOptionalValue() const;

 private:
  friend class internal::NoDestructor<ProcessLocalValueCache>;

  ProcessLocalValueCache();

  const ErrorValue default_error_value_;
  absl::flat_hash_map<ListTypeView, ListValue> list_values_;
  absl::flat_hash_map<MapTypeView, MapValue> map_values_;
  absl::flat_hash_map<OptionalTypeView, OptionalValue> optional_values_;
  absl::optional<ListValueView> dyn_list_value_;
  absl::optional<MapValueView> dyn_dyn_map_value_;
  absl::optional<OptionalValueView> dyn_optional_value_;
};

}  // namespace cel::common_internal

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_VALUE_CACHE_H_
