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

#include <cstddef>
#include <tuple>

#include "absl/base/attributes.h"
#include "absl/log/absl_check.h"
#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "common/casting.h"
#include "common/value.h"
#include "common/value_kind.h"
#include "internal/status_macros.h"

namespace cel {

namespace {

absl::Status InvalidMapKeyTypeError(ValueKind kind) {
  return absl::InvalidArgumentError(
      absl::StrCat("Invalid map key type: '", ValueKindToString(kind), "'"));
}

}  // namespace

namespace common_internal {

absl::StatusOr<ValueView> MapValueEqual(ValueManager& value_manager,
                                        MapValueView lhs, MapValueView rhs,
                                        Value& scratch) {
  if (Is(lhs, rhs)) {
    return BoolValueView{true};
  }
  const auto lhs_size = lhs.Size();
  const auto rhs_size = rhs.Size();
  if (lhs_size != rhs_size) {
    return BoolValueView{false};
  }
  CEL_ASSIGN_OR_RETURN(auto lhs_iterator, lhs.NewIterator(value_manager));
  Value lhs_key_scratch;
  Value rhs_value_scratch;
  ValueView rhs_value;
  Value lhs_value_scratch;
  for (size_t index = 0; index < lhs_size; ++index) {
    ABSL_CHECK(lhs_iterator->HasNext());  // Crash OK
    CEL_ASSIGN_OR_RETURN(auto lhs_key, lhs_iterator->Next(lhs_key_scratch));
    bool rhs_value_found;
    CEL_ASSIGN_OR_RETURN(std::tie(rhs_value, rhs_value_found),
                         rhs.Find(value_manager, lhs_key, rhs_value_scratch));
    if (!rhs_value_found) {
      return BoolValueView{false};
    }
    CEL_ASSIGN_OR_RETURN(auto lhs_value,
                         lhs.Get(value_manager, lhs_key, lhs_value_scratch));
    CEL_ASSIGN_OR_RETURN(auto result,
                         lhs_value.Equal(value_manager, rhs_value, scratch));
    if (auto bool_value = As<BoolValueView>(result);
        bool_value.has_value() && !bool_value->NativeValue()) {
      return result;
    }
  }
  ABSL_DCHECK(!lhs_iterator->HasNext());
  return BoolValueView{true};
}

absl::StatusOr<ValueView> MapValueEqual(ValueManager& value_manager,
                                        const ParsedMapValueInterface& lhs,
                                        MapValueView rhs, Value& scratch) {
  const auto lhs_size = lhs.Size();
  const auto rhs_size = rhs.Size();
  if (lhs_size != rhs_size) {
    return BoolValueView{false};
  }
  CEL_ASSIGN_OR_RETURN(auto lhs_iterator, lhs.NewIterator(value_manager));
  Value lhs_key_scratch;
  Value rhs_value_scratch;
  ValueView rhs_value;
  Value lhs_value_scratch;
  for (size_t index = 0; index < lhs_size; ++index) {
    ABSL_CHECK(lhs_iterator->HasNext());  // Crash OK
    CEL_ASSIGN_OR_RETURN(auto lhs_key, lhs_iterator->Next(lhs_key_scratch));
    bool rhs_value_found;
    CEL_ASSIGN_OR_RETURN(std::tie(rhs_value, rhs_value_found),
                         rhs.Find(value_manager, lhs_key, rhs_value_scratch));
    if (!rhs_value_found) {
      return BoolValueView{false};
    }
    CEL_ASSIGN_OR_RETURN(auto lhs_value,
                         lhs.Get(value_manager, lhs_key, lhs_value_scratch));
    CEL_ASSIGN_OR_RETURN(auto result,
                         lhs_value.Equal(value_manager, rhs_value, scratch));
    if (auto bool_value = As<BoolValueView>(result);
        bool_value.has_value() && !bool_value->NativeValue()) {
      return result;
    }
  }
  ABSL_DCHECK(!lhs_iterator->HasNext());
  return BoolValueView{true};
}

}  // namespace common_internal

absl::Status CheckMapKey(ValueView key) {
  switch (key.kind()) {
    case ValueKind::kBool:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kInt:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kUint:
      ABSL_FALLTHROUGH_INTENDED;
    case ValueKind::kString:
      return absl::OkStatus();
    default:
      return InvalidMapKeyTypeError(key.kind());
  }
}

}  // namespace cel
