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

#include "checker/standard_library.h"

#include "absl/status/status.h"
#include "checker/type_checker_builder.h"
#include "internal/status_macros.h"

namespace cel {
namespace {

absl::Status AddArithmeticOps(TypeCheckerBuilder& builder) {
  return absl::OkStatus();
}

absl::Status AddTypeConversions(TypeCheckerBuilder& builder) {
  return absl::OkStatus();
}

absl::Status AddEqualityOps(TypeCheckerBuilder& builder) {
  return absl::OkStatus();
}

absl::Status AddStandardLibraryDecls(TypeCheckerBuilder& builder) {
  CEL_RETURN_IF_ERROR(AddArithmeticOps(builder));
  CEL_RETURN_IF_ERROR(AddTypeConversions(builder));
  CEL_RETURN_IF_ERROR(AddEqualityOps(builder));

  return absl::OkStatus();
}

}  // namespace

// Returns a CheckerLibrary containing all of the standard CEL declarations.
CheckerLibrary StandardLibrary() { return {"stdlib", AddStandardLibraryDecls}; }
}  // namespace cel
