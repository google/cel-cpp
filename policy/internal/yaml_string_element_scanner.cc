// Copyright 2026 Google LLC
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

#include "policy/internal/yaml_string_element_scanner.h"

#include <optional>

#include "absl/strings/string_view.h"
#include "common/source.h"
#include "internal/utf8.h"

namespace cel::policy_internal {
namespace {

SourceRange ScanDoubleQuotedExpression(const SourceContentView& view,
                                       SourcePosition pos) {
  SourcePosition start = pos + 1;
  SourcePosition cur = start;
  while (cur < view.size()) {
    char32_t ch = view.at(cur);
    if (ch == '\\') {
      cur += 2;
      continue;
    }
    if (ch == '"') {
      break;
    }
    cur++;
  }
  return SourceRange{start, cur};
}

SourceRange ScanSingleQuotedExpression(const SourceContentView& view,
                                       SourcePosition pos) {
  SourcePosition start = pos + 1;
  SourcePosition cur = start;
  while (cur < view.size()) {
    char32_t ch = view.at(cur);
    if (ch == '\'') {
      if (cur + 1 < view.size() && view.at(cur + 1) == '\'') {
        cur += 2;
        continue;
      }
      break;
    }
    cur++;
  }
  return SourceRange{start, cur};
}

SourceRange ScanPlainOrBlockExpression(const SourceContentView& view,
                                       SourcePosition pos,
                                       absl::string_view val) {
  if (val.empty()) {
    return SourceRange{pos, pos};
  }

  char32_t first_char = view.at(pos);
  SourcePosition start = pos;
  if (first_char == '|' || first_char == '>') {
    // Skip block header line
    while (start < view.size() && view.at(start) != '\n') {
      start++;
    }
    if (start < view.size() && view.at(start) == '\n') {
      start++;
    }
    while (start < view.size() &&
           (view.at(start) == ' ' || view.at(start) == '\t')) {
      start++;
    }
  }

  SourcePosition cur = start;
  absl::string_view remaining = val;
  while (!remaining.empty()) {
    auto [code_point, code_units] = cel::internal::Utf8Decode(remaining);
    remaining.remove_prefix(code_units);
    if (code_point == ' ' || code_point == '\t' || code_point == '\n') continue;
    while (cur < view.size() && view.at(cur) != code_point) {
      cur++;
    }
    if (cur < view.size()) cur++;
  }
  return SourceRange{start, cur};
}

}  // namespace

// Scans the YAML string element starting at `pos` in `view` with the parsed
// string value `val`.
//
// Returns a `YamlStringElement` reporting the starting position, optional
// source range, and whether it was quoted.
//
// TODO(b/525495513): Implement support for correctly aligning the source YAML
// to the decoded string value (the component CEL expression).
YamlStringElement ScanYamlStringElement(const SourceContentView& view,
                                        SourcePosition pos,
                                        absl::string_view val) {
  if (pos < 0 || pos >= view.size()) {
    return YamlStringElement{pos, std::nullopt, false};
  }

  char32_t first_char = view.at(pos);
  if (first_char == '"') {
    return YamlStringElement{pos, ScanDoubleQuotedExpression(view, pos), true};
  }
  if (first_char == '\'') {
    return YamlStringElement{pos, ScanSingleQuotedExpression(view, pos), true};
  }
  return YamlStringElement{pos, ScanPlainOrBlockExpression(view, pos, val),
                           false};
}

}  // namespace cel::policy_internal
