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
#include <string>

#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/casting.h"
#include "common/value.h"
#include "internal/status_macros.h"

namespace cel::common_internal {

absl::StatusOr<ValueView> StructValueEqual(ValueManager& value_manager,
                                           StructValueView lhs,
                                           StructValueView rhs,
                                           Value& scratch) {
  if (lhs.GetTypeName() != rhs.GetTypeName()) {
    return BoolValueView{false};
  }
  absl::flat_hash_map<std::string, Value> lhs_fields;
  CEL_RETURN_IF_ERROR(lhs.ForEachField(
      value_manager,
      [&lhs_fields](absl::string_view name,
                    ValueView lhs_value) -> absl::StatusOr<bool> {
        lhs_fields.insert_or_assign(std::string(name), Value(lhs_value));
        return true;
      }));
  bool equal = true;
  size_t rhs_fields_count = 0;
  CEL_RETURN_IF_ERROR(rhs.ForEachField(
      value_manager,
      [&value_manager, &scratch, &lhs_fields, &equal, &rhs_fields_count](
          absl::string_view name, ValueView rhs_value) -> absl::StatusOr<bool> {
        auto lhs_field = lhs_fields.find(name);
        if (lhs_field == lhs_fields.end()) {
          equal = false;
          return false;
        }
        CEL_ASSIGN_OR_RETURN(
            auto result,
            lhs_field->second.Equal(value_manager, rhs_value, scratch));
        if (auto bool_value = As<BoolValueView>(result);
            bool_value.has_value() && !bool_value->NativeValue()) {
          equal = false;
          return false;
        }
        ++rhs_fields_count;
        return true;
      }));
  if (!equal || rhs_fields_count != lhs_fields.size()) {
    return BoolValueView{false};
  }
  return BoolValueView{true};
}

absl::StatusOr<ValueView> StructValueEqual(
    ValueManager& value_manager, const ParsedStructValueInterface& lhs,
    StructValueView rhs, Value& scratch) {
  if (lhs.GetTypeName() != rhs.GetTypeName()) {
    return BoolValueView{false};
  }
  absl::flat_hash_map<std::string, Value> lhs_fields;
  CEL_RETURN_IF_ERROR(lhs.ForEachField(
      value_manager,
      [&lhs_fields](absl::string_view name,
                    ValueView lhs_value) -> absl::StatusOr<bool> {
        lhs_fields.insert_or_assign(std::string(name), Value(lhs_value));
        return true;
      }));
  bool equal = true;
  size_t rhs_fields_count = 0;
  CEL_RETURN_IF_ERROR(rhs.ForEachField(
      value_manager,
      [&value_manager, &scratch, &lhs_fields, &equal, &rhs_fields_count](
          absl::string_view name, ValueView rhs_value) -> absl::StatusOr<bool> {
        auto lhs_field = lhs_fields.find(name);
        if (lhs_field == lhs_fields.end()) {
          equal = false;
          return false;
        }
        CEL_ASSIGN_OR_RETURN(
            auto result,
            lhs_field->second.Equal(value_manager, rhs_value, scratch));
        if (auto bool_value = As<BoolValueView>(result);
            bool_value.has_value() && !bool_value->NativeValue()) {
          equal = false;
          return false;
        }
        ++rhs_fields_count;
        return true;
      }));
  if (!equal || rhs_fields_count != lhs_fields.size()) {
    return BoolValueView{false};
  }
  return BoolValueView{true};
}

}  // namespace cel::common_internal
