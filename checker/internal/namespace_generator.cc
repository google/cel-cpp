// Copyright 2024 Google LLC
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

#include "checker/internal/namespace_generator.h"

#include <algorithm>
#include <string>
#include <utility>
#include <vector>

#include "absl/functional/function_ref.h"
#include "absl/log/absl_check.h"
#include "absl/status/statusor.h"
#include "absl/strings/match.h"
#include "absl/strings/str_cat.h"
#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/types/span.h"
#include "common/container.h"
#include "internal/lexis.h"

namespace cel::checker_internal {
namespace {

bool FieldSelectInterpretationCandidatesImpl(
    absl::string_view prefix,
    absl::Span<const std::string> partly_qualified_name, bool prefix_is_alias,
    absl::FunctionRef<bool(absl::string_view, int)> callback) {
  for (int i = 0; i < partly_qualified_name.size(); ++i) {
    std::string buf;
    int count = partly_qualified_name.size() - i;
    auto end_idx = count - (prefix_is_alias ? 0 : 1);
    auto ident = absl::StrJoin(partly_qualified_name.subspan(0, count), ".");
    absl::string_view candidate = ident;
    if (absl::StartsWith(candidate, ".")) {
      candidate = candidate.substr(1);
    }
    if (!prefix.empty()) {
      buf = absl::StrCat(prefix, ".", candidate);
      candidate = buf;
    }
    if (!callback(candidate, end_idx)) {
      return false;
    }
  }
  if (prefix_is_alias) {
    return callback(prefix, 0);
  }
  return true;
}

bool FieldSelectInterpretationCandidates(
    absl::string_view prefix,
    absl::Span<const std::string> partly_qualified_name,
    absl::FunctionRef<bool(absl::string_view, int)> callback) {
  return FieldSelectInterpretationCandidatesImpl(
      prefix, partly_qualified_name, /*prefix_is_alias=*/false, callback);
}

bool FieldSelectInterpretationCandidatesWithAlias(
    absl::string_view prefix,
    absl::Span<const std::string> partly_qualified_name,
    absl::FunctionRef<bool(absl::string_view, int)> callback) {
  return FieldSelectInterpretationCandidatesImpl(
      prefix, partly_qualified_name, /*prefix_is_alias=*/true, callback);
}

}  // namespace

absl::StatusOr<NamespaceGenerator> NamespaceGenerator::Create(
    const ExpressionContainer& expression_container) {
  std::vector<std::string> candidates;

  absl::string_view container = expression_container.container();
  if (container.empty()) {
    return NamespaceGenerator(&expression_container, std::move(candidates));
  }

  std::string prefix;
  for (auto segment : absl::StrSplit(container, '.')) {
    // Assumes the the ExpressionContainer has already validated the container
    // and aliases.
    ABSL_DCHECK(internal::LexisIsIdentifier(segment));
    if (prefix.empty()) {
      prefix = segment;
    } else {
      absl::StrAppend(&prefix, ".", segment);
    }
    candidates.push_back(prefix);
  }
  std::reverse(candidates.begin(), candidates.end());
  return NamespaceGenerator(&expression_container, std::move(candidates));
}

void NamespaceGenerator::GenerateCandidates(
    absl::string_view simple_name,
    absl::FunctionRef<bool(absl::string_view)> callback) const {
  // Special case for root-relative names. Aliases still apply first.
  bool is_root_relative = absl::StartsWith(simple_name, ".");
  if (is_root_relative) {
    simple_name = simple_name.substr(1);
  }

  // The name is unqualified, but may include a namespace (struct creation).
  // This is just a quirk of the parser.
  if (auto dot_pos = simple_name.find('.');
      dot_pos != absl::string_view::npos) {
    absl::string_view first_segment = simple_name.substr(0, dot_pos);
    absl::string_view rest = simple_name.substr(dot_pos + 1);
    if (auto resolved_alias = expression_container_->FindAlias(first_segment);
        !resolved_alias.empty()) {
      callback(absl::StrCat(resolved_alias, ".", rest));
      return;
    }
  } else {
    if (auto resolved_alias = expression_container_->FindAlias(simple_name);
        !resolved_alias.empty()) {
      callback(resolved_alias);
      return;
    }
  }

  if (is_root_relative) {
    callback(simple_name);
    return;
  }

  for (const auto& prefix : candidates_) {
    std::string candidate = absl::StrCat(prefix, ".", simple_name);
    if (!callback(candidate)) {
      return;
    }
  }
  callback(simple_name);
}

void NamespaceGenerator::GenerateCandidates(
    absl::Span<const std::string> partly_qualified_name,
    absl::FunctionRef<bool(absl::string_view, int)> callback) const {
  if (partly_qualified_name.empty()) {
    return;
  }

  // Special case for root-relative names. Aliases still apply first.
  absl::string_view first_segment = partly_qualified_name[0];
  bool is_root_relative = absl::StartsWith(first_segment, ".");
  if (is_root_relative) {
    first_segment = first_segment.substr(1);
  }

  if (auto resolved_alias = expression_container_->FindAlias(first_segment);
      !resolved_alias.empty()) {
    FieldSelectInterpretationCandidatesWithAlias(
        resolved_alias, partly_qualified_name.subspan(1), callback);
    // If the alias matches, we don't check the container even if name
    // resolution fails.
    return;
  }

  if (is_root_relative) {
    FieldSelectInterpretationCandidates("", partly_qualified_name, callback);
    return;
  }

  for (const auto& prefix : candidates_) {
    if (!FieldSelectInterpretationCandidates(prefix, partly_qualified_name,
                                             callback)) {
      return;
    }
  }
  FieldSelectInterpretationCandidates("", partly_qualified_name, callback);
}

}  // namespace cel::checker_internal
