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

#include "base/types/struct_type.h"

#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "absl/types/variant.h"

namespace cel {

CEL_INTERNAL_TYPE_IMPL(StructType);

struct StructType::FindFieldVisitor final {
  const StructType& struct_type;
  TypeManager& type_manager;

  absl::StatusOr<Field> operator()(absl::string_view name) const {
    return struct_type.FindFieldByName(type_manager, name);
  }

  absl::StatusOr<Field> operator()(int64_t number) const {
    return struct_type.FindFieldByNumber(type_manager, number);
  }
};

absl::StatusOr<StructType::Field> StructType::FindField(
    TypeManager& type_manager, FieldId id) const {
  return absl::visit(FindFieldVisitor{*this, type_manager}, id.data_);
}

}  // namespace cel
