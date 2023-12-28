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

#include "common/values/thread_compatible_value_provider.h"

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/memory.h"
#include "common/type.h"
#include "common/value.h"

namespace cel::common_internal {

absl::StatusOr<Unique<StructValueBuilder>>
ThreadCompatibleValueProvider::NewStructValueBuilder(ValueFactory&,
                                                     StructTypeView type) {
  return absl::NotFoundError(
      absl::StrCat("value builder for type not found: ", type.DebugString()));
}

absl::StatusOr<ValueView> ThreadCompatibleValueProvider::FindValue(
    ValueFactory&, absl::string_view name, Value&) {
  return absl::NotFoundError(absl::StrCat("value not found: ", name));
}

}  // namespace cel::common_internal
