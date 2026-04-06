// Copyright 2026 Google LLC
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

#include "validator/ast_depth_validator.h"

#include "absl/strings/str_cat.h"
#include "validator/validator.h"

namespace cel {

Validation AstDepthValidator(int max_depth) {
  return Validation([max_depth](ValidationContext& context) {
    int height = context.navigable_ast().Root().height();
    if (height > max_depth) {
      context.ReportError(absl::StrCat("AST depth ", height,
                                       " exceeds maximum of ", max_depth));
      return false;
    }
    return true;
  });
}

}  // namespace cel
