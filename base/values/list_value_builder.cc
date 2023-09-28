// Copyright 2022 Google LLC
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

#include "base/values/list_value_builder.h"

#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "base/kind.h"
#include "base/type.h"

namespace cel::base_internal {

absl::Status CheckListElement(const Type& expected_type, const Value& value) {
  const ValueKind value_kind = value.kind();
  if (ABSL_PREDICT_FALSE(value_kind == ValueKind::kError ||
                         value_kind == ValueKind::kUnknown)) {
    return TypeConversionError(*value.type(), expected_type);
  }
  const TypeKind expected_type_kind = expected_type.kind();
  if (ABSL_PREDICT_FALSE(expected_type_kind != TypeKind::kDyn &&
                         !expected_type.Equals(*value.type()))) {
    return TypeConversionError(*value.type(), expected_type);
  }
  return absl::OkStatus();
}

}  // namespace cel::base_internal
