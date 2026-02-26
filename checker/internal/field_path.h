// Copyright 2025 Google LLC
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

#ifndef THIRD_PARTY_CEL_CPP_CHECKER_FIELD_PATH_H_
#define THIRD_PARTY_CEL_CPP_CHECKER_FIELD_PATH_H_

#include <string>
#include <utility>
#include <vector>

#include "absl/strings/str_join.h"
#include "absl/strings/str_split.h"
#include "absl/strings/string_view.h"
#include "absl/strings/substitute.h"
#include "absl/types/span.h"

namespace cel::checker_internal {

// Represents a single path within a FieldMask.
class FieldPath {
 public:
  explicit FieldPath(std::string path)
      : path_(std::move(path)),
        field_selection_(absl::StrSplit(path_, kPathDelimiter)) {}

  absl::string_view GetPath() const { return path_; }

  absl::Span<const std::string> GetFieldSelection() const {
    return field_selection_;
  }

  // Returns the first field name in the path.
  std::string GetFieldName() const { return field_selection_.front(); }

  std::string DebugString() const {
    return absl::Substitute(
        "FieldPath { field path: '$0', field selection: {'$1'} }", path_,
        absl::StrJoin(field_selection_, "', '"));
  }

 private:
  static inline constexpr char kPathDelimiter = '.';

  // The input path. For example: "f.b.d".
  std::string path_;
  // The list of nested field names in the path. For example: {"f", "b", "d"}.
  std::vector<std::string> field_selection_;
};

inline bool operator==(const FieldPath& lhs, const FieldPath& rhs) {
  return lhs.GetFieldSelection() == rhs.GetFieldSelection();
}

// Compares the field selections in the field paths.
// This is only intended as an arbitrary ordering for a set.
inline bool operator<(const FieldPath& lhs, const FieldPath& rhs) {
  return lhs.GetFieldSelection() < rhs.GetFieldSelection();
}

}  // namespace cel::checker_internal

#endif  // THIRD_PARTY_CEL_CPP_CHECKER_FIELD_PATH_H_
