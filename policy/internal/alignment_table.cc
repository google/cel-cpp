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

#include "policy/internal/alignment_table.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "common/source.h"

namespace cel::policy_internal {

AlignmentTable::AlignmentTable(std::vector<AlignmentPoint> points)
    : points_(std::move(points)) {}

SourcePosition AlignmentTable::MapPosition(SourcePosition val_position) const {
  if (points_.empty()) {
    return val_position;
  }
  auto it = std::upper_bound(points_.begin(), points_.end(), val_position,
                             [](SourcePosition pos, const AlignmentPoint& pt) {
                               return pos < pt.value_position;
                             });
  if (it == points_.begin()) {
    return points_.front().source_position +
           (val_position - points_.front().value_position);
  }
  --it;
  return it->source_position + (val_position - it->value_position);
}

AlignmentTable AlignmentTable::CreateOffset(SourcePosition offset) {
  if (offset == 0) {
    return AlignmentTable();
  }
  return AlignmentTable({AlignmentPoint{0, offset}});
}

}  // namespace cel::policy_internal
