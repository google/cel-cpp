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
#include <utility>
#include <vector>

#include "absl/strings/string_view.h"
#include "common/source.h"
#include "internal/utf8.h"
#include "policy/internal/alignment_table.h"

namespace cel::policy_internal {
namespace {

AlignmentTable ScanAlignmentTable(const SourceContentView& view,
                                  SourcePosition start_pos,
                                  SourcePosition end_pos, absl::string_view val,
                                  bool double_quoted, bool single_quoted) {
  std::vector<AlignmentPoint> points;
  if (val.empty()) {
    return AlignmentTable();
  }

  SourcePosition view_pos = start_pos;
  SourcePosition val_pos = 0;
  SourcePosition current_offset = -123456789;  // sentinel

  absl::string_view remaining = val;
  while (!remaining.empty()) {
    auto [cp, code_units] = cel::internal::Utf8Decode(remaining);
    remaining.remove_prefix(code_units);

    // If cp is whitespace, skip matching it directly; the next non-whitespace
    // token will anchor the alignment.
    if (cp == ' ' || cp == '\t' || cp == '\n' || cp == '\r') {
      val_pos++;
      continue;
    }

    // Search forward in view for cp
    while (view_pos < end_pos) {
      char32_t ch = view.at(view_pos);

      // Handle YAML escape sequences in double-quoted strings
      if (double_quoted && ch == '\\' && view_pos + 1 < end_pos) {
        char32_t next_ch = view.at(view_pos + 1);
        if (next_ch == 'x' || next_ch == 'u' || next_ch == 'U') {
          // This hex/unicode escape decoded to cp in val.
          break;
        }
        if ((next_ch == '"' && cp == '"') || (next_ch == '\\' && cp == '\\') ||
            (next_ch == '/' && cp == '/')) {
          break;
        }
        // For 2-char escape sequences like \n, \t, etc., since cp is
        // non-whitespace, \n or \t cannot match cp. We skip the escape
        // sequence.
        view_pos += 2;
        continue;
      }

      // Handle YAML single-quoted escaped quote ''
      if (single_quoted && ch == '\'' && view_pos + 1 < end_pos &&
          view.at(view_pos + 1) == '\'') {
        if (cp == '\'') {
          break;
        }
        view_pos += 2;
        continue;
      }

      if (ch == cp) {
        break;
      }
      view_pos++;
    }

    if (view_pos == end_pos) {
      break;
    }

    SourcePosition offset = view_pos - val_pos;
    if (points.empty() || offset != current_offset) {
      points.push_back(AlignmentPoint{val_pos, view_pos});
      current_offset = offset;
    }

    // Advance view_pos past this matched character or escape sequence
    if (double_quoted && view.at(view_pos) == '\\' && view_pos + 1 < end_pos) {
      char32_t next_ch = view.at(view_pos + 1);
      if (next_ch == 'x') {
        view_pos += 4;
      } else if (next_ch == 'u') {
        view_pos += 6;
      } else if (next_ch == 'U') {
        view_pos += 10;
      } else {
        view_pos += 2;
      }
    } else if (single_quoted && view.at(view_pos) == '\'' &&
               view_pos + 1 < end_pos && view.at(view_pos + 1) == '\'') {
      view_pos += 2;
    } else {
      view_pos++;
    }
    val_pos++;
  }

  if (points.empty()) {
    points.push_back(AlignmentPoint{0, start_pos});
  }
  return AlignmentTable(std::move(points));
}

std::pair<SourceRange, AlignmentTable> ScanDoubleQuotedExpression(
    const SourceContentView& view, SourcePosition pos, absl::string_view val) {
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
  AlignmentTable table =
      ScanAlignmentTable(view, start, cur, val, /*double_quoted=*/true,
                         /*single_quoted=*/false);
  return {SourceRange{start, cur}, std::move(table)};
}

std::pair<SourceRange, AlignmentTable> ScanSingleQuotedExpression(
    const SourceContentView& view, SourcePosition pos, absl::string_view val) {
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
  AlignmentTable table =
      ScanAlignmentTable(view, start, cur, val, /*double_quoted=*/false,
                         /*single_quoted=*/true);
  return {SourceRange{start, cur}, std::move(table)};
}

std::pair<SourceRange, AlignmentTable> ScanPlainOrBlockExpression(
    const SourceContentView& view, SourcePosition pos, absl::string_view val) {
  if (val.empty()) {
    return {SourceRange{pos, pos}, AlignmentTable()};
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
  AlignmentTable table =
      ScanAlignmentTable(view, start, view.size(), val, /*double_quoted=*/false,
                         /*single_quoted=*/false);
  return {SourceRange{start, cur}, std::move(table)};
}

}  // namespace

// Scans the YAML string element starting at `pos` in `view` with the parsed
// string value `val`.
//
// Returns a `YamlStringElement` reporting the starting position, optional
// source range, whether it was quoted, and the alignment table.
YamlStringElement ScanYamlStringElement(const SourceContentView& view,
                                        SourcePosition pos,
                                        absl::string_view val) {
  if (pos < 0 || pos >= view.size()) {
    return YamlStringElement{pos, std::nullopt, false, {}};
  }

  char32_t first_char = view.at(pos);
  if (first_char == '"') {
    auto [range, table] = ScanDoubleQuotedExpression(view, pos, val);
    return YamlStringElement{pos, range, true, table.release()};
  }
  if (first_char == '\'') {
    auto [range, table] = ScanSingleQuotedExpression(view, pos, val);
    return YamlStringElement{pos, range, true, table.release()};
  }
  auto [range, table] = ScanPlainOrBlockExpression(view, pos, val);
  return YamlStringElement{pos, range, false, table.release()};
}

}  // namespace cel::policy_internal
