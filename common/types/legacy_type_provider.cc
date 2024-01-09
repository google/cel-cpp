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

// IWYU pragma: private

#include "common/types/legacy_type_provider.h"

#include "absl/status/status.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"
#include "common/type.h"
#include "common/type_provider.h"

namespace cel::common_internal {

absl::StatusOr<TypeView> LegacyTypeProvider::FindType(TypeFactory&,
                                                      absl::string_view,
                                                      Type&) {
  return absl::UnimplementedError(
      "google::api::expr::runtime::CelValue does not support type "
      "introspection");
}

absl::StatusOr<StructTypeFieldView>
LegacyTypeProvider::FindStructTypeFieldByName(TypeFactory&, absl::string_view,
                                              absl::string_view,
                                              StructTypeField&) {
  return absl::UnimplementedError(
      "google::api::expr::runtime::CelValue does not support type "
      "introspection");
}

absl::StatusOr<StructTypeFieldView>
LegacyTypeProvider::FindStructTypeFieldByName(TypeFactory&, StructTypeView,
                                              absl::string_view,
                                              StructTypeField&) {
  return absl::UnimplementedError(
      "google::api::expr::runtime::CelValue does not support type "
      "introspection");
}

}  // namespace cel::common_internal
