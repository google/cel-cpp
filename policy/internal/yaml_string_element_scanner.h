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

#ifndef THIRD_PARTY_CEL_CPP_POLICY_INTERNAL_YAML_STRING_ELEMENT_SCANNER_H_
#define THIRD_PARTY_CEL_CPP_POLICY_INTERNAL_YAML_STRING_ELEMENT_SCANNER_H_

#include <optional>
#include <vector>

#include "absl/strings/string_view.h"
#include "common/source.h"
#include "policy/internal/alignment_table.h"

namespace cel::policy_internal {

struct YamlStringElement {
  SourcePosition starting_position = -1;
  std::optional<SourceRange> source_range;
  bool quoted = false;
  std::vector<AlignmentPoint> alignment_table;
};

// Scans a YAML scalar string element directly from the SourceContentView
// (behaving as an array of char32_t unicode codepoints) starting at `pos`,
// matching against the decoded value `val`.
YamlStringElement ScanYamlStringElement(const SourceContentView& view,
                                        SourcePosition pos,
                                        absl::string_view val);

}  // namespace cel::policy_internal

#endif  // THIRD_PARTY_CEL_CPP_POLICY_INTERNAL_YAML_STRING_ELEMENT_SCANNER_H_
