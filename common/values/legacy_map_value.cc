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

#include "common/values/legacy_map_value.h"

#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/types/optional.h"
#include "absl/types/variant.h"
#include "common/casting.h"
#include "common/value_manager.h"
#include "common/values/map_value_interface.h"
#include "common/values/values.h"

namespace cel::common_internal {

absl::Status LegacyMapValue::Equal(ValueManager& value_manager,
                                   const Value& other, Value& result) const {
  if (auto map_value = As<MapValue>(other); map_value.has_value()) {
    return MapValueEqual(value_manager, *this, *map_value, result);
  }
  result = BoolValue{false};
  return absl::OkStatus();
}

bool IsLegacyMapValue(const Value& value) {
  return absl::holds_alternative<LegacyMapValue>(value.variant_);
}

LegacyMapValue GetLegacyMapValue(const Value& value) {
  ABSL_DCHECK(IsLegacyMapValue(value));
  return absl::get<LegacyMapValue>(value.variant_);
}

absl::optional<LegacyMapValue> AsLegacyMapValue(const Value& value) {
  if (IsLegacyMapValue(value)) {
    return GetLegacyMapValue(value);
  }
  return absl::nullopt;
}

}  // namespace cel::common_internal
