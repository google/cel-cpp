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

#include <cstddef>

#include "absl/log/absl_check.h"
#include "absl/status/statusor.h"
#include "common/casting.h"
#include "common/value.h"
#include "internal/status_macros.h"

namespace cel {

namespace common_internal {

absl::StatusOr<ValueView> ListValueEqual(ValueManager& value_manager,
                                         ListValueView lhs, ListValueView rhs,
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
  CEL_ASSIGN_OR_RETURN(auto rhs_iterator, rhs.NewIterator(value_manager));
  Value lhs_scratch;
  Value rhs_scratch;
  for (size_t index = 0; index < lhs_size; ++index) {
    ABSL_CHECK(lhs_iterator->HasNext());  // Crash OK
    ABSL_CHECK(rhs_iterator->HasNext());  // Crash OK
    CEL_ASSIGN_OR_RETURN(auto lhs_element, lhs_iterator->Next(lhs_scratch));
    CEL_ASSIGN_OR_RETURN(auto rhs_element, rhs_iterator->Next(lhs_scratch));
    CEL_ASSIGN_OR_RETURN(
        auto result, lhs_element.Equal(value_manager, rhs_element, scratch));
    if (auto bool_value = As<BoolValueView>(result);
        bool_value.has_value() && !bool_value->NativeValue()) {
      return result;
    }
  }
  ABSL_DCHECK(!lhs_iterator->HasNext());
  ABSL_DCHECK(!rhs_iterator->HasNext());
  return BoolValueView{true};
}

absl::StatusOr<ValueView> ListValueEqual(ValueManager& value_manager,
                                         const ParsedListValueInterface& lhs,
                                         ListValueView rhs, Value& scratch) {
  const auto lhs_size = lhs.Size();
  const auto rhs_size = rhs.Size();
  if (lhs_size != rhs_size) {
    return BoolValueView{false};
  }
  CEL_ASSIGN_OR_RETURN(auto lhs_iterator, lhs.NewIterator(value_manager));
  CEL_ASSIGN_OR_RETURN(auto rhs_iterator, rhs.NewIterator(value_manager));
  Value lhs_scratch;
  Value rhs_scratch;
  for (size_t index = 0; index < lhs_size; ++index) {
    ABSL_CHECK(lhs_iterator->HasNext());  // Crash OK
    ABSL_CHECK(rhs_iterator->HasNext());  // Crash OK
    CEL_ASSIGN_OR_RETURN(auto lhs_element, lhs_iterator->Next(lhs_scratch));
    CEL_ASSIGN_OR_RETURN(auto rhs_element, rhs_iterator->Next(lhs_scratch));
    CEL_ASSIGN_OR_RETURN(
        auto result, lhs_element.Equal(value_manager, rhs_element, scratch));
    if (auto bool_value = As<BoolValueView>(result);
        bool_value.has_value() && !bool_value->NativeValue()) {
      return result;
    }
  }
  ABSL_DCHECK(!lhs_iterator->HasNext());
  ABSL_DCHECK(!rhs_iterator->HasNext());
  return BoolValueView{true};
}

}  // namespace common_internal

}  // namespace cel
