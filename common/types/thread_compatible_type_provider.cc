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

// IWYU pragma: private

#include "common/types/thread_compatible_type_provider.h"

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "common/type.h"
#include "common/type_provider.h"

namespace cel::common_internal {

absl::StatusOr<TypeView> ThreadCompatibleTypeProvider::FindType(
    TypeFactory&, absl::string_view name, Type&) {
  return absl::NotFoundError(absl::StrCat("no such type: ", name));
}

absl::StatusOr<StructTypeFieldView>
ThreadCompatibleTypeProvider::FindStructTypeFieldByName(TypeFactory&,
                                                        absl::string_view type,
                                                        absl::string_view,
                                                        StructTypeField&) {
  return absl::NotFoundError(absl::StrCat("no such struct type: ", type));
}

absl::StatusOr<StructTypeFieldView>
ThreadCompatibleTypeProvider::FindStructTypeFieldByName(TypeFactory&,
                                                        StructTypeView type,
                                                        absl::string_view,
                                                        StructTypeField&) {
  return absl::NotFoundError(
      absl::StrCat("no such struct type: ", type.DebugString()));
}

}  // namespace cel::common_internal
