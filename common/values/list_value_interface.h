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

// IWYU pragma: private, include "common/value.h"
// IWYU pragma: friend "common/value.h"

#ifndef THIRD_PARTY_CEL_CPP_COMMON_VALUES_LIST_VALUE_INTERFACE_H_
#define THIRD_PARTY_CEL_CPP_COMMON_VALUES_LIST_VALUE_INTERFACE_H_

#include <cstddef>
#include <string>

#include "absl/functional/function_ref.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/casting.h"
#include "common/json.h"
#include "common/type.h"
#include "common/type_manager.h"
#include "common/value_interface.h"
#include "common/value_kind.h"

namespace cel {

class ValueView;
class ListValue;
class ListValueView;

class ListValueInterface : public ValueInterface {
 public:
  using alternative_type = ListValue;
  using view_alternative_type = ListValueView;

  static constexpr ValueKind kKind = ValueKind::kList;

  ValueKind kind() const final { return kKind; }

  ListType GetType(TypeManager& type_manager) const {
    return Cast<ListType>(GetTypeImpl(type_manager));
  }

  absl::string_view GetTypeName() const final { return "list"; }

  absl::StatusOr<std::string> GetTypeUrl(absl::string_view prefix) const final;

  absl::StatusOr<Json> ConvertToJson(ValueManager& value_manager) const final {
    return ConvertToJsonArray(value_manager);
  }

  virtual absl::StatusOr<JsonArray> ConvertToJsonArray(
      ValueManager& value_manager) const = 0;

  using ForEachCallback = absl::FunctionRef<absl::StatusOr<bool>(ValueView)>;

  using ForEachWithIndexCallback =
      absl::FunctionRef<absl::StatusOr<bool>(size_t, ValueView)>;

 protected:
  Type GetTypeImpl(TypeManager& type_manager) const override {
    return Type(type_manager.GetDynListType());
  }
};

}  // namespace cel

#endif  // THIRD_PARTY_CEL_CPP_COMMON_VALUES_LIST_VALUE_INTERFACE_H_
