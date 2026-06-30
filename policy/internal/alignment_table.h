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

#ifndef THIRD_PARTY_CEL_CPP_POLICY_INTERNAL_ALIGNMENT_TABLE_H_
#define THIRD_PARTY_CEL_CPP_POLICY_INTERNAL_ALIGNMENT_TABLE_H_

#include <utility>
#include <vector>

#include "common/source.h"

namespace cel::policy_internal {

struct AlignmentPoint {
  SourcePosition value_position = -1;   // Interpreted code point offset in val
  SourcePosition source_position = -1;  // Code point offset in raw YAML doc
};

class AlignmentTable {
 public:
  AlignmentTable() = default;
  explicit AlignmentTable(std::vector<AlignmentPoint> points);

  // Maps a SourcePosition in the interpreted CEL expression (val) to a
  // SourcePosition in the underlying YAML source document.
  SourcePosition MapPosition(SourcePosition val_position) const;

  bool empty() const { return points_.empty(); }
  const std::vector<AlignmentPoint>& points() const { return points_; }
  std::vector<AlignmentPoint> release() { return std::move(points_); }

  static AlignmentTable CreateOffset(SourcePosition offset);

 private:
  // A sparse table mapping code point offsets in the expression to offsets
  // in the YAML document. Only records entries where the relative delta
  // (source_position - value_position) shifts.
  std::vector<AlignmentPoint> points_;
};

}  // namespace cel::policy_internal

#endif  // THIRD_PARTY_CEL_CPP_POLICY_INTERNAL_ALIGNMENT_TABLE_H_
