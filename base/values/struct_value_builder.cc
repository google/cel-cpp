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

#include "base/values/struct_value_builder.h"

#include <utility>

#include "absl/functional/overload.h"
#include "absl/types/variant.h"

namespace cel {

absl::Status StructValueBuilderInterface::SetField(StructValue::FieldId id,
                                                   Handle<Value> value) {
  return absl::visit(absl::Overload(
                         [this, &value](absl::string_view name) {
                           return SetFieldByName(name, std::move(value));
                         },
                         [this, &value](int64_t number) {
                           return SetFieldByNumber(number, std::move(value));
                         }),
                     id.data_);
}

}  // namespace cel
