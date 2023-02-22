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

#include "extensions/protobuf/enum_type.h"

#include <limits>

#include "absl/base/macros.h"
#include "absl/base/optimization.h"

namespace cel::extensions {

absl::StatusOr<absl::optional<ProtoEnumType::Constant>>
ProtoEnumType::FindConstantByName(absl::string_view name) const {
  const auto* value_desc = descriptor().FindValueByName(name);
  if (ABSL_PREDICT_FALSE(value_desc == nullptr)) {
    return absl::nullopt;
  }
  ABSL_ASSERT(value_desc->name() == name);
  return Constant{value_desc->name(), value_desc->number(), value_desc};
}

absl::StatusOr<absl::optional<ProtoEnumType::Constant>>
ProtoEnumType::FindConstantByNumber(int64_t number) const {
  if (ABSL_PREDICT_FALSE(number < std::numeric_limits<int>::min() ||
                         number > std::numeric_limits<int>::max())) {
    // Treat it as not found.
    return absl::nullopt;
  }
  const auto* value_desc =
      descriptor().FindValueByNumber(static_cast<int>(number));
  if (ABSL_PREDICT_FALSE(value_desc == nullptr)) {
    return absl::nullopt;
  }
  ABSL_ASSERT(value_desc->number() == number);
  return Constant{value_desc->name(), value_desc->number(), value_desc};
}

}  // namespace cel::extensions
