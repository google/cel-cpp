// Copyright 2021 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef THIRD_PARTY_CEL_CPP_TOOLS_REFERENCE_INLINER_H_
#define THIRD_PARTY_CEL_CPP_TOOLS_REFERENCE_INLINER_H_

#include <utility>

#include "google/api/expr/v1alpha1/checked.pb.h"
#include "absl/container/flat_hash_map.h"
#include "absl/status/statusor.h"
#include "absl/strings/string_view.h"

namespace cel::ast {

class Inliner {
 public:
  Inliner() {}
  explicit Inliner(absl::flat_hash_map<absl::string_view,
                                       const google::api::expr::v1alpha1::CheckedExpr*>
                       rewrites)
      : rewrites_(std::move(rewrites)) {}

  // Add a qualified ident to replace with a checked expression.
  // The supplied CheckedExpr must outlive the Inliner.
  // Replaces any existing rewrite rules for the given identifier -- the last
  // call will always overwrite any prior calls for a given identifier.
  absl::Status SetRewriteRule(absl::string_view qualified_identifier,
                              const google::api::expr::v1alpha1::CheckedExpr& expr);

  // Apply all of the rewrites to expr.
  // Returns an error if expr is not valid (i.e. unsupported expr ids).
  absl::StatusOr<google::api::expr::v1alpha1::CheckedExpr> Inline(
      const google::api::expr::v1alpha1::CheckedExpr& expr) const;

 private:
  absl::flat_hash_map<absl::string_view, const google::api::expr::v1alpha1::CheckedExpr*>
      rewrites_;
};

}  // namespace cel::ast
#endif  // THIRD_PARTY_CEL_CPP_TOOLS_REFERENCE_INLINER_H_
