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

#include "base/values/map_value_builder.h"

#include "absl/base/macros.h"
#include "absl/base/optimization.h"
#include "absl/status/status.h"
#include "base/kind.h"
#include "base/type.h"
#include "base/values/map_value.h"
#include "internal/status_macros.h"

namespace cel::base_internal {

namespace {

absl::Status CheckMapKeyOrValue(TypeKind expected_type_kind,
                                const Type& expected_type, ValueKind value_kind,
                                const Value& value) {
  ABSL_ASSERT(expected_type_kind == expected_type.kind());
  ABSL_ASSERT(value_kind == value.kind());
  if (ABSL_PREDICT_FALSE(value_kind == ValueKind::kError ||
                         value_kind == ValueKind::kUnknown)) {
    return TypeConversionError(*value.type(), expected_type);
  }
  if (ABSL_PREDICT_FALSE(expected_type_kind != TypeKind::kDyn &&
                         !expected_type.Equals(*value.type()))) {
    return TypeConversionError(*value.type(), expected_type);
  }
  return absl::OkStatus();
}

}  // namespace

absl::Status CheckMapKey(const Type& expected_type, const Value& value) {
  const ValueKind value_kind = value.kind();
  const TypeKind expected_type_kind = expected_type.kind();
  CEL_RETURN_IF_ERROR(
      CheckMapKeyOrValue(expected_type_kind, expected_type, value_kind, value));
  if (expected_type_kind == TypeKind::kDyn) {
    return MapValue::CheckKey(value);
  }
  return absl::OkStatus();
}

absl::Status CheckMapValue(const Type& expected_type, const Value& value) {
  return CheckMapKeyOrValue(expected_type.kind(), expected_type, value.kind(),
                            value);
}

absl::Status CheckMapKeyAndValue(const Type& expected_key_type,
                                 const Type& expected_value_type,
                                 const Value& key_value,
                                 const Value& value_value) {
  CEL_RETURN_IF_ERROR(CheckMapKey(expected_key_type, key_value));
  return CheckMapValue(expected_value_type, value_value);
}

}  // namespace cel::base_internal
