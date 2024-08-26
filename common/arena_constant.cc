// Copyright 2024 Google LLC
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

#include "common/arena_constant.h"

#include <cstddef>
#include <cstdint>

#include "absl/base/nullability.h"
#include "absl/functional/overload.h"
#include "absl/strings/string_view.h"
#include "absl/time/time.h"
#include "absl/types/variant.h"
#include "common/arena_bytes_pool.h"
#include "common/arena_string_pool.h"
#include "common/constant.h"

namespace cel {

absl::string_view ArenaConstantKindName(ArenaConstantKind constant_kind) {
  switch (constant_kind) {
    case ArenaConstantKind::kUnspecified:
      return "UNSPECIFIED";
    case ArenaConstantKind::kNull:
      return "NULL";
    case ArenaConstantKind::kBool:
      return "BOOL";
    case ArenaConstantKind::kInt:
      return "INT";
    case ArenaConstantKind::kUint:
      return "UINT";
    case ArenaConstantKind::kDouble:
      return "DOUBLE";
    case ArenaConstantKind::kBytes:
      return "BYTES";
    case ArenaConstantKind::kString:
      return "STRING";
    case ArenaConstantKind::kDuration:
      return "DURATION";
    case ArenaConstantKind::kTimestamp:
      return "TIMESTAMP";
    default:
      return "ERROR";
  }
}

ArenaConstant MakeArenaConstant(absl::Nonnull<ArenaStringPool*> string_pool,
                                absl::Nonnull<ArenaBytesPool*> bytes_pool,
                                const Constant& constant) {
  return absl::visit(
      absl::Overload([](absl::monostate) { return ArenaConstant(); },
                     [](std::nullptr_t value) { return ArenaConstant(value); },
                     [](bool value) { return ArenaConstant(value); },
                     [](int64_t value) { return ArenaConstant(value); },
                     [](uint64_t value) { return ArenaConstant(value); },
                     [](double value) { return ArenaConstant(value); },
                     [&](const BytesConstant& value) {
                       return ArenaConstant(bytes_pool->InternBytes(value));
                     },
                     [&](const StringConstant& value) {
                       return ArenaConstant(string_pool->InternString(value));
                     },
                     [](absl::Duration value) { return ArenaConstant(value); },
                     [](absl::Time value) { return ArenaConstant(value); }),
      constant.kind());
}

}  // namespace cel
