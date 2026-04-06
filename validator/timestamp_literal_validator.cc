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

#include "validator/timestamp_literal_validator.h"

#include "absl/base/no_destructor.h"
#include "absl/status/status.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "common/constant.h"
#include "common/navigable_ast.h"
#include "common/standard_definitions.h"
#include "internal/time.h"
#include "tools/navigable_ast.h"
#include "validator/validator.h"

namespace cel {
namespace {

bool ValidateTimestamps(ValidationContext& context) {
  bool valid = true;
  for (const auto& node :
       context.navigable_ast().Root().DescendantsPostorder()) {
    if (node.node_kind() != NodeKind::kCall ||
        node.expr()->call_expr().function() != StandardFunctions::kTimestamp) {
      continue;
    }
    if (node.children().size() != 1) {
      // Checker should have already reported an error.
      continue;
    }
    const NavigableAstNode& child = *node.children()[0];
    if (child.node_kind() != NodeKind::kConstant) {
      // Not a literal, so nothing to do.
      continue;
    }
    absl::Time ts;
    const Constant& constant = child.expr()->const_expr();
    if (constant.has_string_value()) {
      absl::string_view timestamp_str =
          child.expr()->const_expr().string_value();
      if (!absl::ParseTime(absl::RFC3339_full, timestamp_str, &ts, nullptr)) {
        context.ReportErrorAt(child.expr()->id(), "invalid timestamp literal");
        valid = false;
        continue;
      }
    } else if (constant.has_int_value()) {
      ts = absl::FromUnixSeconds(constant.int_value());
    } else {
      // Checker should have already reported an error.
      continue;
    }

    if (absl::Status status = internal::ValidateTimestamp(ts); !status.ok()) {
      context.ReportErrorAt(
          child.expr()->id(),
          absl::StrCat("invalid timestamp literal: ", status.message()));
      valid = false;
    }
  }

  return valid;
}

bool ValidateDurations(ValidationContext& context) {
  bool valid = true;
  for (const auto& node :
       context.navigable_ast().Root().DescendantsPostorder()) {
    if (node.node_kind() != NodeKind::kCall ||
        node.expr()->call_expr().function() != StandardFunctions::kDuration) {
      continue;
    }
    if (node.children().size() != 1) {
      // Checker should have already reported an error.
      continue;
    }
    const NavigableAstNode& child = *node.children()[0];
    if (child.node_kind() != NodeKind::kConstant) {
      // Not a literal, so nothing to do.
      continue;
    }
    const Constant& constant = child.expr()->const_expr();
    if (!constant.has_string_value()) {
      continue;
    }
    absl::Duration duration;

    absl::string_view duration_str = child.expr()->const_expr().string_value();
    if (!absl::ParseDuration(duration_str, &duration)) {
      context.ReportErrorAt(child.expr()->id(), "invalid duration literal");
      valid = false;
      continue;
    }

    if (absl::Status status = internal::ValidateDuration(duration);
        !status.ok()) {
      context.ReportErrorAt(
          child.expr()->id(),
          absl::StrCat("invalid duration literal: ", status.message()));
      valid = false;
    }
  }

  return valid;
}

}  // namespace

const Validation& TimestampLiteralValidator() {
  static const absl::NoDestructor<Validation> kInstance(
      ValidateTimestamps, "cel.validator.timestamp");
  return *kInstance;
}

// Returns a validator that checks duration literals.
const Validation& DurationLiteralValidator() {
  static const absl::NoDestructor<Validation> kInstance(
      ValidateDurations, "cel.validator.duration");
  return *kInstance;
}

}  // namespace cel
